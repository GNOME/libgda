/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
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

GdaSqlParser *parser;

GdaConnection *open_connection (void);
void display_customers (GdaConnection *cnc);
void run_sql_non_select (GdaConnection *cnc, const gchar *sql);

int
main (int argc, char *argv[])
{
        gda_init ();

        GdaConnection *cnc;
	GError *error = NULL;
	GdaStatement *stmt;
	GdaDataModel *model;
	gchar *str;
	GValue *name;

	/* open connection */
	cnc = open_connection ();

	/* begin transaction */
	if (! gda_connection_begin_transaction (cnc, NULL, GDA_TRANSACTION_ISOLATION_UNKNOWN,
						&error)) {
		g_print ("Could not begin transaction: %s\n",
                         error && error->message ? error->message : "No detail");
                exit (1);
	}
	
	/* execute SELECT */
	stmt = gda_sql_parser_parse_string (parser, "SELECT id, name FROM customers ORDER BY id", NULL, NULL);
	g_assert (stmt);
	model = gda_connection_statement_execute_select (cnc, stmt, NULL, &error);
	g_object_unref (stmt);
	if (!model) {
		g_print ("Could not execute SELECT statement: %s\n",
                         error && error->message ? error->message : "No detail");
                exit (1);
	}

	g_print ("** Data model is:\n");
	gda_data_model_dump (model, stdout);

	/*
	 * make sure the mete data is up to date
	 */
	g_print ("Computing meta data, this may take a while; in real applications, this should be cached to avoid waiting\n");
	if (! gda_connection_update_meta_store (cnc, NULL, &error)) {
		g_print ("Could not fetch meta data: %s\n",
                         error && error->message ? error->message : "No detail");
                exit (1);
	}

	/*
	 * Make the data model compute the modification statements which
	 * will actually be executed when the data model is modified
	 */
	if (! gda_data_select_compute_modification_statements (GDA_DATA_SELECT (model), &error)) {
		g_print ("Could not compute modification statements: %s\n",
                         error && error->message ? error->message : "No detail");
                exit (1);
	}

	g_object_get (G_OBJECT (model), "update-stmt", &stmt, NULL);
	str = gda_statement_to_sql (stmt, NULL, NULL);
	g_print ("Computed UPDATE: %s\n", str);
	g_free (str);
	g_object_unref (stmt);
	g_object_get (G_OBJECT (model), "delete-stmt", &stmt, NULL);
	str = gda_statement_to_sql (stmt, NULL, NULL);
	g_print ("Computed DELETE: %s\n", str);
	g_free (str);
	g_object_unref (stmt);
	g_object_get (G_OBJECT (model), "insert-stmt", &stmt, NULL);
	str = gda_statement_to_sql (stmt, NULL, NULL);
	g_print ("Computed INSERT: %s\n", str);
	g_free (str);
	g_object_unref (stmt);

	/*
	 * remove row 0 (1st row)
	 */
	g_print ("\n\n** Removing row 0\n");
	if (! gda_data_model_remove_row (model, 0, &error)) {
		g_print ("Could not remove row 0: %s\n",
                         error && error->message ? error->message : "No detail");
                exit (1);
	}
	g_print ("** Data model is now:\n");
	gda_data_model_dump (model, stdout);
	g_print ("** Table's contents is now:\n");
	display_customers (cnc);

	/*
	 * add a row: the row's values is a list of GValue pointers 
	 * (or NULL pointers where the default value should be inserted
	 */
	GList *list;
	g_print ("\n\n** Adding a row\n");
	list = g_list_append (NULL, NULL); 
	g_value_set_string ((name = gda_value_new (G_TYPE_STRING)), "Hiro");
	list = g_list_append (list, name);
	if (gda_data_model_append_values (model, list, &error) == -1) {
		g_print ("Could not add a row: %s\n",
                         error && error->message ? error->message : "No detail");
                exit (1);
	}
	gda_value_free (name);
	g_list_free (list);
	g_print ("** Data model is now:\n");
	gda_data_model_dump (model, stdout);
	g_print ("** Table's contents is now:\n");
	display_customers (cnc);

	/*
	 * alter row 2
	 */
	g_print ("\n\n** Modifying row 2\n");
	g_value_set_string ((name = gda_value_new (G_TYPE_STRING)), "Tom");
	if (! gda_data_model_set_value_at (model, 1, 2, name, &error)) {
		g_print ("Could not modify row 2: %s\n",
                         error && error->message ? error->message : "No detail");
                exit (1);
	}
	gda_value_free (name);
	g_print ("** Data model is now:\n");
	gda_data_model_dump (model, stdout);
	g_print ("** Table's contents is now:\n");
	display_customers (cnc);

	/* rollback transaction */
	gda_connection_rollback_transaction (cnc, NULL, NULL);

        gda_connection_close (cnc);

        return 0;
}

/*
 * Open a connection to the example.db file
 */
GdaConnection *
open_connection ()
{
        GdaConnection *cnc;
        GError *error = NULL;

	/* open connection */
        cnc = gda_connection_open_from_dsn ("SalesTest", NULL,
					    GDA_CONNECTION_OPTIONS_NONE,
					    &error);
        if (!cnc) {
                g_print ("Could not open connection: %s\n",
                         error && error->message ? error->message : "No detail");
                exit (1);
        }

	/* create an SQL parser */
	parser = gda_connection_create_parser (cnc);
	if (!parser) /* @cnc does not provide its own parser => use default one */
		parser = gda_sql_parser_new ();
	/* attach the parser object to the connection */
	g_object_set_data_full (G_OBJECT (cnc), "parser", parser, g_object_unref);

        return cnc;
}

/* 
 * display the contents of the 'products' table 
 */
void
display_customers (GdaConnection *cnc)
{
	GdaDataModel *data_model;
	GdaSqlParser *parser;
	GdaStatement *stmt;
	gchar *sql = "SELECT id, name FROM customers ORDER BY id";
	GError *error = NULL;

	parser = g_object_get_data (G_OBJECT (cnc), "parser");
	stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
	data_model = gda_connection_statement_execute_select (cnc, stmt, NULL, &error);
	g_object_unref (stmt);
        if (!data_model) 
                g_error ("Could not get the contents of the 'products' table: %s\n",
                         error && error->message ? error->message : "No detail");
	gda_data_model_dump (data_model, stdout);
	g_object_unref (data_model);
}
