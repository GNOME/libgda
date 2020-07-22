/*
 * Copyright (C) 2007 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2015 Gergely Polonkai <gergely@polonkai.eu>
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

GdaConnection *open_connection (void);
void display_products_contents (GdaConnection *cnc);
void create_table (GdaConnection *cnc);
void add_product_reccord(GdaConnection *cnc);

int
main (int argc, char *argv[])
{
        gda_init ();

        GdaConnection *cnc;
	GError *error = NULL;
	gboolean closeresults = FALSE;

	/* open connections */
	cnc = open_connection ();
	create_table (cnc);
	add_product_reccord (cnc);
	display_products_contents (cnc);
	closeresults = gda_connection_close (cnc,&error);

	if (!closeresults) {
		g_print("Connection close reported an error: %s\n",
				error && error->message ? error->message : "No detail");
		g_object_unref (cnc);
		exit (-1);
	}

        return 0;
}

/*
 * Open a connection to the example.db file
 */
GdaConnection *
open_connection (void)
{
        GdaConnection *cnc;
        GError *error = NULL;
        cnc = gda_connection_open_from_string ("SQLite", "DB_DIR=.;DB_NAME=ddl_db", NULL,
					       GDA_CONNECTION_OPTIONS_NONE,
					       &error);
        if (!cnc) {
                g_print ("Could not open connection to SQLite database in ddl_db.db file: %s\n",
                         error && error->message ? error->message : "No detail");
                exit (1);
        }
        return cnc;
}

/*
 * Create a "products" table using a GdaServerOperation object
 */
void
create_table (GdaConnection *cnc)
{
	GError *error = NULL;
	GdaServerProvider *provider;
	GdaServerOperation *op;
	gchar *sql;
	gint i;

	/* create a new GdaServerOperation object */
	provider = gda_connection_get_provider (cnc);
	GdaDbTable *products = gda_db_table_new ();
	gda_db_base_set_name (GDA_DB_BASE (products), "products");

	GdaDbColumn *id = gda_db_column_new();
	gda_db_column_set_name (id, "id");
	gda_db_column_set_type (id, G_TYPE_INT);
	gda_db_column_set_autoinc (id, TRUE);
	gda_db_column_set_pkey (id, TRUE);

	gda_db_table_append_column (products, id);
	g_object_unref (id);

	GdaDbColumn *product_name = gda_db_column_new();
	gda_db_column_set_name (product_name, "product_name");
	gda_db_column_set_type (product_name, G_TYPE_STRING);
	gda_db_column_set_size (product_name, 50);
	gda_db_column_set_nnul (product_name, TRUE);

	gda_db_table_append_column (products, product_name);
	g_object_unref (product_name);

	/* Actually execute the operation */
	if (!gda_ddl_modifiable_create (GDA_DDL_MODIFIABLE (products), cnc, NULL, &error)) {
		g_print ("Error executing the operation: %s\n",
			 error && error->message ? error->message : "No detail");
		exit (1);
	}
	g_object_unref (products);
}


/*
 * display the contents of the 'products' table
 */
void
display_products_contents (GdaConnection *cnc)
{
	GdaDataModel *data_model;
	GdaStatement *stmt;
	gchar *sql = "SELECT * FROM products";
	GError *error = NULL;
	GdaSqlParser *parser;

	parser = gda_connection_create_parser (cnc);
        if (!parser) /* @cnc does not provide its own parser => use default one */
                parser = gda_sql_parser_new ();

	stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
	data_model = gda_connection_statement_execute_select (cnc, stmt, NULL, &error);
	g_object_unref (stmt);
        if (!data_model)
                g_error ("Could not get the contents of the 'products' table: %s\n",
                         error && error->message ? error->message : "No detail");
	gda_data_model_dump (data_model, stdout);
	g_object_unref (data_model);
}

void
add_product_reccord (GdaConnection *cnc)
{
	GError *error = NULL;
	GValue *name = gda_value_new (G_TYPE_STRING);
	g_value_set_string (name, "John Smith");

	if (!gda_connection_insert_row_into_table (
		cnc, "products", NULL, "product_name", name, NULL)) {
		g_print ("Error insering data to the column \"products\": %s",
			 error && error->message ? error->message : "No Default");
		exit (1);
	}
}
