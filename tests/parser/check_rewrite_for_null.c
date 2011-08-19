/*
 * Copyright (C) 2011 Vivien Malerba <malerba@gnome-db.org>
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
	gchar *id;
        gchar *sql;
	gchar *result;
} ATest;

ATest tests[] = {
        {"T1",
	 "select * from mytable where id = ##id::int::null AND name = ##name::string",
	 "{\"sql\":\"select * from mytable where id = ##id::int::null AND name = ##name::string\",\"stmt_type\":\"SELECT\",\"contents\":{\"distinct\":\"false\",\"fields\":[{\"expr\":{\"value\":\"*\"}}],\"from\":{\"targets\":[{\"expr\":{\"value\":\"mytable\"},\"table_name\":\"mytable\"}]},\"where\":{\"operation\":{\"operator\":\"AND\",\"operand0\":{\"operation\":{\"operator\":\"IS NULL\",\"operand0\":{\"value\":\"id\"}}},\"operand1\":{\"operation\":{\"operator\":\"=\",\"operand0\":{\"value\":\"name\"},\"operand1\":{\"value\":null,\"param_spec\":{\"name\":\"name\",\"descr\":null,\"type\":\"string\",\"is_param\":true,\"nullok\":false}}}}}}}}"},
        {"T2",
	 "select * from mytable where ##id::int::null = ##id::int::null AND name = ##name::string",
	 "{\"sql\":\"select * from mytable where ##id::int::null = ##id::int::null AND name = ##name::string\",\"stmt_type\":\"SELECT\",\"contents\":{\"distinct\":\"false\",\"fields\":[{\"expr\":{\"value\":\"*\"}}],\"from\":{\"targets\":[{\"expr\":{\"value\":\"mytable\"},\"table_name\":\"mytable\"}]},\"where\":{\"operation\":{\"operator\":\"AND\",\"operand0\":{\"operation\":{\"operator\":\"IS NULL\",\"operand0\":{\"value\":null,\"param_spec\":{\"name\":\"id\",\"descr\":null,\"type\":\"int\",\"is_param\":true,\"nullok\":true}}}},\"operand1\":{\"operation\":{\"operator\":\"=\",\"operand0\":{\"value\":\"name\"},\"operand1\":{\"value\":null,\"param_spec\":{\"name\":\"name\",\"descr\":null,\"type\":\"string\",\"is_param\":true,\"nullok\":false}}}}}}}}"},
        {"T3",
	 "select * from mytable where id!=##id::int::null AND name != ##name::string",
	 "{\"sql\":\"select * from mytable where id!=##id::int::null AND name != ##name::string\",\"stmt_type\":\"SELECT\",\"contents\":{\"distinct\":\"false\",\"fields\":[{\"expr\":{\"value\":\"*\"}}],\"from\":{\"targets\":[{\"expr\":{\"value\":\"mytable\"},\"table_name\":\"mytable\"}]},\"where\":{\"operation\":{\"operator\":\"AND\",\"operand0\":{\"operation\":{\"operator\":\"IS NOT NULL\",\"operand0\":{\"value\":\"id\"}}},\"operand1\":{\"operation\":{\"operator\":\"!=\",\"operand0\":{\"value\":\"name\"},\"operand1\":{\"value\":null,\"param_spec\":{\"name\":\"name\",\"descr\":null,\"type\":\"string\",\"is_param\":true,\"nullok\":false}}}}}}}}"},
        {"T4",
	 "select * from mytable where ##id::int::null != ##id::int::null AND name = ##name::string",
	 "{\"sql\":\"select * from mytable where ##id::int::null != ##id::int::null AND name = ##name::string\",\"stmt_type\":\"SELECT\",\"contents\":{\"distinct\":\"false\",\"fields\":[{\"expr\":{\"value\":\"*\"}}],\"from\":{\"targets\":[{\"expr\":{\"value\":\"mytable\"},\"table_name\":\"mytable\"}]},\"where\":{\"operation\":{\"operator\":\"AND\",\"operand0\":{\"operation\":{\"operator\":\"IS NOT NULL\",\"operand0\":{\"value\":null,\"param_spec\":{\"name\":\"id\",\"descr\":null,\"type\":\"int\",\"is_param\":true,\"nullok\":true}}}},\"operand1\":{\"operation\":{\"operator\":\"=\",\"operand0\":{\"value\":\"name\"},\"operand1\":{\"value\":null,\"param_spec\":{\"name\":\"name\",\"descr\":null,\"type\":\"string\",\"is_param\":true,\"nullok\":false}}}}}}}}"},
        {"T5",
	 "pragme mine = ##id::int::null or name = ##name::string",
	 "{\"sql\":\"pragme mine = ##id::int::null or name = ##name::string\",\"stmt_type\":\"UNKNOWN\",\"contents\":[{\"value\":\"pragme\"},{\"value\":\" \"},{\"value\":\"mine\"},{\"value\":\" \"},{\"value\":\" IS NULL\"},{\"value\":\" \"},{\"value\":\"or\"},{\"value\":\" \"},{\"value\":\"name\"},{\"value\":\" \"},{\"value\":\"=\"},{\"value\":\" \"},{\"value\":null,\"param_spec\":{\"name\":\"name\",\"descr\":null,\"type\":\"string\",\"is_param\":true,\"nullok\":false}}]}"},
        {"T6",
	 "pragme mine != ##id::int::null or name =##id::string::null AND col=##name::string",
	 "{\"sql\":\"pragme mine != ##id::int::null or name =##id::string::null AND col=##name::string\",\"stmt_type\":\"UNKNOWN\",\"contents\":[{\"value\":\"pragme\"},{\"value\":\" \"},{\"value\":\"mine\"},{\"value\":\" \"},{\"value\":\" IS NOT NULL\"},{\"value\":\" \"},{\"value\":\"or\"},{\"value\":\" \"},{\"value\":\"name\"},{\"value\":\" \"},{\"value\":\" IS NULL\"},{\"value\":\" \"},{\"value\":\"AND\"},{\"value\":\" \"},{\"value\":\"col=\"},{\"value\":null,\"param_spec\":{\"name\":\"name\",\"descr\":null,\"type\":\"string\",\"is_param\":true,\"nullok\":false}}]}"},
	{"T7",
	 "UPDATE mytable set id=##id::int::null, name=##name::string",
	 "{\"sql\":\"UPDATE mytable set id=##id::int::null, name=##name::string\",\"stmt_type\":\"UPDATE\",\"contents\":{\"table\":\"mytable\",\"fields\":[\"id\",\"name\"],\"expressions\":[{\"value\":null,\"param_spec\":{\"name\":\"id\",\"descr\":null,\"type\":\"int\",\"is_param\":true,\"nullok\":true}},{\"value\":null,\"param_spec\":{\"name\":\"name\",\"descr\":null,\"type\":\"string\",\"is_param\":true,\"nullok\":false}}]}}"},
};

static gboolean
do_test (ATest *test)
{
	GdaSqlParser *parser;
	GdaStatement *stmt;
	GError *error = NULL;
	gchar *tmp;

	g_print ("** test %s\n", test->id);
	parser = gda_sql_parser_new ();

	GdaSet *params;
	GValue *nv;
	GdaHolder *holder;
	stmt = gda_sql_parser_parse_string (parser, test->sql, NULL, &error);
	g_object_unref (parser);
	if (!stmt) {
		g_print ("Parsing error: %s\n", error && error->message ? error->message : "No detail");
		return FALSE;
	}

	if (! gda_statement_get_parameters (stmt, &params, &error)) {
		g_print ("Error: %s\n", error && error->message ? error->message : "No detail");
		return FALSE;
	}
	g_assert (gda_set_set_holder_value (params, NULL, "name", "zzz"));
	nv = gda_value_new_null ();
	holder = gda_set_get_holder (params, "id");
	g_assert (gda_holder_set_value (holder, nv, NULL));
	gda_value_free (nv);

	GdaSqlStatement *sqlst;
	g_object_get (stmt, "structure", &sqlst, NULL);

	sqlst = gda_rewrite_sql_statement_for_null_parameters (sqlst, params, NULL, &error);
	if (!sqlst) {
		g_print ("Rewrite error: %s\n", error && error->message ? error->message : "No detail");
		return FALSE;
	}
	g_object_set (stmt, "structure", sqlst, NULL);

	/* SQL rendering */
	tmp = gda_statement_to_sql_extended (stmt, NULL, NULL, GDA_STATEMENT_SQL_PARAMS_SHORT,
					     NULL, &error);
	if (!tmp) {
		g_print ("Rendering error: %s\n", error && error->message ? error->message : "No detail");
		return FALSE;
	}
	/*g_print ("SQL after mod: [%s]\n", tmp);*/
	g_free (tmp);

	tmp = gda_sql_statement_serialize (sqlst);
	if (!tmp) {
		g_print ("Error: gda_sql_statement_serialize() failed\n");
		return FALSE;
	}
	else if (strcmp (test->result, tmp)) {
		g_print ("Exp: [%s]\nGot: [%s]\n", test->result, tmp);
		return FALSE;
	}

	g_free (tmp);
	gda_sql_statement_free (sqlst);

	g_object_unref (stmt);
	return TRUE;
}

int main()
{
	gda_init();
	guint i;
	gint n_errors = 0;

	for (i = 0; i < sizeof (tests) / sizeof (ATest); i++) {
		ATest *test = &tests[i];

		if (! do_test (test))
			n_errors++;
	}

	if (n_errors == 0)
		g_print ("Ok: %d tests passed\n", i);
	else
		g_print ("Failed: %d tests total, %d failed\n", i, n_errors);
	return n_errors ? 1 : 0;
}
