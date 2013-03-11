/*
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2009 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2010 Jonh Wendell <jwendell@gnome.org>
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

//#define DEBUG_NOTIFICATION

#include <string.h>
#include <glib/gi18n-lib.h>
#include "gda-thread-wrapper.h"
#include <libgda/gda-mutex.h>
#include <gobject/gvaluecollector.h>
#include <libgda/gda-debug-macros.h>
#include <libgda/gda-value.h>
#include <unistd.h>
#include <sys/stat.h>
#ifdef G_OS_WIN32
#include <fcntl.h>
#include <io.h>
#endif

/* this GPrivate holds a pointer to the GAsyncQueue used by the job being currently treated
 * by the worker thread. It is used to avoid creating signal data for threads for which
 * no job is being performed 
 */
#if GLIB_CHECK_VERSION(2,31,7)
GPrivate worker_thread_current_queue;
#else
GStaticPrivate worker_thread_current_queue = G_STATIC_PRIVATE_INIT;
#endif

typedef struct _ThreadData ThreadData;
typedef struct _Job Job;
typedef struct _SignalSpec SignalSpec;
typedef struct _Pipe Pipe;

struct _GdaThreadWrapperPrivate {
#if GLIB_CHECK_VERSION(2,31,7)
	GRecMutex    rmutex;
#else
	GdaMutex    *mutex;
#endif
	guint        next_job_id;
	GThread     *worker_thread;
	GAsyncQueue *to_worker_thread;

	GHashTable  *threads_hash; /* key = a GThread, value = a #ThreadData pointer */
	GHashTable  *pipes_hash; /* key = a GThread, value = a #Pipe pointer */
};

/*
 * Threads synchronization with notifications
 *
 * Both Unix and Windows create a set of 2 file descriptors, the one at potision 0 for reading
 * and the one at position 1 for writing.
 */
struct _Pipe {
	GThread     *thread;
	int          fds[2]; /* [0] for reading and [1] for writing */
	GIOChannel  *ioc;

#if GLIB_CHECK_VERSION(2,31,7)
	GMutex       mutex;
#else
	GMutex      *mutex; /* locks @ref_count */
#endif
	guint        ref_count;
};

#if GLIB_CHECK_VERSION(2,31,7)
#define pipe_lock(x) g_mutex_lock(& (((Pipe*)x)->mutex))
#define pipe_unlock(x) g_mutex_unlock(& (((Pipe*)x)->mutex))
#else
#define pipe_lock(x) g_mutex_lock(((Pipe*)x)->mutex);
#define pipe_unlock(x) g_mutex_unlock(((Pipe*)x)->mutex);
#endif

static Pipe *
pipe_ref (Pipe *p)
{
	if (p) {
		pipe_lock (p);
		p->ref_count++;
#ifdef DEBUG_NOTIFICATION
		g_print ("Pipe %p ++: %u\n", p, p->ref_count);
#endif
		pipe_unlock (p);
	}
	return p;
}

static void
pipe_unref (Pipe *p)
{
	if (p) {
		pipe_lock (p);
		p->ref_count--;
#ifdef DEBUG_NOTIFICATION
		g_print ("Pipe %p --: %u\n", p, p->ref_count);
#endif
		if (p->ref_count == 0) {
			/* destroy @p */
#if GLIB_CHECK_VERSION(2,31,7)
			GMutex *m = &(p->mutex);
#else
			GMutex *m = p->mutex;
#endif

			if (p->ioc)
				g_io_channel_unref (p->ioc);
#ifdef G_OS_WIN32
			if (p->fds[0] >= 0)
				_close (p->fds[0]);
			if (p->fds[1] >= 0)
				_close (p->fds[1]);
#else
			if (p->fds[0] >= 0)
				close (p->fds[0]);
			if (p->fds[1] >= 0)
				close (p->fds[1]);
#endif

#ifdef DEBUG_NOTIFICATION
			g_print ("Destroyed Pipe %p\n", p);
#endif
			g_free (p);

			g_mutex_unlock (m);
#if GLIB_CHECK_VERSION(2,31,7)
			g_mutex_clear (m);
#else
			g_mutex_free (m);
#endif

		}
		else
			pipe_unlock (p);
	}
}

/*
 * May return %NULL
 */
static Pipe *
pipe_new (void)
{
	Pipe *p;

	p = g_new0 (Pipe, 1);
#if GLIB_CHECK_VERSION(2,31,7)
	g_mutex_init (&(p->mutex));
#else
	p->mutex = g_mutex_new ();
#endif
	p->ref_count = 1;
	p->thread = g_thread_self ();
#ifdef G_OS_WIN32
	if (_pipe (p->fds, 156, O_BINARY) != 0) {
#else
	if (pipe (p->fds) != 0) {
#endif
		pipe_unref (p);
		p = NULL;
		goto out;
	}
#ifdef G_OS_WIN32
	p->ioc = g_io_channel_win32_new_fd (p->fds [0]);
#else
	p->ioc = g_io_channel_unix_new (p->fds [0]);
#endif

	/* we want raw data */
	if (g_io_channel_set_encoding (p->ioc, NULL, NULL) != G_IO_STATUS_NORMAL) {
		g_warning ("Can't set IO encoding to NULL\n");
		pipe_unref (p);
		p = NULL;
	}

 out:
#ifdef DEBUG_NOTIFICATION
	g_print ("Created Pipe %p\n", p);
#endif
	return p;
}

static Pipe *
get_pipe (GdaThreadWrapper *wrapper, GThread *thread)
{
	Pipe *p = NULL;
#if GLIB_CHECK_VERSION(2,31,7)
	g_rec_mutex_lock (&(wrapper->priv->rmutex));
#else
	gda_mutex_lock (wrapper->priv->mutex);
#endif
	if (wrapper->priv->pipes_hash)
		p = g_hash_table_lookup (wrapper->priv->pipes_hash, thread);
#if GLIB_CHECK_VERSION(2,31,7)
	g_rec_mutex_unlock (&(wrapper->priv->rmutex));
#else
	gda_mutex_unlock (wrapper->priv->mutex);
#endif
	return p;
}

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
	JOB_TYPE_EXECUTE, /* a "real" job for the GdaThreadWrapper */
	JOB_TYPE_DESTROY, /* internal to signal the internal thread to shutdown */
	JOB_TYPE_SIGNAL, /* a signal from an object in the internal thread */
	JOB_TYPE_NOTIFICATION_ERROR /* internal to signal notification error and sutdown */
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
	Pipe                    *notif; /* if not %NULL, notification when job finished */

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
	pipe_unref (job->notif);
	if (job->arg && job->arg_destroy_func)
		job->arg_destroy_func (job->arg);
	if (job->reply_queue)
		g_async_queue_unref (job->reply_queue);

	if (job->type == JOB_TYPE_EXECUTE) {
		if (job->u.exe.error)
			g_error_free (job->u.exe.error);
	}
	else if (job->type == JOB_TYPE_SIGNAL) {
		guint i;
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
	else if (job->type == JOB_TYPE_NOTIFICATION_ERROR) {
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
	Pipe         *notif; /* if not %NULL, notification */

        gpointer      instance;
        gulong        signal_id;

        GdaThreadWrapperCallback callback;
        gpointer                 data;

#if GLIB_CHECK_VERSION(2,31,7)
	GMutex        mutex;
#else
	GMutex       *mutex;
#endif
	guint         ref_count;
};

#if GLIB_CHECK_VERSION(2,31,7)
#define signal_spec_lock(x) g_mutex_lock(& (((SignalSpec*)x)->mutex))
#define signal_spec_unlock(x) g_mutex_unlock(& (((SignalSpec*)x)->mutex))
#else
#define signal_spec_lock(x) g_mutex_lock(((SignalSpec*)x)->mutex);
#define signal_spec_unlock(x) g_mutex_unlock(((SignalSpec*)x)->mutex);
#endif

/*
 * call signal_spec_lock() before calling this function
 */
static void
signal_spec_unref (SignalSpec *sigspec)
{
	sigspec->ref_count --;
	if (sigspec->ref_count == 0) {
		signal_spec_unlock (sigspec);
#if GLIB_CHECK_VERSION(2,31,7)
		g_mutex_clear (&(sigspec->mutex));
#else
		g_mutex_free (sigspec->mutex);
#endif
		if (sigspec->instance && (sigspec->signal_id > 0))
			g_signal_handler_disconnect (sigspec->instance, sigspec->signal_id);
		if (sigspec->reply_queue)
			g_async_queue_unref (sigspec->reply_queue);
		pipe_unref (sigspec->notif);
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

	Pipe        *notif; /* if not %NULL, notification when any job has finished */
};
#define THREAD_DATA(x) ((ThreadData*)(x))

static ThreadData *
get_thread_data (GdaThreadWrapper *wrapper, GThread *thread)
{
	ThreadData *td;
#if GLIB_CHECK_VERSION(2,31,7)
	g_rec_mutex_lock (&(wrapper->priv->rmutex));
#else
	gda_mutex_lock (wrapper->priv->mutex);
#endif
	td = g_hash_table_lookup (wrapper->priv->threads_hash, thread);
	if (!td) {
		Pipe *p;
		p = get_pipe (wrapper, thread);

		td = g_new0 (ThreadData, 1);
		td->owner = thread;
		td->from_worker_thread = g_async_queue_new_full ((GDestroyNotify) job_free);
		td->jobs = NULL;
		td->results = NULL;
		td->notif = pipe_ref (p);
		g_hash_table_insert (wrapper->priv->threads_hash, thread, td);
	}
#if GLIB_CHECK_VERSION(2,31,7)
	g_rec_mutex_unlock (&(wrapper->priv->rmutex));
#else
	gda_mutex_unlock (wrapper->priv->mutex);
#endif
	return td;
}

static void
thread_data_free (ThreadData *td)
{
	pipe_unref (td->notif);
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

static void
clean_notifications (GdaThreadWrapper *wrapper, ThreadData *td)
{
#ifdef DEBUG_NOTIFICATION
	g_print ("%s(Pipe:%p)\n", __FUNCTION__, td->notif);
#endif
	GSList *list;
	for (list = td->signals_list; list; list = list->next) {
		SignalSpec *sigspec;
		sigspec = (SignalSpec*) list->data;
		signal_spec_lock (sigspec);
		if (sigspec->notif == td->notif) {
			pipe_unref (sigspec->notif);
			sigspec->notif = NULL;
		}
		signal_spec_unlock (sigspec);
	}

	pipe_unref (td->notif);
	td->notif = NULL;

	g_hash_table_remove (wrapper->priv->pipes_hash, td->owner);
}

/*
 * @wrapper: (allow-none): may be %NULL
 * @td: (allow-none): may be %NULL
 *
 * Either @wrapper and @td are both NULL, or they are both NOT NULL
 *
 * It is assumed that pipe_ref(p) has been called before calling this function
 */
static gboolean
write_notification (GdaThreadWrapper *wrapper, ThreadData *td,
		    Pipe *p, GdaThreadNotificationType type, guint job_id)
{
	g_assert ((wrapper && td) || (!wrapper && !td));

	if (!p || (p->fds[1] < 0)) {
		pipe_unref (p);
		return TRUE;
	}
#ifdef DEBUG_NOTIFICATION_FORCE
	static guint c = 0;
	c++;
	if (c == 4)
		goto onerror;
#endif

	GdaThreadNotification notif;
	ssize_t nw;
	notif.type = type;
	notif.job_id = job_id;
#ifdef G_OS_WIN32
	nw = _write (p->fds[1], &notif, sizeof (notif));
#else
	nw = write (p->fds[1], &notif, sizeof (notif));
#endif
	if (nw != sizeof (notif)) {
		/* Error */
		goto onerror;
	}
#ifdef DEBUG_NOTIFICATION
	g_print ("Wrote notification %d.%u to pipe %p\n", type, job_id, p);
#endif
	pipe_unref (p);
	return TRUE;

 onerror:
#ifdef DEBUG_NOTIFICATION
	g_print ("%s(): returned FALSE\n", __FUNCTION__);
	g_print ("Closed FD %d\n", p->fds [1]);
#endif
	/* close the writing end of the pipe */
#ifdef G_OS_WIN32
	_close (p->fds [1]);
#else
	close (p->fds [1]);
#endif
	p->fds [1] = -1;
	pipe_unref (p);
	if (td)
		clean_notifications (wrapper, td);
	return FALSE;
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
#if GLIB_CHECK_VERSION(2,31,7)
		g_private_set (&worker_thread_current_queue, NULL);
#else
		g_static_private_set (&worker_thread_current_queue, NULL, NULL);
#endif
		g_async_queue_lock (in);
		job = g_async_queue_pop_unlocked (in);
		job->processed = TRUE;
#if GLIB_CHECK_VERSION(2,31,7)
		g_private_set (&worker_thread_current_queue, job->reply_queue);
#else
		g_static_private_set (&worker_thread_current_queue, job->reply_queue, NULL);
#endif
		g_async_queue_unlock (in);

		if (job->cancelled) {
			job_free (job);
			continue;
		}

		if (job->type == JOB_TYPE_DESTROY) {
			g_assert (! job->arg_destroy_func);
			job_free (job);
#ifdef THREAD_WRAPPER_DEBUG
			g_print ("... exit sub thread %p for wrapper\n", g_thread_self ());
#endif

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

			guint jid = job->job_id;
			Pipe *jpipe = pipe_ref (job->notif);
			g_async_queue_push (job->reply_queue, job);
			if (! write_notification (NULL, NULL, jpipe, GDA_THREAD_NOTIFICATION_JOB, jid)) {
				Job *je = g_new0 (Job, 1);
				je->type = JOB_TYPE_NOTIFICATION_ERROR;
				g_async_queue_push (job->reply_queue, je);
			}
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
#if GLIB_CHECK_VERSION(2,31,7)
	g_rec_mutex_init (&(wrapper->priv->rmutex));
#else
	wrapper->priv->mutex = gda_mutex_new ();
#endif
	wrapper->priv->next_job_id = 1;

	wrapper->priv->threads_hash = g_hash_table_new_full (NULL, NULL, NULL, (GDestroyNotify) thread_data_free);

	wrapper->priv->to_worker_thread = g_async_queue_new ();
#if GLIB_CHECK_VERSION(2,31,7)
	wrapper->priv->worker_thread = g_thread_new ("worker",
						     (GThreadFunc) worker_thread_entry_point,
						     g_async_queue_ref (wrapper->priv->to_worker_thread)); /* inc. ref for sub thread usage */
#else
	wrapper->priv->worker_thread = g_thread_create ((GThreadFunc) worker_thread_entry_point,
							g_async_queue_ref (wrapper->priv->to_worker_thread), /* inc. ref for sub thread usage */
							FALSE, NULL);
#endif
	
	wrapper->priv->pipes_hash = NULL;

#ifdef THREAD_WRAPPER_DEBUG
	g_print ("... new wrapper %p, worker_thread=%p\n", wrapper, wrapper->priv->worker_thread);
#endif
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
		job->notif = NULL;
		g_async_queue_push (wrapper->priv->to_worker_thread, job);
#ifdef THREAD_WRAPPER_DEBUG
		g_print ("... pushed JOB_TYPE_DESTROY for wrapper %p\n", wrapper);
#endif
		
		g_async_queue_lock (wrapper->priv->to_worker_thread);
		if (wrapper->priv->threads_hash) {
			g_hash_table_foreach_remove (wrapper->priv->threads_hash,
						     (GHRFunc) thread_data_remove_jobs_func, NULL);
			g_hash_table_destroy (wrapper->priv->threads_hash);
		}
		g_async_queue_unlock (wrapper->priv->to_worker_thread);
		g_async_queue_unref (wrapper->priv->to_worker_thread);
		wrapper->priv->worker_thread = NULL; /* side note: don't wait for sub thread to terminate */

#if GLIB_CHECK_VERSION(2,31,7)
		g_rec_mutex_clear (&(wrapper->priv->rmutex));
#else
		gda_mutex_free (wrapper->priv->mutex);
#endif

		if (wrapper->priv->pipes_hash)
			g_hash_table_destroy (wrapper->priv->pipes_hash);

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
 * gda_thread_wrapper_get_type:
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
			       GParamSpec *pspec)
{
	GdaThreadWrapper *wrapper;

        wrapper = GDA_THREAD_WRAPPER (object);
        if (wrapper->priv) {
                switch (param_id) {
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
	}
}

static void
gda_thread_wrapper_get_property (GObject *object,
			       guint param_id,
			       G_GNUC_UNUSED GValue *value,
			       GParamSpec *pspec)
{
	GdaThreadWrapper *wrapper;
	
	wrapper = GDA_THREAD_WRAPPER (object);
	if (wrapper->priv) {
		switch (param_id) {
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
	}
}

/**
 * gda_thread_wrapper_new:
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
 * gda_thread_wrapper_get_io_channel:
 * @wrapper: a #GdaThreadWrapper object
 *
 * Allow @wrapper to notify when an execution job is finished, by making its exec ID
 * readable through a new #GIOChannel. This function is useful when the notification needs
 * to be included into a main loop. This also notifies that signals (emitted by objects in
 * @wrapper's internal thread) are available.
 *
 * The returned #GIOChannel will have something to read everytime an execution job is finished
 * for an execution job submitted from the calling thread. The user whould read #GdaThreadNotification
 * structures from the channel and analyse its contents to call gda_thread_wrapper_iterate()
 * or gda_thread_wrapper_fetch_result().
 *
 * Note1: the new communication channel will only be operational for jobs submitted after this
 * function returns, and for signals which have been connected after this function returns. A safe
 * practice is to call this function before the @wrapper object has been used.
 *
 * Note2: this function will return the same #GIOChannel everytime it's called from the same thread.
 *
 * Note3: if the usage of the returned #GIOChannel reveals an error, then g_io_channel_shutdown() and
 * g_io_channel_unref() should be called on the #GIOChannel to let @wrapper know it should not use
 * that object anymore.
 *
 * Returns: (transfer none): a new #GIOChannel, or %NULL if it could not be created
 *
 * Since: 4.2.9
 */
GIOChannel *
gda_thread_wrapper_get_io_channel (GdaThreadWrapper *wrapper)
{
	Pipe *p;
	GThread *th;
	g_return_val_if_fail (GDA_IS_THREAD_WRAPPER (wrapper), NULL);
	g_return_val_if_fail (wrapper->priv, NULL);

	th = g_thread_self ();
#if GLIB_CHECK_VERSION(2,31,7)
	g_rec_mutex_lock (&(wrapper->priv->rmutex));
#else
	gda_mutex_lock (wrapper->priv->mutex);
#endif
	p = get_pipe (wrapper, th);
	if (!p) {
		p = pipe_new ();
		if (p) {
			if (! wrapper->priv->pipes_hash)
				wrapper->priv->pipes_hash = g_hash_table_new_full (g_direct_hash,
										   g_direct_equal,
										   NULL,
										   (GDestroyNotify) pipe_unref);
			g_hash_table_insert (wrapper->priv->pipes_hash, th, p);
		}
	}
#if GLIB_CHECK_VERSION(2,31,7)
	g_rec_mutex_unlock (&(wrapper->priv->rmutex));
#else
	gda_mutex_unlock (wrapper->priv->mutex);
#endif
	if (p)
		return p->ioc;
	else
		return NULL;
}

/**
 * gda_thread_wrapper_unset_io_channel:
 * @wrapper: a #GdaThreadWrapper
 *
 * Does the opposite of gda_thread_wrapper_get_io_channel()
 *
 * Since: 4.2.9
 */
void
gda_thread_wrapper_unset_io_channel (GdaThreadWrapper *wrapper)
{
	GThread *th;
	ThreadData *td;

	g_return_if_fail (GDA_IS_THREAD_WRAPPER (wrapper));
	g_return_if_fail (wrapper->priv);

#if GLIB_CHECK_VERSION(2,31,7)
	g_rec_mutex_lock (&(wrapper->priv->rmutex));
#else
	gda_mutex_lock (wrapper->priv->mutex);
#endif
	th = g_thread_self ();
	td = g_hash_table_lookup (wrapper->priv->threads_hash, th);
	if (td) {
		Pipe *p;
		p = get_pipe (wrapper, th);
		if (p)
			clean_notifications (wrapper, td);
	}
#if GLIB_CHECK_VERSION(2,31,7)
	g_rec_mutex_unlock (&(wrapper->priv->rmutex));
#else
	gda_mutex_unlock (wrapper->priv->mutex);
#endif
}

/**
 * gda_thread_wrapper_execute:
 * @wrapper: a #GdaThreadWrapper object
 * @func: the function to execute, not %NULL
 * @arg: (allow-none): argument to pass to @func, or %NULL
 * @arg_destroy_func: (allow-none): function to be called when the execution has finished, to destroy @arg, or %NULL
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
 * Once @func's execution is finished, if @arg is not %NULL, the @arg_destroy_func destruction function is called
 * on @arg. This call occurs in the thread calling gda_thread_wrapper_fetch_result().
 *
 * If an error occurred in this function, then the @arg_destroy_func function is not called to free @arg.
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
#if GLIB_CHECK_VERSION(2,31,7)
	g_rec_mutex_lock (&(wrapper->priv->rmutex));
#else
	gda_mutex_lock (wrapper->priv->mutex);
#endif
	job->job_id = wrapper->priv->next_job_id++;
#if GLIB_CHECK_VERSION(2,31,7)
	g_rec_mutex_unlock (&(wrapper->priv->rmutex));
#else
	gda_mutex_unlock (wrapper->priv->mutex);
#endif
	job->func = func;
	job->void_func = NULL;
	job->arg = arg;
	job->arg_destroy_func = arg_destroy_func;
	job->reply_queue = g_async_queue_ref (td->from_worker_thread);
	job->notif = pipe_ref (td->notif);

	id = job->job_id;
#ifdef THREAD_WRAPPER_DEBUG
	g_print ("... submitted job %d for wrapper %p from thread %p\n", id, wrapper, g_thread_self());
#endif

	td->jobs = g_slist_append (td->jobs, job);

	if (g_thread_self () == wrapper->priv->worker_thread) {
		job->processed = TRUE;
                if (job->func)
                        job->u.exe.result = job->func (job->arg, &(job->u.exe.error));
                else {
                        job->u.exe.result = NULL;
                        job->void_func (job->arg, &(job->u.exe.error));
                }
#ifdef THREAD_WRAPPER_DEBUG
		g_print ("... IMMEDIATELY done job %d => %p\n", job->job_id, job->u.exe.result);
#endif
		guint jid = job->job_id;
		Pipe *jpipe = pipe_ref (job->notif);
                g_async_queue_push (job->reply_queue, job);
		write_notification (wrapper, td, jpipe, GDA_THREAD_NOTIFICATION_JOB, jid);
        }
        else
                g_async_queue_push (wrapper->priv->to_worker_thread, job);

	return id;
}

/**
 * gda_thread_wrapper_execute_void:
 * @wrapper: a #GdaThreadWrapper object
 * @func: the function to execute, not %NULL
 * @arg: (allow-none): argument to pass to @func
 * @arg_destroy_func: (allow-none): function to be called when the execution has finished, to destroy @arg, or %NULL
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
 * Once @func's execution is finished, if @arg is not %NULL, the @arg_destroy_func destruction function is called
 * on @arg. This call occurs in the thread calling gda_thread_wrapper_fetch_result().
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
#if GLIB_CHECK_VERSION(2,31,7)
	g_rec_mutex_lock (&(wrapper->priv->rmutex));
#else
	gda_mutex_lock (wrapper->priv->mutex);
#endif
	job->job_id = wrapper->priv->next_job_id++;
#if GLIB_CHECK_VERSION(2,31,7)
	g_rec_mutex_unlock (&(wrapper->priv->rmutex));
#else
	gda_mutex_unlock (wrapper->priv->mutex);
#endif
	job->func = NULL;
	job->void_func = func;
	job->arg = arg;
	job->arg_destroy_func = arg_destroy_func;
	job->reply_queue = g_async_queue_ref (td->from_worker_thread);
	job->notif = pipe_ref (td->notif);

	id = job->job_id;
#ifdef THREAD_WRAPPER_DEBUG
	g_print ("... submitted VOID job %d\n", id);
#endif

	td->jobs = g_slist_append (td->jobs, job);

	if (g_thread_self () == wrapper->priv->worker_thread) {
		job->processed = TRUE;
                if (job->func)
                        job->u.exe.result = job->func (job->arg, &(job->u.exe.error));
                else {
                        job->u.exe.result = NULL;
                        job->void_func (job->arg, &(job->u.exe.error));
                }
#ifdef THREAD_WRAPPER_DEBUG
		g_print ("... IMMEDIATELY done VOID job %d => %p\n", job->job_id, job->u.exe.result);
#endif
		guint jid = job->job_id;
		Pipe *jpipe = pipe_ref (job->notif);
                g_async_queue_push (job->reply_queue, job);
		write_notification (wrapper, td, jpipe, GDA_THREAD_NOTIFICATION_JOB, jid);
        }
        else
		g_async_queue_push (wrapper->priv->to_worker_thread, job);

	return id;
}

/**
 * gda_thread_wrapper_cancel:
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

#if GLIB_CHECK_VERSION(2,31,7)
	g_rec_mutex_lock (&(wrapper->priv->rmutex));
#else
	gda_mutex_lock (wrapper->priv->mutex);
#endif

	td = g_hash_table_lookup (wrapper->priv->threads_hash, g_thread_self());
	if (!td) {
		/* nothing to be done for this thread */
#if GLIB_CHECK_VERSION(2,31,7)
		g_rec_mutex_unlock (&(wrapper->priv->rmutex));
#else
		gda_mutex_unlock (wrapper->priv->mutex);
#endif
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
#if GLIB_CHECK_VERSION(2,31,7)
	g_rec_mutex_unlock (&(wrapper->priv->rmutex));
#else
	gda_mutex_unlock (wrapper->priv->mutex);
#endif

	return retval;
}

/**
 * gda_thread_wrapper_iterate:
 * @wrapper: a #GdaThreadWrapper object
 * @may_block: whether the call may block
 *
 * This method gives @wrapper a chance to check if some functions to be executed have finished
 * <emphasis>for the calling thread</emphasis>. In this case it handles the execution result and
 * makes it ready to be processed using gda_thread_wrapper_fetch_result().
 *
 * This method also allows @wrapper to handle signals which may have been emitted by objects
 * while in the worker thread, and call the callback function specified when gda_thread_wrapper_connect_raw()
 * was used.
 *
 * If @may_block is %TRUE, then it will block untill there is one finished execution 
 * (functions returning void and signals are ignored regarding this argument).
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

#if GLIB_CHECK_VERSION(2,31,7)
	g_rec_mutex_lock (&(wrapper->priv->rmutex));
#else
	gda_mutex_lock (wrapper->priv->mutex);
#endif
	td = g_hash_table_lookup (wrapper->priv->threads_hash, g_thread_self());
#if GLIB_CHECK_VERSION(2,31,7)
	g_rec_mutex_unlock (&(wrapper->priv->rmutex));
#else
	gda_mutex_unlock (wrapper->priv->mutex);
#endif
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
#ifdef THREAD_WRAPPER_DEBUG
		g_print ("Popped job %d (type %d), for wrapper %p from thread %p\n",
			 job->job_id, job->type, wrapper, g_thread_self ());
#endif

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
#ifdef THREAD_WRAPPER_DEBUG
			else
				g_print ("Not propagating signal %s\n", ((GSignalQuery*)spec)->signal_name);
#endif
			job->u.signal.spec = NULL;
			job_free (job);
			signal_spec_lock (spec);
			signal_spec_unref (spec);
			do_again = TRUE;
		}
		else if (job->type == JOB_TYPE_NOTIFICATION_ERROR) {
			job_free (job);
			clean_notifications (wrapper, td);
		}
		else
			g_assert_not_reached ();

		if (do_again)
			goto again;
	}
}

/**
 * gda_thread_wrapper_fetch_result:
 * @wrapper: a #GdaThreadWrapper object
 * @may_lock: TRUE if this funct must lock the caller untill a result is available
 * @exp_id: ID of the job for which a result is expected
 * @error: a place to store errors, for errors which may have occurred during the execution, or %NULL
 *
 * Use this method to check if the execution of a function is finished. The function's execution must have
 * been requested using gda_thread_wrapper_execute().
 *
 * Returns: (transfer none) (allow-none): the pointer returned by the execution, or %NULL if no result is available
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

#if GLIB_CHECK_VERSION(2,31,7)
	g_rec_mutex_lock (&(wrapper->priv->rmutex));
#else
	gda_mutex_lock (wrapper->priv->mutex);
#endif
	td = g_hash_table_lookup (wrapper->priv->threads_hash, g_thread_self());
#if GLIB_CHECK_VERSION(2,31,7)
	g_rec_mutex_unlock (&(wrapper->priv->rmutex));
#else
	gda_mutex_unlock (wrapper->priv->mutex);
#endif
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
#if GLIB_CHECK_VERSION(2,31,7)
						g_rec_mutex_lock (&(wrapper->priv->rmutex));
#else
						gda_mutex_lock (wrapper->priv->mutex);
#endif
						g_hash_table_remove (wrapper->priv->threads_hash, g_thread_self());
#if GLIB_CHECK_VERSION(2,31,7)
						g_rec_mutex_unlock (&(wrapper->priv->rmutex));
#else
						gda_mutex_unlock (wrapper->priv->mutex);
#endif
					}
					goto out;
				}
			}
		}
		
		if (may_lock) 
			gda_thread_wrapper_iterate (wrapper, TRUE);
		else {
			gsize len;
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
 * gda_thread_wrapper_get_waiting_size:
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

#if GLIB_CHECK_VERSION(2,31,7)
	g_rec_mutex_lock (&(wrapper->priv->rmutex));
#else
	gda_mutex_lock (wrapper->priv->mutex);
#endif
	td = g_hash_table_lookup (wrapper->priv->threads_hash, g_thread_self());
	if (!td) {
		/* nothing to be done for this thread */
#if GLIB_CHECK_VERSION(2,31,7)
		g_rec_mutex_unlock (&(wrapper->priv->rmutex));
#else
		gda_mutex_unlock (wrapper->priv->mutex);
#endif
		return 0;
	}
	
	/* lock job consuming queue to avoid that the worker thread "consume" a Job */
	g_async_queue_lock (wrapper->priv->to_worker_thread);
	for (size = 0, list = td->jobs; list; list = list->next) {
		if (!JOB (list->data)->cancelled)
			size++;
	}
	g_async_queue_unlock (wrapper->priv->to_worker_thread);
#if GLIB_CHECK_VERSION(2,31,7)
	g_rec_mutex_unlock (&(wrapper->priv->rmutex));
#else
	gda_mutex_unlock (wrapper->priv->mutex);
#endif
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

	/* check that the worker thread is working on a job for which job->reply_queue == sigspec->reply_queue */
#if GLIB_CHECK_VERSION(2,31,7)
	if (sigspec->private &&
	    g_private_get (&worker_thread_current_queue) != sigspec->reply_queue)
		return;
#else
	if (sigspec->private &&
	    g_static_private_get (&worker_thread_current_queue) != sigspec->reply_queue)
		return;
#endif

	gsize i;
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

		g_value_init (dest, G_VALUE_TYPE (src));
		g_value_copy (src, dest);
	}

	Pipe *jpipe = pipe_ref (sigspec->notif);
	g_async_queue_push (sigspec->reply_queue, job);
	if (! write_notification (NULL, NULL, jpipe, GDA_THREAD_NOTIFICATION_SIGNAL, 0)) {
		Job *je = g_new0 (Job, 1);
		je->type = JOB_TYPE_NOTIFICATION_ERROR;
		g_async_queue_push (sigspec->reply_queue, je);
	}
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

	gsize i;
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
	job->notif = NULL;
	for (i = 1; i < n_param_values; i++) {
		const GValue *src;
		GValue *dest;

		src = param_values + i;
		dest = job->u.signal.param_values + i - 1;

		g_value_init (dest, G_VALUE_TYPE (src));
		g_value_copy (src, dest);
	}

	Pipe *jpipe = pipe_ref (sigspec->notif);
	g_async_queue_push (sigspec->reply_queue, job);
	if (! write_notification (NULL, NULL, jpipe, GDA_THREAD_NOTIFICATION_SIGNAL, 0)) {
		Job *je = g_new0 (Job, 1);
		je->type = JOB_TYPE_NOTIFICATION_ERROR;
		g_async_queue_push (sigspec->reply_queue, je);
	}
	signal_spec_unlock (sigspec);
}

/**
 * gda_thread_wrapper_connect_raw:
 * @wrapper: a #GdaThreadWrapper object
 * @instance: the instance to connect to
 * @sig_name: a string of the form "signal-name::detail"
 * @private_thread:  set to %TRUE if @callback is to be invoked only if the signal has
 *    been emitted while in @wrapper's private sub thread (ie. used when @wrapper is executing some functions
 *    specified by gda_thread_wrapper_execute() or gda_thread_wrapper_execute_void()), and to %FALSE if the
 *    callback is to be invoked whenever the signal is emitted, independently of the thread in which the
 *    signal is emitted.
 * @private_job: set to %TRUE if @callback is to be invoked only if the signal has
 *    been emitted when a job created for the calling thread is being executed, and to %FALSE
 *    if @callback has to be called whenever the @sig_name signal is emitted by @instance. Note that
 *    this argument is not taken into account if @private_thread is set to %FALSE.
 * @callback: (scope call): a #GdaThreadWrapperCallback function
 * @data: (closure): data to pass to @callback's calls
 *
 * Connects a callback function to a signal for a particular object. The difference with g_signal_connect() and
 * similar functions are:
 * <itemizedlist>
 *  <listitem><para>the @callback argument is not a #GCallback function, so the callback signature is not
 *    dependent on the signal itself</para></listitem>
 *  <listitem><para>the signal handler must not have to return any value</para></listitem>
 *  <listitem><para>the @callback function will be called asynchronously, the caller may need to use 
 *    gda_thread_wrapper_iterate() to get the notification</para></listitem>
 *  <listitem><para>if @private_job and @private_thread control in which case the signal is propagated.</para></listitem>
 * </itemizedlist>
 *
 * Also note that signal handling is done asynchronously: when emitted in the worker thread, it
 * will be "queued" to be processed in the user thread when it has the chance (when gda_thread_wrapper_iterate()
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

#if GLIB_CHECK_VERSION(2,31,7)
	g_rec_mutex_lock (&(wrapper->priv->rmutex));
#else
	gda_mutex_lock (wrapper->priv->mutex);
#endif
	
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
	sigspec->notif = pipe_ref (td->notif);
        sigspec->instance = instance;
        sigspec->callback = callback;
        sigspec->data = data;
#if GLIB_CHECK_VERSION(2,31,7)
	g_mutex_init (&(sigspec->mutex));
#else
	sigspec->mutex = g_mutex_new ();
#endif
	sigspec->ref_count = 1;

	GClosure *cl;
	cl = g_closure_new_simple (sizeof (GClosure), sigspec);
	if (private_thread)
		g_closure_set_marshal (cl, (GClosureMarshal) worker_thread_closure_marshal);
	else
		g_closure_set_marshal (cl, (GClosureMarshal) worker_thread_closure_marshal_anythread);
	sigspec->signal_id = g_signal_connect_closure (instance, sig_name, cl, FALSE);

	td->signals_list = g_slist_append (td->signals_list, sigspec);

#if GLIB_CHECK_VERSION(2,31,7)
	g_rec_mutex_unlock (&(wrapper->priv->rmutex));
#else
	gda_mutex_unlock (wrapper->priv->mutex);
#endif

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
 * gda_thread_wrapper_disconnect:
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

#if GLIB_CHECK_VERSION(2,31,7)
	g_rec_mutex_lock (&(wrapper->priv->rmutex));
#else
	gda_mutex_lock (wrapper->priv->mutex);
#endif

	td = g_hash_table_lookup (wrapper->priv->threads_hash, g_thread_self());
	if (!td) {
		gulong theid = id;
		td = g_hash_table_find (wrapper->priv->threads_hash,
					(GHRFunc) find_signal_r_func, &theid);
	}
	if (!td) {
		g_warning (_("Signal %lu does not exist"), id);
#if GLIB_CHECK_VERSION(2,31,7)
		g_rec_mutex_unlock (&(wrapper->priv->rmutex));
#else
		gda_mutex_unlock (wrapper->priv->mutex);
#endif
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
#if GLIB_CHECK_VERSION(2,31,7)
		g_rec_mutex_unlock (&(wrapper->priv->rmutex));
#else
		gda_mutex_unlock (wrapper->priv->mutex);
#endif
		return;
	}

	signal_spec_lock (sigspec);
#ifdef THREAD_WRAPPER_DEBUG
	g_print ("Disconnecting signal %s for wrapper %p\n", ((GSignalQuery*)sigspec)->signal_name, wrapper);
#endif
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
#if GLIB_CHECK_VERSION(2,31,7)
	g_rec_mutex_unlock (&(wrapper->priv->rmutex));
#else
	gda_mutex_unlock (wrapper->priv->mutex);
#endif
}

/**
 * gda_thread_wrapper_steal_signal:
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

#if GLIB_CHECK_VERSION(2,31,7)
	g_rec_mutex_lock (&(wrapper->priv->rmutex));
#else
	gda_mutex_lock (wrapper->priv->mutex);
#endif

	gulong theid = id;
	old_td = g_hash_table_find (wrapper->priv->threads_hash,
				    (GHRFunc) find_signal_r_func, &theid);
	if (!old_td) {
		g_warning (_("Signal %lu does not exist"), id);
#if GLIB_CHECK_VERSION(2,31,7)
		g_rec_mutex_unlock (&(wrapper->priv->rmutex));
#else
		gda_mutex_unlock (wrapper->priv->mutex);
#endif
		return;
	}

	if (old_td->owner == g_thread_self ()) {
#if GLIB_CHECK_VERSION(2,31,7)
		g_rec_mutex_unlock (&(wrapper->priv->rmutex));
#else
		gda_mutex_unlock (wrapper->priv->mutex);
#endif
		return;
	}

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

#if GLIB_CHECK_VERSION(2,31,7)
	g_rec_mutex_unlock (&(wrapper->priv->rmutex));
#else
	gda_mutex_unlock (wrapper->priv->mutex);
#endif
}
