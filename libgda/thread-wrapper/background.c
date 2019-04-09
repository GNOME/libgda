/*
 * Copyright (C) 2014 Vivien Malerba <malerba@gnome-db.org>
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

/*
 * The "maintenance", or "background" thread is responsible to:
 *   - cache GdaWorker objects ready to be reused
 *   - join worker threads from GdaWorker
 *
 *   - trim spare ITSignaler ?
 */

#include "gda-worker.h"
#include "background.h"

#define SPARE_WORKER_DELAY_MS 2000
#define SPARE_ITS_DELAY_MS 2000
#define MAKE_STATS
#undef MAKE_STATS

#define DEBUG_BG
#undef DEBUG_BG

#ifdef MAKE_STATS
  #ifndef G_OS_WIN32
    #include <sys/types.h>
    #include <unistd.h>
  #endif
#endif

/*
 * global data
 */
GMutex bg_mutex; /* protects:
		  *   - the "background" thread creation, AND
		  *   - the @spare_workers array
		  *   - the @spare_its array */


/*
 * Job type: JOB_JOIN_THREAD
 */
typedef struct {
	GThread   *thread;
} JoinThreadData;

static void
JoinThreadData_free (JoinThreadData *data)
{
	g_slice_free (JoinThreadData, data);
}

/*
 * Job type: JOB_SPARE_WORKER
 */
typedef struct {
	GdaWorker *worker;
	guint      ms;
} WorkerSpareData;
static GArray *spare_workers = NULL; /* array of WorkerSpareData pointers (ref held), only used from within the "background" thread.
				     * the last entries have a higher value of @ms (because new elements are appended) */
static void
WorkerSpareData_free (WorkerSpareData *data)
{
	if (data->worker) {
#ifdef DEBUG_BG
		g_print ("[_gda_worker_bg_unref(%p)]\n", data->worker);
#endif
		_gda_worker_bg_unref (data->worker);
	}
	g_slice_free (WorkerSpareData, data);
}

/*
 * Job type: JOB_SPARE_ITS
 */
typedef struct {
	ITSignaler *its;
	guint       ms;
} ItsSpareData;
static GArray *spare_its = NULL; /* array of ItsSpareData pointers (ref held), only used from within the "background" thread.
				  * the last entries have a higher value of @ms (because new elements are appended) */
static void
ItsSpareData_free (ItsSpareData *data)
{
	if (data->its) {
#ifdef DEBUG_BG
		g_print ("[_its_bg_unref(%p)]\n", data->its);
#endif
		_itsignaler_bg_unref (data->its);
	}
	g_slice_free (ItsSpareData, data);
}

/*
 * Job transmission through a GAsyncQueue
 */
static GAsyncQueue *bgqueue = NULL; /* vector to pass jobs to "background" thread */
typedef enum {
	JOB_JOIN_THREAD,
	JOB_SPARE_WORKER,
	JOB_SPARE_ITS
} JobType;

typedef struct {
	JobType  type;
	union {
		JoinThreadData  *u1;
		WorkerSpareData *u2;
		ItsSpareData    *u3;
	} u;
} Job;

/*
 * Utility
 */
static guint
compute_wait_delay (void)
{
	guint delay = 0;

	g_mutex_lock (&bg_mutex);
	/*
	guint i;
	for (i = 0; i < spare_workers->len; i++) {
		WorkerSpareData *data;
		data = g_array_index (spare_workers, WorkerSpareData*, i);
		if (i == 0)
		    delay = data->ms;
		else
		    delay = MIN (delay, data->ms);
	}
	*/
	if (spare_workers->len > 0) { /* we use here the fact that @spare_workers is ordered */
		WorkerSpareData *data;
		data = g_array_index (spare_workers, WorkerSpareData*, 0);
		delay = data->ms;
	}
	if (spare_its->len > 0) { /* we use here the fact that @spare_its is ordered */
		ItsSpareData *data;
		data = g_array_index (spare_its, ItsSpareData*, 0);
		if (delay == 0)
			delay = data->ms;
		else
			delay = MIN (data->ms, delay);
	}
	g_mutex_unlock (&bg_mutex);

	return delay;
}

/*
 * MAIN part of the "background" thread
 */
static gpointer
background_main (G_GNUC_UNUSED gpointer data)
{
	GTimer *timer;
	guint elapsed_ms = 0;
	timer = g_timer_new ();
	g_timer_stop (timer);

	while (1) {
		/* honor delayed operations */
		g_mutex_lock (&bg_mutex);
		guint i;
		for (i = 0; i < spare_workers->len; ) {
			WorkerSpareData *data;
			data = g_array_index (spare_workers, WorkerSpareData*, i);

			if (data->ms <= elapsed_ms) {
				g_array_remove_index (spare_workers, 0);
				WorkerSpareData_free (data);
			}
			else {
				data->ms -= elapsed_ms;
				i++;
			}
		}
		for (i = 0; i < spare_its->len; ) {
			ItsSpareData *data;
			data = g_array_index (spare_its, ItsSpareData*, i);

			if (data->ms <= elapsed_ms) {
				g_array_remove_index (spare_its, 0);
				ItsSpareData_free (data);
			}
			else {
				data->ms -= elapsed_ms;
				i++;
			}
		}
		g_mutex_unlock (&bg_mutex);

		/* compute maximum time to wait */
		guint next_delay_ms;
		next_delay_ms = compute_wait_delay ();

		/* fetch new job submissions from the queue */
		Job *job;
		g_timer_start (timer);
		if (next_delay_ms == 0)
			job = g_async_queue_pop (bgqueue);
		else
			job = g_async_queue_timeout_pop (bgqueue, next_delay_ms * 1000);
		g_timer_stop (timer);
		elapsed_ms = (guint) (g_timer_elapsed (timer, NULL) * 1000.);

		if (job) {
			switch (job->type) {
			case JOB_SPARE_WORKER:
				g_mutex_lock (&bg_mutex);
				g_array_append_val (spare_workers, job->u.u2);
#ifdef DEBUG_BG
				g_print ("[Cached GdaWorker %p]\n", job->u.u2->worker);
#endif
				g_mutex_unlock (&bg_mutex);
				break;
			case JOB_SPARE_ITS:
				g_mutex_lock (&bg_mutex);
				g_array_append_val (spare_its, job->u.u3);
#ifdef DEBUG_BG
				g_print ("[Cached ITS %p]\n", job->u.u3->its);
#endif
				g_mutex_unlock (&bg_mutex);
				break;
			case JOB_JOIN_THREAD: {
				JoinThreadData *data;
				data = job->u.u1;
#ifdef DEBUG_BG
				g_print ("[g_thread_join(%p)]\n", data->thread);
#endif

				bg_update_stats (BG_JOINED_THREADS);

				g_thread_join (data->thread);
				JoinThreadData_free (data);
				break;
			}
			default:
				g_assert_not_reached ();
			}

			g_slice_free (Job, job); /* free the Job "shell" */
		}
	}
	return NULL;
}


/**
 * _bg_start:
 *
 * Have the "background" thread start. May be called several times.
 */
static void
_bg_start (void)
{
	static gboolean th_started = FALSE;
	g_mutex_lock (&bg_mutex);

	if (!bgqueue)
		bgqueue = g_async_queue_new ();

	if (!spare_its)
		spare_its = g_array_new (FALSE, FALSE, sizeof (ItsSpareData*));

	if (!spare_workers)
		spare_workers = g_array_new (FALSE, FALSE, sizeof (WorkerSpareData*));

	if (!th_started) {
		GThread *th;
		th = g_thread_new ("gdaBackground", background_main, NULL);
		if (th) {
			th_started = TRUE;
		}
		else
			g_warning ("Failed to start Gda's background thread\n");
	}

	g_mutex_unlock (&bg_mutex);
}

/**
 * bg_join_thread:
 *
 * This function must be called by a thread right before it exits, it tells the "background" thread that
 * it can call g_thread_join() without having the risk of blocking. It is called by worker threads right before
 * they exit.
 */
void
bg_join_thread ()
{
	_bg_start ();

	Job *job;
	job = g_slice_new (Job);
	job->type = JOB_JOIN_THREAD;
	job->u.u1 = g_slice_new (JoinThreadData);
	job->u.u1->thread = g_thread_self ();

	g_async_queue_push (bgqueue, job);
}

/**
 * bg_set_spare_gda_worker:
 * @worker: a #GdaWorker
 *
 * This function requests that the "background" handle the caching or the destruction of @worker. It is intended to be called
 * only from within the gda_worker_unref() method when the reference count is 0, but right before destroying it.
 *
 * The caller (the GdaWorker's code) must first set the reference count to 1 (and not destroy the object).
 */
void
bg_set_spare_gda_worker (GdaWorker *worker)
{
	g_return_if_fail (worker);
	_bg_start ();

	Job *job;
	job = g_slice_new (Job);
	job->type = JOB_SPARE_WORKER;
	job->u.u2 = g_slice_new (WorkerSpareData);
	job->u.u2->worker = worker;
	job->u.u2->ms = SPARE_WORKER_DELAY_MS; /* fixed delay => array is ordered, see compute_wait_delay() */

	bg_update_stats (BG_CACHED_WORKER_REQUESTS);

	g_async_queue_push (bgqueue, job);
}

/**
 * bg_get_spare_gda_worker:
 *
 * Requests the "background" thread to provide a #GdaWorker which it has kept as a cache (see bg_set_spare_gda_worker()).
 * The return value may be %NULL (if no #GdaWorker object is available), or a pointer to a #GdaWorker which refcount is 1
 * and which has a worker thread available immediately (no job pending or in process).
 *
 * Returns: (transfer full): a #GdaWorker, or %NULL
 */
GdaWorker *
bg_get_spare_gda_worker (void)
{
	GdaWorker *worker = NULL;
	_bg_start ();

	g_mutex_lock (&bg_mutex);
	if (spare_workers->len > 0) {
		WorkerSpareData *data;
		data = g_array_index (spare_workers, WorkerSpareData*, 0);

		worker = data->worker;
		data->worker = NULL;
		WorkerSpareData_free (data);

		g_array_remove_index (spare_workers, 0);
#ifdef DEBUG_BG
		g_print ("[Fetched cached GdaWorker %p]\n", worker);
#endif

		bg_update_stats (BG_REUSED_WORKER_FROM_CACHE);
	}
	g_mutex_unlock (&bg_mutex);

	return worker;
}

/*
 * ITSignaler caching
 */
/**
 * bg_set_spare_its:
 * @its: a #ITSignaler
 *
 * This function requests that the "background" handle the caching or the destruction of @its. It is intended to be called
 * only from within the itsignaler_unref() method when the reference count is 0, but right before destroying it.
 *
 * The caller (the ITSIgnaler's code) must first set the reference count to 1 (and not destroy the object).
 */
void
bg_set_spare_its (ITSignaler *its)
{
	g_return_if_fail (its);
	_bg_start ();

	Job *job;
	job = g_slice_new (Job);
	job->type = JOB_SPARE_ITS;
	job->u.u3 = g_slice_new (ItsSpareData);
	job->u.u3->its = its;
	job->u.u3->ms = SPARE_ITS_DELAY_MS; /* fixed delay => array is ordered, see compute_wait_delay() */

	bg_update_stats (BG_CACHED_ITS_REQUESTS);

	g_async_queue_push (bgqueue, job);
}

/**
 * bg_get_spare_its:
 *
 * Requests the "background" thread to provide a #ITSignaler which it has kept as a cache (see bg_set_spare_its()).
 * The return value may be %NULL (if no #ITSignaler object is available), or a pointer to a #ITSignaler which refcount is 1.
 *
 * Returns: (transfer full): a #ITSignaler, or %NULL
 */
ITSignaler *
bg_get_spare_its (void)
{
	ITSignaler *its = NULL;
	_bg_start ();

	g_mutex_lock (&bg_mutex);
	if (spare_its->len > 0) {
		ItsSpareData *data;
		data = g_array_index (spare_its, ItsSpareData*, 0);

		its = data->its;
		data->its = NULL;
		ItsSpareData_free (data);

		g_array_remove_index (spare_its, 0);
#ifdef DEBUG_BG
		g_print ("[Fetched cached ITS %p]\n", its);
#endif

		bg_update_stats (BG_REUSED_ITS_FROM_CACHE);
	}
	g_mutex_unlock (&bg_mutex);

	return its;
}


/*
 * Statistics
 */
#ifdef MAKE_STATS
guint stats [BG_STATS_MAX];

static void
write_stats (void)
{
	gchar *strings [] = {
		"GdaWorker created",
		"GdaWorker destroyed",
		"GdaWorker cache requests",
		"GdaWorker reused from cache",

		"Started threads",
		"Joined threads",

		"ITS created",
		"ITS destroyed",
		"ITS cache requests",
		"ITS reused from cache"
	};
	BackgroundStats i;
	GString *string;
	string = g_string_new ("=== stats start ===\n");
	for (i = BG_STATS_MIN; i < BG_STATS_MAX; i++)
		g_string_append_printf (string, "%s: %u\n", strings[i], stats [i]);
	g_string_append (string, "=== stats end ===\n");

	gchar *fname;
#ifndef G_OS_WIN32
	fname = g_strdup_printf ("gda_stats_%u", getpid ());
#else
	fname = g_strdup_printf ("gda_stats");
#endif
	g_file_set_contents (fname, string->str, -1, NULL);
	g_free (fname);
	g_string_free (string, TRUE);
}

/**
 * bg_update_stats:
 */
void
bg_update_stats (BackgroundStats type)
{
	g_assert ((type >= BG_STATS_MIN) && (type < BG_STATS_MAX));
	stats [type] ++;
	if ((type == BG_CREATED_WORKER) || (type == BG_DESTROYED_WORKER) ||
	    (type == BG_STARTED_THREADS) || (type == BG_JOINED_THREADS) ||
	    (type == BG_CREATED_ITS) || (type == BG_DESTROYED_ITS))
		write_stats ();
}
#else
void
bg_update_stats (G_GNUC_UNUSED BackgroundStats type)
{
}
#endif
