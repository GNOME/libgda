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
#include <errno.h>
#ifdef G_OS_WIN32
//#include <fcntl.h>
//#include <io.h>
#else
  #include <poll.h>
#endif
#include <stdio.h>
#include <unistd.h>

#ifdef PERF
  #define MAX_ITERATIONS 1000000
#else
  #define MAX_ITERATIONS 50
#endif

/*
 * Data to be passed between threads
 */
typedef struct {
	gchar  dummy[10000];
	gint   counter;
} Data;

static gpointer thread1_start (ITSignaler *its);

int
main (G_GNUC_UNUSED int argc, G_GNUC_UNUSED char** argv)
{
	g_print ("Test started\n");

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

	GThread *th;
	th = g_thread_new ("sub", (GThreadFunc) thread1_start, its);
	gint counter = 0;
	guint ticks;
#ifdef G_OS_WIN32
	struct timeval timeout;
	fd_set fds;
	SOCKET sock;
	timeout.tv_sec = 0;
	timeout.tv_usec = 100000;
	sock = itsignaler_windows_get_poll_fd (its);
	FD_ZERO (&fds);
	FD_SET (sock, &fds);
#else
	struct pollfd fds[1];
	fds[0].fd = itsignaler_unix_get_poll_fd (its);
	fds[0].events = POLLIN;
	fds[0].revents = 0;
#endif

	GTimer *timer;
	timer = g_timer_new ();

	for (ticks = 0; ticks < MAX_ITERATIONS * 4; ticks++) {
		data = NULL;
		int res;
#ifndef PERF
		g_print (".");
		fflush (stdout);
#endif

#ifdef G_OS_WIN32
		res = select (1, &fds, 0, 0, &timeout);
		if (res == SOCKET_ERROR) {
			g_warning ("Failure on select(): %d", WSAGetLastError ()); /* FIXME! */
			return EXIT_FAILURE;
		}
		else if (res > 0) {
			data = (Data*) itsignaler_pop_notification (its, 0);
		}
#else
		res = poll (fds, 1, 100);
		if (res == -1) {
			g_warning ("Failure on poll(): %s", strerror (errno));
			return EXIT_FAILURE;
		}
		else if (res > 0) {
			if (fds[0].revents | POLLIN) {
				data = (Data*) itsignaler_pop_notification (its, 0);
			}
			else {
				g_warning ("Something nasty happened on file desciptor...");
				return EXIT_FAILURE;
			}
		}
#endif

		if (data) {
			if (counter != data->counter) {
				g_warning ("itsignaler_pop_notification() returned wrong value %d instead of %d",
					   data->counter, counter);
				return EXIT_FAILURE;
			}
			else {
#ifndef PERF
				g_print ("Popped %d\n", counter);
#endif
				counter++;
			}
			g_free (data);
			if (counter == MAX_ITERATIONS * 2)
				break;
		}
	}

	g_timer_stop (timer);

	g_thread_join (th);
	itsignaler_unref (its);

	if (counter == MAX_ITERATIONS * 2) {
		gdouble duration;
		duration = g_timer_elapsed (timer, NULL);
		g_print ("Test Ok, got %u notification in %0.5f s\n", MAX_ITERATIONS * 2, duration);
		g_timer_destroy (timer);
		return EXIT_SUCCESS;
	}
	else {
		g_print ("Test Failed: got %d notification(s) out of %u\n", counter, MAX_ITERATIONS * 2);
		g_timer_destroy (timer);
		return EXIT_SUCCESS;
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
		data = g_new0 (Data, 1);
		data->counter = counter++;
#ifndef PERF
		g_print ("Pushing %d...\n", counter);
#endif
		itsignaler_push_notification (its, data, g_free);

		data = g_new0 (Data, 1);
		data->counter = counter++;
		itsignaler_push_notification (its, data, g_free);
	}
	itsignaler_unref (its);
	return (gpointer) 0x01;
}
