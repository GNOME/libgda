#include <libgda/libgda.h>
#include "common.h"
#include <sql-parser/gda-sql-parser.h>

GdaConnection *
open_source_connection (GdaClient *client)
{
	GdaConnection *cnc;
	GError *error = NULL;
	cnc = gda_client_open_connection (client, "SalesTest", NULL, NULL,
                                          GDA_CONNECTION_OPTIONS_DONT_SHARE,
                                          &error);
        if (!cnc) {
                g_print ("Could not open connection to DSN 'SalesTest': %s\n",
                         error && error->message ? error->message : "No detail");
                exit (1);
        }
	return cnc;
}

GdaConnection *
open_destination_connection (GdaClient *client)
{
	/* create connection */
	GdaConnection *cnc;
	GError *error = NULL;
	cnc = gda_client_open_connection_from_string (client, "SQLite", "DB_DIR=.;DB_NAME=copy",
						      NULL, NULL,
						      GDA_CONNECTION_OPTIONS_DONT_SHARE,
						      &error);
        if (!cnc) {
                g_print ("Could not open connection to local SQLite database: %s\n",
                         error && error->message ? error->message : "No detail");
                exit (1);
        }

        /* table "products_copied1" */
	run_sql_non_select (cnc, "DROP table IF EXISTS products_copied1");
	run_sql_non_select (cnc, "CREATE table products_copied1 (ref string not null primary key, "
			    "name string not null, price real, wh_stored integer REFERENCES warehouses(id))");

        /* table "products_copied2" */
	run_sql_non_select (cnc, "DROP table IF EXISTS products_copied2");
	run_sql_non_select (cnc, "CREATE table products_copied2 (ref string not null primary key, "
			    "name string not null, price real, wh_stored integer REFERENCES warehouses(id))");

        /* table "products_copied3" */
	run_sql_non_select (cnc, "DROP table IF EXISTS products_copied3");
	run_sql_non_select (cnc, "CREATE table products_copied3 (ref string not null primary key, "
			    "name string not null, price real, wh_stored integer REFERENCES warehouses(id))");

	return cnc;
}

void
run_sql_non_select (GdaConnection *cnc, const gchar *sql)
{
        GdaStatement *stmt;
        GError *error = NULL;
        gint nrows;
        GdaSqlParser *parser;

        parser = gda_connection_create_parser (cnc);
        stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
        g_object_unref (parser);

        nrows = gda_connection_statement_execute_non_select (cnc, stmt, NULL, NULL, &error);
        g_object_unref (stmt);
        if (nrows == -1)
                g_error ("NON SELECT error: %s\n", error && error->message ? error->message : "no detail");
}
