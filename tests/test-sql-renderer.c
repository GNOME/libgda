/*
 * Copyright (C) 2013 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2017 Daniel Espinosa <esodan@gmail.com>
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
#include <string.h>

static gboolean
string_equal_to_template (const gchar *str, const gchar *tmpl)
{
	const gchar *ptrs, *ptrt;
	for (ptrs = str; *ptrs != '('; ptrs++);

	for (ptrt = tmpl;
	     *ptrs && *ptrt;
	     ptrs++, ptrt++) {
		if (*ptrt == '@')
			continue;
		if (*ptrs != *ptrt)
			break;
	}
	if (*ptrs || *ptrt)
		return FALSE;
	else
		return TRUE;
}

GDateTime* create_timestamp (void) {
	GTimeZone *tz = g_time_zone_new_identifier ("+02");
	return g_date_time_new (tz, 2013, 8, 28, 17, 10, 23.0);
}


/*
 * @prov may be NULL
 */
static guint
do_a_test (GdaServerProvider *prov, GdaSqlParser *parser)
{
	guint nfailed = 0;
	GdaStatement *stmt;
	GdaSet *params;
	gchar *sql;
	GError *error = NULL;
	GdaTime* gt = gda_time_new_from_values (16, 9, 22, 0, -3600*3);

	/* SQL parsed as an INSERT statement */
	sql = "INSERT INTO tstest VALUES (##ts::timestamp, ##time::time)";
	stmt = gda_sql_parser_parse_string (parser, sql, NULL, &error);
	if (!stmt) {
		g_print ("Failed to parse [%s]: %s\n", sql, error && error->message ? error->message : "No detail");
		g_clear_error (&error);
		nfailed ++;
		goto endtest;
	}

	g_assert (gda_statement_get_statement_type (stmt) == GDA_SQL_STATEMENT_INSERT);
	if (! gda_statement_get_parameters (stmt, &params, &error)) {
		g_print ("Failed to obtain parameters: %s\n", error && error->message ? error->message : "No detail");
		g_clear_error (&error);
		g_object_unref (stmt);
		nfailed ++;
		goto endtest;
	}
	GDateTime *ts = create_timestamp ();
	if (! gda_set_set_holder_value (params, &error, "ts", ts)) {
		g_print ("Failed to bind 'ts' parameter: %s\n", error && error->message ? error->message : "No detail");
		g_clear_error (&error);
		g_object_unref (stmt);
		g_object_unref (params);
		nfailed ++;
		goto endtest;
	}

	if (! gda_set_set_holder_value (params, &error, "time", gt)) {
		g_print ("Failed to bind 'time' parameter: %s\n", error && error->message ? error->message : "No detail");
		g_clear_error (&error);
		g_object_unref (stmt);
		g_object_unref (params);
		nfailed ++;
		goto endtest;
	}

	GdaConnection *cnc = NULL;
	if (prov) {
		cnc = gda_connection_new_from_string (gda_server_provider_get_name (prov), "DB_NAME=dummy;HOST=dummy", NULL,
						      GDA_CONNECTION_OPTIONS_NONE, &error);
		if (!cnc) {
			g_print ("Failed to create GdaConnection object: %s\n",
				 error && error->message ? error->message : "No detail");
			g_clear_error (&error);
			g_object_unref (stmt);
			g_object_unref (params);
			nfailed ++;
			goto endtest;
		}
	}
	gchar *expected;
	expected = "('@@@@@@@@@@T17:10:23+02', '19:09:22+00')";
	if (cnc)
		sql = gda_connection_statement_to_sql (cnc, stmt, params, 0, NULL, &error);
	else
		sql = gda_statement_to_sql_extended (stmt, NULL, params, 0, NULL, &error);
	if (!sql) {
		g_print ("Failed to render as SQL: %s\n", error && error->message ? error->message : "No detail");
		g_clear_error (&error);
		g_object_unref (stmt);
		g_object_unref (params);
		nfailed ++;
		goto endtest;
	}
	if (!string_equal_to_template (sql, expected)) {
		g_print ("Wrong rendered SQL: [%s] instead of [%s]\n", sql, expected);
		g_object_unref (stmt);
		g_object_unref (params);
		g_free (sql);
		nfailed ++;
		g_assert_not_reached ();
		goto endtest;
	}
	g_free (sql);

	expected = "('@@@@@@@@@@T15:10:23+00', '19:09:22+00')";
	if (cnc)
		sql = gda_connection_statement_to_sql (cnc, stmt, params, GDA_STATEMENT_SQL_TIMEZONE_TO_GMT, NULL, &error);
	else
		sql = gda_statement_to_sql_extended (stmt, NULL, params, GDA_STATEMENT_SQL_TIMEZONE_TO_GMT, NULL, &error);
	if (!sql) {
		g_print ("Failed to render as SQL: %s\n", error && error->message ? error->message : "No detail");
		g_clear_error (&error);
		g_object_unref (stmt);
		g_object_unref (params);
		nfailed ++;
		g_assert_not_reached ();
		goto endtest;
	}
	if (!string_equal_to_template (sql, expected)) {
		g_print ("Wrong rendered SQL for GMT timezone: [%s] instead of [%s]\n", sql, expected);
		g_object_unref (stmt);
		g_object_unref (params);
		g_free (sql);
		nfailed ++;
		g_assert_not_reached ();
		goto endtest;
	}
	g_free (sql);

	/* SQL not parsed as a valid statement */
	sql = "AAAA (##ts::timestamp, ##time::time)";
	stmt = gda_sql_parser_parse_string (parser, sql, NULL, &error);
	if (!stmt) {
		g_print ("Failed to parse [%s]: %s\n", sql, error && error->message ? error->message : "No detail");
		g_clear_error (&error);
		nfailed ++;
		goto endtest;
	}

	g_assert (gda_statement_get_statement_type (stmt) == GDA_SQL_STATEMENT_UNKNOWN);
	if (! gda_statement_get_parameters (stmt, &params, &error)) {
		g_print ("Failed to obtain parameters: %s\n", error && error->message ? error->message : "No detail");
		g_clear_error (&error);
		g_object_unref (stmt);
		nfailed ++;
		goto endtest;
	}
	if (! gda_set_set_holder_value (params, &error, "ts", ts)) {
		g_print ("Failed to bind 'ts' parameter: %s\n", error && error->message ? error->message : "No detail");
		g_clear_error (&error);
		g_object_unref (stmt);
		g_object_unref (params);
		nfailed ++;
		goto endtest;
	}
	g_date_time_unref (ts);

	if (! gda_set_set_holder_value (params, &error, "time", gt)) {
		g_print ("Failed to bind 'time' parameter: %s\n", error && error->message ? error->message : "No detail");
		g_clear_error (&error);
		g_object_unref (stmt);
		g_object_unref (params);
		nfailed ++;
		goto endtest;
	}

	expected = "('@@@@@@@@@@T17:10:23+02', '19:09:22+00')";
	if (cnc)
		sql = gda_connection_statement_to_sql (cnc, stmt, params, 0, NULL, &error);
	else
		sql = gda_statement_to_sql_extended (stmt, NULL, params, 0, NULL, &error);
	if (!sql) {
		g_print ("Failed to render as SQL: %s\n", error && error->message ? error->message : "No detail");
		g_clear_error (&error);
		g_object_unref (stmt);
		g_object_unref (params);
		nfailed ++;
		goto endtest;
	}
	if (!string_equal_to_template (sql, expected)) {
		g_print ("Wrong rendered SQL: [%s] instead of [%s]\n", sql, expected);
		g_object_unref (stmt);
		g_object_unref (params);
		g_free (sql);
		nfailed ++;
		g_assert_not_reached ();
		goto endtest;
	}
	g_free (sql);

	expected = "('@@@@@@@@@@T15:10:23+00', '19:09:22+00')";
	if (cnc)
		sql = gda_connection_statement_to_sql (cnc, stmt, params, GDA_STATEMENT_SQL_TIMEZONE_TO_GMT, NULL, &error);
	else
		sql = gda_statement_to_sql_extended (stmt, NULL, params, GDA_STATEMENT_SQL_TIMEZONE_TO_GMT, NULL, &error);
	if (!sql) {
		g_print ("Failed to render as SQL: %s\n", error && error->message ? error->message : "No detail");
		g_clear_error (&error);
		g_object_unref (stmt);
		g_object_unref (params);
		nfailed ++;
		goto endtest;
	}
	if (!string_equal_to_template (sql, expected)) {
		g_print ("Wrong rendered SQL for GMT timezone: [%s] instead of [%s]\n", sql, expected);
		g_object_unref (stmt);
		g_object_unref (params);
		g_free (sql);
		nfailed ++;
		g_assert_not_reached ();
		goto endtest;
	}
	g_free (sql);

 endtest:
	gda_time_free (gt);
	return nfailed;
}

int
main (G_GNUC_UNUSED int argc, G_GNUC_UNUSED char** argv)
{
	GdaDataModel *model;
	guint i, nrows;
	guint nfailed = 0;
	gda_init ();

	/* generic parser */
	GdaSqlParser *parser;
	parser = gda_sql_parser_new ();
	nfailed += do_a_test (NULL, parser);
	g_object_unref (parser);

	/* test other parsers only if generic one is Ok */
	if (nfailed == 0) {
		model = gda_config_list_providers ();
		nrows = gda_data_model_get_n_rows (model);
		for (i = 0; i < nrows; i++) {
			const GValue *cvalue;
			cvalue = gda_data_model_get_value_at (model, 0, i, NULL);
			g_assert (cvalue && (G_VALUE_TYPE (cvalue) == G_TYPE_STRING));
			if (!g_ascii_strcasecmp (g_value_get_string (cvalue), "Oracle"))
				continue; /* ignore Oracle for now */
			g_print ("Testing database provider '%s'\n", g_value_get_string (cvalue));

			GdaServerProvider *prov;
			prov = gda_config_get_provider (g_value_get_string (cvalue), NULL);
			g_assert (prov);

			GdaSqlParser *parser;
			parser = gda_server_provider_create_parser (prov, NULL);
			if (!parser)
				parser = gda_sql_parser_new ();

			nfailed += do_a_test (prov, parser);
			g_object_unref (parser);
		}
		g_object_unref (model);
	}

	if (nfailed == 0) {
		g_print ("Ok\n");
		return EXIT_SUCCESS;
	}
	else {
		g_print ("%u failed\n", nfailed);
		return EXIT_FAILURE;
	}
}
