/* GDA library
 * Copyright (C) 2009 - 2010 The GNOME Foundation.
 *
 * AUTHORS:
 *      Vivien Malerba <malerba@gnome-db.org>
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <string.h>
#include <glib/gi18n-lib.h>
#include "gda-thread-wrapper.h"
#include <libgda/gda-mutex.h>
#include <gobject/gvaluecollector.h>
#include <libgda/gda-debug-macros.h>
#include <libgda/gda-value.h>

/* this GPrivate holds a pointer to the GAsyncQueue used by the job being currently treated
 * by the worker thread. It is used to avoid creating signal data for threads for which
 * no job is being performed 
 */
GStaticPrivate worker_thread_current_queue = G_STATIC_PRIVATE_INIT;

typedef struct _ThreadData ThreadData;
typedef struct _Job Job;
typedef struct _SignalSpec SignalSpec;

struct _GdaThreadWrapperPrivate {
	GdaMutex    *mutex;
	guint        next_job_id;
	GThread     *worker_thread;
	GAsyncQueue *to_worker_thread;

	GHashTable  *threads_hash; /* key = a GThread, value = a #ThreadData pointer */
};

/* 
 * One instance for each job to execute (and its result) and
 * one instance for each emitted signal
 *
 * Created and destroyed exclusively by the thread(s) using the GdaThreadWrapper object,
 * except for the job where job->type == JOB_TYPE_DESTROY which is destroyed by the sub thread.
 *
 * Passed to the sub job through obj->to_worker_thread
 */
typedef enum {
	JOB_TYPE_EXECUTE,
	JOB_TYPE_DESTROY,
	JOB_TYPE_SIGNAL
} JobType;
struct _Job {
	JobType                  type;
	gboolean                 processed; /* TRUE when worker thread has started to work on it */
	gboolean                 cancelled; /* TRUE when job has been cancelled before being executed */
	guint                    job_id;
	GdaThreadWrapperFunc     func;
	GdaThreadWrapperVoidFunc void_func;
	gpointer                 arg;
	GDestroyNotify           arg_destroy_func;
	GAsyncQueue             *reply_queue; /* holds a ref to it */

	/* result part */
	union {
		struct {
			gpointer             result;
			GError              *error;
		} exe;
		struct {
			SignalSpec   *spec;
			guint         n_param_values;
			GValue       *param_values; /* array of GValue structures */
		} signal;
	} u;
};
#define JOB(x) ((Job*)(x))
static void
job_free (Job *job)
{
	if (job->arg && job->arg_destroy_func)
		job->arg_destroy_func (job->arg);
	if (job->reply_queue)
		g_async_queue_unref (job->reply_queue);

	if (job->type == JOB_TYPE_EXECUTE) {
		if (job->u.exe.error)
			g_error_free (job->u.exe.error);
	}
	else if (job->type == JOB_TYPE_SIGNAL) {
		gint i;
		for (i = 0; i < job->u.signal.n_param_values; i++) {
			GValue *value = job->u.signal.param_values + i;
			if (G_VALUE_TYPE (value) != GDA_TYPE_NULL)
				g_value_reset (value);
		}
		g_free (job->u.signal.param_values);
	}
	else if (job->type == JOB_TYPE_DESTROY) {
		/* nothing to do here */
	}
	else
		g_assert_not_reached ();
	g_free (job);
}

/*
 * Signal specification, created when using _connect().
 *
 * A SignalSpec only exists as long as the correcponding ThreadData exists.
 */
struct _SignalSpec {
        GSignalQuery  sigprop; /* must be first */

	gboolean      private;
	GThread      *worker_thread;
	GAsyncQueue  *reply_queue; /* a ref is held here */

        gpointer      instance;
        gulong        signal_id;

        GdaThreadWrapperCallback callback;
        gpointer                 data;

	GMutex       *mutex;
	guint         ref_count;
};

#define signal_spec_lock(x) g_mutex_lock(((SignalSpec*)x)->mutex);
#define signal_spec_unlock(x) g_mutex_unlock(((SignalSpec*)x)->mutex);

/*
 * call signal_spec_lock() before calling this function
 */
static void
signal_spec_unref (SignalSpec *sigspec)
{
	sigspec->ref_count --;
	if (sigspec->ref_count == 0) {
		signal_spec_unlock (sigspec);
		g_mutex_free (sigspec->mutex);
		if (sigspec->instance && (sigspec->signal_id > 0))
			g_signal_handler_disconnect (sigspec->instance, sigspec->signal_id);
		if (sigspec->reply_queue)
			g_async_queue_unref (sigspec->reply_queue);
		g_free (sigspec);
	}
	else
		signal_spec_unlock (sigspec);
}

/*
 * call signal_spec_unlock() after this function
 */
static SignalSpec *
signal_spec_ref (SignalSpec *sigspec)
{
	signal_spec_lock (sigspec);
	sigspec->ref_count ++;
	return sigspec;
}

/*
 * Per thread accounting data.
 * Each new job increases the ref count
 */
struct _ThreadData {
	GThread     *owner;
	GSList      *signals_list; /* list of SignalSpec pointers, owns all the structures */
	GAsyncQueue *from_worker_thread; /* holds a ref to it */

	GSList      *jobs; /* list of Job pointers not yet handled, or being handled (ie not yet poped from @from_worker_thread) */
	GSList      *results; /* list of Job pointers to completed jobs (ie. poped from @from_worker_thread) */
};
#define THREAD_DATA(x) ((ThreadData*)(x))

static ThreadData *
get_thread_data (GdaThreadWrapper *wrapper, GThread *thread)
{
	ThreadData *td;

	gda_mutex_lock (wrapper->priv->mutex);
	td = g_hash_table_lookup (wrapper->priv->threads_hash, thread);
	if (!td) {
		td = g_new0 (ThreadData, 1);
		td->owner = thread;
		td->from_worker_thread = g_async_queue_new_full ((GDestroyNotify) job_free);
		td->jobs = NULL;
		td->results = NULL;

		g_hash_table_insert (wrapper->priv->threads_hash, thread, td);
	}
	gda_mutex_unlock (wrapper->priv->mutex);
	return td;
}

static void
thread_data_free (ThreadData *td)
{
	g_async_queue_unref (td->from_worker_thread);
	td->from_worker_thread = NULL;
	g_assert (!td->jobs);
	if (td->results) {
		g_slist_foreach (td->results, (GFunc) job_free, NULL);
		g_slist_free (td->results);
		td->results = NULL;
	}
	if (td->signals_list) {
		GSList *list;
		for (list = td->signals_list; list; list = list->next) {
			/* clear SignalSpec */
			SignalSpec *sigspec = (SignalSpec*) list->data;

			signal_spec_lock (sigspec);
			g_signal_handler_disconnect (sigspec->instance, sigspec->signal_id);
			sigspec->instance = NULL;
			sigspec->signal_id = 0;
			g_async_queue_unref (sigspec->reply_queue);
			sigspec->reply_queue = NULL;
			sigspec->callback = NULL;
			sigspec->data = NULL;
			signal_spec_unref (sigspec);
		}
		g_slist_free (td->signals_list);
	}
	g_free (td);
}

static void gda_thread_wrapper_class_init (GdaThreadWrapperClass *klass);
static void gda_thread_wrapper_init       (GdaThreadWrapper *wrapper, GdaThreadWrapperClass *klass);
static void gda_thread_wrapper_dispose    (GObject *object);
static void gda_thread_wrapper_set_property (GObject *object,
					     guint param_id,
					     const GValue *value,
					     GParamSpec *pspec);
static void gda_thread_wrapper_get_property (GObject *object,
					     guint param_id,
					     GValue *value,
					     GParamSpec *pspec);

/* properties */
enum {
	PROP_0
};

static GObjectClass *parent_class = NULL;

/*
 * GdaThreadWrapper class implementation
 * @klass:
 */
static void
gda_thread_wrapper_class_init (GdaThreadWrapperClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	/* Properties */
        object_class->set_property = gda_thread_wrapper_set_property;
        object_class->get_property = gda_thread_wrapper_get_property;

	object_class->dispose = gda_thread_wrapper_dispose;
}

/*
 * Executed in the sub thread:
 * takes a Job in (from the wrapper->priv->to_worker_thread queue) and creates a new Result which 
 * it pushed to Job->reply_queue
 */
static gpointer
worker_thread_entry_point (GAsyncQueue *to_worker_thread)
{
	GAsyncQueue *in;

	in = to_worker_thread;

	while (1) {
		Job *job;
		
		/* pop next job and mark it as being processed */
		g_static_private_set (&worker_thread_current_queue, NULL, NULL);
		g_async_queue_lock (in);
		job = g_async_queue_pop_unlocked (in);
		job->processed = TRUE;
		g_static_private_set (&worker_thread_current_queue, job->reply_queue, NULL);
		g_async_queue_unlock (in);

		if (job->cancelled) {
			job_free (job);
			continue;
		}

		if (job->type == JOB_TYPE_DESTROY) {
			g_assert (! job->arg_destroy_func);
			job_free (job);
			/*g_print ("... exit sub thread for wrapper\n");*/

			/* exit sub thread */
			break;
		}
		else if (job->type == JOB_TYPE_EXECUTE) {
			if (job->func)
				job->u.exe.result = job->func (job->arg, &(job->u.exe.error));
			else {
				job->u.exe.result = NULL;
				job->void_func (job->arg, &(job->u.exe.error));
			}
			g_async_queue_push (job->reply_queue, job);
		}
		else
			g_assert_not_reached ();
	}

	g_async_queue_unref (in);

	return NULL;
}

static void
gda_thread_wrapper_init (GdaThreadWrapper *wrapper, G_GNUC_UNUSED GdaThreadWrapperClass *klass)
{
	g_return_if_fail (GDA_IS_THREAD_WRAPPER (wrapper));

	wrapper->priv = g_new0 (GdaThreadWrapperPrivate, 1);
	wrapper->priv->mutex = gda_mutex_new ();
	wrapper->priv->next_job_id = 1;

	wrapper->priv->threads_hash = g_hash_table_new_full (NULL, NULL, NULL, (GDestroyNotify) thread_data_free);

	wrapper->priv->to_worker_thread = g_async_queue_new ();
	wrapper->priv->worker_thread = g_thread_create ((GThreadFunc) worker_thread_entry_point,
						     g_async_queue_ref (wrapper->priv->to_worker_thread), /* inc. ref for sub thread usage */
						     FALSE, NULL);
	/*g_print ("... new wrapper %p, worker_thread=%p\n", wrapper, wrapper->priv->worker_thread);*/
}

static gboolean
thread_data_remove_jobs_func (G_GNUC_UNUSED GThread *key, ThreadData *td, G_GNUC_UNUSED gpointer data)
{
	if (td->jobs)  {
		GSList *list;
		for (list = td->jobs; list; list = list->next) {
			Job *job = JOB (list->data);
			if (job->processed) {
				/* we can't free that job because it is probably being used by the 
				 * worker thread, so just emit a warning
				 */
				if (job->arg_destroy_func) {
					g_warning ("The argument of Job ID %d will be destroyed by sub thread",
						   job->job_id);
				}
			}
			else {
				/* cancel this job */
				job->cancelled = TRUE;
				if (job->arg && job->arg_destroy_func) {
					job->arg_destroy_func (job->arg);
					job->arg = NULL;
				}
			}
		}
		g_slist_free (td->jobs);
		td->jobs = NULL;
	}
	return TRUE; /* remove this ThreadData */
}

static void
gda_thread_wrapper_dispose (GObject *object)
{
	GdaThreadWrapper *wrapper = (GdaThreadWrapper *) object;

	g_return_if_fail (GDA_IS_THREAD_WRAPPER (wrapper));

	if (wrapper->priv) {
		Job *job = g_new0 (Job, 1);
		job->type = JOB_TYPE_DESTROY;
		g_async_queue_push (wrapper->priv->to_worker_thread, job);
		/*g_print ("... pushed JOB_TYPE_DESTROY for wrapper %p\n", wrapper);*/
		
		g_async_queue_lock (wrapper->priv->to_worker_thread);
		if (wrapper->priv->threads_hash) {
			g_hash_table_foreach_remove (wrapper->priv->threads_hash,
						     (GHRFunc) thread_data_remove_jobs_func, NULL);
			g_hash_table_destroy (wrapper->priv->threads_hash);
		}
		g_async_queue_unlock (wrapper->priv->to_worker_thread);
		g_async_queue_unref (wrapper->priv->to_worker_thread);
		wrapper->priv->worker_thread = NULL; /* side note: don't wait for sub thread to terminate */

		gda_mutex_free (wrapper->priv->mutex);

		g_free (wrapper->priv);
		wrapper->priv = NULL;
	}

	/* chain to parent class */
	parent_class->dispose (object);
}

/* module error */
GQuark gda_thread_wrapper_error_quark (void)
{
        static GQuark quark;
        if (!quark)
                quark = g_quark_from_static_string ("gda_thread_wrapper_error");
        return quark;
}

/**
 * gda_thread_wrapper_get_type
 * 
 * Registers the #GdaThreadWrapper class on the GLib type system.
 * 
 * Returns: the GType identifying the class.
 */
GType
gda_thread_wrapper_get_type (void)
{
        static GType type = 0;

        if (G_UNLIKELY (type == 0)) {
                static GStaticMutex registering = G_STATIC_MUTEX_INIT;
                static const GTypeInfo info = {
                        sizeof (GdaThreadWrapperClass),
                        (GBaseInitFunc) NULL,
                        (GBaseFinalizeFunc) NULL,
                        (GClassInitFunc) gda_thread_wrapper_class_init,
                        NULL,
                        NULL,
                        sizeof (GdaThreadWrapper),
                        0,
                        (GInstanceInitFunc) gda_thread_wrapper_init,
			0
                };

                g_static_mutex_lock (&registering);
                if (type == 0)
                        type = g_type_register_static (G_TYPE_OBJECT, "GdaThreadWrapper", &info, 0);
                g_static_mutex_unlock (&registering);
        }
        return type;
}

static void
gda_thread_wrapper_set_property (GObject *object,
			       guint param_id,
			       G_GNUC_UNUSED const GValue *value,
			       G_GNUC_UNUSED GParamSpec *pspec)
{
	GdaThreadWrapper *wrapper;

        wrapper = GDA_THREAD_WRAPPER (object);
        if (wrapper->priv) {
                switch (param_id) {
		}	
	}
}

static void
gda_thread_wrapper_get_property (GObject *object,
			       guint param_id,
			       G_GNUC_UNUSED GValue *value,
			       G_GNUC_UNUSED GParamSpec *pspec)
{
	GdaThreadWrapper *wrapper;
	
	wrapper = GDA_THREAD_WRAPPER (object);
	if (wrapper->priv) {
		switch (param_id) {
		}
	}	
}

/**
 * gda_thread_wrapper_new
 *
 * Creates a new #GdaThreadWrapper object
 *
 * Returns: a new #GdaThreadWrapper object, or %NULL if threads are not supported/enabled
 *
 * Since: 4.2
 */
GdaThreadWrapper *
gda_thread_wrapper_new (void)
{
	if (! g_thread_supported ())
		return NULL;
	return (GdaThreadWrapper *) g_object_new (GDA_TYPE_THREAD_WRAPPER, NULL);
}

/**
 * gda_thread_wrapper_execute
 * @wrapper: a #GdaThreadWrapper object
 * @func: the function to execute
 * @arg: argument to pass to @func
 * @arg_destroy_func: function to be called when the execution has finished, to destroy @arg
 * @error: a place to store errors, for errors occurring in this method, not errors occurring while @func
 *         is executed, or %NULL
 *
 * Make @wrapper execute the @func function with the @arg argument (along with a #GError which is not @error)
 * in the sub thread managed by @wrapper. To execute a function which does not return anything,
 * use gda_thread_wrapper_execute_void().
 *
 * This method returns immediately, and the caller then needs to use gda_thread_wrapper_fetch_result() to
 * check if the execution has finished and get the result.
 *
 * Once @func's execution is finished, if it is not %NULL, the @arg_destroy_func destruction function is called
 * on @arg. This occurs in the thread calling gda_thread_wrapper_fetch_result().
 *
 * Returns: the job ID, or 0 if an error occurred
 *
 * Since: 4.2
 */
guint
gda_thread_wrapper_execute (GdaThreadWrapper *wrapper, GdaThreadWrapperFunc func,
			    gpointer arg, GDestroyNotify arg_destroy_func, G_GNUC_UNUSED GError **error)
{
	Job *job;
	guint id;
	ThreadData *td;
	
	g_return_val_if_fail (GDA_IS_THREAD_WRAPPER (wrapper), 0);
	g_return_val_if_fail (wrapper->priv, 0);
	g_return_val_if_fail (func, 0);

	td = get_thread_data (wrapper, g_thread_self());

	job = g_new0 (Job, 1);
	job->type = JOB_TYPE_EXECUTE;
	job->processed = FALSE;
	job->cancelled = FALSE;
	gda_mutex_lock (wrapper->priv->mutex);
	job->job_id = wrapper->priv->next_job_id++;
	gda_mutex_unlock (wrapper->priv->mutex);
	job->func = func;
	job->void_func = NULL;
	job->arg = arg;
	job->arg_destroy_func = arg_destroy_func;
	job->reply_queue = g_async_queue_ref (td->from_worker_thread);

	id = job->job_id;
	/* g_print ("... submitted job %d from thread %p\n", id, g_thread_self()); */

	td->jobs = g_slist_append (td->jobs, job);

	if (g_thread_self () == wrapper->priv->worker_thread) {
		job->processed = TRUE;
                if (job->func)
                        job->u.exe.result = job->func (job->arg, &(job->u.exe.error));
                else {
                        job->u.exe.result = NULL;
                        job->void_func (job->arg, &(job->u.exe.error));
                }
		/* g_print ("... IMMEDIATELY done job %d => %p\n", job->job_id, job->u.exe.result); */
                g_async_queue_push (job->reply_queue, job);
        }
        else
                g_async_queue_push (wrapper->priv->to_worker_thread, job);

	return id;
}

/**
 * gda_thread_wrapper_execute_void
 * @wrapper: a #GdaThreadWrapper object
 * @func: the function to execute
 * @arg: argument to pass to @func
 * @arg_destroy_func: function to be called when the execution has finished, to destroy @arg
 * @error: a place to store errors, for errors occurring in this method, not errors occurring while @func
 *         is executed, or %NULL
 *
 * Make @wrapper execute the @func function with the @arg argument (along with a #GError which is not @error)
 * in the sub thread managed by @wrapper. To execute a function which returns some pointer,
 * use gda_thread_wrapper_execute().
 *
 * This method returns immediately. Calling gda_thread_wrapper_fetch_result() is not necessary as @func
 * does not return any result. However, it may be necessary to call gda_thread_wrapper_iterate() to give @wrapper a
 * chance to execute the @arg_destroy_func function if not %NULL (note that gda_thread_wrapper_iterate() is
 * called by gda_thread_wrapper_fetch_result() itself).
 *
 * Once @func's execution is finished, if it is not %NULL, the @arg_destroy_func destruction function is called
 * on @arg. This occurs in the thread calling gda_thread_wrapper_fetch_result().
 *
 * Returns: the job ID, or 0 if an error occurred
 *
 * Since: 4.2
 */
guint
gda_thread_wrapper_execute_void (GdaThreadWrapper *wrapper, GdaThreadWrapperVoidFunc func,
				 gpointer arg, GDestroyNotify arg_destroy_func, G_GNUC_UNUSED GError **error)
{
	Job *job;
	guint id;
	ThreadData *td;

	g_return_val_if_fail (GDA_IS_THREAD_WRAPPER (wrapper), 0);
	g_return_val_if_fail (wrapper->priv, 0);
	g_return_val_if_fail (func, 0);

	td = get_thread_data (wrapper, g_thread_self());

	job = g_new0 (Job, 1);
	job->type = JOB_TYPE_EXECUTE;
	job->processed = FALSE;
	job->cancelled = FALSE;
	gda_mutex_lock (wrapper->priv->mutex);
	job->job_id = wrapper->priv->next_job_id++;
	gda_mutex_unlock (wrapper->priv->mutex);
	job->func = NULL;
	job->void_func = func;
	job->arg = arg;
	job->arg_destroy_func = arg_destroy_func;
	job->reply_queue = g_async_queue_ref (td->from_worker_thread);

	id = job->job_id;
	/* g_print ("... submitted VOID job %d\n", id); */

	td->jobs = g_slist_append (td->jobs, job);

	if (g_thread_self () == wrapper->priv->worker_thread) {
		job->processed = TRUE;
                if (job->func)
                        job->u.exe.result = job->func (job->arg, &(job->u.exe.error));
                else {
                        job->u.exe.result = NULL;
                        job->void_func (job->arg, &(job->u.exe.error));
                }
		/* g_print ("... IMMEDIATELY done VOID job %d => %p\n", job->job_id, job->u.exe.result); */
                g_async_queue_push (job->reply_queue, job);
        }
        else
		g_async_queue_push (wrapper->priv->to_worker_thread, job);

	return id;
}

/**
 * gda_thread_wrapper_cancel
 * @wrapper: a #GdaThreadWrapper object
 * @id: the ID of a job as returned by gda_thread_wrapper_execute() or gda_thread_wrapper_execute_void()
 * 
 * Cancels a job not yet executed. This may fail for the following reasons:
 * <itemizedlist>
 *  <listitem><para>the job @id could not be found, either because it has already been treated or because
 *                  it does not exist or because it was created in another thread</para></listitem>
 *  <listitem><para>the job @id is currently being treated by the worker thread</para></listitem>
 * </itemizedlist>
 *
 * Returns: %TRUE if the job has been cancelled, or %FALSE in any other case.
 *
 * Since: 4.2
 */
gboolean
gda_thread_wrapper_cancel (GdaThreadWrapper *wrapper, guint id)
{
	GSList *list;
	gboolean retval = FALSE; /* default if job not found */
	ThreadData *td;

	g_return_val_if_fail (GDA_IS_THREAD_WRAPPER (wrapper), FALSE);
	
	gda_mutex_lock (wrapper->priv->mutex);

	td = g_hash_table_lookup (wrapper->priv->threads_hash, g_thread_self());
	if (!td) {
		/* nothing to be done for this thread */
		gda_mutex_unlock (wrapper->priv->mutex);
		return FALSE;
	}

	g_async_queue_lock (wrapper->priv->to_worker_thread);
	for (list = td->jobs; list; list = list->next) {
		Job *job = JOB (list->data);
		if (job->job_id == id) {
			if (job->processed) {
				/* can't cancel it as it's being treated */
				break;
			}

			retval = TRUE;
			job->cancelled = TRUE;
			if (job->arg && job->arg_destroy_func) {
				job->arg_destroy_func (job->arg);
				job->arg = NULL;
			}
			td->jobs = g_slist_delete_link (td->jobs, list);
			break;
		}
	}
	g_async_queue_unlock (wrapper->priv->to_worker_thread);
	gda_mutex_unlock (wrapper->priv->mutex);

	return retval;
}

/**
 * gda_thread_wrapper_iterate
 * @wrapper: a #GdaThreadWrapper object
 * @may_block: whether the call may block
 *
 * This method gives @wrapper a chance to check if some functions to be executed have finished
 * <emphasis>for the calling thread</emphasis>. It handles one function's execution result, and
 * if @may_block is %TRUE, then it will block untill there is one (functions returning void are
 * ignored).
 *
 * Since: 4.2
 */
void
gda_thread_wrapper_iterate (GdaThreadWrapper *wrapper, gboolean may_block)
{
	ThreadData *td;
	Job *job;

	g_return_if_fail (GDA_IS_THREAD_WRAPPER (wrapper));
	g_return_if_fail (wrapper->priv);

	gda_mutex_lock (wrapper->priv->mutex);
	td = g_hash_table_lookup (wrapper->priv->threads_hash, g_thread_self());
	gda_mutex_unlock (wrapper->priv->mutex);
	if (!td) {
		/* nothing to be done for this thread */
		return;
	}

 again:
	if (may_block)
		job = g_async_queue_pop (td->from_worker_thread);
	else
		job = g_async_queue_try_pop (td->from_worker_thread);
	if (job) {
		gboolean do_again = FALSE;
		td->jobs = g_slist_remove (td->jobs, job);

		if (job->type == JOB_TYPE_EXECUTE) {
			if (!job->func) {
				job_free (job); /* ignore as there is no result */
				do_again = TRUE;
			}
			else
				td->results = g_slist_append (td->results, job);
		}
		else if (job->type == JOB_TYPE_SIGNAL) {
			/* run callback now */
			SignalSpec *spec = job->u.signal.spec;

			if (spec->callback)
				spec->callback (wrapper, spec->instance, ((GSignalQuery*)spec)->signal_name,
						job->u.signal.n_param_values, job->u.signal.param_values, NULL,
						spec->data);
			else
				g_print ("Not propagating signal %s\n", ((GSignalQuery*)spec)->signal_name);
			job->u.signal.spec = NULL;
			job_free (job);
			signal_spec_lock (spec);
			signal_spec_unref (spec);
			do_again = TRUE;
		}
		else
			g_assert_not_reached ();

		if (do_again)
			goto again;
	}
}

/**
 * gda_thread_wrapper_fetch_result
 * @wrapper: a #GdaThreadWrapper object
 * @may_lock: TRUE if this funct must lock the caller untill a result is available
 * @exp_id: ID of the job for which a result is expected
 * @error: a place to store errors, for errors which may have occurred during the execution, or %NULL
 *
 * Use this method to check if the execution of a function is finished. The function's execution must have
 * been requested using gda_thread_wrapper_execute().
 *
 * Returns: the pointer returned by the execution, or %NULL if no result is available
 *
 * Since: 4.2
 */
gpointer
gda_thread_wrapper_fetch_result (GdaThreadWrapper *wrapper, gboolean may_lock, guint exp_id, GError **error)
{
	ThreadData *td;
	Job *job = NULL;
	gpointer retval = NULL;

	g_return_val_if_fail (GDA_IS_THREAD_WRAPPER (wrapper), NULL);
	g_return_val_if_fail (wrapper->priv, NULL);
	g_return_val_if_fail (exp_id > 0, NULL);

	gda_mutex_lock (wrapper->priv->mutex);
	td = g_hash_table_lookup (wrapper->priv->threads_hash, g_thread_self());
	gda_mutex_unlock (wrapper->priv->mutex);
	if (!td) {
		/* nothing to be done for this thread */
		return NULL;
	}
	
	do {
		if (td->results) {
			/* see if we have the result we want */
			GSList *list;
			for (list = td->results; list; list = list->next) {
				job = JOB (list->data);
				if (job->job_id == exp_id) {
					/* found it */
					td->results = g_slist_delete_link (td->results, list);
					if (!td->results &&
					    !td->jobs &&
					    (g_async_queue_length (td->from_worker_thread) == 0) &&
					    !td->signals_list) {
						/* remove this ThreadData */
						gda_mutex_lock (wrapper->priv->mutex);
						g_hash_table_remove (wrapper->priv->threads_hash, g_thread_self());
						gda_mutex_unlock (wrapper->priv->mutex);
					}
					goto out;
				}
			}
		}
		
		if (may_lock) 
			gda_thread_wrapper_iterate (wrapper, TRUE);
		else {
			gint len;
			len = g_slist_length (td->results);
			gda_thread_wrapper_iterate (wrapper, FALSE);
			if (g_slist_length (td->results) == len) {
				job = NULL;
				break;
			}
		}
	} while (1);

 out:
	if (job) {
		g_assert (job->type == JOB_TYPE_EXECUTE);
		if (job->u.exe.error) {
			g_propagate_error (error, job->u.exe.error);
			job->u.exe.error = NULL;
		}
		retval = job->u.exe.result;
		job->u.exe.result = NULL;
		job_free (job);
	}

	return retval;
}

/**
 * gda_thread_wrapper_get_waiting_size
 * @wrapper: a #GdaThreadWrapper object
 *
 * Use this method to query the number of functions which have been queued to be executed
 * but which have not yet been executed.
 *
 * Returns: the number of jobs not yet executed
 *
 * Since: 4.2
 */
gint
gda_thread_wrapper_get_waiting_size (GdaThreadWrapper *wrapper)
{
	GSList *list;
	gint size = 0;
	ThreadData *td;

	g_return_val_if_fail (GDA_IS_THREAD_WRAPPER (wrapper), 0);
	g_return_val_if_fail (wrapper->priv, 0);

	gda_mutex_lock (wrapper->priv->mutex);
	td = g_hash_table_lookup (wrapper->priv->threads_hash, g_thread_self());
	if (!td) {
		/* nothing to be done for this thread */
		gda_mutex_unlock (wrapper->priv->mutex);
		return 0;
	}
	
	/* lock job consuming queue to avoid that the worker thread "consume" a Job */
	g_async_queue_lock (wrapper->priv->to_worker_thread);
	for (size = 0, list = td->jobs; list; list = list->next) {
		if (!JOB (list->data)->cancelled)
			size++;
	}
	g_async_queue_unlock (wrapper->priv->to_worker_thread);
	gda_mutex_unlock (wrapper->priv->mutex);
	return size;
}

/* 
 * Executed in sub thread (or potentially in other threads, in which case will be ignored)
 * pushes data into the queue 
 */
static void
worker_thread_closure_marshal (GClosure *closure,
			       G_GNUC_UNUSED GValue *return_value,
			       guint n_param_values,
			       const GValue *param_values,
			       G_GNUC_UNUSED gpointer invocation_hint,
			       G_GNUC_UNUSED gpointer marshal_data)
{
	SignalSpec *sigspec = (SignalSpec *) closure->data;

	/* if the signal is not emitted from the working thread then don't do anything */
	if (g_thread_self () !=  sigspec->worker_thread)
		return;

	/* check that the worker thread is working on a job for which job->reply_queue == sigspec->reply_queue */	if (sigspec->private &&
	    g_static_private_get (&worker_thread_current_queue) != sigspec->reply_queue)
		return;

	gint i;
	/*
	  for (i = 1; i < n_param_values; i++) {
		g_print ("\t%d => %s\n", i, gda_value_stringify (param_values + i));
	}
	*/
	Job *job= g_new0 (Job, 1);
	job->type = JOB_TYPE_SIGNAL;
	job->u.signal.spec = signal_spec_ref (sigspec);
	job->u.signal.n_param_values = n_param_values - 1;
	job->u.signal.param_values = g_new0 (GValue, job->u.signal.n_param_values);
	for (i = 1; i < n_param_values; i++) {
		const GValue *src;
		GValue *dest;

		src = param_values + i;
		dest = job->u.signal.param_values + i - 1;

		if (G_VALUE_TYPE (src) != GDA_TYPE_NULL) {
			g_value_init (dest, G_VALUE_TYPE (src));
			g_value_copy (src, dest);
		}
	}

	g_async_queue_push (sigspec->reply_queue, job);
	signal_spec_unlock (sigspec);
}

/* 
 * Executed in sub thread (or potentially in other threads, in which case will be ignored)
 * pushes data into the queue 
 */
static void
worker_thread_closure_marshal_anythread (GClosure *closure,
					 G_GNUC_UNUSED GValue *return_value,
					 guint n_param_values,
					 const GValue *param_values,
					 G_GNUC_UNUSED gpointer invocation_hint,
					 G_GNUC_UNUSED gpointer marshal_data)
{
	SignalSpec *sigspec = (SignalSpec *) closure->data;

	gint i;
	/*
	  for (i = 1; i < n_param_values; i++) {
		g_print ("\t%d => %s\n", i, gda_value_stringify (param_values + i));
	}
	*/
	Job *job= g_new0 (Job, 1);
	job->type = JOB_TYPE_SIGNAL;
	job->u.signal.spec = signal_spec_ref (sigspec);
	job->u.signal.n_param_values = n_param_values - 1;
	job->u.signal.param_values = g_new0 (GValue, job->u.signal.n_param_values);
	for (i = 1; i < n_param_values; i++) {
		const GValue *src;
		GValue *dest;

		src = param_values + i;
		dest = job->u.signal.param_values + i - 1;

		if (G_VALUE_TYPE (src) != GDA_TYPE_NULL) {
			g_value_init (dest, G_VALUE_TYPE (src));
			g_value_copy (src, dest);
		}
	}

	g_async_queue_push (sigspec->reply_queue, job);
	signal_spec_unlock (sigspec);
}

/**
 * gda_thread_wrapper_connect_raw
 * @wrapper: a #GdaThreadWrapper object
 * @instance: the instance to connect to
 * @sig_name: a string of the form "signal-name::detail"
 * @private_thread:  set to %TRUE if @callback is to be invoked only if the signal has
 *    been emitted while in @wrapper's private sub thread (ie. used when @wrapper is executing some functions
 *    specified by gda_thread_wrapper_execute() or gda_thread_wrapper_execute_void()), and to %FALSE if the
 *    callback is to be invoked whenever the signal is emitted, independently of th thread in which the
 *    signal is emitted.
 * @private_job: set to %TRUE if @callback is to be invoked only if the signal has
 *    been emitted when a job created for the calling thread is being executed, and to %FALSE
 *    if @callback has to be called whenever the @sig_name signal is emitted by @instance. Note that
 *    this argument is not taken into account if @private_thread is set to %FALSE.
 * @callback: a #GdaThreadWrapperCallback function
 * @data: data to pass to @callback's calls
 *
 * Connects a callback function to a signal for a particular object. The difference with g_signal_connect() and
 * similar functions are:
 * <itemizedlist>
 *  <listitem><para>the @callback argument is not a #GCallback function, so the callback signature is not
 *    dependent on the signal itself</para></listitem>
 *  <listitem><para>the signal handler must not have to return any value</para></listitem>
 *  <listitem><para>the @callback function will be called asynchronously, the caller may need to use 
 *    gda_thread_wrapper_iterate() to get the notification</para></listitem>
 *  <listitem><para>if @private is set to %TRUE, then the @callback function will be called only
 *    if the signal has been emitted by @instance while doing a job on behalf of the current
 *    calling thread. If @private is set to %FALSE, then @callback will be called whenever @instance
 *    emits the @sig_name signal.</para></listitem>
 * </itemizedlist>
 *
 * Also note that signal handling is done asynchronously: when emitted in the worker thread, it
 * will be "queued" to be processed in the user thread when it has the chance (when gda_thread_wrapper()
 * is called directly or indirectly). The side effect is that the callback function is usually
 * called long after the object emitting the signal has finished emitting it.
 *
 * To disconnect a signal handler, don't use any of the g_signal_handler_*() functions but the
 * gda_thread_wrapper_disconnect() method.
 *
 * Returns: the handler ID
 *
 * Since: 4.2
 */
gulong
gda_thread_wrapper_connect_raw (GdaThreadWrapper *wrapper,
				gpointer instance,
				const gchar *sig_name,
				gboolean private_thread, gboolean private_job,
				GdaThreadWrapperCallback callback,
				gpointer data)
{
	guint sigid;
        SignalSpec *sigspec;
	ThreadData *td;

	g_return_val_if_fail (GDA_IS_THREAD_WRAPPER (wrapper), 0);
	g_return_val_if_fail (wrapper->priv, 0);

	gda_mutex_lock (wrapper->priv->mutex);
	
	td = get_thread_data (wrapper, g_thread_self());

        sigid = g_signal_lookup (sig_name, /* FIXME: use g_signal_parse_name () */
				 G_TYPE_FROM_INSTANCE (instance)); 
        if (sigid == 0) {
                g_warning (_("Signal does not exist\n"));
                return 0;
        }

        sigspec = g_new0 (SignalSpec, 1);
	sigspec->private = private_job;
        g_signal_query (sigid, (GSignalQuery*) sigspec);

	if (((GSignalQuery*) sigspec)->return_type != G_TYPE_NONE) {
		g_warning (_("Signal to connect to must not have a return value\n"));
		g_free (sigspec);
		return 0;
	}
	sigspec->worker_thread = wrapper->priv->worker_thread;
	sigspec->reply_queue = g_async_queue_ref (td->from_worker_thread);
        sigspec->instance = instance;
        sigspec->callback = callback;
        sigspec->data = data;
	sigspec->mutex = g_mutex_new ();
	sigspec->ref_count = 1;

	GClosure *cl;
	cl = g_closure_new_simple (sizeof (GClosure), sigspec);
	if (private_thread)
		g_closure_set_marshal (cl, (GClosureMarshal) worker_thread_closure_marshal);
	else
		g_closure_set_marshal (cl, (GClosureMarshal) worker_thread_closure_marshal_anythread);
	sigspec->signal_id = g_signal_connect_closure (instance, sig_name, cl, FALSE);

	td->signals_list = g_slist_append (td->signals_list, sigspec);

	gda_mutex_unlock (wrapper->priv->mutex);

	return sigspec->signal_id;
}

static gboolean
find_signal_r_func (G_GNUC_UNUSED GThread *thread, ThreadData *td, gulong *id)
{
	GSList *list;
	for (list = td->signals_list; list; list = list->next) {
		SignalSpec *sigspec;
		sigspec = (SignalSpec*) list->data;
		if (sigspec->signal_id == *id)
			return TRUE;
	}
	return FALSE;
}

/**
 * gda_thread_wrapper_disconnect
 * @wrapper: a #GdaThreadWrapper object
 * @id: a handler ID, as returned by gda_thread_wrapper_connect_raw()
 *
 * Disconnects the emission of a signal, does the opposite of gda_thread_wrapper_connect_raw().
 *
 * As soon as this method returns, the callback function set when gda_thread_wrapper_connect_raw()
 * was called will not be called anymore (even if the object has emitted the signal in the worker
 * thread and this signal has not been handled in the user thread).
 *
 * Since: 4.2
 */
void
gda_thread_wrapper_disconnect (GdaThreadWrapper *wrapper, gulong id)
{
	SignalSpec *sigspec = NULL;
	ThreadData *td;
	GSList *list;

	g_return_if_fail (GDA_IS_THREAD_WRAPPER (wrapper));
	g_return_if_fail (wrapper->priv);

	gda_mutex_lock (wrapper->priv->mutex);

	td = g_hash_table_lookup (wrapper->priv->threads_hash, g_thread_self());
	if (!td) {
		gulong theid = id;
		td = g_hash_table_find (wrapper->priv->threads_hash,
					(GHRFunc) find_signal_r_func, &theid);
	}
	if (!td) {
		g_warning (_("Signal %lu does not exist"), id);
		gda_mutex_unlock (wrapper->priv->mutex);
		return;
	}

	for (list = td->signals_list; list; list = list->next) {
		if (((SignalSpec*) list->data)->signal_id == id) {
			sigspec = (SignalSpec*) list->data;
			break;
		}
	}

	if (!sigspec) {
		g_warning (_("Signal does not exist\n"));
		gda_mutex_unlock (wrapper->priv->mutex);
		return;
	}

	signal_spec_lock (sigspec);
	/*g_print ("Disconnecting signal %s\n", ((GSignalQuery*)sigspec)->signal_name);*/
	td->signals_list = g_slist_remove (td->signals_list, sigspec);
	g_signal_handler_disconnect (sigspec->instance, sigspec->signal_id);
	sigspec->instance = NULL;
	sigspec->signal_id = 0;
	g_async_queue_unref (sigspec->reply_queue);
	sigspec->reply_queue = NULL;
	sigspec->callback = NULL;
	sigspec->data = NULL;
	signal_spec_unref (sigspec);

	if (!td->results &&
	    !td->jobs &&
	    (g_async_queue_length (td->from_worker_thread) == 0) &&
	    !td->signals_list) {
		/* remove this ThreadData */
		g_hash_table_remove (wrapper->priv->threads_hash, g_thread_self());
	}

	gda_mutex_unlock (wrapper->priv->mutex);
}

/**
 * gda_thread_wrapper_steal_signal
 * @wrapper: a #GdaThreadWrapper object
 * @id: a signal ID
 *
 * Requests that the signal which ID is @id (which has been obtained using gda_thread_wrapper_connect_raw())
 * be treated by the calling thread instead of by the thread in which gda_thread_wrapper_connect_raw()
 * was called.
 *
 * Since: 4.2
 */

void
gda_thread_wrapper_steal_signal (GdaThreadWrapper *wrapper, gulong id)
{
	ThreadData *old_td, *new_td = NULL;
        g_return_if_fail (GDA_IS_THREAD_WRAPPER (wrapper));
        g_return_if_fail (wrapper->priv);
	g_return_if_fail (id > 0);

        gda_mutex_lock (wrapper->priv->mutex);

	gulong theid = id;
	old_td = g_hash_table_find (wrapper->priv->threads_hash,
				    (GHRFunc) find_signal_r_func, &theid);
	if (!old_td) {
		g_warning (_("Signal %lu does not exist"), id);
		gda_mutex_unlock (wrapper->priv->mutex);
		return;
	}

	if (old_td->owner == g_thread_self ())
		return;

        /* merge old_td and new_td */
        if (old_td->signals_list) {
                GSList *list;
                for (list = old_td->signals_list; list; list = list->next) {
                        SignalSpec *sigspec = (SignalSpec*) list->data;
			if (sigspec->signal_id == id) {
				new_td = get_thread_data (wrapper, g_thread_self ());
				new_td->signals_list = g_slist_prepend (new_td->signals_list, sigspec);
				old_td->signals_list = g_slist_remove (old_td->signals_list, sigspec);
				g_async_queue_unref (sigspec->reply_queue);
				sigspec->reply_queue = g_async_queue_ref (new_td->from_worker_thread);
				break;
			}
                }
        }

	gda_mutex_unlock (wrapper->priv->mutex);
}
