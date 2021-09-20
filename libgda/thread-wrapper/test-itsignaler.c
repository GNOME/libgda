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
#include "itsignaler.h"
#include <stdlib.h>

#ifdef PERF
  #define MAX_ITERATIONS 1000000
#else
  #define MAX_ITERATIONS 50
#endif

int test1 (void);
int test2 (void);
int test3 (void);


int
main (G_GNUC_UNUSED int argc, G_GNUC_UNUSED char** argv)
{
	gint nfailed = 0;

	nfailed += test1 ();
	nfailed += test2 ();
	nfailed += test3 ();

	return nfailed > 0 ? 1 : 0;
}

/*
 * Data to be passed between threads
 */
typedef struct {
	gchar  dummy[10000];
	gint   counter;
} Data;

typedef struct {
  ITSignaler *its;
	gint       counter;
	GMainLoop *loop;
} CbData;

static gpointer thread1_start (ITSignaler *its);

static gboolean
source_callback (CbData *cbdata)
{
  if (cbdata == NULL) {
    g_warning ("Raice condition, data is null");
    g_main_loop_quit (cbdata->loop);
		return G_SOURCE_REMOVE;
  }
	Data *data;
	data = itsignaler_pop_notification (cbdata->its, 0);
  if (data != NULL) {
	  if (cbdata->counter != data->counter) {
		  g_warning ("itsignaler_pop_notification() returned wrong value %d instead of %d",
			     cbdata->counter, data->counter);
		  return EXIT_FAILURE;
	  }
	  g_print ("Popped %d\n", data->counter);
	  g_free (data);
  }

	cbdata->counter ++;
	if (cbdata->counter == MAX_ITERATIONS * 2) {
		g_main_loop_quit (cbdata->loop);
		return FALSE;
	}
	else
		return TRUE;
}

static GMutex mutex;

int
test1 (void)
{
	g_print ("Test1 started\n");

	ITSignaler *its;
	its = itsignaler_new ();

	Data *data;
#ifndef PERF
	g_print ("Try popping initially...\n");
#endif
	data = itsignaler_pop_notification (its, 0);
	if (data) {
		g_warning ("itsignaler_pop_notification() sould have returned NULL");
		return EXIT_FAILURE;
	}
#ifndef PERF
	g_print ("Done\n");
#endif

	GTimer *timer;
	timer = g_timer_new ();

	GMainLoop *loop;
	loop = g_main_loop_new (NULL, FALSE);

	GSource *source;
	source = itsignaler_create_source (its);
	itsignaler_unref (its);

  g_mutex_lock (&mutex);
	CbData cbdata;
  cbdata.its = its;
	cbdata.counter = 0;
	cbdata.loop = loop;
	g_source_set_callback (source, G_SOURCE_FUNC (source_callback), &cbdata, NULL);
	g_source_attach (source, NULL);
  g_mutex_unlock (&mutex);

	GThread *th;
	th = g_thread_new ("sub", (GThreadFunc) thread1_start, its);

	g_main_loop_run (loop);
	g_source_unref (source);

	g_timer_stop (timer);

	g_thread_join (th);

	if (cbdata.counter == MAX_ITERATIONS * 2) {
		gdouble duration;
		duration = g_timer_elapsed (timer, NULL);
		g_print ("Test Ok, %0.5f s\n", duration);
		g_timer_destroy (timer);
		return 0;
	}
	else {
		g_print ("Test Failed: got %d notification(s) out of %d\n", cbdata.counter, MAX_ITERATIONS * 2);
		g_timer_destroy (timer);
		return 1;
	}
}

/*
 * Send notifications from this thread, to be caught in the main thread
 */
static gpointer
thread1_start (ITSignaler *its)
{
	gint i, counter;
	itsignaler_ref (its);
	for (counter = 0, i = 0; i < MAX_ITERATIONS; i++) {
		Data *data;
    g_mutex_lock (&mutex);
		data = g_new0 (Data, 1);
		data->counter = counter++;
#ifndef PERF
		g_print ("Pushing %d...\n", counter);
#endif
		itsignaler_push_notification (its, data, g_free);
    g_mutex_unlock (&mutex);

    g_mutex_lock (&mutex);
		data = g_new0 (Data, 1);
		data->counter = counter++;
		itsignaler_push_notification (its, data, g_free);
    g_mutex_unlock (&mutex);
	}
	itsignaler_unref (its);
	return (gpointer) 0x01;
}

int
test2 (void)
{
	g_print ("Test2 started\n");

	ITSignaler *its;
	its = itsignaler_new ();


	GTimer *timer;
	timer = g_timer_new ();

	GMainLoop *loop;
	loop = g_main_loop_new (NULL, FALSE);

  g_mutex_lock (&mutex);
	CbData cbdata;
	cbdata.counter = 0;
	cbdata.loop = loop;
	cbdata.its = its;
	itsignaler_add (its, NULL, (ITSignalerFunc) source_callback, &cbdata, NULL);
	itsignaler_unref (its);
  g_mutex_unlock (&mutex);

	GThread *th;
	th = g_thread_new ("sub", (GThreadFunc) thread1_start, its);

	g_main_loop_run (loop);
	g_main_loop_unref (loop);

	g_timer_stop (timer);

	g_thread_join (th);

	if (cbdata.counter == MAX_ITERATIONS * 2) {
		gdouble duration;
		duration = g_timer_elapsed (timer, NULL);
		g_print ("Test Ok, %0.5f s\n", duration);
		g_timer_destroy (timer);
		return 0;
	}
	else {
		g_print ("Test Failed: got %d notification(s) out of %d\n", cbdata.counter, MAX_ITERATIONS * 2);
		g_timer_destroy (timer);
		return 1;
	}
}


int test3_destroyed;

static void
test3_destroy_func (gpointer data)
{
	g_print ("Destroy function called for data %p\n", data);
	test3_destroyed++;
}

int
test3 (void)
{
	g_print ("Test3 started\n");

	ITSignaler *its;
	its = itsignaler_new ();
	test3_destroyed = 0;
	itsignaler_push_notification (its, (gpointer) 0x01, test3_destroy_func);
	itsignaler_push_notification (its, (gpointer) 0x02, test3_destroy_func);
	itsignaler_push_notification (its, (gpointer) 0x03, test3_destroy_func);
	
	gpointer data;
	data = itsignaler_pop_notification (its, 100);
	if (!data || data != (gpointer) 0x01) {
		g_print ("Popped wrong value: %p instead of %p\n", data, (gpointer) 0x01);
		itsignaler_unref (its);
		return 1;
	}
	
	itsignaler_unref (its);

	return test3_destroyed == 2 ? 0 : 1;
}
