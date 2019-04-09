/*
 * Copyright (C) 2014 - 2015 Vivien Malerba <malerba@gnome-db.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "gda-worker.h"
#include "itsignaler.h"
#include "background.h"
#include <gda-debug-macros.h>
#include <glib/gi18n-lib.h>

#define DEBUG_NOTIFICATION
#undef DEBUG_NOTIFICATION

typedef struct {
	ITSignaler        *its; /* ref held */
	GdaWorker         *worker; /* no ref held */
	GMainContext      *context; /* ref held */
	GdaWorkerCallback  callback;
	gpointer           user_data;
	guint              source_sig_id;
} DeclaredCallback;

static DeclaredCallback *
declared_callback_new (GdaWorker *worker, GMainContext *context, GdaWorkerCallback callback, gpointer user_data)
{
	g_assert (callback);
	g_assert (context);

	ITSignaler *its;
	its = itsignaler_new ();
	if (!its)
		return NULL;

	DeclaredCallback *dc;
	dc = g_slice_new0 (DeclaredCallback);
	dc->its = its;
	dc->worker = worker;
	dc->context = g_main_context_ref (context);
	dc->callback = callback;
	dc->user_data = user_data;
	dc->source_sig_id = 0;

	return dc;
}

static void
declared_callback_free (DeclaredCallback *dc)
{
	if (!dc)
		return;
	if (dc->source_sig_id)
		g_assert (itsignaler_remove (dc->its, dc->context, dc->source_sig_id));
	if (dc->its)
		itsignaler_unref (dc->its);
	g_main_context_unref (dc->context);
	g_slice_free (DeclaredCallback, dc);
}

/* structure is completely opaque for end user */
struct _GdaWorker {
	GRecMutex rmutex; /* protects all the attributes in GdaWorker and any WorkerJob */

	guint ref_count; /* +1 for any user
			 * Object is destroyed when ref_count reaches 0 */

	GThread *worker_thread; /* used to join thread when cleaning up, created when 1st job is submitted */
	gint8 worker_must_quit; /* set to 1 if worker is requested to exit */

	ITSignaler *submit_its; /* used to submit jobs to worker thread, jobs IDs are pushed */

	GHashTable *callbacks_hash; /* used to declare callbacks
				     * gda_worker_set_callback() was called
				     * key = a GMainContext (ref held),
				     * value = corresponding DeclaredCallback (memory managed here) */

	GHashTable *jobs_hash; /* locked by @mutex when reading or writing, by any thread
				* key = a #WorkerJob's job ID, value = the #WorkerJob */

	GdaWorker **location;
};

GdaWorker *
gda_worker_copy (GdaWorker *src) {
	return gda_worker_ref (src);
}
void
gda_worker_free (GdaWorker *w) {
	gda_worker_unref (w);
}

G_DEFINE_BOXED_TYPE(GdaWorker, gda_worker, gda_worker_copy, gda_worker_free)


/* module error */
GQuark gda_worker_error_quark (void)
{
        static GQuark quark;
        if (!quark)
                quark = g_quark_from_static_string ("gda_worker_error");
        return quark;
}

/*
 * Individual jobs
 *
 * If the status is in JOB_QUEUED or JOB_BEING_PROCESSED, then it may be in the ITSignaler's internal queue
 * It the status is JOB_PROCESSED, then it will _not_ be in the ITSignaler's internal queue
 */
typedef enum {
	JOB_QUEUED =          1,
	JOB_BEING_PROCESSED = 1 << 1,
	JOB_PROCESSED       = 1 << 2,
	JOB_CANCELLED       = 1 << 3,
} JobStatus;

typedef struct {
	guint          id;
	JobStatus      status; /* protected by associated worker's mutex */

	ITSignaler    *reply_its; /* where to push the result to, ref held */

	/* information to call function in worker thread */
	GdaWorkerFunc  func;
	gpointer       data;
	GDestroyNotify data_destroy_func; /* to destroy @data */

	/* result */
	gpointer       result;
	GDestroyNotify result_destroy_func; /* to destroy @result */
	GError        *error;
} WorkerJob;

static WorkerJob *
worker_job_new (ITSignaler *reply_its, GdaWorkerFunc func, gpointer data, GDestroyNotify data_destroy_func,
		GDestroyNotify result_destroy_func)
{
	static guint counter = 0;

	WorkerJob *job;
	job = g_slice_new0 (WorkerJob);
	counter ++;
	job->id = counter;
	job->status = JOB_QUEUED;
	job->reply_its = reply_its ? itsignaler_ref (reply_its) : NULL;

	job->func = func;
	job->data = data;
	job->data_destroy_func = data_destroy_func;

	job->result = NULL;
	job->result_destroy_func = result_destroy_func;
	job->error = NULL;

	return job;
}

static void
worker_job_free (WorkerJob *job)
{
	if (!job)
		return;
	if (job->data_destroy_func && job->data)
		job->data_destroy_func (job->data);
	if (job->result) {
		if (job->result_destroy_func)
			job->result_destroy_func (job->result);
		else
			g_warning (_("Possible memory leak as no destroy function provided to free value returned by the worker thread"));
	}
	if (job->error)
		g_error_free (job->error);
	if (job->reply_its)
		itsignaler_unref (job->reply_its);
	g_slice_free (WorkerJob, job);
}

/*
 * main function of the worker thread
 */
static gpointer
worker_thread_main (GdaWorker *worker)
{
#define TIMER 150
#ifdef DEBUG_NOTIFICATION
	g_print ("[W] GdaWorker %p, worker thread %p started!\n", worker, g_thread_self());
#endif
	itsignaler_ref (worker->submit_its);
	while (1) {
		WorkerJob *job;
		job = itsignaler_pop_notification (worker->submit_its, TIMER);
		if (job) {
			g_rec_mutex_lock (&worker->rmutex);
			if (! (job->status & JOB_CANCELLED)) {
				/* handle job */
				job->status |= JOB_BEING_PROCESSED;
				g_rec_mutex_unlock (&worker->rmutex);

				job->result = job->func (job->data, & job->error);

				g_rec_mutex_lock (&worker->rmutex);
				job->status |= JOB_PROCESSED;
				if (job->reply_its)
					itsignaler_push_notification (job->reply_its, job, NULL);
			}

			if ((job->status & JOB_CANCELLED) && worker->jobs_hash)
				g_hash_table_remove (worker->jobs_hash, &job->id);
			g_rec_mutex_unlock (&worker->rmutex);
		}

		if (worker->worker_must_quit) {
#ifdef DEBUG_NOTIFICATION
			g_print ("[W] GdaWorker %p, worker thread %p finished!\n", worker, g_thread_self());
#endif
			itsignaler_unref (worker->submit_its);
			g_rec_mutex_clear (& worker->rmutex);
			g_slice_free (GdaWorker, worker);
			bg_update_stats (BG_DESTROYED_WORKER);

			bg_join_thread ();
			return NULL;
		}
	}
	return NULL;
}

/**
 * gda_worker_new:
 *
 * Creates a new #GdaWorker object.
 *
 * Returns: (transfer full): a new #GdaWorker, or %NULL if an error occurred
 *
 * Since: 6.0
 */
GdaWorker *
gda_worker_new (void)
{
	GdaWorker *worker;
	worker = bg_get_spare_gda_worker ();
	if (worker)
		return worker;

	worker = g_slice_new0 (GdaWorker);
	worker->submit_its = itsignaler_new ();
	if (!worker->submit_its) {
		g_slice_free (GdaWorker, worker);
		return NULL;
	}

	worker->ref_count = 1;

	worker->callbacks_hash = g_hash_table_new_full (NULL, NULL, NULL,
							(GDestroyNotify) declared_callback_free);
	worker->jobs_hash = g_hash_table_new_full (g_int_hash, g_int_equal, NULL, (GDestroyNotify) worker_job_free);

	worker->worker_must_quit = 0;
	worker->location = NULL;

	g_rec_mutex_init (& worker->rmutex);

	gchar *str;
	static guint counter = 0;
	str = g_strdup_printf ("gdaWrkrTh%u", counter);
	counter++;
	worker->worker_thread = g_thread_try_new (str, (GThreadFunc) worker_thread_main, worker, NULL);
	g_free (str);
	if (!worker->worker_thread) {
		itsignaler_unref (worker->submit_its);
		g_hash_table_destroy (worker->callbacks_hash);
		g_hash_table_destroy (worker->jobs_hash);
		g_rec_mutex_clear (& worker->rmutex);
		g_slice_free (GdaWorker, worker);
		return NULL;
	}
	else
		bg_update_stats (BG_STARTED_THREADS);

#ifdef DEBUG_NOTIFICATION
	g_print ("[W] created GdaWorker %p\n", worker);
#endif

	bg_update_stats (BG_CREATED_WORKER);
	return worker;
}

static GMutex unique_worker_mutex;

/**
 * gda_worker_new_unique:
 * @location: a place to store and test for existence, not %NULL
 * @allow_destroy: defines if the created @GdaWorker (see case 1 below) will allow its reference to drop to 0 and be destroyed
 *
 * This function creates a new #GdaWorker, or reuses the one at @location. Specifically:
 * <orderedlist>
 *   <listitem><para>if *@location is %NULL, then a new #GdaWorker is created. In this case if @allow_destroy is %FALSE, then the returned 
 *     #GdaWorker's reference count is 2, thus preventing it form ever being destroyed (unless gda_worker_unref() is called somewhere else)</para></listitem>
 *   <listitem><para>if *@location is not %NULL, the the #GdaWorker it points to is returned, its reference count increased by 1</para></listitem>
 * </orderedlist>
 *
 * When the returned #GdaWorker's reference count reaches 0, then it is destroyed, and *@location is set to %NULL.
 *
 * In any case, the returned value is the same as *@location.
 *
 * Returns: (transfer full): a #GdaWorker
 */
GdaWorker *
gda_worker_new_unique (GdaWorker **location, gboolean allow_destroy)
{
	g_return_val_if_fail (location, NULL);

	g_mutex_lock (&unique_worker_mutex);
	if (*location)
		gda_worker_ref (*location);
	else {
		GdaWorker *worker;
		worker = gda_worker_new ();
		if (! allow_destroy)
			gda_worker_ref (worker);
		worker->location = location;
		*location = worker;
	}
	g_mutex_unlock (&unique_worker_mutex);

	return *location;
}

/**
 * gda_worker_ref:
 * @worker: a #GdaWorker
 *
 * Increases @worker's reference count.
 *
 * Returns: (transfer full): @worker
 *
 * Since: 6.0
 */
GdaWorker *
gda_worker_ref (GdaWorker *worker)
{
	g_return_val_if_fail (worker, NULL);
	g_rec_mutex_lock (& worker->rmutex);
	worker->ref_count ++;
#ifdef DEBUG_NOTIFICATION
	g_print ("[W] GdaWorker %p reference increased to %u\n", worker, worker->ref_count);
#endif
	g_rec_mutex_unlock (& worker->rmutex);

	return worker;
}

void
_gda_worker_unref (GdaWorker *worker, gboolean give_to_bg)
{
	g_assert (worker);

	gboolean unique_locked = FALSE;
	if (worker->location) {
		g_mutex_lock (&unique_worker_mutex);
		unique_locked = TRUE;
	}

#ifdef DEBUG_NOTIFICATION
	g_print ("[W] GdaWorker %p reference decreased to ", worker);
#endif

	g_rec_mutex_lock (& worker->rmutex);
	worker->ref_count --;

#ifdef DEBUG_NOTIFICATION
	g_print ("%u\n", worker->ref_count);
#endif

	if (worker->ref_count == 0) {
		/* destroy all the interal resources which will not be reused even if the GdaWorker is reused */
		g_hash_table_destroy (worker->callbacks_hash);
		worker->callbacks_hash = NULL;
		g_hash_table_destroy (worker->jobs_hash);
		worker->jobs_hash = NULL;
		if (worker->location)
			*(worker->location) = NULL;

		if (give_to_bg) {
			/* re-create the resources so the GdaWorker is ready to be used again */
			worker->ref_count = 1;
			worker->callbacks_hash = g_hash_table_new_full (NULL, NULL, NULL,
									(GDestroyNotify) declared_callback_free);
			worker->jobs_hash = g_hash_table_new_full (g_int_hash, g_int_equal, NULL, (GDestroyNotify) worker_job_free);
			worker->worker_must_quit = 0;
			worker->location = NULL;

			bg_set_spare_gda_worker (worker);
		}
		else {
			/* REM: we don't need to g_thread_join() the worker thread because the "background" will do it */

			/* free all the resources used by @worker */
			itsignaler_unref (worker->submit_its);
			worker->worker_must_quit = 1; /* request the worker thread to exit */
		}
		g_rec_mutex_unlock (& worker->rmutex);
	}
	else
		g_rec_mutex_unlock (& worker->rmutex);

	if (unique_locked)
		g_mutex_unlock (&unique_worker_mutex);
}

void
_gda_worker_bg_unref (GdaWorker *worker)
{
	g_assert (worker);
	g_assert (worker->ref_count == 1);

	_gda_worker_unref (worker, FALSE);
}

/**
 * gda_worker_unref:
 * @worker: (nullable): a #GdaWorker, or %NULL
 *
 * Decreases @worker's reference count. When reference count reaches %0, then the
 * object is destroyed, note that in this case this function only returns when the
 * worker thread actually has terminated, which can take some time if it's busy.
 *
 * If @worker is %NULL, then nothing happens.
 *
 * Since: 6.0
 */
void
gda_worker_unref (GdaWorker *worker)
{
	if (worker)
		_gda_worker_unref (worker, TRUE);
}

/*
 * WARNING: calling this function, the worker->rmutex _must_ be locked
 *
 * Returns: a job ID, or %0 if an error occurred
 */
static guint
_gda_worker_submit_job_with_its (GdaWorker *worker, ITSignaler *reply_its, GdaWorkerFunc func,
				 gpointer data, GDestroyNotify data_destroy_func,
				 GDestroyNotify result_destroy_func, G_GNUC_UNUSED GError **error)
{
	guint jid = 0;
	WorkerJob *job;
	job = worker_job_new (reply_its, func, data, data_destroy_func, result_destroy_func);
	g_assert (job);
	if (worker->worker_thread == g_thread_self ()) {
		/* run the job right away */
		g_hash_table_insert (worker->jobs_hash, & job->id, job);
		jid = job->id;
		job->status |= JOB_BEING_PROCESSED;
		job->result = job->func (job->data, & job->error);
		job->status |= JOB_PROCESSED;
		if (job->reply_its)
			itsignaler_push_notification (job->reply_its, job, NULL);
	}
	else {
		if (itsignaler_push_notification (worker->submit_its, job, NULL)) {
			g_hash_table_insert (worker->jobs_hash, & job->id, job);
			jid = job->id;
		}
		else
			worker_job_free (job);
	}

	return jid;
}

/**
 * gda_worker_submit_job:
 * @worker: a #GdaWorker object
 * @callback_context: (nullable): a #GMainContext, or %NULL (ignored if no setting has been defined with gda_worker_set_callback())
 * @func: the function to call from the worker thread
 * @data: (nullable): the data to pass to @func, or %NULL
 * @data_destroy_func: (nullable): a function to destroy @data, or %NULL
 * @result_destroy_func: (nullable): a function to destroy the result, if any, of the execution of @func, or %NULL
 * @error: (nullable): a place to store errors, or %NULL.
 *
 * Request that the worker thread call @func with the @data argument.
 *
 * Notes:
 * <itemizedlist>
 *   <listitem><para>if @data_destroy_func is not %NULL, then it will be called to destroy @data when the job is removed,
 *    which can occur within the context of the worker thread, or within the context of any thread using @worker.</para></listitem>
 *   <listitem><para>if @result_destroy_func is not %NULL, then it will be called to destroy the result produced by @func.
 *    Similarly to @data_destroy_func, if it is not %NULL (and if there is a non %NULL result), then that function can be
 *    called in the context of any thread.</para></listitem>
 *   <listitem><para>the error here can only report failures while executing gda_worker_submit_job(), not any error which may occur
 *    while executing @func from the worker thread.</para></listitem>
 *   <listitem><para>when this function returns, the job may already have been completed, so you should not assume that the job
 *    is in any specific state.</para></listitem>
 *  <listitem><para>passing %NULL for @callback_context is similar to passing the result of g_main_context_ref_thread_default()</para></listitem>
 * </itemizedlist>
 *
 * Returns: a job ID, or %0 if an error occurred
 *
 * Since: 6.0
 */
guint
gda_worker_submit_job (GdaWorker *worker, GMainContext *callback_context, GdaWorkerFunc func,
		       gpointer data, GDestroyNotify data_destroy_func,
		       GDestroyNotify result_destroy_func, GError **error)
{
	g_return_val_if_fail (worker, 0);
	g_return_val_if_fail (func, 0);
	if (!worker->callbacks_hash) {
		g_warning ("GdaWorker has been destroyed\n");
		return 0;
	}

	DeclaredCallback *dc;
	guint jid;
	GMainContext *co;
	gboolean unref_co = FALSE;

	co = callback_context;
	if (!co) {
		co = g_main_context_ref_thread_default ();
		unref_co = TRUE;
	}

	g_rec_mutex_lock (& worker->rmutex);
	dc = g_hash_table_lookup (worker->callbacks_hash, co);
	jid = _gda_worker_submit_job_with_its (worker, dc ? dc->its : NULL,
					       func, data, data_destroy_func, result_destroy_func, error);
	g_rec_mutex_unlock (& worker->rmutex);

	if (unref_co)
		g_main_context_unref (co);

	return jid;
}

/**
 * gda_worker_fetch_job_result:
 * @worker: a #GdaWorker object
 * @job_id: the ID of the job, as returned by gda_worker_submit_job()
 * @out_result: (nullable): a place to store the value returned by the execution of the requested function within the worker thread, or %NULL
 * @error: (nullable): a place to store errors, or %NULL
 *
 * Fetch the value returned by execution the @job_id job.
 *
 * Warning: if an error occurred during the
 * execution of the requested function within the worker thread, then it will show as @error, while the return value
 * of this function will be %TRUE.
 *
 * Note: if there is a result, it will be stored in @out_result, and it's up to the caller to free
 * the result, the #GdaWorker object will not do it (ownership of the result is transfered to the caller).
 *
 * Returns: %TRUE if the jobs has completed
 *
 * Since: 6.0
 */
gboolean
gda_worker_fetch_job_result (GdaWorker *worker, guint job_id, gpointer *out_result, GError **error)
{
	g_return_val_if_fail (worker, FALSE);
	if (!worker->jobs_hash) {
		g_warning ("GdaWorker has been destroyed\n");
		return FALSE;
	}

	gda_worker_ref (worker); /* increase reference count to having @worker freed while destroying job */

	if (out_result)
		*out_result = NULL;

	g_rec_mutex_lock (&worker->rmutex);
	WorkerJob *job;
	job = g_hash_table_lookup (worker->jobs_hash, &job_id);
	if (!job) {
		g_rec_mutex_unlock (& worker->rmutex);
		g_set_error (error, GDA_WORKER_ERROR, GDA_WORKER_JOB_NOT_FOUND_ERROR,
			     _("Unknown requested job %u"), job_id);
		gda_worker_unref (worker);
		return FALSE;
	}

	gboolean retval = FALSE;
	if (job->status & JOB_CANCELLED)
		g_set_error (error, GDA_WORKER_ERROR, GDA_WORKER_JOB_CANCELLED_ERROR,
			     _("Job %u has been cancelled"), job_id);
	else if (job->status & JOB_PROCESSED) {
		retval = TRUE;
		if (out_result) {
			*out_result = job->result;
			job->result = NULL;
		}
		if (job->error) {
			g_propagate_error (error, job->error);
			job->error = NULL;
		}
		g_hash_table_remove (worker->jobs_hash, &job_id);
	}
	else if (job->status & JOB_BEING_PROCESSED)
		g_set_error (error, GDA_WORKER_ERROR, GDA_WORKER_JOB_BEING_PROCESSED_ERROR,
			     _("Job %u is being processed"), job_id);
	else
		g_set_error (error, GDA_WORKER_ERROR, GDA_WORKER_JOB_QUEUED_ERROR,
			     _("Job %u has not yet been processed"), job_id);
	g_rec_mutex_unlock (&worker->rmutex);

	gda_worker_unref (worker);

	return retval;
}

/**
 * gda_worker_cancel_job:
 * @worker: a #GdaWorker object
 * @job_id: the ID of the job, as returned by gda_worker_submit_job()
 * @error: (nullable): a place to store errors, or %NULL
 *
 * Cancels a job which has not yet been processed. If the job cannot be found, is being processed or has already been processed,
 * then this function returns %FALSE.
 *
 * This function can be called on already cancelled jobs, and simply returns %TRUE in that case.
 *
 * Returns: %TRUE if the job was cancelled
 *
 * Since: 6.0
 */
gboolean
gda_worker_cancel_job (GdaWorker *worker, guint job_id, GError **error)
{
	g_return_val_if_fail (worker, FALSE);

	if (!worker->jobs_hash) {
		g_warning ("GdaWorker has been destroyed\n");
		return FALSE;
	}

	g_rec_mutex_lock (& worker->rmutex);

	WorkerJob *job;
	job = g_hash_table_lookup (worker->jobs_hash, &job_id);
	if (!job) {
		g_rec_mutex_unlock (& worker->rmutex);
		g_set_error (error, GDA_WORKER_ERROR, GDA_WORKER_JOB_NOT_FOUND_ERROR,
			     _("Unknown requested job %u"), job_id);
		return FALSE;
	}

	gboolean retval = FALSE;
	if (job->status & JOB_BEING_PROCESSED)
		g_set_error (error, GDA_WORKER_ERROR, GDA_WORKER_JOB_BEING_PROCESSED_ERROR,
			     _("Job %u is being processed"), job_id);
	else if (job->status & JOB_PROCESSED)
		g_set_error (error, GDA_WORKER_ERROR, GDA_WORKER_JOB_QUEUED_ERROR,
			     _("Job %u has already been processed"), job_id);
	else {
		retval = TRUE;
		job->status |= JOB_CANCELLED;
	}

	g_rec_mutex_unlock (&worker->rmutex);

	return retval;
}

/**
 * gda_worker_forget_job:
 * @worker: a #GdaWorker object
 * @job_id: the ID of the job, as returned by gda_worker_submit_job()
 *
 * Forget all about the job with ID @job_id. As opposed to gda_worker_cancel_job(), this function can be used to tell
 * @worker that whatever happens to the specific job, you are not interrested anymore (i.e. that @worker can
 * do whatever is possible to simple discard everything related to that job).
 *
 * Since: 6.0
 */
void
gda_worker_forget_job (GdaWorker *worker, guint job_id)
{
	g_return_if_fail (worker);

	if (!worker->jobs_hash) {
		g_warning ("GdaWorker has been destroyed\n");
		return;
	}

	g_rec_mutex_lock (& worker->rmutex);
	WorkerJob *job;
	job = g_hash_table_lookup (worker->jobs_hash, &job_id);
	if (job) {
		if (job->status & JOB_PROCESSED)
			/* only case where we can remove the job because the worker thread won't use it */
			g_hash_table_remove (worker->jobs_hash, & job->id);
		else
			/* the job will be 'cleared' by the worker thread */
			job->status |= JOB_CANCELLED;
	}
	g_rec_mutex_unlock (&worker->rmutex);
}

static gboolean
dc_callback (DeclaredCallback *dc)
{
	WorkerJob *job;
	job = itsignaler_pop_notification (dc->its, 0);
	g_assert (job);

	gpointer result;
	guint jid;
	GError *error = NULL;
	jid = job->id;
	if (gda_worker_fetch_job_result (dc->worker, jid, &result, &error)) {
		dc->callback (dc->worker, jid, result, error, dc->user_data);
		g_clear_error (&error);
	}
	return TRUE; /* don't remove the source from poll, this can be done using gda_worker_set_callback() */
}

/**
 * gda_worker_set_callback:
 * @worker: a #GdaWorker object
 * @context: (nullable): a #GMainContext, or %NULL
 * @callback: (nullable) (scope call): the function to call when a job submitted from within the calling thread using gda_worker_submit_job() has finished being processed.
 * @user_data: argument passed to @callback
 * @error: (nullable): a place to store errors, or %NULL
 *
 * Declare a callback function to be called when a job has been processed. If @callback is %NULL, then any previously
 * effect of this function is removed. If the same function is called with a different @callback value, then the previous one
 * is simply replaced.
 *
 * Since this function adds a new source of events to the specified #GMainContext (or the default one if @context is %NULL),
 *
 * Notes:
 * <itemizedlist>
 *  <listitem><para>before calling this function, @worker internally gets rid of the job, so the @jib_id passed
 *   to @callback does not actually designate a known job ID, and so calling gda_worker_fetch_job_result() for that
 *   job ID will fail</para></listitem>
 *  <listitem><para>the job's result, if any, has to be freed by @callback (@worker does not do it)</para></listitem>
 *  <listitem><para>any call to this function will only be honored for the jobs submitted _after_ calling it, the ones
 *   submitted before are not affected</para></listitem>
 *  <listitem><para>passing %NULL for @context is similar to passing the result of g_main_context_ref_thread_default()</para></listitem>
 * </itemizedlist>
 *
 * Returns: %TRUE if no error occurred.
 *
 * Since: 6.0
 */
gboolean
gda_worker_set_callback (GdaWorker *worker, GMainContext *context, GdaWorkerCallback callback,
			 gpointer user_data, GError **error)
{
	g_return_val_if_fail (worker, FALSE);
	g_return_val_if_fail (callback, FALSE);
	if (!worker->callbacks_hash) {
		g_warning ("GdaWorker has been destroyed\n");
		return FALSE;
	}

	GMainContext *co;
	gboolean unref_co = FALSE;

	co = context;
	if (!co) {
		co = g_main_context_ref_thread_default ();
		unref_co = TRUE;
	}

#ifdef DEBUG_NOTIFICATION
	g_print ("[W] %s() GMainContext=%p, thread=%p\n", __FUNCTION__, co, g_thread_self());
#endif
	g_rec_mutex_lock (& worker->rmutex);

	gboolean retval = TRUE;
	DeclaredCallback *dc = NULL;
	dc = g_hash_table_lookup (worker->callbacks_hash, co);

	if (dc) {
		if (callback) {
			/* change callback's attributes */
			dc->callback = callback;
			dc->user_data = user_data;
		}
		else {
			/* remove callback */
			g_hash_table_remove (worker->callbacks_hash, co);
		}
	}
	if (!dc) {
		if (callback) {
			/* add callback */
			dc = declared_callback_new (worker, co, callback, user_data);
			if (!dc) {
				g_set_error (error, GDA_WORKER_ERROR, GDA_WORKER_INTER_THREAD_ERROR,
					     _("Cannot build inter thread communication device"));
				g_rec_mutex_unlock (& worker->rmutex);
				return FALSE;
			}
			g_hash_table_insert (worker->callbacks_hash, co, dc);
			dc->source_sig_id = itsignaler_add (dc->its, co, (ITSignalerFunc) dc_callback, dc, NULL);
			if (dc->source_sig_id == 0) {
				g_hash_table_remove (worker->callbacks_hash, co);
				g_set_error (error, GDA_WORKER_ERROR, GDA_WORKER_INTER_THREAD_ERROR,
					     _("GdaWorker internal error"));
				retval = FALSE;
			}
		}
		else {
			/* nothing to do */
		}
	}

	g_rec_mutex_unlock (& worker->rmutex);

	if (unref_co)
		g_main_context_unref (co);

	return retval;
}

static gboolean
do_timer_cb (GMainLoop *loop)
{
	if (g_main_loop_is_running (loop))
		g_main_loop_quit (loop);
	return FALSE; /* remove timer */
}

static gboolean
do_itsignaler_cb (GMainLoop *loop)
{
	if (g_main_loop_is_running (loop))
		g_main_loop_quit (loop);
	return TRUE; /* keep */
}

/**
 * gda_worker_do_job:
 * @worker: a #GdaWorker object
 * @context: (nullable): a #GMainContext to execute a main loop in (while waiting), or %NULL
 * @timeout_ms: the maximum number of milisecons to wait before returning, or %0 for unlimited wait
 * @out_result: (nullable): a place to store the result, if any, of @func's execution, or %NULL
 * @out_job_id: (nullable): a place to store the ID of the job having been submitted, or %NULL
 * @func: the function to call from the worker thread
 * @data: (nullable): the data to pass to @func, or %NULL
 * @data_destroy_func: (nullable): a function to destroy @data, or %NULL
 * @result_destroy_func: (nullable): a function to destroy the result, if any, of @func's execution, or %NULL
 * @error: (nullable): a place to store errors, or %NULL.
 *
 * Request that the worker thread call @func with the @data argument, much like gda_worker_submit_job(),
 * but waits (starting a #GMainLoop) for a maximum of @timeout_ms miliseconds for @func to be executed.
 *
 * If this function is called from within @worker's worker thread, then this function simply calls @func with @data and does not
 * use @context.
 *
 * The following cases are possible if this function is not called from within @worker's worker thread:
 * <itemizedlist>
 *  <listitem><para>the call to @func took less than @timeout_ms miliseconds: the return value is %TRUE and 
 *    @out_result contains the result of the @func's execution, and @out_job_id contains %NULL. Note in this
 *    case that @error may still contain an error code if @func's execution produced an error. Also note that in this case
 *    any setting defined by gda_worker_set_callback() is not applied (as the result is immediately returned)</para></listitem>
 *  <listitem><para>The call to @func takes more then @timeout_ms miliseconds: the return value is %TRUE and
 *    @out_result is %NULL and @out_job_id contains the ID of the job as if it had been submitted using gda_worker_submit_job().
 *    If @out_job_id is %NULL, and if no setting has been defined using gda_worker_set_callback(), then the job will be discarded
 *    (as if gda_worker_forget_job() had been called).
 *    </para></listitem>
 *  <listitem><para>The call to @func could not be done (some kind of plumbing error for instance): the returned value is %FALSE
 *    and @out_result and @out_job_id are set to %NULL (if they are not %NULL)</para></listitem>
 * </itemizedlist>
 *
 * Notes:
 * <itemizedlist>
 *  <listitem><para>@result_destroy_func is needed in case @out_result is %NULL (to avoid memory leaks)</para></listitem>
 *  <listitem><para>passing %NULL for @context is similar to passing the result of g_main_context_ref_thread_default()</para></listitem>
 * </itemizedlist>
 *
 * Returns: %TRUE if no error occurred
 *
 * Since: 6.0
 */
gboolean
gda_worker_do_job (GdaWorker *worker, GMainContext *context, gint timeout_ms,
		   gpointer *out_result, guint *out_job_id,
		   GdaWorkerFunc func, gpointer data, GDestroyNotify data_destroy_func,
		   GDestroyNotify result_destroy_func,
		   GError **error)
{
	if (out_result)
		*out_result = NULL;
	if (out_job_id)
		*out_job_id = 0;

	g_return_val_if_fail (worker, FALSE);
	g_return_val_if_fail (func, FALSE);
	if (!worker->callbacks_hash || !worker->jobs_hash) {
		g_warning ("GdaWorker has been destroyed\n");
		return FALSE;
	}

	if (gda_worker_thread_is_worker (worker)) {
		/* we are called from within the worker thread => call the function directly */
		gpointer result;
		result = func (data, error);
		if (data_destroy_func)
			data_destroy_func (data);
		if (out_result)
			*out_result = result;
		else if (result && result_destroy_func)
			result_destroy_func (result);
		return TRUE;
	}

	guint jid, itsid, timer = 0;

	/* determine which GMainContext to use */
	GMainContext *co;
	gboolean unref_co = FALSE;

	co = context;
	if (!co) {
		co = g_main_context_ref_thread_default ();
		unref_co = TRUE;
	}

	/* prepare main loop */
	GMainLoop *loop;
        loop = g_main_loop_new (co, FALSE);

	/* prepare ITSignaler to be notified */
	ITSignaler *its;
	its = itsignaler_new ();
	itsid = itsignaler_add (its, co, (ITSignalerFunc) do_itsignaler_cb,
				g_main_loop_ref (loop), (GDestroyNotify) g_main_loop_unref);

	/* push job */
	g_rec_mutex_lock (& worker->rmutex); /* required to call _gda_worker_submit_job_with_its() */
	jid = _gda_worker_submit_job_with_its (worker, its,
					       func, data, data_destroy_func, result_destroy_func, error);
	g_rec_mutex_unlock (& worker->rmutex);
	if (jid == 0) {
		/* an error occurred */
		g_assert (itsignaler_remove (its, co, itsid));
		itsignaler_unref (its);
		g_main_loop_unref (loop);

		if (unref_co)
			g_main_context_unref (co);

		return FALSE;
	}

	/* check if result is already here */
	WorkerJob *job;
	job = itsignaler_pop_notification (its, 0);
	if (!job) {
		if (timeout_ms > 0) {
			/* start timer to limit waiting time */
			GSource *timer_src;
			timer_src = g_timeout_source_new (timeout_ms);
			g_source_set_callback (timer_src, (GSourceFunc) do_timer_cb, loop, NULL);
			timer = g_source_attach (timer_src, co);
			g_source_unref (timer_src);
		}
		g_main_loop_run (loop);

		/* either timer has arrived or job has been done */
		job = itsignaler_pop_notification (its, 0);
	}
	g_main_loop_unref (loop);

	g_assert (itsignaler_remove (its, co, itsid));
	itsignaler_unref (its);

	if (job) {
		/* job done before the timer, if any, elapsed */

		if (timer > 0)
			g_assert (g_source_remove (timer));

		g_assert (gda_worker_fetch_job_result (worker, jid, out_result, error));
	}
	else {
		/* timer came first, job is not yet finished */

		/* apply settings from gda_worker_set_callback(), if any */
		g_rec_mutex_lock (&worker->rmutex);
		DeclaredCallback *dc;
		dc = g_hash_table_lookup (worker->callbacks_hash, co);
		if (dc) {
			job = g_hash_table_lookup (worker->jobs_hash, &jid);
			g_assert (job);
			g_assert (!job->reply_its);
			job->reply_its = itsignaler_ref (dc->its);
		}
		g_rec_mutex_unlock (& worker->rmutex);

		/* cleanups */
		if (out_job_id)
			*out_job_id = jid;
		else if (!dc)
			/* forget all about the job */
			gda_worker_forget_job (worker, jid);
	}

	if (unref_co)
		g_main_context_unref (co);

	return TRUE;
}

/**
 * gda_worker_wait_job:
 * @worker: a #GdaWorker object
 * @func: the function to call from the worker thread
 * @data: (nullable): the data to pass to @func, or %NULL
 * @data_destroy_func: (nullable): a function to destroy @data, or %NULL
 * @error: (nullable): a place to store errors, or %NULL.
 *
 * Request that the worker thread call @func with the @data argument, much like gda_worker_submit_job(),
 * but waits (blocks) until @func has been executed.
 *
 * Note: it's up to the caller to free the result, the #GdaWorker object will not do it (ownership of the result is
 * transfered to the caller).
 *
 * Returns: (transfer full): the result of @func's execution
 *
 * Since: 6.0
 */
gpointer
gda_worker_wait_job (GdaWorker *worker, GdaWorkerFunc func, gpointer data, GDestroyNotify data_destroy_func,
		     GError **error)
{
	g_return_val_if_fail (worker, NULL);
	g_return_val_if_fail (func, NULL);

	if (gda_worker_thread_is_worker (worker)) {
		/* we are called from within the worker thread => call the function directly */
		gpointer retval;
		retval = func (data, error);
		if (data_destroy_func)
			data_destroy_func (data);
		return retval;
	}

	guint jid;

	/* prepare ITSignaler to be notified */
	ITSignaler *its;
	its = itsignaler_new ();

	/* push job */
	g_rec_mutex_lock (& worker->rmutex); /* required to call _gda_worker_submit_job_with_its() */
	jid = _gda_worker_submit_job_with_its (worker, its,
					       func, data, data_destroy_func, NULL, error);
	g_rec_mutex_unlock (& worker->rmutex);

	if (jid == 0) {
		/* an error occurred */
		itsignaler_unref (its);
		return NULL;
	}

	WorkerJob *job;
	job = itsignaler_pop_notification (its, -1);
	g_assert (job);
	itsignaler_unref (its);

	gpointer result;
	g_assert (gda_worker_fetch_job_result (worker, jid, &result, error));

	return result;
}

/**
 * gda_worker_thread_is_worker:
 * @worker: a #GdaWorker
 *
 * Tells if the thread from which this function is called is @worker's worker thread.
 *
 * Returns: %TRUE if this function is called is @worker's worker thread
 *
 * Since: 6.0
 */
gboolean
gda_worker_thread_is_worker (GdaWorker *worker)
{
	g_return_val_if_fail (worker, FALSE);
	return worker->worker_thread == g_thread_self () ? TRUE : FALSE;
}

/**
 * gda_worker_get_worker_thread:
 * @worker: a #GdaWorker
 *
 * Get a pointer to @worker's inner worker thread
 *
 * Returns: (transfer none): the #GThread
 *
 * Since: 6.0
 */
GThread *
gda_worker_get_worker_thread (GdaWorker *worker)
{
	g_return_val_if_fail (worker, NULL);
	return worker->worker_thread;
}
