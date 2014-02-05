/*
 * Copyright (C) 2013 Vivien Malerba <malerba@gnome-db.org>
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

static guint test1 (void);


int main(int argc, const char *argv[])
{
	gda_init ();
	guint nfailed = 0;

	nfailed += test1 ();

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
setup_main_context (GdaConnection *cnc, guint *ptr_to_incr)
{
	GMainContext *context;
	GSource *idle;

	context = g_main_context_new ();
	idle = g_idle_source_new ();
	g_source_set_priority (idle, G_PRIORITY_DEFAULT);
	g_source_attach (idle, context);
	g_source_set_callback (idle, (GSourceFunc) idle_incr, ptr_to_incr, NULL);
	g_source_unref (idle);
	gda_connection_set_main_context (cnc, context);
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
	setup_main_context (cnc, &counter);

	/* connection open */
	if (! gda_connection_open (cnc, &error)) {
		g_print ("gda_connection_open([%s]) failed: %s\n", cnc_string,
			 error && error->message ? error->message : "No detail");
		g_free (cnc_string);
		return 1;
	}
	g_free (cnc_string);

	if (counter == 0) {
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

	if (counter == 0) {
		g_print ("gda_connection_open() failed: did not make GMainContext 'run'\n");
		g_object_unref (cnc);
		return 1;
	}
	else
		g_print ("Counter incremented to %u\n", counter);


	g_object_unref (cnc);

	return 0;
}
