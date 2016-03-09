/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
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
#include <libgda/thread-wrapper/gda-thread-wrapper.h>

/*
 * Display the contents of the 'data' data model
 */
static void
list_table_columns (GdaDataModel* data)
{
	gint nrows;

	nrows = gda_data_model_get_n_rows (data);
	if (nrows == 0)
		g_print ("No column...\n");
	else {
		gint i;
		const GValue* col_name;
		const GValue* col_type;

		g_print ("Tables' columns:\n");
		for (i = 0; i < nrows; ++ i) {
			col_name = gda_data_model_get_value_at (data, 0, i, NULL);
			g_assert (col_name);
			
			col_type = gda_data_model_get_value_at (data, 2, i, NULL);
			g_assert (col_type);
			
			printf("  %s: %s\n", g_value_get_string (col_name), g_value_get_string (col_type));
		}
	}
}

/* this function is executed in a sub thread */
gboolean *
sub_thread_update_meta_store (GdaConnection *cnc, GError **error)
{
	gboolean *result;
	result = g_new (gboolean, 1);
	*result = gda_connection_update_meta_store (cnc, NULL, error);
	return result;
}

int
main (int argc, char *argv[])
{
	GdaConnection* connection;
	GdaDataModel* data;
	GError* error = NULL;
	GValue *value;

	gda_init();
	error = NULL;

	/* open connection to the SalesTest data source, using the GDA_CONNECTION_OPTIONS_THREAD_SAFE flag
	 * to make sure we can use the same connection from multiple threads at once */
	connection = gda_connection_open_from_dsn ("SalesTest", NULL, GDA_CONNECTION_OPTIONS_THREAD_SAFE, &error);
	if (!connection) {
		fprintf (stderr, "%s\n", error->message);
		return -1;
	}

	/* update meta data */
	GdaThreadWrapper *wrapper;
	guint job_id;

	g_print ("Requesting a meta data update in the background.\n");
	wrapper = gda_thread_wrapper_new ();
	job_id = gda_thread_wrapper_execute (wrapper, (GdaThreadWrapperFunc) sub_thread_update_meta_store,
					     connection, NULL, &error);
	if (job_id == 0) {
		fprintf (stderr, "Can't use thread wrapper: %s\n", error->message);
		return -1;
	}

	while (1) {
		gboolean *result;
		g_print ("Doing some business here...\n");
		g_usleep (100000);
		result = (gboolean *) gda_thread_wrapper_fetch_result (wrapper, FALSE, job_id, &error);
		if (result) {
			gboolean meta_ok;

			g_object_unref (wrapper);
			meta_ok = *result;
			g_free (result);
			if (!meta_ok) {
				fprintf (stderr, "Could not update meta data: %s\n", error->message);
				return -1;
			}
			g_print ("Meta data has been updated!\n");
			break;
		}
	}

	/*
	 * Get columns of the 'products' table
	 */
	g_value_set_string ((value = gda_value_new (G_TYPE_STRING)), "products");
	data = gda_connection_get_meta_store_data (connection, GDA_CONNECTION_META_FIELDS, &error, 1,
						   "name", value);
	if (!data)
		return -1;
	list_table_columns (data);
	g_object_unref (data);

	return 0;
}
