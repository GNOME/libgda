/*
 * Copyright (C) 2007 - 2012 Vivien Malerba <malerba@gnome-db.org>
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
#include "common.h"
#include <sql-parser/gda-sql-parser.h>

GdaConnection *
open_source_connection (void)
{
	GdaConnection *cnc;
	GError *error = NULL;
	cnc = gda_connection_open_from_dsn ("SalesTest", NULL,
					    GDA_CONNECTION_OPTIONS_NONE,
					    &error);
        if (!cnc) {
                g_print ("Could not open connection to DSN 'SalesTest': %s\n",
                         error && error->message ? error->message : "No detail");
                exit (1);
        }
	return cnc;
}

GdaConnection *
open_destination_connection (void)
{
	/* create connection */
	GdaConnection *cnc;
	GError *error = NULL;
	cnc = gda_connection_open_from_string ("SQLite", "DB_DIR=.;DB_NAME=copy",
					       NULL, 
					       GDA_CONNECTION_OPTIONS_NONE,
					       &error);
        if (!cnc) {
                g_print ("Could not open connection to local SQLite database: %s\n",
                         error && error->message ? error->message : "No detail");
                exit (1);
        }

        /* table "products_copied1" */
	run_sql_non_select (cnc, "DROP table IF EXISTS products_copied1");
	run_sql_non_select (cnc, "CREATE table products_copied1 (ref string not null primary key, "
			    "name string not null, price real, wh_stored integer)");

        /* table "products_copied2" */
	run_sql_non_select (cnc, "DROP table IF EXISTS products_copied2");
	run_sql_non_select (cnc, "CREATE table products_copied2 (ref string not null primary key, "
			    "name string not null, price real, wh_stored integer)");

        /* table "products_copied3" */
	run_sql_non_select (cnc, "DROP table IF EXISTS products_copied3");
	run_sql_non_select (cnc, "CREATE table products_copied3 (ref string not null primary key, "
			    "name string not null, price real, wh_stored integer)");

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
