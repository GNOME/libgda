/*
 * Copyright (C) 2008 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2008 - 2014 Vivien Malerba <malerba@gnome-db.org>
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
#include "../test-errors.h"
#include <string.h>
#include <sql-parser/gda-sql-parser.h>
#include <glib/gstdio.h>

/*
 * Creates an SQLite .db file from the definitions in @sqlfile
 */
gboolean
create_sqlite_db (const gchar *dir, const gchar *dbname, const gchar *sqlfile, GError **error)
{
	GdaBatch *batch;
	GdaSqlParser *parser;
	GdaServerProvider *prov;
	GdaConnection *cnc;

	/* create batch */
	prov = gda_config_get_provider ("SQLite", NULL);
	if (!prov) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "Cannot find the SQLite provider");
		return FALSE;
	}
	parser = gda_server_provider_create_parser (prov, NULL);
	if (!parser)
		parser = gda_sql_parser_new ();
	
	batch = gda_sql_parser_parse_file_as_batch (parser, sqlfile, error);
	g_object_unref (parser);
	if (!batch)
		return FALSE;

	/* clean any previous DB file */
	gchar *fname, *tmp;
	tmp = g_strdup_printf ("%s.db", dbname);
	fname = g_build_filename (dir, tmp, NULL);
	g_free (tmp);
	g_unlink (fname);
	g_free (fname);

	/* open a connection */
	gchar *cnc_string;
	gchar *edir, *edbname;

	edir = gda_rfc1738_encode (dir);
	edbname = gda_rfc1738_encode (dbname);
	cnc_string = g_strdup_printf ("DB_DIR=%s;DB_NAME=%s", edir, edbname);
	g_free (edir);
	g_free (edbname);
	cnc = gda_connection_open_from_string ("SQLite", cnc_string, NULL, 
					       GDA_CONNECTION_OPTIONS_NONE, error);
	g_free (cnc_string);
	if (!cnc) {
		g_object_unref (batch);
		return FALSE;
	}

	/* execute batch */
	GSList *list;
	const GSList *stmt_list;
	gboolean retval = TRUE;
	list = gda_connection_batch_execute (cnc, batch, NULL, GDA_STATEMENT_MODEL_RANDOM_ACCESS, error);
	stmt_list = gda_batch_get_statements (batch);
	if (g_slist_length (list) != g_slist_length ((GSList *) stmt_list))
		retval = FALSE;

	g_slist_free_full (list, (GDestroyNotify) g_object_unref);

	g_assert (gda_connection_close (cnc, NULL));
	g_object_unref (cnc);
	g_object_unref (batch);
	return retval;
}

gboolean
run_sql_non_select (GdaConnection *cnc, const gchar *sql)
{
        GdaStatement *stmt;
        GError *error = NULL;
        gint nrows;
        GdaSqlParser *parser;

        parser = gda_connection_create_parser (cnc);
	if (!parser)
		parser = gda_sql_parser_new ();
        stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
        g_object_unref (parser);

        nrows = gda_connection_statement_execute_non_select (cnc, stmt, NULL, NULL, &error);
        g_object_unref (stmt);
        if (nrows == -1) {
                g_print ("NON SELECT error: %s\n", error && error->message ? error->message : "no detail");
		if (error)
			g_error_free (error);
		return FALSE;
	}
	return TRUE;
}

GdaDataModel *
run_sql_select (GdaConnection *cnc, const gchar *sql)
{
        GdaStatement *stmt;
        GError *error = NULL;
        GdaDataModel *res;
        GdaSqlParser *parser;

        parser = gda_connection_create_parser (cnc);
	if (!parser)
		parser = gda_sql_parser_new ();

        stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
        g_object_unref (parser);

        res = gda_connection_statement_execute_select (cnc, stmt, NULL, &error);
        g_object_unref (stmt);
        if (!res)
                g_print ("Could not execute query: %s\n",
                         error && error->message ? error->message : "no detail");
        return res;
}

GdaDataModel *
run_sql_select_cursor (GdaConnection *cnc, const gchar *sql)
{
	GdaStatement *stmt;
        GError *error = NULL;
        GdaDataModel *res;
        GdaSqlParser *parser;

        parser = gda_connection_create_parser (cnc);
        stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
        g_object_unref (parser);

        res = gda_connection_statement_execute_select_full (cnc, stmt, NULL, GDA_STATEMENT_MODEL_CURSOR_FORWARD, 
							    NULL, &error);
        g_object_unref (stmt);
        if (!res)
                g_print ("Could not execute query in cursor mode: %s\n",
                         error && error->message ? error->message : "no detail");
        return res;
}

gboolean
data_models_equal (GdaDataModel *m1, GdaDataModel *m2)
{
	GdaDataComparator *cmp;
	GError *error = NULL;
	cmp = (GdaDataComparator*) gda_data_comparator_new (m1, m2);
	if (! gda_data_comparator_compute_diff (cmp, &error)) {
		g_print ("Can't compare data models: %s\n", error && error->message ? error->message : "no detail");
		if (error)
			g_error_free (error);
		g_object_unref (cmp);
		return FALSE;
	}
	if (gda_data_comparator_get_n_diffs (cmp) != 0) {
		g_print ("Data models differ: %d differences\n", gda_data_comparator_get_n_diffs (cmp));
		g_object_unref (cmp);
		return FALSE;
	}
	g_object_unref (cmp);
	return TRUE;
}
