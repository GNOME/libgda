/*
 * Copyright (C) 2014 Vivien Malerba <malerba@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <glib.h>
#include <string.h>
#include "gda-worker.h"
#include <stdlib.h>

int test1 (void);
int test3 (void);
int test4 (void);
int test5 (void);
int test6 (void);
int test7 (void);
int test8 (void);
int test9 (void);
int test10 (void);
int test11 (void);
int test12 (void);
int test13 (void);


int
main ()
{
	gint nfailed = 0;

	nfailed += test1 ();
	nfailed += test3 ();
	nfailed += test4 ();
	nfailed += test5 ();
	nfailed += test6 ();
	nfailed += test7 ();
	nfailed += test8 ();
	nfailed += test9 ();
	nfailed += test10 ();
	nfailed += test11 ();
	nfailed += test12 ();
	nfailed += test13 ();

	g_print ("Test %s\n", nfailed > 0 ? "Failed" : "Ok");
	return nfailed > 0 ? 1 : 0;
}

/*
 * Test 1: so basic
 */
int
test1 (void)
{
	g_print ("Test1 started\n");
	GdaWorker *worker;

	worker = gda_worker_new ();
	gda_worker_unref (worker);
	return 0;
}

/*
 * Test 3: fetching results
 */
typedef struct {
	guint counter;
} Data3;

static void
data3_free (Data3 *data)
{
	g_print ("%s() from thread %p\n", __FUNCTION__, g_thread_self());
	g_free (data);
}

static gpointer
test3_worker_func (Data3 *data, G_GNUC_UNUSED GError **error)
{
	gint *retval;
	g_print ("%s() called from thread %p\n", __FUNCTION__, g_thread_self());
	g_usleep (500000); /* short delay */
	retval = g_new0 (gint, 1);
	*retval = data->counter + 1;
	return retval;
}

int
test3 (void)
{
	g_print ("Test3 started, main thread is %p\n", g_thread_self ());
	GdaWorker *worker;
	gint nfailed = 0;

	worker = gda_worker_new ();
	guint jid;
	GError *error = NULL;
	Data3 *data;
	data = g_new (Data3, 1);
	data->counter = 5;
	jid = gda_worker_submit_job (worker, NULL,
				     (GdaWorkerFunc) test3_worker_func, data, (GDestroyNotify) data3_free, NULL, &error);
	if (jid == 0) {
		g_print ("Error in gda_worker_submit_job(): %s\n", error && error->message ? error->message : "no detail");
		g_clear_error (&error);
		nfailed++;
		goto out;
	}

	while (1) {
		gint *result;
		if (! gda_worker_fetch_job_result (worker, jid, (gpointer*) &result, &error)) {
			g_print ("Still not done, error: %s\n", error && error->message ? error->message : "no detail");
			if ((error->domain == GDA_WORKER_ERROR) && (error->code == GDA_WORKER_JOB_NOT_FOUND_ERROR)) {
				nfailed++;
				g_clear_error (&error);
				break;
			}
			g_clear_error (&error);
		}
		else {
			if (result) {
				g_print ("Got result value: %d\n", *result);
				g_free (result);
			}
			else {
				g_print ("Error: got no result value!\n");
				nfailed++;
			}
			break;
		}
		g_usleep (100000);
	}

 out:
	gda_worker_unref (worker);

	return nfailed;
}

/*
 * Test 4: cancellation tests
 */
static gpointer
test4_worker_func (G_GNUC_UNUSED gpointer data, G_GNUC_UNUSED GError **error)
{
	/* just spend some idle time */
	g_print ("%s() called from thread %p\n", __FUNCTION__, g_thread_self());
	g_usleep (500000); /* short delay */
	return NULL;
}

int
test4 (void)
{
	g_print ("Test4 started, main thread is %p\n", g_thread_self ());
	GdaWorker *worker;
	gint nfailed = 0;

	worker = gda_worker_new ();
	guint jid1, jid2;
	GError *error = NULL;
	jid1 = gda_worker_submit_job (worker, NULL, (GdaWorkerFunc) test4_worker_func, NULL, NULL, NULL, &error);
	if (jid1 == 0) {
		g_print ("Error in gda_worker_submit_job(): %s\n", error && error->message ? error->message : "no detail");
		g_clear_error (&error);
		nfailed++;
		goto out;
	}
	jid2 = gda_worker_submit_job (worker, NULL, (GdaWorkerFunc) test4_worker_func, NULL, NULL, NULL, &error);
	if (jid2 == 0) {
		g_print ("Error in gda_worker_submit_job(): %s\n", error && error->message ? error->message : "no detail");
		g_clear_error (&error);
		nfailed++;
		goto out;
	}

	if (! gda_worker_cancel_job (worker, jid2, &error)) {
		g_print ("Error in gda_worker_cancel_job(): %s\n", error && error->message ? error->message : "no detail");
		g_clear_error (&error);
		nfailed++;
		goto out;
	}
	if (! gda_worker_cancel_job (worker, jid2, &error)) {
		g_print ("Error in gda_worker_cancel_job(): %s\n", error && error->message ? error->message : "no detail");
		g_clear_error (&error);
		nfailed++;
		goto out;
	}
	if (gda_worker_cancel_job (worker, 10, NULL)) {
		g_print ("Error in gda_worker_cancel_job(): should have failed!\n");
		nfailed++;
		goto out;
	}

	while (1) {
		gint *result;
		if (! gda_worker_fetch_job_result (worker, jid1, (gpointer*) &result, &error)) {
			g_print ("Still not done, error: %s\n", error && error->message ? error->message : "no detail");
			if ((error->domain == GDA_WORKER_ERROR) && (error->code == GDA_WORKER_JOB_NOT_FOUND_ERROR)) {
				nfailed++;
				g_clear_error (&error);
				break;
			}
			g_clear_error (&error);
		}
		else {
			if (result) {
				g_print ("Error: got result value when expected none!\n");
				nfailed++;
			}
			break;
		}
		g_usleep (100000);
	}
 out:
	gda_worker_unref (worker);

	return nfailed;
}

/*
 * Test 5: with main loop
 */
static gpointer
test5_worker_func (G_GNUC_UNUSED gpointer data, G_GNUC_UNUSED GError **error)
{
	/* just spend some idle time */
	g_print ("%s() called from thread %p\n", __FUNCTION__, g_thread_self());
	g_usleep (100000); /* short delay */
	return NULL;
}

static gboolean
test5_idle_push_jobs (GdaWorker *worker)
{
	static guint i = 0;
	i++;
	guint jid;
	jid = gda_worker_submit_job (worker, NULL, (GdaWorkerFunc) test5_worker_func, NULL, NULL, NULL, NULL);
	if (jid == 0) {
		g_print ("gda_worker_submit_job() Failed!\n");
		return FALSE;
	}
	else
		g_print ("Pushed job %u\n", i);

	return i == 5 ? FALSE : TRUE;
}

static void
test5_worker_callback (G_GNUC_UNUSED GdaWorker *worker, guint job_id, G_GNUC_UNUSED gpointer result_data, G_GNUC_UNUSED GError *error, GMainLoop *loop)
{
	static guint i = 0;
	i++;
	g_print ("test5_worker_callback called for job %u\n", job_id);
	if (i == 5) {
		g_print ("Requesting loop quit, from thread %p\n", g_thread_self ());
		g_main_loop_quit (loop);
	}
}

int
test5 (void)
{
	g_print ("Test5 started, main thread is %p\n", g_thread_self ());
	GdaWorker *worker;
	gint nfailed = 0;

	worker = gda_worker_new ();
	GError *error = NULL;

	GMainLoop *loop;
        loop = g_main_loop_new (NULL, FALSE);
	if (! gda_worker_set_callback (worker, NULL, (GdaWorkerCallback) test5_worker_callback, loop, &error)) {
		g_print ("gda_worker_set_callback() error: %s\n", error && error->message ? error->message : "no detail");
		nfailed++;
	}
	else {
		g_assert (g_idle_add ((GSourceFunc) test5_idle_push_jobs, worker) != 0);
	}
	g_main_loop_run (loop);
	g_main_loop_unref (loop);
	gda_worker_unref (worker);
	g_print ("Test 5 done\n");

	return nfailed;
}

/*
 * Test 6: gda_worker_do_job()
 * - A ticker is run to make sure the events are handled while in gda_worker_do_job()
 * - a simple job is run
 * - job finishes before timer
 */
static gpointer
test6_worker_func (gint *timeout_ms, G_GNUC_UNUSED GError **error)
{
	if (*timeout_ms > 0)
		g_usleep ((gulong) *timeout_ms * 1000);
	return g_strdup ("Test6Done");
}

static void
test6_string_free (gchar *str)
{
	g_print ("%s (%s)\n", __FUNCTION__, str);
	g_free (str);
}

static gboolean
test6_timer_cb (guint *nticks)
{
	(*nticks) ++;
	return TRUE; /* keep timer */
}

int
test6 (void)
{
	g_print ("%s started, main thread is %p\n", __FUNCTION__, g_thread_self ());
	GdaWorker *worker;
	gint nfailed = 0;

	worker = gda_worker_new ();
	GError *error = NULL;

	guint nticks = 0;
	guint timer;
	timer = g_timeout_add (10, (GSourceFunc) test6_timer_cb, &nticks);

	guint jid;
	gpointer out;
	gint wait;
	wait = 50;
	if (gda_worker_do_job (worker, NULL, 100, &out, &jid,
			       (GdaWorkerFunc) test6_worker_func, (gpointer) &wait, NULL,
			       (GDestroyNotify) test6_string_free, &error)) {
		if (!out || strcmp (out, "Test6Done")) {
			g_print ("Expected out to be [Test6Done] and got [%s]\n", (gchar*) out);
			nfailed++;
		}
		g_free (out);
		if (jid != 0) {
			g_print ("Expected JID to be 0 and got %u\n", jid);
			nfailed++;
		}
	}
	else {
		g_print ("gda_worker_do_job() failed: %s\n", error && error->message ? error->message : "No detail");
		g_clear_error (&error);
		nfailed ++;
	}
	g_source_remove (timer);

	if (nticks < 3) {
		g_print ("Tick timer was not called while in gda_worker_do_job()\n");
		nfailed++;
	}
	g_print ("Unref worker...\n");

	gda_worker_unref (worker);
	g_print ("%s done\n", __FUNCTION__);

	return nfailed;
}

/*
 * Test 7: gda_worker_do_job()
 * - A ticker is run to make sure the events are handled while in gda_worker_do_job()
 * - a simple job is run
 * - job finishes after timer
 */
int
test7 (void)
{
	g_print ("%s started, main thread is %p\n", __FUNCTION__, g_thread_self ());
	GdaWorker *worker;
	gint nfailed = 0;

	worker = gda_worker_new ();
	GError *error = NULL;

	guint nticks = 0;
	guint timer;
	timer = g_timeout_add (10, (GSourceFunc) test6_timer_cb, &nticks);

	guint jid;
	gpointer out;
	gint wait;
	wait = 150;
	if (gda_worker_do_job (worker, NULL, 100, &out, &jid,
			       (GdaWorkerFunc) test6_worker_func, (gpointer) &wait, NULL,
			       (GDestroyNotify) test6_string_free, &error)) {
		g_print ("out: [%s], JID: %u\n", (gchar*) out, jid);
		if (out) {
			g_print ("Expected out to be NULL and got [%s]\n", (gchar*) out);
			nfailed++;
			g_free (out);
		}
		if (jid == 0) {
			g_print ("Expected JID to be > 0 and got %u\n", jid);
			nfailed++;
		}

		while (1) {
			g_print ("Waiting for result using gda_worker_fetch_job_result()\n");
			if (gda_worker_fetch_job_result (worker, jid, &out, &error)) {
				if (!out || strcmp (out, "Test6Done")) {
					g_print ("Expected out to be [Test6Done] and got [%s]\n", (gchar*) out);
					nfailed++;
				}
				g_free (out);
				break;
			}
			else
				g_clear_error (&error);
			g_usleep (100000);
		}
	}
	else {
		g_print ("gda_worker_do_job() failed: %s\n", error && error->message ? error->message : "No detail");
		g_clear_error (&error);
		nfailed ++;
	}
	g_source_remove (timer);

	if (nticks < 3) {
		g_print ("Tick timer was not called while in gda_worker_do_job()\n");
		nfailed++;
	}
	g_print ("Unref worker...\n");

	gda_worker_unref (worker);
	g_print ("%s done\n", __FUNCTION__);

	return nfailed;
}

/*
 * Test 8: submit a job from within the worker thread
 */
static gpointer
test8_sub_func (G_GNUC_UNUSED gpointer unused_data, G_GNUC_UNUSED GError **error)
{
	g_print ("%s() called in thread %p\n", __FUNCTION__, g_thread_self());
	return "test8_sub_func";
}

static gpointer
test8_func (GdaWorker *worker, G_GNUC_UNUSED GError **error)
{
	guint jid;
	jid = gda_worker_submit_job (worker, NULL, (GdaWorkerFunc) test8_sub_func, NULL, NULL, NULL, error);
	if (jid == 0)
		return NULL;
	else {
		g_print ("%s() submitted job %u with thread %p\n", __FUNCTION__, jid, g_thread_self ());
		while (1) {
			gpointer out;
			GError *lerror = NULL;
			g_print ("Waiting for result...\n");
			if (gda_worker_fetch_job_result (worker, jid, &out, &lerror)) {
				if (lerror)
					g_propagate_error (error, lerror);
				return out;
			}
			else
				g_clear_error (&lerror);
			g_usleep (100000);
		}
	}
}

int
test8 (void)
{
	g_print ("%s started, main thread is %p\n", __FUNCTION__, g_thread_self ());
	GdaWorker *worker;
	gint nfailed = 0;

	worker = gda_worker_new ();
	GError *error = NULL;

	guint jid;
	jid = gda_worker_submit_job (worker, NULL, (GdaWorkerFunc) test8_func, worker, NULL, NULL, &error);
	if (jid == 0) {
		g_print ("gda_worker_submit_job() failed: %s\n", error && error->message ? error->message : "No detail");
		g_clear_error (&error);
		nfailed ++;
	}
	else {
		while (1) {
			gpointer out;
			g_print ("Waiting for result using gda_worker_fetch_job_result()\n");
			if (gda_worker_fetch_job_result (worker, jid, &out, &error)) {
				if (!out || strcmp (out, "test8_sub_func")) {
					g_print ("Expected out to be [test8_sub_func] and got %s\n", (gchar*) out);
					nfailed++;
				}
				if (error) {
					g_print ("Got error: %s\n", error && error->message ? error->message : "No detail");
					g_clear_error (&error);
					nfailed++;
				}
				break;
			}
			else
				g_clear_error (&error);
			g_usleep (100000);
		}
	}

	g_print ("Unref worker...\n");

	gda_worker_unref (worker);
	g_print ("%s done\n", __FUNCTION__);

	return nfailed;
}

/*
 * test 9: gda_worker_set_callback() with multiple threads submitting jobs and retreiving them
 */
typedef struct {
	guint in;
	guint out;

	GMainLoop *loop;
} Test9Data;

static void
test9data_free (G_GNUC_UNUSED Test9Data *data)
{
	/* should not be called by GdaWorker, instead it is freed manually using g_free()  */
	g_assert_not_reached ();
}

/* function run by the worker thread */
static gpointer
test9_job_func (Test9Data *data, G_GNUC_UNUSED GError **error)
{
	g_print ("job_func (thread => %p, data => %p)\n", g_thread_self(), data);
	data->out = data->in;
	return data;
}

/* function called back in the context which submitted the jobs */
static void
test9_worker_cb (G_GNUC_UNUSED GdaWorker *worker, G_GNUC_UNUSED guint job_id, Test9Data *data, G_GNUC_UNUSED GError *error, GThread *thread)
{
	g_print ("%s (thread => %p, result_data => %p)\n", __FUNCTION__, g_thread_self(), data);

	if (thread != g_thread_self()) {
		g_print ("callback from gda_worker_set_callback() called in thread %p, expected in thread %p\n",
			 g_thread_self(), thread);
		exit (1);
	}
	else if (data->in != data->out) {
		g_print ("Expected data->out[%u] to be equal to data->in[%u]\n", data->out, data->in);
		exit (1);
	}

	if (data->loop)
		g_main_loop_quit (data->loop);
	g_free (data);
}

/* main function run by each thread, which submit jobs */
static gpointer
test9_main_func (GdaWorker *worker)
{
	GError *error = NULL;
	GMainContext *context = NULL;
	static guint counter = 0;

	context = g_main_context_new ();

	if (! gda_worker_set_callback (worker, context, (GdaWorkerCallback) test9_worker_cb, g_thread_self (), &error)) {
		g_print ("gda_worker_set_callback() from thread %p error: %s\n", g_thread_self (),
			 error && error->message ? error->message : "No detail");
		g_clear_error (&error);
		return (gpointer) 0x01;
	}

	GMainLoop *loop;
	loop = g_main_loop_new (context, FALSE);
#define N_JOBS 2
	guint i;
	for (i = 0; i < N_JOBS; i++) {
		guint jid;
		Test9Data *data;
		data = g_new (Test9Data, 1);
		data->in = (guint) GPOINTER_TO_INT (g_thread_self ()) + (counter++);
		data->out = 0;
		data->loop = (i == N_JOBS - 1) ? loop : NULL;
		jid = gda_worker_submit_job (worker, context, (GdaWorkerFunc) test9_job_func, data,
					     NULL, (GDestroyNotify) test9data_free, &error);
		if (jid == 0) {
			g_print ("gda_worker_submit_job() from thread %p error: %s\n", g_thread_self (),
				 error && error->message ? error->message : "No detail");
			g_clear_error (&error);
			return (gpointer) 0x01;
		}
		else
			g_print ("Submitted job %u from thread %p, with data %p\n", jid, g_thread_self(), data);
	}
				     
	g_main_loop_run (loop);
	g_main_loop_unref (loop);

	g_main_context_unref (context);
	return NULL;
}

int
test9 (void)
{
	g_print ("%s started, main thread is %p\n", __FUNCTION__, g_thread_self ());
	GdaWorker *worker;
	gint nfailed = 0;

	worker = gda_worker_new ();

#define NB_THREADS 2
	GThread *threads[NB_THREADS];
	guint i;
	for (i = 0; i < NB_THREADS; i++) {
		gchar *name;
		name = g_strdup_printf ("th%u", i);
		threads[i] = g_thread_new (name, (GThreadFunc) test9_main_func, worker);
		g_free (name);
	}

	for (i = 0; i < NB_THREADS; i++) {
		if (g_thread_join (threads[i]))
			nfailed++;
	}

	g_print ("Unref worker...\n");

	gda_worker_unref (worker);
	g_print ("%s done\n", __FUNCTION__);

	return nfailed;
}

/*
 * Test 10: test gda_worker_wait_job()
 */
static gpointer
test10_func_slow (G_GNUC_UNUSED gpointer data, G_GNUC_UNUSED GError **error)
{
	g_print ("In %s()...\n", __FUNCTION__);
	g_usleep (500000); /* wait half a second */
	g_print ("Leaving %s()\n", __FUNCTION__);
	return (gpointer) 0x01;
}

static gpointer
test10_func_fast (G_GNUC_UNUSED gpointer data, G_GNUC_UNUSED GError **error)
{
	g_print ("Passed through %s()...\n", __FUNCTION__);
	return (gpointer) 0x02;
}

int
test10 (void)
{
	g_print ("%s started, main thread is %p\n", __FUNCTION__, g_thread_self ());
	GdaWorker *worker;
	gint nfailed = 0;

	worker = gda_worker_new ();
	GError *error = NULL;

	g_print ("Waiting for test10_func() to execute\n");
	if (gda_worker_wait_job (worker, (GdaWorkerFunc) test10_func_slow, NULL, NULL, &error) != (gpointer) 0x01) {
		g_print ("gda_worker_wait_job() failed: %s\n", error && error->message ? error->message : "No detail");
		g_clear_error (&error);
		nfailed ++;
	}
	else {
		if (gda_worker_wait_job (worker, (GdaWorkerFunc) test10_func_fast, NULL, NULL, &error) != (gpointer) 0x02) {
			g_print ("gda_worker_wait_job() failed: %s\n", error && error->message ? error->message : "No detail");
			g_clear_error (&error);
			nfailed ++;
		}
	}

	g_print ("Unref worker...\n");

	gda_worker_unref (worker);
	g_print ("%s done\n", __FUNCTION__);

	return nfailed;
}

/*
 * Test 11: test gda_worker_do_job(), no timeout
 */
static gpointer
test11_func (G_GNUC_UNUSED gpointer data, G_GNUC_UNUSED GError **error)
{
	g_print ("In %s()...\n", __FUNCTION__);
	g_usleep (500000); /* wait half a second */
	g_print ("Leaving %s()\n", __FUNCTION__);
	return (gpointer) 0x01;
}

static gpointer
test11_func_e (G_GNUC_UNUSED gpointer data, GError **error)
{
	g_print ("In %s()...\n", __FUNCTION__);
	g_usleep (500000); /* wait half a second */
	g_print ("Leaving %s()\n", __FUNCTION__);
	g_set_error (error, 1, 1,
		     "Expected test11_func_e");
	return NULL;
}

int
test11 (void)
{
	g_print ("%s started, main thread is %p\n", __FUNCTION__, g_thread_self ());
	GdaWorker *worker;
	gint nfailed = 0;

	worker = gda_worker_new ();
	GError *error = NULL;

	g_print ("Waiting for test11_func() to execute\n");
	gpointer result;
	if (!gda_worker_do_job (worker, NULL, 0, &result, NULL,
				(GdaWorkerFunc) test11_func, NULL, NULL,
				NULL, &error) ||
	    (result != (gpointer) 0x01)) {
		g_print ("gda_worker_do_job() failed: %s\n", error && error->message ? error->message : "No detail");
		g_clear_error (&error);
		nfailed ++;
	}
	if (!gda_worker_do_job (worker, NULL, 0, &result, NULL,
				(GdaWorkerFunc) test11_func_e, NULL, NULL,
				NULL, &error) ||
	    result) {
		g_print ("gda_worker_do_job() failed: %s\n", error && error->message ? error->message : "No detail");
		g_clear_error (&error);
		nfailed ++;
	}
	else if (!error || (error->domain != 1) || (error->code != 1)) {
		g_print ("gda_worker_do_job() failed: unexpected empty error, or wrong error domain or code%s\n",
			 error && error->message ? error->message : "No detail");
		g_clear_error (&error);
		nfailed ++;
	}

	g_print ("Unref worker...\n");

	gda_worker_unref (worker);
	g_print ("%s done\n", __FUNCTION__);

	return nfailed;
}

/*
 * Test 12: gda_worker_do_job() for quick jobs
 */
static gpointer
test12_func (G_GNUC_UNUSED gpointer data, G_GNUC_UNUSED GError **error)
{
	/* make it as quick as possible */
	return (gpointer) 0x02;
}

int
test12 (void)
{
	g_print ("%s started, main thread is %p\n", __FUNCTION__, g_thread_self ());
	GdaWorker *worker;
	gint nfailed = 0;

	worker = gda_worker_new ();
	GError *error = NULL;

	g_print ("Waiting for test12_func() to execute\n");
	gpointer result;
	if (!gda_worker_do_job (worker, NULL, 0, &result, NULL,
				(GdaWorkerFunc) test12_func, NULL, NULL,
				NULL, &error) ||
	    (result != (gpointer) 0x02)) {
		g_print ("gda_worker_do_job() failed: %s\n", error && error->message ? error->message : "No detail");
		g_clear_error (&error);
		nfailed ++;
	}

	g_print ("Unref worker...\n");

	gda_worker_unref (worker);
	g_print ("%s done\n", __FUNCTION__);

	return nfailed;
}

/*
 * Test 13: gda_worker_do_job() for several jobs
 */
static gpointer
test13_func (gpointer data, G_GNUC_UNUSED GError **error)
{
	g_print ("%s (%s) in thread %p\n", __FUNCTION__, (gchar *) data, g_thread_self ());
	static guint i = 1;
	g_usleep (G_USEC_PER_SEC / 2); /* wait half a second */
	i++;
	return (gpointer) GUINT_TO_POINTER (i);
}

gboolean test13_sub_failed = FALSE;
gboolean
test13_time_func (GdaWorker *worker)
{
	/* this time we should get 3, because the previous call to test13_func() should have returned 2 */
	g_print ("Sumbitting another job for test13_func()\n");
	gpointer result;
	GError *error = NULL;
	if (!gda_worker_do_job (worker, NULL, 0, &result, NULL,
				(GdaWorkerFunc) test13_func, "2nd", NULL,
				NULL, &error)) {
		g_print ("gda_worker_do_job() failed: %s\n", error && error->message ? error->message : "No detail");
		g_clear_error (&error);
		test13_sub_failed = TRUE;
	}
	else if (GPOINTER_TO_UINT (result) != 3) {
		g_print ("In %s(): expected 3 and got %u\n", __FUNCTION__, GPOINTER_TO_UINT (result));
		test13_sub_failed = TRUE;
	}

	gda_worker_unref (worker);

	return G_SOURCE_REMOVE;
}

int
test13 (void)
{
	g_print ("%s started, main thread is %p\n", __FUNCTION__, g_thread_self ());
	GdaWorker *worker;
	gint nfailed = 0;

	worker = gda_worker_new ();
	GError *error = NULL;

	g_timeout_add (200, (GSourceFunc) test13_time_func, gda_worker_ref (worker));

	g_print ("Sumbitting job for test13_func()\n");
	gpointer result;
	if (!gda_worker_do_job (worker, NULL, 0, &result, NULL,
				(GdaWorkerFunc) test13_func, "1st", NULL,
				NULL, &error)) {
		g_print ("gda_worker_do_job() failed: %s\n", error && error->message ? error->message : "No detail");
		g_clear_error (&error);
		nfailed ++;
	}
	else if (GPOINTER_TO_UINT (result) != 2) {
		g_print ("In %s(): expected 2 and got %u\n", __FUNCTION__, GPOINTER_TO_UINT (result));
		nfailed ++;
	}
	else if (test13_sub_failed)
		nfailed ++;

	g_print ("Unref worker...\n");

	gda_worker_unref (worker);
	g_print ("%s done\n", __FUNCTION__);

	return nfailed;
}
