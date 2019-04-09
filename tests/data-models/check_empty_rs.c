/*
 * Copyright (C) 2008 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2008 - 2013 Vivien Malerba <malerba@gnome-db.org>
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
#include <libgda/gda-server-provider-extra.h>
#include <stdlib.h>
#include <string.h>
#include "../test-errors.h"

static gboolean run_test (const gchar *sql, const gchar *empty_rs_serial, GError **error);

typedef struct {
	gchar *sql;
	gchar *empty_rs_serial;
} ATestData;

ATestData tests [] = {
	{"SELECT * FROM table WHERE id=3", "{\"statement\":{\"sql\":null,\"stmt_type\":\"SELECT\",\"contents\":{\"distinct\":\"false\",\"fields\":[{\"expr\":{\"value\":\"*\"}}],\"from\":{\"targets\":[{\"expr\":{\"value\":\"table\"},\"table_name\":\"table\"}]},\"where\":{\"operation\":{\"operator\":\"=\",\"operand0\":{\"value\":\"0\"},\"operand1\":{\"value\":\"1\"}}}}}}"},
	{"SELECT id, func1 (3, func2('rr'), ##aparam::string), NULL FROM a, b", "{\"statement\":{\"sql\":null,\"stmt_type\":\"SELECT\",\"contents\":{\"distinct\":\"false\",\"fields\":[{\"expr\":{\"value\":\"id\"},\"field_name\":\"id\"},{\"expr\":{\"func\":{\"function_name\":\"func1\",\"function_args\":[{\"value\":\"3\"},{\"func\":{\"function_name\":\"func2\",\"function_args\":[{\"value\":\"'rr'\"}]}},{\"value\":\"0\"}]}}},{\"expr\":{\"value\":null}}],\"from\":{\"targets\":[{\"expr\":{\"value\":\"a\"},\"table_name\":\"a\"},{\"expr\":{\"value\":\"b\"},\"table_name\":\"b\"}],\"joins\":[{\"join_type\":\"CROSS\",\"join_pos\":\"1\"}]},\"where\":{\"operation\":{\"operator\":\"=\",\"operand0\":{\"value\":\"0\"},\"operand1\":{\"value\":\"1\"}}}}}}"},
	{"SELECT ##p1 + ##p2", "{\"statement\":{\"sql\":null,\"stmt_type\":\"SELECT\",\"contents\":{\"distinct\":\"false\",\"fields\":[{\"expr\":{\"operation\":{\"operator\":\"+\",\"operand0\":{\"value\":\"0\"},\"operand1\":{\"value\":\"0\"}}}}],\"where\":{\"operation\":{\"operator\":\"=\",\"operand0\":{\"value\":\"0\"},\"operand1\":{\"value\":\"1\"}}}}}}"},
	{NULL, NULL}
};

int 
main (G_GNUC_UNUSED int argc, G_GNUC_UNUSED char **argv)
{
	GError *error = NULL;	
	gint i, nfailed;

	gda_init ();
	nfailed = 0;
	for (i = 0; ; i++) {
		ATestData *td = &(tests[i]);
		if (!td->sql)
			break;
		if (!run_test (td->sql, td->empty_rs_serial, &error)) {
			g_print ("Test %d failed: %s\n", i, 
				 error && error->message ? error->message : "No detail");
			nfailed++;
			if (error)
				g_error_free (error);
			error = NULL;
		}
	}

	if (nfailed == 0)
		g_print ("Ok.\n");
	return nfailed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

static gboolean
run_test (const gchar *sql, const gchar *empty_rs_serial, GError **error)
{
	static GdaSqlParser *parser = NULL;
	GdaStatement *orig, *trans;
	gboolean retval = FALSE;
	gchar *tsql;

	if (!parser)
		parser = gda_sql_parser_new ();
	orig = gda_sql_parser_parse_string (parser, sql, NULL, error);
	if (!orig)
		return FALSE;

	trans = gda_select_alter_select_for_empty (orig, error);
	if (!trans) 
		goto out;
	
	tsql = gda_statement_to_sql_extended (trans, NULL, NULL, 0, NULL, error);
	if (!tsql)
		goto out;
	g_print ("SQL: %s\nTRA: %s\n", sql, tsql);
	g_free (tsql);

	tsql = gda_statement_serialize (trans);
	if (!empty_rs_serial)
		g_print ("Missing test data!\n  SQL: %s\n  SER: %s\n", sql, tsql);
	else if (strcmp (tsql, empty_rs_serial)) {
		g_print ("Test failed!\n  SQL: %s\n  EXP: %s\n  GOT: %s\n", sql, empty_rs_serial, tsql);
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", 
			     "Failed serialized comparison");
		g_free (tsql);
		goto out;
	}

	retval = TRUE;
 out:

	if (trans)
		g_object_unref (trans);
	g_object_unref (orig);
	return retval;
}
