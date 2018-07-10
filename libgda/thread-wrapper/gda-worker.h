/*
 * Copyright (C) 2014 Murray Cumming <murrayc@murrayc.com>
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

#ifndef __GDA_WORKER_H__
#define __GDA_WORKER_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _GdaWorker GdaWorker;

/**
 * SECTION:gda-worker
 * @short_description: Execute functions in a sub thread
 * @title: GdaWorker
 * @stability: Stable
 * @see_also:
 *
 * The purpose of the #GdaWorker object is to execute "jobs" (functions with arguments) within a worker thread, using
 * gda_worker_submit_job().
 *
 * Any code executing in any thread context can submit jobs to @worker and is guaranted not to be blocked (except if using
 * gda_worker_wait_job() or if the
 * jobs are submitted within the worker thread itself). Once jobs have been submitted, it's up to the caller to regularly
 * check if a job has completed using gda_worker_fetch_job_result(). If you don't want to have to check regularly (which is
 * like some polling operation), then you can use gda_worker_set_callback() which adds a callback when any job has completed.
 *
 * gda_worker_wait_job() allows you to execute a job in the worker thread while blocking the calling thread.
 *
 * Before fetching a jobs's result, it is also possible to request the cancellation of the job using gda_worker_cancel_job(), or
 * completely discard the job using gda_worker_forget_job().
 *
 * Jobs can also be submitted using gda_worker_do_job(), which internally runs a #GMainLoop and allows you to execute a job
 * while at the same time processing events for the specified #GMainContext.
 *
 * The #GdaWorker implements its own locking mechanism and can safely be used from multiple
 * threads at once without needing further locking.
 */

/* error reporting */
extern GQuark gda_worker_error_quark (void);
#define GDA_WORKER_ERROR gda_worker_error_quark ()

typedef enum {
	GDA_WORKER_INTER_THREAD_ERROR,

	GDA_WORKER_JOB_NOT_FOUND_ERROR,
	GDA_WORKER_JOB_QUEUED_ERROR,
	GDA_WORKER_JOB_BEING_PROCESSED_ERROR,
	GDA_WORKER_JOB_PROCESSED_ERROR,
	GDA_WORKER_JOB_CANCELLED_ERROR,
	GDA_WORKER_THREAD_KILLED
} GdaWorkerError;

#define GDA_TYPE_WORKER gda_worker_get_type()
GType gda_worker_get_type(void) G_GNUC_CONST;
GdaWorker *gda_worker_new (void);
GdaWorker *gda_worker_new_unique (GdaWorker **location, gboolean allow_destroy);
GdaWorker *gda_worker_ref (GdaWorker *worker);
void       gda_worker_unref (GdaWorker *worker);

/**
 * GdaWorkerFunc:
 * @user_data: pointer to the data (which is the argument passed to gda_worker_submit_job())
 * @error: a place to store errors
 * @Returns: a pointer to some data which will be returned by gda_worker_fetch_job_result()
 *
 * Specifies the type of function to be passed to gda_worker_submit_job(), gda_worker_do_job() and gda_worker_wait_job().
 */
typedef gpointer (*GdaWorkerFunc) (gpointer user_data, GError **error);
guint      gda_worker_submit_job (GdaWorker *worker, GMainContext *callback_context,
				  GdaWorkerFunc func, gpointer data, GDestroyNotify data_destroy_func,
				  GDestroyNotify result_destroy_func, GError **error);

gboolean   gda_worker_fetch_job_result (GdaWorker *worker, guint job_id, gpointer *out_result, GError **error);
gboolean   gda_worker_cancel_job (GdaWorker *worker, guint job_id, GError **error);
void       gda_worker_forget_job (GdaWorker *worker, guint job_id);

gboolean   gda_worker_do_job (GdaWorker *worker, GMainContext *context, gint timeout_ms,
			      gpointer *out_result, guint *out_job_id,
			      GdaWorkerFunc func, gpointer data, GDestroyNotify data_destroy_func,
			      GDestroyNotify result_destroy_func,
			      GError **error);
gpointer   gda_worker_wait_job (GdaWorker *worker,
				GdaWorkerFunc func, gpointer data, GDestroyNotify data_destroy_func,
				GError **error);


typedef void (*GdaWorkerCallback) (GdaWorker *worker, guint job_id, gpointer result_data, GError *error, gpointer user_data);
gboolean   gda_worker_set_callback (GdaWorker *worker, GMainContext *context, GdaWorkerCallback callback,
				    gpointer user_data, GError **error);

gboolean   gda_worker_thread_is_worker (GdaWorker *worker);
GThread   *gda_worker_get_worker_thread (GdaWorker *worker);

/*
 * Private
 */
void       _gda_worker_bg_unref (GdaWorker *worker);

G_END_DECLS

#endif
