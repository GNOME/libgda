/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2012 Murray Cumming <murrayc@murrayc.com>
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

static const gchar* TABLE_NAME = "customers";

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

int
main (int argc, char *argv[])
{
	GdaConnection* connection;
	GdaDataModel* data;
	GError* error = NULL;
	GValue *value;

	gda_init();
	error = NULL;

	/* open connection to the SalesTest data source */
	connection = gda_connection_open_from_dsn ("SalesTest", NULL, GDA_CONNECTION_OPTIONS_NONE, &error);
	if (!connection) {
		fprintf (stderr, "%s\n", error->message);
		return -1;
	}

	/* Initial meta store, to show it has no information about the TABLE_NAME table */
	g_print ("Initial metastore state\n");
	g_value_set_string ((value = gda_value_new (G_TYPE_STRING)), TABLE_NAME);
	data = gda_connection_get_meta_store_data (connection, GDA_CONNECTION_META_FIELDS, &error, 1, 
						   "name", value);
	if (!data)
		return -1;
	list_table_columns (data);
	g_object_unref (data);

	/* Request partial update for the table we are interested in 
	 * the GdaMetaContext specifies to update the "_tables" table, where the "table_name"
	 * attribute is TABLE_NAME */
	g_print ("\nPartial metastore update for table '%s'...\n", TABLE_NAME);
	GdaMetaContext mcontext = {"_tables", 1, NULL, NULL};
	mcontext.column_names = g_new (gchar *, 1);
	mcontext.column_names[0] = "table_name";
	mcontext.column_values = g_new (GValue *, 1);
	g_value_set_string ((mcontext.column_values[0] = gda_value_new (G_TYPE_STRING)), TABLE_NAME);
	if (!gda_connection_update_meta_store (connection, &mcontext, &error))
		return -1;

	/* Query the same information about the TABLE_NAME table's columns
	 * this time there should be some information */
	data = gda_connection_get_meta_store_data (connection, GDA_CONNECTION_META_FIELDS, &error, 1,
						   "name", value);
	if (!data)
		return -1;
	list_table_columns (data);
	g_object_unref (data);

	/*
	 * Request partial update for all the tables in the database
	 */
	g_print ("\nPartial metastore update for all the tables...\n");
	GdaMetaContext mcontext2 = {"_tables", 0, NULL, NULL};
	if (!gda_connection_update_meta_store (connection, &mcontext2, &error))
		return -1;

	/*
	 * Get columns of the 'products' table
	 */
	gda_value_free (value);
	g_value_set_string ((value = gda_value_new (G_TYPE_STRING)), "products");
	data = gda_connection_get_meta_store_data (connection, GDA_CONNECTION_META_FIELDS, &error, 1,
						   "name", value);
	if (!data)
		return -1;
	list_table_columns (data);
	g_object_unref (data);

	return 0;
}
