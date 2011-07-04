/*
 * Copyright (C) 2008 Vivien Malerba <malerba@gnome-db.org>
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
#include <libgda/libgda.h>
#include <string.h>
#include <unistd.h>

#define NTHREADS 3
#define DEBUG_PRINT
#undef DEBUG_PRINT


/* 
 * tests
 */
typedef gboolean (*TestFunc) (GError **);
static gboolean test1 (GError **error);
static gboolean test2 (GError **error);
static gboolean test3 (GError **error);

TestFunc tests[] = {
	test1,
	test2,
	test3
};

int 
main (int argc, char** argv)
{
	g_type_init ();
	gda_init ();

	gint failures = 0;
	gint j, ntests = 0;;
	for (j = 0; j < 10; j++) {
		guint i;
		
		for (i = 0; i < sizeof (tests) / sizeof (TestFunc); i++) {
			GError *error = NULL;
			if (! tests[i] (&error)) {
				g_print ("Test %d failed: %s\n", i+1, 
					 error && error->message ? error->message : "No detail");
				if (error)
					g_error_free (error);
				failures ++;
			}
			ntests ++;
		}
	}

	g_print ("TESTS COUNT: %d\n", ntests);
	g_print ("FAILURES: %d\n", failures);

	return failures != 0 ? 1 : 0;
}

typedef struct {
	GThread  *thread;
	gint      th_id;
	gboolean  finished;
	GdaMutex *mutex;
} ThData;

static gboolean
test_multiple_threads (GThreadFunc func, GError **error)
{
	GdaMutex *mutex;
	ThData data[NTHREADS];
	gint i;

	mutex = gda_mutex_new ();

	for (i = 0; i < NTHREADS; i++) {
		ThData *d = &(data[i]);
		d->th_id = i;
		d->mutex = mutex;
		d->finished = FALSE;
	}

	for (i = 0; i < NTHREADS; i++) {
		ThData *d = &(data[i]);
#ifdef DEBUG_PRINT
		g_print ("Running thread %d\n", d->th_id);
#endif
		d->thread = g_thread_create (func, d, TRUE, NULL);
	}

	/* wait a bit */
	usleep (100000);

	for (i = 0; i < NTHREADS; i++) {
		ThData *d = &(data[i]);
		if (!d->finished) {
			/* there are some threads locked somethere */
			exit (EXIT_FAILURE);
		}
		else
			g_thread_join (d->thread);
	}
	
	gda_mutex_free (mutex);

	return TRUE;
}

/*
 * Creation and destruction test
 */
static gboolean
test1 (GError **error)
{
	GdaMutex *mutex;

	mutex = gda_mutex_new ();
	gda_mutex_free (mutex);

	return TRUE;
}

/*
 * Twice locking
 */
gpointer
test2_start_thread (ThData *data)
{
	gda_mutex_lock (data->mutex);
#ifdef DEBUG_PRINT
	g_print ("Mutex locked for %d\n", data->th_id);
#endif
	g_thread_yield ();
	gda_mutex_lock (data->mutex);
#ifdef DEBUG_PRINT
	g_print ("Mutex re-locked for %d\n", data->th_id);
#endif
	gda_mutex_unlock (data->mutex);
#ifdef DEBUG_PRINT
	g_print ("Mutex unlocked for %d\n", data->th_id);
	g_print ("Mutex unlocked for %d\n", data->th_id);
#endif
	gda_mutex_unlock (data->mutex);
	data->finished = TRUE;
	return NULL;
}

static gboolean
test2 (GError **error)
{
	return test_multiple_threads ((GThreadFunc) test2_start_thread, error);
}

/*
 * Try lock
 */
gpointer
test3_start_thread (ThData *data)
{
	gboolean tried = FALSE;
	while (!tried) {
		tried = gda_mutex_trylock (data->mutex);
#ifdef DEBUG_PRINT
		g_print ("TRY for %d: %s\n", data->th_id, tried ? "got it" : "no luck");
#endif
		g_thread_yield ();
	}
#ifdef DEBUG_PRINT
	g_print ("Mutex locked for %d\n", data->th_id);
#endif
	g_thread_yield ();
	gda_mutex_lock (data->mutex);
#ifdef DEBUG_PRINT
	g_print ("Mutex re-locked for %d\n", data->th_id);
#endif
	gda_mutex_unlock (data->mutex);
#ifdef DEBUG_PRINT
	g_print ("Mutex unlocked for %d\n", data->th_id);
	g_print ("Mutex unlocked for %d\n", data->th_id);
#endif
	gda_mutex_unlock (data->mutex);
	data->finished = TRUE;
	return NULL;
}

static gboolean
test3 (GError **error)
{
	return test_multiple_threads ((GThreadFunc) test3_start_thread, error);
}
