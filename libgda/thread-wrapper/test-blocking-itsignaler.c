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

/*
 * Data to be passed between threads
 */
typedef struct {
	gchar  dummy[10000];
	gint   counter;
} Data;

static gpointer thread1_start (ITSignaler *its);

int
main ()
{
	g_print ("Test started\n");

	ITSignaler *its;
	its = itsignaler_new ();

	Data *data;

	/* simple blocking when there is non notification to pop */
	GTimer *timer;
	gdouble waited;
	timer = g_timer_new ();
	data = itsignaler_pop_notification (its, 500);
	if (data) {
		g_print ("itsignaler_pop_notification() sould have returned NULL\n");
		return EXIT_FAILURE;
	}
	g_timer_stop (timer);
	waited = g_timer_elapsed (timer, NULL);
	g_timer_destroy (timer);
	g_print ("Waited for %.0f ms\n", waited * 1000);
	if ((waited < 0.5) || (waited > 0.55))
		return EXIT_FAILURE;

	/* waiting with a notification to pop */
	GThread *th;
	th = g_thread_new ("sub", (GThreadFunc) thread1_start, its);
	timer = g_timer_new ();
	data = itsignaler_pop_notification (its, 500);
	if (!data) {
		g_print ("itsignaler_pop_notification() should have returned a value\n");
		return EXIT_FAILURE;
	}
	g_timer_stop (timer);
	waited = g_timer_elapsed (timer, NULL);
	g_timer_destroy (timer);
	g_print ("Waited for %.0f ms\n", waited * 1000);
	if (strcmp (data->dummy, "Delayed...") || (data->counter != 345)) {
		g_print ("itsignaler_pop_notification() returned an unexpected notification content\n");
		return EXIT_FAILURE;
	}
	g_free (data);
	g_thread_join (th);

	return EXIT_SUCCESS;
}

/*
 * Send notifications from this thread, to be caught in the main thread
 */
static gpointer
thread1_start (ITSignaler *its)
{
	itsignaler_ref (its);
	g_usleep (300000);

	Data *data;
	data = g_new0 (Data, 1);
	strcpy (data->dummy, "Delayed...");
	data->counter = 345;
#ifndef PERF
	g_print ("Pushing %d...\n", data->counter);
#endif
	itsignaler_push_notification (its, data, g_free);
	itsignaler_unref (its);
	return (gpointer) 0x01;
}
