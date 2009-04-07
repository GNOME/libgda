/* GDA library
 * Copyright (C) 2009 The GNOME Foundation.
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

typedef struct _ThreadData ThreadData;
typedef struct _SharedData SharedData;
typedef struct _Job Job;
typedef struct _Result Result;
typedef struct _SignalSpec SignalSpec;
typedef struct _SignalEmissionData SignalEmissionData;

struct _GdaThreadWrapperPrivate {
	GdaMutex    *mutex;
	guint        next_job_id;
	GThread     *sub_thread;
	GAsyncQueue *to_sub_thread;

	GHashTable *threads_hash; /* key = a GThread, value = a #ThreadData pointer */
};

/*
 * Data shared between the GdaThreadWrapper (and the threads which use it) and its sub thread.
 * It implements its own locking mechanism.
 */
struct _SharedData {
        gint     nb_waiting;
	Job     *current_job;
	GThread *wrapper_sub_thread;

        gint     ref_count;
	GMutex  *mutex;
};

SharedData *
shared_data_new (void)
{
	SharedData *shd = g_new0 (SharedData, 1);
	shd->ref_count = 1;
	shd->nb_waiting = 0;
	shd->mutex = g_mutex_new ();
	return shd;
}

static SharedData *
shared_data_ref (SharedData *shd)
{
	g_mutex_lock (shd->mutex);
	shd->ref_count++;
	g_mutex_unlock (shd->mutex);
	return shd;
}
static void
shared_data_unref (SharedData *shd)
{
	g_mutex_lock (shd->mutex);
	shd->ref_count--;
	if (shd->ref_count == 0) {
		g_mutex_unlock (shd->mutex);
		g_mutex_free (shd->mutex);
		g_free (shd);
	}
	else
		g_mutex_unlock (shd->mutex);
}

static void
shared_data_add_nb_waiting (SharedData *shd, gint to_add)
{
	g_mutex_lock (shd->mutex);
	shd->nb_waiting += to_add;
	g_mutex_unlock (shd->mutex);
}


/* 
 * Jobs.
 * Created and destroyed exclusively by the thread(s) using the GdaThreadWrapper object,
 * except for the job where job->type == JOB_TYPE_DESTROY which is destroyed by the sub thread.
 *
 * Passed to the sub job through obj->to_sub_thread
 */
typedef enum {
	JOB_TYPE_EXECUTE,
	JOB_TYPE_DESTROY
} JobType;
struct _Job {
	JobType                  type;
	guint                    job_id;
	GdaThreadWrapperFunc     func;
	GdaThreadWrapperVoidFunc void_func;
	gpointer                 arg;
	GDestroyNotify           arg_destroy_func;
	GAsyncQueue             *reply_queue; /* holds a ref to it */
	SharedData              *shared; /* holds a ref to it */
};
static void
job_free (Job *job)
{
	if (job->arg && job->arg_destroy_func)
		job->arg_destroy_func (job->arg);
	if (job->reply_queue)
		g_async_queue_unref (job->reply_queue);
	if (job->shared)
		shared_data_unref (job->shared);
	g_free (job);
}

/*
 * Signal specification, created when using _connect().
 *
 * A SignalSpec only exists as long as the correcponding ThreadData exists.
 */
struct _SignalSpec {
        GSignalQuery  sigprop; /* must be first */
        gpointer      instance;
        gulong        signal_id;

        GdaThreadWrapperCallback callback;
        gpointer                 data;

	ThreadData      *td;
};

struct _SignalEmissionData {
        guint      n_param_values;
        GValue    *param_values; /* array of GValue structures */
};

static void
signal_emission_data_free (SignalEmissionData *sd)
{
	gint i;
	for (i = 0; i < sd->n_param_values; i++) {
		GValue *value = sd->param_values + i;
		if (G_VALUE_TYPE (value) != GDA_TYPE_NULL)
			g_value_reset (value);
	}
	g_free (sd->param_values);
	g_free (sd);
}

/*
 * Result of a job executed by the sub thread.
 * Created exclusively by the sub thread, and destroyed by the thread(s) using the GdaThreadWrapper object
 *
 * Passed from the sub thread through obj->from_sub_thread
 */
typedef enum {
	RESULT_TYPE_EXECUTE,
	RESULT_TYPE_SIGNAL
} ResultType;
struct _Result {
	Job                 *job;
	ResultType           type;
	union {
		struct {
			gpointer             result;
			GError              *error;
		} exe;
		struct {
			SignalSpec         *spec;
			SignalEmissionData *data;
		} signal;
	} u;
};
#define RESULT(x) ((Result*)(x))
static void
result_free (Result *res)
{
	if (res->job)
		job_free (res->job);
	if (res->type == RESULT_TYPE_EXECUTE) {
		if (res->u.exe.error)
			g_error_free (res->u.exe.error);
	}
	else if (res->type == RESULT_TYPE_SIGNAL) 
		signal_emission_data_free (res->u.signal.data);
	else
		g_assert_not_reached ();
	g_free (res);
}

/*
 * Per thread accounting data.
 * Each new job increases the ref count
 */
struct _ThreadData {
	GThread *owner;
	GSList *signals_list; /* list of SignalSpec pointers, owns all the structures */
	GAsyncQueue *from_sub_thread; /* holds a ref to it */
	GSList *results; /* list of Result pointers */
	SharedData *shared; /* number of jobs waiting from that thread, holds a ref to it */
};
#define THREAD_DATA(x) ((ThreadData*)(x))

static ThreadData *
get_thread_data (GdaThreadWrapper *wrapper)
{
	ThreadData *td;

	td = g_hash_table_lookup (wrapper->priv->threads_hash, g_thread_self());
	if (!td) {
		td = g_new0 (ThreadData, 1);
		td->owner = g_thread_self();
		td->from_sub_thread = g_async_queue_new ();
		td->results = NULL;
		td->shared = shared_data_new ();
		td->shared->wrapper_sub_thread = wrapper->priv->sub_thread;

		g_hash_table_insert (wrapper->priv->threads_hash, g_thread_self(), td);
	}
	return td;
}

static void
thread_data_free (ThreadData *td)
{
	g_async_queue_unref (td->from_sub_thread);
	if (td->results) {
		g_slist_foreach (td->results, (GFunc) result_free, NULL);
		g_slist_free (td->results);
	}
	if (td->signals_list) {
		GSList *list;
		for (list = td->signals_list; list; list = list->next) {
			/* free each SignalSpec */
			SignalSpec *sigspec = (SignalSpec*) list->data;
			g_signal_handler_disconnect (sigspec->instance, sigspec->signal_id);
			g_free (sigspec);
		}
		g_slist_free (td->signals_list);
	}
	shared_data_unref (td->shared);
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

enum {
	LAST_SIGNAL
};

static gint gda_thread_wrapper_signals[LAST_SIGNAL] = { };

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
 * takes a Job in (from the wrapper->priv->to_sub_thread queue) and creates a new Result which 
 * it pushed to Job->reply_queue
 */
static gpointer
sub_thread_entry_point (GAsyncQueue *to_sub_thread)
{
	GAsyncQueue *in;

	in = to_sub_thread;

	/* don't use @priv anymore */
	while (1) {
		Job *job;
		
		job = g_async_queue_pop (in);

		if (job->type == JOB_TYPE_DESTROY) {
			g_assert (! job->arg_destroy_func);
			job_free (job);
			/*g_print ("... exit sub thread for wrapper\n");*/
			/* exit sub thread */
			break;
		}
		else if (job->type == JOB_TYPE_EXECUTE) {
			Result *res = g_new0 (Result, 1);
			res->job = job;
			res->type = RESULT_TYPE_EXECUTE;
			job->shared->current_job = job;
			if (job->func)
				res->u.exe.result = job->func (job->arg, &(res->u.exe.error));
			else {
				res->u.exe.result = NULL;
				job->void_func (job->arg, &(res->u.exe.error));
			}
			job->shared->current_job = NULL;
			shared_data_add_nb_waiting (job->shared, -1);
			g_async_queue_push (job->reply_queue, res);
		}
		else
			g_assert_not_reached ();
	}

	g_async_queue_unref (in);

	return NULL;
}

static void
gda_thread_wrapper_init (GdaThreadWrapper *wrapper, GdaThreadWrapperClass *klass)
{
	g_return_if_fail (GDA_IS_THREAD_WRAPPER (wrapper));

	wrapper->priv = g_new0 (GdaThreadWrapperPrivate, 1);
	wrapper->priv->mutex = gda_mutex_new ();
	wrapper->priv->next_job_id = 1;

	wrapper->priv->threads_hash = g_hash_table_new_full (NULL, NULL, NULL, (GDestroyNotify) thread_data_free);

	wrapper->priv->to_sub_thread = g_async_queue_new ();
	wrapper->priv->sub_thread = g_thread_create ((GThreadFunc) sub_thread_entry_point,
						     g_async_queue_ref (wrapper->priv->to_sub_thread), /* inc. ref for sub thread usage */
						     FALSE, NULL);
	/*g_print ("... new wrapper %p, sub_thread=%p\n", wrapper, wrapper->priv->sub_thread);*/
}

static void
gda_thread_wrapper_dispose (GObject *object)
{
	GdaThreadWrapper *wrapper = (GdaThreadWrapper *) object;

	g_return_if_fail (GDA_IS_THREAD_WRAPPER (wrapper));

	if (wrapper->priv) {
		Job *job = g_new0 (Job, 1);
		job->type = JOB_TYPE_DESTROY;
		g_async_queue_push (wrapper->priv->to_sub_thread, job);
		/*g_print ("... pushed JOB_TYPE_DESTROY for wrapper %p\n", wrapper);*/

		g_async_queue_unref (wrapper->priv->to_sub_thread);
		wrapper->priv->sub_thread = NULL; /* side note: don't wait for sub thread to terminate */
		
		if (wrapper->priv->threads_hash)
			g_hash_table_destroy (wrapper->priv->threads_hash);

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
                        (GInstanceInitFunc) gda_thread_wrapper_init
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
			       const GValue *value,
			       GParamSpec *pspec)
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
			       GValue *value,
			       GParamSpec *pspec)
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
 * Returns: a new #GdaThreadWrapper object
 *
 * Since: 4.2
 */
GdaThreadWrapper *
gda_thread_wrapper_new (void)
{
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
			    gpointer arg, GDestroyNotify arg_destroy_func, GError **error)
{
	Job *job;
	guint id;
	ThreadData *td;
	
	g_return_val_if_fail (GDA_IS_THREAD_WRAPPER (wrapper), 0);
	g_return_val_if_fail (wrapper->priv, 0);
	g_return_val_if_fail (func, 0);

	gda_mutex_lock (wrapper->priv->mutex);

	td = get_thread_data (wrapper);
	shared_data_add_nb_waiting (td->shared, 1);

	job = g_new0 (Job, 1);
	job->type = JOB_TYPE_EXECUTE;
	job->job_id = wrapper->priv->next_job_id++;
	job->func = func;
	job->void_func = NULL;
	job->arg = arg;
	job->arg_destroy_func = arg_destroy_func;
	job->reply_queue = g_async_queue_ref (td->from_sub_thread);
	job->shared = shared_data_ref (td->shared);

	id = job->job_id;
	gda_mutex_unlock (wrapper->priv->mutex);

	g_async_queue_push (wrapper->priv->to_sub_thread, job);

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
				 gpointer arg, GDestroyNotify arg_destroy_func, GError **error)
{
	Job *job;
	guint id;
	ThreadData *td;

	g_return_val_if_fail (GDA_IS_THREAD_WRAPPER (wrapper), 0);
	g_return_val_if_fail (wrapper->priv, 0);
	g_return_val_if_fail (func, 0);

	gda_mutex_lock (wrapper->priv->mutex);

	td = get_thread_data (wrapper);
	shared_data_add_nb_waiting (td->shared, 1);

	job = g_new0 (Job, 1);
	job->type = JOB_TYPE_EXECUTE;
	job->job_id = wrapper->priv->next_job_id++;
	job->func = NULL;
	job->void_func = func;
	job->arg = arg;
	job->arg_destroy_func = arg_destroy_func;
	job->reply_queue = g_async_queue_ref (td->from_sub_thread);
	job->shared = shared_data_ref (td->shared);

	id = job->job_id;
	gda_mutex_unlock (wrapper->priv->mutex);

	g_async_queue_push (wrapper->priv->to_sub_thread, job);

	return id;
}

/**
 * gda_thread_wrapper_iterate
 * @wrapper: a #GdaThreadWrapper object
 * @may_block: whether the call may block
 *
 * This method gives @wrapper a chance to check if some functions to be executed have finished
 * <emphasis>in the calling thread</emphasis>. It handles one function's execution result, and
 * if @may_block is %TRUE, then it will block untill there is one (functions returning void are
 * ignored).
 *
 * Since: 4.2
 */
void
gda_thread_wrapper_iterate (GdaThreadWrapper *wrapper, gboolean may_block)
{
	ThreadData *td;
	Result *res;

	g_return_if_fail (GDA_IS_THREAD_WRAPPER (wrapper));
	g_return_if_fail (wrapper->priv);

	gda_mutex_lock (wrapper->priv->mutex);

	td = g_hash_table_lookup (wrapper->priv->threads_hash, g_thread_self());
	if (!td) {
		/* nothing to be done for this thread */
		gda_mutex_unlock (wrapper->priv->mutex);
		return;
	}

 again:
	if (may_block)
		res = g_async_queue_pop (td->from_sub_thread);
	else
		res = g_async_queue_try_pop (td->from_sub_thread);
	if (res) {
		gboolean do_again = FALSE;
		if (res->type == RESULT_TYPE_EXECUTE) {
			if (!res->job->func) {
				result_free (res); /* ignore as there is no result */
				do_again = TRUE;
			}
			else
				td->results = g_slist_append (td->results, res);
		}
		else if (res->type == RESULT_TYPE_SIGNAL) {
			/* run callback now */
			SignalSpec *spec = res->u.signal.spec;
			SignalEmissionData *sd = res->u.signal.data;

			spec->callback (wrapper, spec->instance, ((GSignalQuery*)spec)->signal_name,
					sd->n_param_values, sd->param_values, NULL,
					spec->data);
			result_free (res);
			do_again = TRUE;
		}
		else
			g_assert_not_reached ();

		if (do_again)
			goto again;
	}
	
	gda_mutex_unlock (wrapper->priv->mutex);
}

/**
 * gda_thread_wrapper_fetch_result
 * @wrapper: a #GdaThreadWrapper object
 * @may_lock: TRUE if this funct must lock the caller untill a result is available
 * @id: a place to store the job ID of the function which execution has finished
 * @error: a place to store errors, for errors which may have occurred during the execution, or %NULL
 *
 * Use this method to check if the execution of a function is finished. The function's execution must have
 * been requested using gda_thread_wrapper_execute().
 *
 * If @id is not %NULL, then it will contain the ID of the request if a function's execution has finished, or
 * %0 otherwise.
 *
 * Returns: the pointer returned by the execution, or %NULL if no result is available
 *
 * Since: 4.2
 */
gpointer
gda_thread_wrapper_fetch_result (GdaThreadWrapper *wrapper, gboolean may_lock, guint *out_id, GError **error)
{
	ThreadData *td;
	Result *res = NULL;
	gpointer retval = NULL;

	g_return_val_if_fail (GDA_IS_THREAD_WRAPPER (wrapper), NULL);
	g_return_val_if_fail (wrapper->priv, NULL);

	gda_mutex_lock (wrapper->priv->mutex);
	td = g_hash_table_lookup (wrapper->priv->threads_hash, g_thread_self());
	if (!td) {
		/* nothing to be done for this thread */
		gda_mutex_unlock (wrapper->priv->mutex);
		return NULL;
	}
	
	gda_thread_wrapper_iterate (wrapper, may_lock);
	if (td->results) {
		res = RESULT (td->results->data);
		td->results = g_slist_delete_link (td->results, td->results);
		if (!(td->results) &&
		    (td->shared->nb_waiting == 0) &&
		    (g_async_queue_length (td->from_sub_thread) == 0) &&
		    !td->signals_list) {
			/* remove this ThreadData */
			g_hash_table_remove (wrapper->priv->threads_hash, g_thread_self());
		}
	}

	if (res) {
		g_assert (res->type == RESULT_TYPE_EXECUTE);
		if (out_id)
			*out_id = res->job->job_id;
		if (res->u.exe.error) {
			g_propagate_error (error, res->u.exe.error);
			res->u.exe.error = NULL;
		}
		retval = res->u.exe.result;
		res->u.exe.result = NULL;
		result_free (res);
	}

	gda_mutex_unlock (wrapper->priv->mutex);
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
	gint size;
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
	size = td->shared->nb_waiting;
	gda_mutex_unlock (wrapper->priv->mutex);
	return size;
}

/* 
 * Executed in sub thread (or potentially in other threads, in which case will be ignored)
 * pushes data into the queue 
 */
static void
sub_thread_closure_marshal (GClosure *closure,
			    GValue *return_value,
			    guint n_param_values,
			    const GValue *param_values,
			    gpointer invocation_hint,
			    gpointer marshal_data)
{
	SignalSpec *sigspec = (SignalSpec *) closure->data;

	/* if the signal is not emitted from the sub thread then don't do anything */
	if (g_thread_self () !=  sigspec->td->shared->wrapper_sub_thread)
		return;

	/* if we are not currently doing a job, then don't do anything */
	if (!sigspec->td->shared->current_job)
		return;

	gint i;
	/*
	for (i = 1; i < n_param_values; i++) {
		g_print ("\t%d => %s\n", i, gda_value_stringify (param_values + i));
	}
	*/
	SignalEmissionData *sdata;
	sdata = g_new0 (SignalEmissionData, 1);
	sdata->n_param_values = n_param_values - 1;
	sdata->param_values = g_new0 (GValue, sdata->n_param_values);
	for (i = 1; i < n_param_values; i++) {
		const GValue *src;
		GValue *dest;

		src = param_values + i;
		dest = sdata->param_values + i - 1;

		if (G_VALUE_TYPE (src) != GDA_TYPE_NULL) {
			g_value_init (dest, G_VALUE_TYPE (src));
			g_value_copy (src, dest);
		}
	}

	Result *res = g_new0 (Result, 1);
	g_assert (sigspec->td->shared);
	res->job = NULL;
	res->type = RESULT_TYPE_SIGNAL;
	res->u.signal.spec = sigspec;
	res->u.signal.data = sdata;
	g_async_queue_push (sigspec->td->shared->current_job->reply_queue, res);
}

/**
 * gda_thread_wrapper_connect_raw
 * @wrapper: a #GdaThreadWrapper object
 * @instance: the instance to connect to
 * @sig_name: a string of the form "signal-name::detail"
 * @callback: a #GdaThreadWrapperCallback function
 * @data: data to pass to @callback's calls
 *
 * Connects a callbeck function to a signal for a particular object. The difference with g_signal_connect() and
 * similar functions are:
 * <itemizedlist>
 *  <listitem><para>the @callback argument is not a #GCallback function, so the callback signature is not
 *    dependant on the signal itself</para></listitem>
 *  <listitem><para>the signal handler must not have to return any value</para></listitem>
 *  <listitem><para>the @callback function will be called asynchronously, the caller may need to use 
 *    gda_thread_wrapper_iterate() to get the notification</para></listitem>
 *  <listitem><para>the @callback function will be called only if the signal has been emitted by @instance 
 *    while being used in @wrapper's private sub thread (ie. used when @wrapper is executing some functions
 *    specified by
 *    gda_thread_wrapper_execute() or gda_thread_wrapper_execute_void() have been used)</para></listitem>
 * </itemizedlist>
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
				GdaThreadWrapperCallback callback,
				gpointer data)
{
	guint sigid;
        SignalSpec *sigspec;
	ThreadData *td;

	g_return_val_if_fail (GDA_IS_THREAD_WRAPPER (wrapper), 0);
	g_return_val_if_fail (wrapper->priv, 0);

	gda_mutex_lock (wrapper->priv->mutex);
	
	td = get_thread_data (wrapper);

        sigid = g_signal_lookup (sig_name, /* FIXME: use g_signal_parse_name () */
				 G_TYPE_FROM_INSTANCE (instance)); 
        if (sigid == 0) {
                g_warning (_("Signal does not exist\n"));
                return 0;
        }

        sigspec = g_new0 (SignalSpec, 1);
        g_signal_query (sigid, (GSignalQuery*) sigspec);

	if (((GSignalQuery*) sigspec)->return_type != G_TYPE_NONE) {
		g_warning (_("Signal to connect to must not have a return value\n"));
		g_free (sigspec);
		return 0;
	}
        sigspec->instance = instance;
        sigspec->callback = callback;
        sigspec->data = data;

	GClosure *cl;
	cl = g_closure_new_simple (sizeof (GClosure), sigspec);
	g_closure_set_marshal (cl, (GClosureMarshal) sub_thread_closure_marshal);
	sigspec->signal_id = g_signal_connect_closure (instance, sig_name, cl, FALSE);

	sigspec->td = td;
	td->signals_list = g_slist_append (td->signals_list, sigspec);

	gda_mutex_unlock (wrapper->priv->mutex);

	return sigspec->signal_id;
}

/**
 * gda_thread_wrapper_disconnect
 * @wrapper: a #GdaThreadWrapper object
 * @id: a handler ID, as returned by gda_thread_wrapper_connect_raw()
 *
 * Disconnects the emission of a signal, does the opposite of gda_thread_wrapper_connect_raw()
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
		g_warning (_("Signal does not exist\n"));
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

	td->signals_list = g_slist_remove (td->signals_list, sigspec);
	g_signal_handler_disconnect (sigspec->instance, sigspec->signal_id);
	g_free (sigspec);

	if (!(td->results) &&
	    (td->shared->nb_waiting == 0) &&
	    (g_async_queue_length (td->from_sub_thread) == 0) &&
	    !td->signals_list) {
		/* remove this ThreadData */
		g_hash_table_remove (wrapper->priv->threads_hash, g_thread_self());
	}

	gda_mutex_unlock (wrapper->priv->mutex);
}
