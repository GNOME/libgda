/*
 * Copyright (C) 2010 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
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
#include <virtual/libgda-virtual.h>
#include <sql-parser/gda-sql-parser.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* utility functions */

/* Commented out because it is not used.
static void          run_sql_non_select (GdaConnection *cnc, const gchar *sql);
*/

static GdaDataModel *run_sql_select (GdaConnection *cnc, const gchar *sql);
static gint          run_and_show_sql_select (GdaConnection *cnc, const gchar *sql, const gchar *title);
int
main ()
{
	GError *error = NULL;
	GdaConnection *hub, *cnc;
        GdaVirtualProvider *provider;

	gda_init ();

	/* open connection */
	cnc = gda_connection_open_from_dsn_name ("SalesTest", NULL,
					    GDA_CONNECTION_OPTIONS_READ_ONLY, &error);
	if (!cnc) {
		g_print ("Could not open connection to the SalesTest DSN: %s\n", 
			 error && error->message ? error->message : "No detail");
		exit (1);
	}

	/* Set up Connection hub */
        provider = gda_vprovider_hub_new ();
        hub = gda_virtual_connection_open (provider, GDA_CONNECTION_OPTIONS_NONE, NULL);
        g_assert (hub);

	if (!gda_vconnection_hub_add (GDA_VCONNECTION_HUB (hub), cnc, "sales", &error)) {
		g_print ("Could not add SalesTest connection to hub: %s\n", 
			 error && error->message ? error->message : "No detail");
		exit (1);
	}
	g_object_unref (cnc);
 
	/* some tests */	
	run_and_show_sql_select (hub, "SELECT * FROM sales.customers where id=3", NULL);
	run_and_show_sql_select (hub, "SELECT * FROM sales.customers where name='Lew Bonito'", NULL);
	run_and_show_sql_select (hub, "SELECT * FROM sales.customers where id >= 4 order by name DESC", NULL);
	run_and_show_sql_select (hub, "SELECT * FROM sales.customers where id >= 5 order by name DESC", NULL);
	/*
	run_and_show_sql_select (hub, "SELECT * FROM sales.customers where id=3 OR id=4", NULL);
	run_and_show_sql_select (hub, "SELECT * FROM sales.customers where id=3 AND id=4", NULL);
	run_and_show_sql_select (hub, "SELECT * FROM sales.customers where id=3 AND id=4", NULL);
	*/

	g_object_unref (hub);

	return 0;
}

static gint
run_and_show_sql_select (GdaConnection *cnc, const gchar *sql, const gchar *title)
{
	GdaDataModel *model;
	gint nrows;

	model = run_sql_select (cnc, sql);
	g_print ("\n====== %s ======\n", title ? title : sql);
	nrows = gda_data_model_get_n_rows (model);
	if (nrows != 0)
		gda_data_model_dump (model, stdout);
	else
		g_print ("None\n");
	g_object_unref (model);
	g_print ("\n");
	return nrows;
}

static GdaDataModel *
run_sql_select (GdaConnection *cnc, const gchar *sql)
{
	GdaStatement *stmt;
	GError *error = NULL;
	GdaDataModel *res;
	GdaSqlParser *parser;

	parser = gda_connection_create_parser (cnc);
	stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
	g_object_unref (parser);

        res = gda_connection_statement_execute_select (cnc, stmt, NULL, &error);
        g_object_unref (stmt);
	if (!res) 
		g_error ("Could not execute query: %s\n", 
			 error && error->message ? error->message : "no detail");
	return res;
}

/* Commented out because it is not used.
static void
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
*/
