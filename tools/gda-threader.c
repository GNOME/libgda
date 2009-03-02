/* gda-threader.c
 *
 * Copyright (C) 2005 - 2007 Vivien Malerba
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
#include <tools/gda-threader.h>
#include <stdlib.h>

/* 
 * Main static functions 
 */
static void gda_threader_class_init (GdaThreaderClass * class);
static void gda_threader_init (GdaThreader * srv);
static void gda_threader_dispose (GObject   * object);
static void gda_threader_finalize (GObject   * object);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* signals */
enum
{
	FINISHED,
	CANCELLED,
	LAST_SIGNAL
};

static gint gda_threader_signals[LAST_SIGNAL] = { 0, 0};

struct _GdaThreaderPrivate
{
	guint        next_job;
	guint        nb_jobs;
	GHashTable  *jobs; /* key = job id, value=ThreadJob for the job */
	GAsyncQueue *queue;
	guint        idle_func_id;
};

typedef struct {
	GdaThreader    *thread; /* object to which this jobs belongs to */
	guint           id;
	GThread        *g_thread; /* sub thread for that job */
	GThreadFunc     func;     /* function called in the sub thread */
	gpointer        func_data;/* argument to the function called in the sub thread */
	
	gboolean        cancelled; /* TRUE if this job has been cancelled */
	GdaThreaderFunc normal_end_cb; /* called when job->func ends */
	GdaThreaderFunc cancel_end_cb; /* called when job->func ends and this job has been cancelled */
} ThreadJob;

GType
gda_threader_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static const GTypeInfo info = {
			sizeof (GdaThreaderClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_threader_class_init,
			NULL,
			NULL,
			sizeof (GdaThreader),
			0,
			(GInstanceInitFunc) gda_threader_init
		};
		
		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (G_TYPE_OBJECT, "GdaThreader", &info, 0);
		g_static_mutex_unlock (&registering);
	}

	return type;
}

static void
gda_threader_class_init (GdaThreaderClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	gda_threader_signals[FINISHED] =
		g_signal_new ("finished",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaThreaderClass, finished),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__UINT_POINTER, G_TYPE_NONE,
			      2, G_TYPE_UINT, G_TYPE_POINTER);
	gda_threader_signals[CANCELLED] =
		g_signal_new ("cancelled",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaThreaderClass, cancelled),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__UINT_POINTER, G_TYPE_NONE,
			      2, G_TYPE_UINT, G_TYPE_POINTER);
	class->finished = NULL;
	class->cancelled = NULL;

	object_class->dispose = gda_threader_dispose;
	object_class->finalize = gda_threader_finalize;
}

static void
gda_threader_init (GdaThreader * thread)
{
	thread->priv = g_new0 (GdaThreaderPrivate, 1);

	if (! g_thread_supported()) 
		g_warning ("You must initialize the multi threads environment using g_thread_init()");
	if (getenv ("LIBGDA_NO_THREADS"))
		g_warning ("The LIBGDA_NO_THREADS environment variable has been set, threading not supported");

	thread->priv->next_job = 1;
	thread->priv->nb_jobs = 0;
	thread->priv->jobs = g_hash_table_new (NULL, NULL);
	thread->priv->queue = g_async_queue_new ();
	thread->priv->idle_func_id = 0;
}

/**
 * gda_threader_new
 *
 * Creates a new GdaThreader object.
 * 
 * Returns: the newly created object
 */
GObject *
gda_threader_new (void)
{
	GObject   *obj;

	GdaThreader *thread;
	obj = g_object_new (GDA_TYPE_THREADER, NULL);
	thread = GDA_THREADER (obj);

	return obj;
}


static void
gda_threader_dispose (GObject   * object)
{
	GdaThreader *thread;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_THREADER (object));

	thread = GDA_THREADER (object);
	if (thread->priv) {
		if (thread->priv->idle_func_id) {
			g_idle_remove_by_data (thread);
			thread->priv->idle_func_id = 0;
		}

		if (thread->priv->nb_jobs > 0) {
			/*g_warning ("There are still some running threads, some memory will be leaked!");*/
			thread->priv->nb_jobs = 0;
		}

		if (thread->priv->jobs) {
			g_hash_table_destroy (thread->priv->jobs);
			thread->priv->jobs = NULL;
		}
	}

	/* parent class */
	parent_class->dispose (object);
}

static void
gda_threader_finalize (GObject   * object)
{
	GdaThreader *thread;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_THREADER (object));

	thread = GDA_THREADER (object);
	if (thread->priv) {
		g_free (thread->priv);
		thread->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}

static gboolean idle_catch_threads_end (GdaThreader *thread);
static gpointer spawn_new_thread (ThreadJob *job);

/**
 * gda_threader_start_thread
 * @thread: a #GdaThreader object
 * @func: the function to be called in the newly created thread
 * @func_arg: @func's argument
 * @ok_callback: callback called when @func terminates
 * @cancel_callback: callback called when @func terminates and the job has been cancelled
 * @error: place to store an error when creating the thread or %NULL
 *
 * Starts a new worker thread, executing the @func function with the @func_arg argument. It is possible
 * to request the worker thread to terminates using gda_threader_cancel().
 * 
 * Returns: the id of the new job executed in another thread.
 */
guint
gda_threader_start_thread (GdaThreader *thread, GThreadFunc func, gpointer func_arg, 
			   GdaThreaderFunc ok_callback, GdaThreaderFunc cancel_callback, 
			   GError **error)
{
	ThreadJob *job;

	g_return_val_if_fail (thread && GDA_IS_THREADER (thread), 0);
	g_return_val_if_fail (func, 0);
	
	job = g_new0 (ThreadJob, 1);
	job->thread = thread;
	job->func = func;
	job->func_data = func_arg;
	job->id = thread->priv->next_job ++;
	job->cancelled = FALSE;
	job->normal_end_cb = ok_callback;
	job->cancel_end_cb = cancel_callback;

	/* g_print ("** New thread starting ..., job = %d\n", job->id); */
	job->g_thread = g_thread_create ((GThreadFunc) spawn_new_thread, job, FALSE, error);

	if (!job->g_thread) {
		g_free (job);
		return 0;
	}
	else {
		thread->priv->nb_jobs ++;

		g_hash_table_insert (thread->priv->jobs, GUINT_TO_POINTER (job->id), job);
		if (! thread->priv->idle_func_id) 
			thread->priv->idle_func_id = g_timeout_add_full (G_PRIORITY_HIGH_IDLE, 150,
									 (GSourceFunc) idle_catch_threads_end, 
									 thread, NULL);
		
		return job->id;
	}
}


/* WARNING: called in another thread */
static gpointer
spawn_new_thread (ThreadJob *job)
{
	GAsyncQueue *queue;

	queue = job->thread->priv->queue;
	g_async_queue_ref (queue);

	/* call job's real function */
	/* g_print ("** T: Calling job function for job %d\n", job->id); */
	(job->func) (job->func_data);

	/* push result when finished */
	/* g_print ("** T: Pushing result for job %d\n", job->id); */
	g_async_queue_push (queue, job);

	/* terminate thread */
	g_async_queue_unref (queue);
	/* g_print ("** T: End of thread for job %d\n", job->id); */
	g_thread_exit (job);
	
	return job;
}

static gboolean
idle_catch_threads_end (GdaThreader *thread)
{
	ThreadJob *job;
	gboolean retval = TRUE;

	job = g_async_queue_try_pop (thread->priv->queue);
	if (job) {
		/* that job has finished */
		/* g_print ("** Signaling end of job %d\n", job->id); */

		thread->priv->nb_jobs --;
		if (thread->priv->nb_jobs == 0) {
			retval = FALSE;
			thread->priv->idle_func_id = 0;
		}
		g_hash_table_remove (thread->priv->jobs, GUINT_TO_POINTER (job->id));

		if (job->cancelled) {
			if (job->cancel_end_cb)
				job->cancel_end_cb (thread, job->id, job->func_data);
		}
		else {
			g_signal_emit (thread, gda_threader_signals [FINISHED], 0, job->id, job->func_data);
			if (job->normal_end_cb)
				job->normal_end_cb (thread, job->id, job->func_data);
		}

		g_free (job);
	}

	return retval;
}

/**
 * gda_threader_cancel
 */
void
gda_threader_cancel (GdaThreader *thread, guint job_id)
{
	ThreadJob *job;

	g_return_if_fail (thread && GDA_IS_THREADER (thread));
	job = g_hash_table_lookup (thread->priv->jobs, GUINT_TO_POINTER (job_id));
	if (!job)
		g_warning ("Could not find threaded job %d", job_id);
	else {
		job->cancelled = TRUE;
		g_signal_emit (thread, gda_threader_signals [CANCELLED], 0, job->id, job->func_data);
	}
}
