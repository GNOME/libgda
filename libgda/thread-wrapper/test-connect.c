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
#include "gda-connect.h"
#include "dummy-object.h"
#include <stdlib.h>
#include <string.h>

#define NB_THREADS 20

int test1 (void);
int test2 (void);
int test3 (void);
int test4 (void);

int
main (int argc, char** argv)
{
	gint nfailed = 0;
#if GLIB_CHECK_VERSION(2,36,0)
#else
	g_type_init ();
#endif

	GMainContext *context;
	context = g_main_context_ref_thread_default ();
	g_main_context_acquire (context);

	g_print ("Main thread is %p\n", g_thread_self ());
	nfailed += test1 ();
	nfailed += test2 ();
	nfailed += test3 ();
	nfailed += test4 ();

	g_main_context_release (context);
	g_main_context_unref (context);

	return nfailed > 0 ? 1 : 0;
}

static gboolean
idle_stop_main_loop (GMainLoop *loop)
{
	g_main_loop_quit (loop);
	return FALSE; /* remove the source */
}

#define EXP_INT 1234
#define EXP_STR "Hell0"

typedef struct {
	GThread *exp_thread;
	gint     exp_int;
	gchar   *exp_str;
	guint    counter;
} SigData;

static void
sig0_cb (DummyObject *obj, SigData *data)
{
	if (data->exp_thread != g_thread_self ()) {
		g_print ("%s() called from the wrong thread!\n", __FUNCTION__);
		exit (1); /* failed! */
	}
	data->counter++;
}

static void
sig1_cb (DummyObject *obj, gint i, SigData *data)
{
	if (data->exp_thread != g_thread_self ()) {
		g_print ("%s() called from the wrong thread!\n", __FUNCTION__);
		exit (1); /* failed! */
	}
	if (data->exp_int != i) {
		g_print ("%s() called with the wrong integer argument!\n", __FUNCTION__);
		exit (1); /* failed! */
	}
	data->counter++;
}

static void
sig2_cb (DummyObject *obj, gint i, gchar *str, SigData *data)
{
	if (data->exp_thread != g_thread_self ()) {
		g_print ("%s() called from the wrong thread!\n", __FUNCTION__);
		exit (1); /* failed! */
	}
	if (data->exp_int != i) {
		g_print ("%s() called with the wrong integer argument!\n", __FUNCTION__);
		exit (1); /* failed! */
	}
	if (strcmp (data->exp_str, str)) {
		g_print ("%s() called with the wrong string argument!\n", __FUNCTION__);
		exit (1); /* failed! */
	}
	data->counter++;
}

static gchar*
sig3_cb (DummyObject *obj, gchar *str, gint i, SigData *data)
{
	g_print ("%s (%s, %d)\n", __FUNCTION__, str, i);
	if (data->exp_thread != g_thread_self ()) {
		g_print ("%s() called from the wrong thread!\n", __FUNCTION__);
		exit (1); /* failed! */
	}
	if (data->exp_int != i) {
		g_print ("%s() called with the wrong integer argument!\n", __FUNCTION__);
		exit (1); /* failed! */
	}
	if (strcmp (data->exp_str, str)) {
		g_print ("%s() called with the wrong string argument!\n", __FUNCTION__);
		exit (1); /* failed! */
	}
	data->counter++;

	return g_strdup_printf ("%d@%s", i, str);
}

/***********************************************/

/*
 * Emit signals from this thread, to be caught in the main thread
 */
static gpointer
thread1_start (DummyObject *obj)
{
	g_signal_emit_by_name (obj, "sig0");
	g_thread_yield ();
	g_signal_emit_by_name (obj, "sig0");
	return (gpointer) 0x01;
}

int
test1 (void)
{
	g_print ("Test1 started\n");
	gint retval = 0;

	DummyObject *obj;
	obj = dummy_object_new ();

	SigData sig_data;
	sig_data.exp_thread = g_thread_self ();
	sig_data.counter = 0;

	gulong hid;
	hid = gda_signal_connect (obj, "sig0",
				  G_CALLBACK (sig0_cb),
				  &sig_data, NULL, 0, NULL);

	/* prepare loop */
	GMainLoop *loop;
	loop = g_main_loop_new (NULL, FALSE);
	g_timeout_add (100, (GSourceFunc) idle_stop_main_loop, loop); /* stop main loop after 100 ms */

	/* run thread to emit signals */
	GThread *ths[NB_THREADS];
	guint i;
	for (i = 0 ; i < NB_THREADS; i++)
		ths[i] = g_thread_new ("sub", (GThreadFunc) thread1_start, obj);

	g_signal_emit_by_name (obj, "sig0");

	/* run loop */
	g_main_loop_run (loop);
	g_main_loop_unref (loop);
	for (i = 0 ; i < NB_THREADS; i++)
		g_thread_join (ths[i]);

	gda_signal_handler_disconnect (obj, hid);

	g_signal_emit_by_name (obj, "sig0");

	g_object_unref (obj);

	/* results! */
	if (sig_data.counter != 2 * NB_THREADS + 1) {
		g_print ("Error: callback has been called %u times when it should have been called %u times!\n",
			 sig_data.counter, 2 * NB_THREADS + 1);
		retval++;
	}
	g_print ("Test %s\n", retval ? "failed": "succeded");
	return retval;
}

/***********************************************/

/*
 * Emit signals from this thread, to be caught in the main thread
 */
static gpointer
thread2_start (DummyObject *obj)
{
	g_signal_emit_by_name (obj, "sig1", EXP_INT);
	g_thread_yield ();
	g_signal_emit_by_name (obj, "sig1", EXP_INT);
	return (gpointer) 0x01;
}

int
test2 (void)
{
	g_print ("Test2 started\n");
	gint retval = 0;

	DummyObject *obj;
	obj = dummy_object_new ();

	SigData sig_data;
	sig_data.exp_thread = g_thread_self ();
	sig_data.exp_int = EXP_INT;
	sig_data.counter = 0;

	gulong hid;
	hid = gda_signal_connect (obj, "sig1",
				  G_CALLBACK (sig1_cb),
				  &sig_data, NULL, 0, NULL);

	/* prepare loop */
	GMainLoop *loop;
	loop = g_main_loop_new (NULL, FALSE);
	g_timeout_add (100, (GSourceFunc) idle_stop_main_loop, loop); /* stop main loop after 100 ms */

	/* run thread to emit signals */
	GThread *ths[NB_THREADS];
	guint i;
	for (i = 0 ; i < NB_THREADS; i++)
		ths[i] = g_thread_new ("sub", (GThreadFunc) thread2_start, obj);

	g_signal_emit_by_name (obj, "sig1", EXP_INT);

	/* run loop */
	g_main_loop_run (loop);
	g_main_loop_unref (loop);
	for (i = 0 ; i < NB_THREADS; i++)
		g_thread_join (ths[i]);

	gda_signal_handler_disconnect (obj, hid);

	g_signal_emit_by_name (obj, "sig1", EXP_INT * 2);

	g_object_unref (obj);

	/* results! */
	if (sig_data.counter != 2 * NB_THREADS + 1) {
		g_print ("Error: callback has been called %u times when it should have been called %u times!\n",
			 sig_data.counter, 2 * NB_THREADS + 1);
		retval++;
	}
	g_print ("Test %s\n", retval ? "failed": "succeded");
	return retval;
}


/***********************************************/

/*
 * Emit signals from this thread, to be caught in the main thread
 */
static gpointer
thread3_start (DummyObject *obj)
{
	g_signal_emit_by_name (obj, "sig2", EXP_INT, EXP_STR);
	g_thread_yield ();
	g_signal_emit_by_name (obj, "sig2", EXP_INT, EXP_STR);
	return (gpointer) 0x01;
}

int
test3 (void)
{
	g_print ("Test3 started\n");
	gint retval = 0;

	DummyObject *obj;
	obj = dummy_object_new ();

	SigData sig_data;
	sig_data.exp_thread = g_thread_self ();
	sig_data.exp_int = EXP_INT;
	sig_data.exp_str = EXP_STR;
	sig_data.counter = 0;

	gulong hid;
	hid = gda_signal_connect (obj, "sig2",
				  G_CALLBACK (sig2_cb),
				  &sig_data, NULL, 0, NULL);

	/* prepare loop */
	GMainLoop *loop;
	loop = g_main_loop_new (NULL, FALSE);
	g_timeout_add (100, (GSourceFunc) idle_stop_main_loop, loop); /* stop main loop after 100 ms */

	/* run thread to emit signals */
	GThread *ths[NB_THREADS];
	guint i;
	for (i = 0 ; i < NB_THREADS; i++)
		ths[i] = g_thread_new ("sub", (GThreadFunc) thread3_start, obj);

	g_signal_emit_by_name (obj, "sig2", EXP_INT, EXP_STR);

	/* run loop */
	g_main_loop_run (loop);
	g_main_loop_unref (loop);
	for (i = 0 ; i < NB_THREADS; i++)
		g_thread_join (ths[i]);

	gda_signal_handler_disconnect (obj, hid);

	g_signal_emit_by_name (obj, "sig2", EXP_INT * 2, NULL);

	g_object_unref (obj);

	/* results! */
	if (sig_data.counter != 2 * NB_THREADS + 1) {
		g_print ("Error: callback has been called %u times when it should have been called %u times!\n",
			 sig_data.counter, 2 * NB_THREADS + 1);
		retval++;
	}
	g_print ("Test %s\n", retval ? "failed": "succeded");
	return retval;
}

/***********************************************/

/*
 * Emit signals from this thread, to be caught in the main thread
 */
static gpointer
thread4_start (DummyObject *obj)
{
	gchar *tmp;
	g_signal_emit_by_name (obj, "sig3", EXP_STR, EXP_INT, &tmp);
	g_free (tmp);
	g_thread_yield ();
	g_signal_emit_by_name (obj, "sig3", EXP_STR, EXP_INT, &tmp);
	g_free (tmp);
	return (gpointer) 0x01;
}

int
test4 (void)
{
	g_print ("Test4 started\n");
	gint retval = 0;

	DummyObject *obj;
	obj = dummy_object_new ();

	SigData sig_data;
	sig_data.exp_thread = g_thread_self ();
	sig_data.exp_int = EXP_INT;
	sig_data.exp_str = EXP_STR;
	sig_data.counter = 0;

	gulong hid;
	hid = gda_signal_connect (obj, "sig3",
				  G_CALLBACK (sig3_cb),
				  &sig_data, NULL, 0, NULL);

	/* prepare loop */
	GMainLoop *loop;
	loop = g_main_loop_new (NULL, FALSE);
	g_timeout_add (100, (GSourceFunc) idle_stop_main_loop, loop); /* stop main loop after 100 ms */

	/* run thread to emit signals */
	GThread *ths[NB_THREADS];
	guint i;
	for (i = 0 ; i < NB_THREADS; i++)
		ths[i] = g_thread_new ("sub", (GThreadFunc) thread4_start, obj);

	gchar *tmp;
	g_signal_emit_by_name (obj, "sig3", EXP_STR, EXP_INT, &tmp);
	g_print ("TMP=[%s]\n", tmp);
	g_free (tmp);

	/* run loop */
	g_main_loop_run (loop);
	g_main_loop_unref (loop);
	for (i = 0 ; i < NB_THREADS; i++)
		g_thread_join (ths[i]);

	gda_signal_handler_disconnect (obj, hid);

	g_signal_emit_by_name (obj, "sig2", EXP_INT * 2, NULL);

	g_object_unref (obj);

	/* results! */
	if (sig_data.counter != 2 * NB_THREADS + 1) {
		g_print ("Error: callback has been called %u times when it should have been called %u times!\n",
			 sig_data.counter, 2 * NB_THREADS + 1);
		retval++;
	}
	g_print ("Test %s\n", retval ? "failed": "succeded");
	return retval;
}

