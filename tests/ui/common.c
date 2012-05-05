/*
 * Copyright (C) 2012 Vivien Malerba <malerba@gnome-db.org>
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
#include "common.h"
#include <sql-parser/gda-sql-parser.h>

GdaConnection *
ui_tests_common_open_connection (void)
{
	GdaConnection *cnc;
	gchar *cnc_string, *fname;
	GError *error = NULL;
	fname = g_build_filename (ROOT_DIR, "data", NULL);
	cnc_string = g_strdup_printf ("DB_DIR=%s;DB_NAME=sales_test", fname);
	g_free (fname);
	cnc = gda_connection_open_from_string ("SQLite", cnc_string, NULL,
					       GDA_CONNECTION_OPTIONS_READ_ONLY, &error);
	if (!cnc) {
		g_print ("Failed to open connection, cnc_string = [%s]: %s\n", cnc_string,
			 error && error->message ? error->message : "No detail");
		exit (1);
	}
	g_free (cnc_string);
	return cnc;
}

static GdaStatement *
stmt_from_string (const gchar *sql)
{
        GdaStatement *stmt;
        GError *error = NULL;

        static GdaSqlParser *parser = NULL;
        if (!parser)
                parser = gda_sql_parser_new ();

        stmt = gda_sql_parser_parse_string (parser, sql, NULL, &error);
        if (!stmt) {
                g_print ("Cound not parse SQL: %s\nSQL was: %s\n",
                         error && error->message ? error->message : "No detail",
                         sql);
                exit (EXIT_FAILURE);
        }
        return stmt;
}

GdaDataModel *
ui_tests_common_get_products (GdaConnection *cnc, gboolean part_only)
{
	GError *error = NULL;
        GdaDataModel *model;
	GdaStatement *stmt;

	if (part_only)
		stmt = stmt_from_string ("SELECT * FROM products WHERE price > 20.2 ORDER BY ref, category LIMIT 10");
	else
		stmt = stmt_from_string ("SELECT * FROM products ORDER BY ref, category LIMIT 15");
        model = gda_connection_statement_execute_select (cnc, stmt, NULL, &error);
	if (!model) {
		g_print ("Failed to get list of products: %s\n",
			 error && error->message ? error->message : "No detail");
		exit (1);
	}

	return model;
}

GdaDataModel *
ui_tests_common_get_products_2cols (GdaConnection *cnc, gboolean part_only)
{
	GError *error = NULL;
        GdaDataModel *model;
	GdaStatement *stmt;

	if (part_only)
		stmt = stmt_from_string ("SELECT ref, category FROM products WHERE price > 20.2 ORDER BY ref, category  LIMIT 10");
	else
		stmt = stmt_from_string ("SELECT ref, category FROM products ORDER BY ref, category LIMIT 15");
        model = gda_connection_statement_execute_select (cnc, stmt, NULL, &error);
	if (!model) {
		g_print ("Failed to get list of products: %s\n",
			 error && error->message ? error->message : "No detail");
		exit (1);
	}

	return model;
}
