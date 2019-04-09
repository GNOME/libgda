/*
 * Copyright (C) 2008 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
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
#include "common.h"

#define NTHREADS 10
#define DBNAME "testdb"
#define DBDIR "."

#define DEBUG_PRINT
#undef DEBUG_PRINT

/* 
 * tests
 */
typedef gboolean (*TestFunc) (GError **);
static gboolean test1 (GError **error);

TestFunc tests[] = {
	test1,
};

int 
main (G_GNUC_UNUSED int argc, G_GNUC_UNUSED char** argv)
{
	gchar *fname;
	GError *error = NULL;

#if GLIB_CHECK_VERSION(2,36,0)
#else
	g_type_init ();
#endif
	gda_init ();

	/* set up the test database */
	fname = g_build_filename (ROOT_DIR, "tests", "multi-threading", "testdb.sql", NULL);
	if (!create_sqlite_db (DBDIR, DBNAME, fname, &error)) {
		g_print ("Cannot create test database: %s\n", error && error->message ? 
			 error->message : "no detail");
		return 1;
	}
	g_free (fname);

	guint failures = 0;
	guint j, ntests = 0;;
	for (j = 0; j < 500; j++) {
		guint i;
		
#ifdef DEBUG_PRINT
		g_print ("================================================== test %d\n", j);
#else
		g_print (".");
		fflush (stdout);
#endif
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

	g_print ("\nTESTS COUNT: %d\n", ntests);
	g_print ("FAILURES: %d\n", failures);

	return failures != 0 ? 1 : 0;
}

typedef struct _ThData {
	GMutex         start_lock;
	GThread       *thread;
	GdaConnection *cnc;
	gint           th_id;
	gboolean       error;
} ThData;

static gboolean
test_multiple_threads (GThreadFunc func, G_GNUC_UNUSED GError **error)
{
	ThData data[NTHREADS];
	gint i;
	GdaConnection *cnc = NULL;

	/* open cnc */
	gchar *cnc_string;
	gchar *edir, *edbname;
	
	edir = gda_rfc1738_encode (DBDIR);
	edbname = gda_rfc1738_encode (DBNAME);
	cnc_string = g_strdup_printf ("DB_DIR=%s;DB_NAME=%s", edir, edbname);
	g_free (edir);
	g_free (edbname);
	cnc = gda_connection_open_from_string ("SQLite", cnc_string, NULL, 
					       GDA_CONNECTION_OPTIONS_NONE, NULL);
	g_free (cnc_string);
	
	if (!cnc) 
		return FALSE;

	/* prepare threads data */
	for (i = 0; i < NTHREADS; i++) {
		ThData *d = &(data[i]);
    g_mutex_init (&(d->start_lock));
    g_mutex_lock (&(d->start_lock));
		d->thread = NULL;
		d->cnc = cnc;
		d->th_id = i;
		d->error = FALSE;
	}

	/* start all the threads, they will lock on d->start_lock */
	for (i = 0; i < NTHREADS; i++) {
		ThData *d = &(data[i]);

#ifdef DEBUG_PRINT
		g_print ("Running thread %d\n", d->th_id);
#endif
		d->thread = g_thread_new ("th", func, d);
#ifdef DEBUG_PRINT
		g_print ("Running thread %d has pointer %p\n", d->th_id, d->thread);
#endif
	}

	/* unlock all the threads */
	for (i = 0; i < NTHREADS; i++) {
		ThData *d = &(data[i]);
		g_mutex_unlock (&(d->start_lock));
	}

	gboolean retval = TRUE;
	for (i = 0; i < NTHREADS; i++) {
		ThData *d = &(data[i]);
		g_thread_join (d->thread);
		if (d->error)
			retval = FALSE;
	}
	for (i = 0; i < NTHREADS; i++) {
		ThData *d = &(data[i]);
    g_mutex_clear (&(d->start_lock));
	}

	g_object_unref (cnc);

	return retval;
}

/*
 * Run SELECT on the same connection
 */
gpointer
test1_start_thread (ThData *data)
{
	/* initially start locked */
	g_mutex_lock (&(data->start_lock));
	g_mutex_unlock (&(data->start_lock));

	/* threads use @cnc */
	if (gda_lockable_trylock ((GdaLockable*) data->cnc)) {
#ifdef DEBUG_PRINT
		g_print ("Th %d has tried to lock cnc and succeeded\n", data->th_id);
#endif
	}
	else {
#ifdef DEBUG_PRINT
		g_print ("Th %d has tried to lock cnc and failed, locking it (may block)...\n", data->th_id);
#endif
		gda_lockable_lock ((GdaLockable*) data->cnc);
	}
	gda_lockable_unlock ((GdaLockable*) data->cnc);
#ifdef DEBUG_PRINT
	g_print ("Th %d has unlocked cnc\n", data->th_id);
#endif

	gda_lockable_lock ((GdaLockable*) data->cnc);
#ifdef DEBUG_PRINT
	g_print ("Th %d has locked cnc\n", data->th_id);
#endif
		
	g_thread_yield ();
	
	gda_lockable_lock ((GdaLockable*) data->cnc);
#ifdef DEBUG_PRINT
	g_print ("Th %d has re-locked cnc\n", data->th_id);
#endif
	g_thread_yield ();
	
	gda_lockable_unlock ((GdaLockable*) data->cnc);
#ifdef DEBUG_PRINT
	g_print ("Th %d has unlocked cnc\n", data->th_id);
#endif
	g_thread_yield ();
	
	gda_lockable_unlock ((GdaLockable*) data->cnc);
#ifdef DEBUG_PRINT
	g_print ("Th %d has re-unlocked cnc\n", data->th_id);
#endif
	g_thread_yield ();
	
	
#ifdef DEBUG_PRINT
	g_print ("Th %d finished\n", data->th_id);
#endif
	return NULL;
}

static gboolean
test1 (GError **error)
{
	return test_multiple_threads ((GThreadFunc) test1_start_thread, error);
}
