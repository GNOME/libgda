/*
 * Copyright (C) 2010 - 2012 Vivien Malerba <malerba@gnome-db.org>
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
#include <sql-parser/gda-sql-parser.h>
#include <string.h>

typedef struct {
	gchar *sql;
	gchar *rsql_remove;
	gchar *rsql_default;
} ATest;

ATest tests[] = {
	{"INSERT INTO mytable (name) VALUES (##name::string)",
	 "INSERT INTO mytable DEFAULT VALUES",
	 "INSERT INTO mytable (name) VALUES (DEFAULT)"},
	{"INSERT INTO mytable (id, name) VALUES (23, ##name::string)",
	 "INSERT INTO mytable (id) VALUES (23)",
	 "INSERT INTO mytable (id, name) VALUES (23, DEFAULT)"},
	{"UPDATE mytable set id=23, name=##name::string where 1",
	 NULL,
	 "UPDATE mytable SET id=23, name=DEFAULT WHERE 1"},
	{"INSERT INTO mytable (id, name) VALUES (23, ##name::string), (44, 'joe')",
	 NULL,
	 "INSERT INTO mytable (id, name) VALUES (23, DEFAULT), (44, 'joe')"
	},
	{"INSERT INTO mytable (id, name) VALUES (23, ##name::string), (##id::int, 'joe')",
	 NULL,
	 "INSERT INTO mytable (id, name) VALUES (23, DEFAULT), (##id::int, 'joe')"
	},
	{"INSERT INTO mytable (id, name) VALUES (23, ##name::string), (##id::int, ##name::string)",
	 NULL,
	 "INSERT INTO mytable (id, name) VALUES (23, DEFAULT), (##id::int, DEFAULT)"
	}
};

static gboolean
do_test (ATest *test, GdaStatement *stmt, GdaSet *params, gboolean remove)
{
	gboolean retval = TRUE;
	GdaSqlStatement *sqlst;
	GError* error = NULL;

	sqlst = gda_statement_rewrite_for_default_values (stmt, params, remove, &error);
	if (!sqlst) {
		gchar *exp;
		exp = remove ? test->rsql_remove : test->rsql_default;
		if (exp) {
			g_print ("Can't rewrite stmt with %s option: %s\n", remove ? "REMOVE" : "DEFAULT keyword",
				 error && error->message ? error->message : "No detail");
			g_clear_error (&error);
			retval = FALSE;
		}
	}
	else {
		GdaStatement *rstmt;
		gchar *sql;
		rstmt = GDA_STATEMENT (g_object_new (GDA_TYPE_STATEMENT, "structure", sqlst, NULL));

		sql = gda_statement_to_sql (rstmt, NULL, &error);
		if (sql) {
			gchar *exp;
			exp = remove ? test->rsql_remove : test->rsql_default;
			if (g_strcmp0 (sql, exp)) {
				g_print ("Error:\n  exp: [%s]\n  got: [%s]\n", exp, sql);
				retval = FALSE;
			}
			g_free (sql);
		}
		else {
			g_print ("Rendering error: %s\n", error && error->message ? error->message : "No detail");
			g_clear_error (&error);
			retval = FALSE;
		}
		
		g_object_unref (rstmt);
		gda_sql_statement_free (sqlst);
	}
	return retval;
}

int main()
{
	gda_init();
	GdaSqlParser *parser;
	GdaStatement *stmt;
	GdaSet *params;
	GdaHolder *h;
	GValue *value;
	guint i;
	gint n_errors = 0;
	
	parser = gda_sql_parser_new ();

	for (i = 0; i < sizeof (tests) / sizeof (ATest); i++) {
		ATest *test = &tests[i];

		stmt = gda_sql_parser_parse_string (parser, test->sql, NULL, NULL);
		g_assert (stmt);
		g_assert (gda_statement_get_parameters (stmt, &params, NULL));

		h = gda_set_get_holder (params, "name");
		g_value_set_int ((value = gda_value_new (G_TYPE_INT)), 5);
		gda_holder_set_default_value (h, value);
		gda_value_free (value);
		g_assert (gda_holder_set_value_to_default (h));
		
		if (! do_test (test, stmt, params, TRUE))
			n_errors++;
		if (! do_test (test, stmt, params, FALSE))
			n_errors++;
		g_object_unref (params);
		g_object_unref (stmt);
	}

	g_object_unref (parser);

	if (n_errors == 0)
		g_print ("Ok: %d tests passed\n", i*2);
	else
		g_print ("Failed: %d tests total, %d failed\n", i*2, n_errors);
	return n_errors ? 1 : 0;
}
