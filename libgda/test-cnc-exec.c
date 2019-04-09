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

#include <libgda/libgda.h>
#include <glib/gstdio.h>
#include <glib-object.h>

static gboolean connected = FALSE;
static guint test1 (void);
static guint test2 (void);


int main(G_GNUC_UNUSED int argc, G_GNUC_UNUSED const char *argv[])
{
	gda_init ();
	guint nfailed = 0;

	nfailed += test1 ();
	nfailed += test2 ();

	if (nfailed > 0)
		g_print ("Test failed %u times\n", nfailed);
	else
		g_print ("Test Ok\n");
	return nfailed ? 1 : 0;
}

static gboolean
idle_incr (guint *ptr)
{
	(*ptr)++;
	return TRUE;
}

static void
connection_opened (void) {
	connected = TRUE;
}

/*
 * Pass %0 as interval to use an idle function
 */
static void
setup_main_context (GdaConnection *cnc, guint interval, guint *ptr_to_incr)
{
	GMainContext *context;
	GSource *idle;

	context = g_main_context_new ();
	if (interval == 0)
		idle = g_idle_source_new ();
	else
		idle = g_timeout_source_new (interval);
	g_source_set_priority (idle, G_PRIORITY_DEFAULT);
	g_source_attach (idle, context);
	g_source_set_callback (idle, (GSourceFunc) idle_incr, ptr_to_incr, NULL);
	g_source_unref (idle);
	gda_connection_set_main_context (cnc, NULL, context);
	g_signal_connect(cnc, "opened", connection_opened, NULL);
	g_main_context_unref (context);
}


/*
 * Test 1: gda_connection_open() when there is no error
 */
static guint
test1 (void)
{
	g_print ("============= %s started =============\n", __FUNCTION__);
	GdaConnection *cnc;
	GError *error = NULL;

	gchar *cnc_string, *fname;
        fname = g_build_filename (ROOT_DIR, "data", NULL);
        cnc_string = g_strdup_printf ("DB_DIR=%s;DB_NAME=sales_test", fname);
        g_free (fname);
        cnc = gda_connection_new_from_string ("SQLite", cnc_string, NULL,
					      GDA_CONNECTION_OPTIONS_READ_ONLY, NULL);
	if (!cnc) {
		g_print ("gda_connection_new_from_string([%s]) failed: %s\n", cnc_string,
			 error && error->message ? error->message : "No detail");
		g_free (cnc_string);
		return 1;
	}

	guint counter = 0;
	setup_main_context (cnc, 0, &counter);

	/* connection open */
	if (! gda_connection_open (cnc, &error)) {
		g_print ("gda_connection_open([%s]) failed: %s\n", cnc_string,
			 error && error->message ? error->message : "No detail");
		g_free (cnc_string);
		return 1;
	}
	g_free (cnc_string);

	if (counter == 0 && !connected) {
		g_print ("gda_connection_open() failed: did not make GMainContext 'run'\n");
		return 1;
	}
	else
		g_print ("Counter incremented to %u\n", counter);
	g_print ("Connection %p Opened\n", cnc);
	

	/* SELECT */
	counter = 0;
	GdaDataModel *model;
	model = gda_connection_execute_select_command (cnc, "SELECT * FROM customers", &error);
	if (model) {
		gda_data_model_dump (model, NULL);
		gint expnrows = 5;
		if (gda_data_model_get_n_rows (model) != expnrows) {
			g_print ("SELECT Exec() failed: expected %d and got %d\n", expnrows,
				 gda_data_model_get_n_rows (model));
			g_object_unref (model);
			g_object_unref (cnc);
			return 1;
		}
		g_object_unref (model);
	}
	else {
		g_print ("gda_connection_execute_select_command() failed: %s\n",
			 error && error->message ? error->message : "No detail");
		g_object_unref (cnc);
		return 1;
	}

	if (counter == 0 && !connected) {
		g_print ("gda_connection_open() failed: did not make GMainContext 'run'\n");
		g_object_unref (cnc);
		return 1;
	}
	else
		g_print ("Counter incremented to %u\n", counter);


	g_object_unref (cnc);

	return 0;
}

/*
 * Test 2:
 */
static gpointer
test2_th (GdaConnection *cnc)
{
	/* setup main context */
	guint counter = 0;
	setup_main_context (cnc, 10, &counter);
#define LOCK_DELAY 50000
	gda_lockable_lock (GDA_LOCKABLE (cnc));
	/*g_print ("Locked by %p\n", g_thread_self());*/
	g_usleep (LOCK_DELAY);
	gda_lockable_unlock (GDA_LOCKABLE (cnc));
	g_thread_yield ();

	gda_lockable_lock (GDA_LOCKABLE (cnc));
	/*g_print ("Locked by %p\n", g_thread_self());*/
	g_usleep (LOCK_DELAY);
	gda_lockable_unlock (GDA_LOCKABLE (cnc));
	g_thread_yield ();

	gda_lockable_lock (GDA_LOCKABLE (cnc));
	/*g_print ("Locked by %p\n", g_thread_self());*/
	g_usleep (LOCK_DELAY);
	gda_lockable_unlock (GDA_LOCKABLE (cnc));
	g_thread_yield ();

	gda_lockable_lock (GDA_LOCKABLE (cnc));
	/*g_print ("Locked by %p\n", g_thread_self());*/
	g_usleep (LOCK_DELAY);
	gda_lockable_unlock (GDA_LOCKABLE (cnc));

	guint *ret;
	ret = g_new (guint, 1);
	*ret = counter;
	return ret;
}

static guint
test2 (void)
{
	g_print ("============= %s started =============\n", __FUNCTION__);
	GdaConnection *cnc;
	GError *error = NULL;

	gchar *cnc_string, *fname;
        fname = g_build_filename (ROOT_DIR, "data", NULL);
        cnc_string = g_strdup_printf ("DB_DIR=%s;DB_NAME=sales_test", fname);
        g_free (fname);
        cnc = gda_connection_new_from_string ("SQLite", cnc_string, NULL,
					      GDA_CONNECTION_OPTIONS_READ_ONLY, NULL);
	if (!cnc) {
		g_print ("gda_connection_new_from_string([%s]) failed: %s\n", cnc_string,
			 error && error->message ? error->message : "No detail");
		g_free (cnc_string);
		return 1;
	}

	/* connection open */
	if (! gda_connection_open (cnc, &error)) {
		g_print ("gda_connection_open([%s]) failed: %s\n", cnc_string,
			 error && error->message ? error->message : "No detail");
		g_free (cnc_string);
		return 1;
	}
	g_free (cnc_string);

#define NB_THREADS 3
	GThread *ths[NB_THREADS];
	guint i;
	for (i = 0; i < NB_THREADS; i++) {
		gchar *tmp;
		tmp = g_strdup_printf ("th%u", i);
		ths[i] = g_thread_new (tmp, (GThreadFunc) test2_th, cnc);
		g_free (tmp);
		g_print ("Thread %u is %p\n", i, ths[i]);
	}

	for (i = 0; i < NB_THREADS; i++) {
		guint *counter;
		counter = g_thread_join (ths[i]);
		if (*counter == 0 && !connected) {
			g_print ("Thread %u: gda_connection_lock() failed: did not make GMainContext 'run'\n", i);
			return 1;
		}
		else
			g_print ("Thread %u: Counter incremented to %u\n", i, *counter);
		g_free (counter);
	}

	g_object_unref (cnc);

	return 0;
}
