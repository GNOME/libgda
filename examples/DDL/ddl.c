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
#include <sql-parser/gda-sql-parser.h>

GdaConnection *open_connection (void);
void display_products_contents (GdaConnection *cnc);
void create_table (GdaConnection *cnc);

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
	display_products_contents (cnc);
	closeresults = gda_connection_close (cnc,&error);

	if (!closeresults) {
		g_print ("connection_close reported error: %s",
				 error && error->message ? error->message : "No default");
		g_object_unref (cnc);
		exit (-1);
	}
	g_object_unref (cnc);
	return 0;
}

	/*
* Open a connection to the example.db file
*/
GdaConnection*
open_connection (void)
{
	GdaConnection *cnc;
	GError *error = NULL;
	cnc = gda_connection_open_from_string ("SQLite", "DB_DIR=.;DB_NAME=ddl_db", NULL,
										   GDA_CONNECTION_OPTIONS_NONE,
										   &error);
	if (!cnc) {
		g_print ("Could not open connection to SQLite database in example_db.db file: %s\n",
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
	op = gda_server_provider_create_operation (provider, cnc, GDA_SERVER_OPERATION_CREATE_TABLE, NULL, &error);
	if (!op) {
		g_print ("CREATE TABLE operation is not supported by the provider: %s\n",
				 error && error->message ? error->message : "No detail");
		exit (1);
	}

	/* Set parameter's values */
	/* table name */
	if (!gda_server_operation_set_value_at (op, "products", &error, "/TABLE_DEF_P/TABLE_NAME"))
		goto on_set_error;

	/* "id' field */
	i = 0;
	if (!gda_server_operation_set_value_at (op, "id", &error, "/FIELDS_A/@COLUMN_NAME/%d", i))
		goto on_set_error;
	if (!gda_server_operation_set_value_at (op, "integer", &error, "/FIELDS_A/@COLUMN_TYPE/%d", i))
		goto on_set_error;
	if (!gda_server_operation_set_value_at (op, "TRUE", &error, "/FIELDS_A/@COLUMN_AUTOINC/%d", i))
		goto on_set_error;
	if (!gda_server_operation_set_value_at (op, "TRUE", &error, "/FIELDS_A/@COLUMN_PKEY/%d", i))
		goto on_set_error;

	/* 'product_name' field */
	i++;
	if (!gda_server_operation_set_value_at (op, "product_name", &error, "/FIELDS_A/@COLUMN_NAME/%d", i))
		goto on_set_error;
	if (!gda_server_operation_set_value_at (op, "varchar", &error, "/FIELDS_A/@COLUMN_TYPE/%d", i))
		goto on_set_error;
	if (!gda_server_operation_set_value_at (op, "50", &error, "/FIELDS_A/@COLUMN_SIZE/%d", i))
		goto on_set_error;
	if (!gda_server_operation_set_value_at (op, "TRUE", &error, "/FIELDS_A/@COLUMN_NNUL/%d", i))
		goto on_set_error;

/* Show the SQL to execute
* This does not always work since some operations may not be accessible through SQL
*/
	sql = gda_server_provider_render_operation (provider, cnc, op, &error);
	if (!sql) {
		g_print ("Error rendering SQL: %s\n",
				 error && error->message ? error->message : "No detail");
		exit (1);
	}
	g_print ("SQL to execute: %s\n", sql);
	g_free (sql);

/* Actually execute the operation */
	if (! gda_server_provider_perform_operation (provider, cnc, op, &error)) {
		g_print ("Error executing the operation: %s\n",
			 error && error->message ? error->message : "No detail");
		exit (1);
	}
	g_object_unref (op);
	return;

on_set_error:
	g_print ("Error setting value in GdaSererOperation: %s\n",
		 error && error->message ? error->message : "No detail");
	exit (1);
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
