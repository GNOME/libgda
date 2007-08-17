#include <libgda/libgda.h>
#include "common.h"

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
        GdaCommand *command;
        GError *error = NULL;
        gint nrows;

        command = gda_command_new (sql, GDA_COMMAND_TYPE_SQL, 0);
        nrows = gda_connection_execute_non_select_command (cnc, command, NULL, &error);
        gda_command_free (command);
        if (nrows == -1)
                g_error ("NON SELECT error: %s\n", error && error->message ? error->message : "no detail");
}
