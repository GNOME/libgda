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

int test1 (void);

int
main (int argc, char** argv)
{
	gint nfailed = 0;
#if GLIB_CHECK_VERSION(2,36,0)
#else
	g_type_init ();
#endif

	nfailed += test1 ();

	return nfailed > 0 ? 1 : 0;
}

static gboolean
idle_stop_main_loop (GMainLoop *loop)
{
	g_main_loop_quit (loop);
	return FALSE; /* remove the source */
}

/***********************************************/

typedef struct {
	GThread *exp_thread;
	guint    counter;
} SigData;

static void
sig0_cb (DummyObject *obj, SigData *data)
{
	g_print ("%s() called from thread %p\n", __FUNCTION__, g_thread_self());

	if (data->exp_thread != g_thread_self ()) {
		g_print ("%s() called from the wrong thread!\n", __FUNCTION__);
		exit (1); /* failed! */
	}
	data->counter++;
}

/*
 * Emit signals from this thread, to be caught in the main thread
 */
static gpointer
thread1_start (DummyObject *obj)
{
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
	hid = gda_connect (obj, "sig0",
			   G_CALLBACK (sig0_cb),
			   &sig_data, NULL, G_CONNECT_SWAPPED, NULL);

	/* prepare loop */
	GMainLoop *loop;
	loop = g_main_loop_new (NULL, FALSE);
	g_timeout_add (100, (GSourceFunc) idle_stop_main_loop, loop); /* stop main loop after 100 ms */

	/* run thread to emit signals */
	GThread *th;
	th = g_thread_new ("sub", (GThreadFunc) thread1_start, obj);

	g_signal_emit_by_name (obj, "sig0");

	/* run loop */
	g_main_loop_run (loop);
	g_main_loop_unref (loop);
	g_thread_join (th);

	gda_disconnect (obj, hid, NULL);

	g_signal_emit_by_name (obj, "sig0");

	g_object_unref (obj);

	/* results! */
	if (sig_data.counter != 2) {
		g_print ("Error: callback has been called %u times when it should have been called %u times!\n",
			 sig_data.counter, 2);
		retval++;
	}
	g_print ("Test %s\n", retval ? "failed": "succeded");
	return retval;
}

