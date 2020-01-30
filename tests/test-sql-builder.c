/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 - 2011 Murray Cumming <murrayc@murrayc.com>
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

#include <string.h>
#include <libgda/libgda.h>

typedef GdaSqlStatement *(*BuilderFunc) (void);

typedef struct {
	gchar *name;
	BuilderFunc build_func;
	gchar *expected_stmt;
} ATest;

static GdaSqlStatement *build0 (void);
static GdaSqlStatement *build1 (void);
static GdaSqlStatement *build2 (void);
static GdaSqlStatement *build3 (void);
static GdaSqlStatement *build4 (void);
static GdaSqlStatement *build5 (void);
static GdaSqlStatement *build6 (void);
static GdaSqlStatement *build7 (void);
static GdaSqlStatement *build8 (void);
static GdaSqlStatement *build9 (void);
static GdaSqlStatement *build10 (void);
static GdaSqlStatement *build11 (void);
static GdaSqlStatement *build12 (void);
static GdaSqlStatement *build13 (void);

static gboolean builder_test_target_id (void);

ATest tests[] = {
	{"build0", build0, "{\"sql\":null,\"stmt_type\":\"SELECT\",\"contents\":{\"distinct\":\"false\",\"fields\":[{\"expr\":{\"value\":\"*\",\"sqlident\":\"TRUE\"}}],\"from\":{\"targets\":[{\"expr\":{\"value\":\"mytable\",\"sqlident\":\"TRUE\"},\"table_name\":\"mytable\"}]}}}"},
	{"build1", build1, "{\"sql\":null,\"stmt_type\":\"SELECT\",\"contents\":{\"distinct\":\"false\",\"fields\":[{\"expr\":{\"value\":\"contents\",\"sqlident\":\"TRUE\"}},{\"expr\":{\"value\":\"descr\",\"sqlident\":\"TRUE\"}},{\"expr\":{\"value\":\"rank\",\"sqlident\":\"TRUE\"}},{\"expr\":{\"value\":\"name\",\"sqlident\":\"TRUE\"}}],\"from\":{\"targets\":[{\"expr\":{\"value\":\"mytable\",\"sqlident\":\"TRUE\"},\"table_name\":\"mytable\"}]},\"where\":{\"operation\":{\"operator\":\"AND\",\"operand0\":{\"operation\":{\"operator\":\"=\",\"operand0\":{\"value\":\"session\",\"sqlident\":\"TRUE\"},\"operand1\":{\"value\":null,\"param_spec\":{\"name\":\"session\",\"descr\":null,\"type\":\"string\",\"is_param\":true,\"nullok\":false}}}},\"operand1\":{\"operation\":{\"operator\":\"AND\",\"operand0\":{\"operation\":{\"operator\":\"=\",\"operand0\":{\"value\":\"type\",\"sqlident\":\"TRUE\"},\"operand1\":{\"value\":\"'TABLE'\"}}},\"operand1\":{\"operation\":{\"operator\":\"=\",\"operand0\":{\"value\":\"name\",\"sqlident\":\"TRUE\"},\"operand1\":{\"value\":\"'alf'\"}}}}}}}}}"},
	{"build2", build2, "{\"sql\":null,\"stmt_type\":\"INSERT\",\"contents\":{\"table\":\"mytable\",\"fields\":[\"session\",\"type\",\"name\",\"contents\",\"descr\"],\"values\":[[{\"value\":null,\"param_spec\":{\"name\":\"session\",\"descr\":null,\"type\":\"int\",\"is_param\":true,\"nullok\":false}},{\"value\":null,\"param_spec\":{\"name\":\"type\",\"descr\":null,\"type\":\"int\",\"is_param\":true,\"nullok\":false}},{\"value\":null,\"param_spec\":{\"name\":\"name\",\"descr\":null,\"type\":\"string\",\"is_param\":true,\"nullok\":true}},{\"value\":null,\"param_spec\":{\"name\":\"contents\",\"descr\":null,\"type\":\"string\",\"is_param\":true,\"nullok\":false}},{\"value\":null,\"param_spec\":{\"name\":\"descr\",\"descr\":null,\"type\":\"string\",\"is_param\":true,\"nullok\":true}}]]}}"},
	{"build3", build3, "{\"sql\":null,\"stmt_type\":\"UPDATE\",\"contents\":{\"table\":\"mytable\",\"fields\":[\"name\",\"contents\",\"descr\"],\"expressions\":[{\"value\":null,\"param_spec\":{\"name\":\"name\",\"descr\":null,\"type\":\"string\",\"is_param\":true,\"nullok\":true}},{\"value\":null,\"param_spec\":{\"name\":\"contents\",\"descr\":null,\"type\":\"string\",\"is_param\":true,\"nullok\":false}},{\"value\":null,\"param_spec\":{\"name\":\"descr\",\"descr\":null,\"type\":\"string\",\"is_param\":true,\"nullok\":true}}],\"condition\":{\"operation\":{\"operator\":\"=\",\"operand0\":{\"value\":\"id\",\"sqlident\":\"TRUE\"},\"operand1\":{\"value\":null,\"param_spec\":{\"name\":\"id\",\"descr\":null,\"type\":\"int\",\"is_param\":true,\"nullok\":false}}}}}}"},
	{"build4", build4, "{\"sql\":null,\"stmt_type\":\"SELECT\",\"contents\":{\"distinct\":\"false\",\"fields\":[{\"expr\":{\"value\":\"fav.*\",\"sqlident\":\"TRUE\"}}],\"from\":{\"targets\":[{\"expr\":{\"value\":\"mytable\",\"sqlident\":\"TRUE\"},\"table_name\":\"mytable\",\"as\":\"fav\"},{\"expr\":{\"value\":\"fav_orders\",\"sqlident\":\"TRUE\"},\"table_name\":\"fav_orders\",\"as\":\"o\"}],\"joins\":[{\"join_type\":\"LEFT\",\"join_pos\":\"1\",\"on_cond\":{\"operation\":{\"operator\":\"=\",\"operand0\":{\"value\":\"fav.id\",\"sqlident\":\"TRUE\"},\"operand1\":{\"value\":\"o.fav_id\",\"sqlident\":\"TRUE\"}}}}]},\"where\":{\"operation\":{\"operator\":\"AND\",\"operand0\":{\"operation\":{\"operator\":\"=\",\"operand0\":{\"value\":\"fav.sesion\",\"sqlident\":\"TRUE\"},\"operand1\":{\"value\":null,\"param_spec\":{\"name\":\"session\",\"descr\":null,\"type\":\"int\",\"is_param\":true,\"nullok\":false}}}},\"operand1\":{\"operation\":{\"operator\":\"=\",\"operand0\":{\"value\":\"o.order_key\",\"sqlident\":\"TRUE\"},\"operand1\":{\"value\":null,\"param_spec\":{\"name\":\"okey\",\"descr\":null,\"type\":\"int\",\"is_param\":true,\"nullok\":false}}}}}},\"order_by\":[{\"expr\":{\"value\":\"o.rank\",\"sqlident\":\"TRUE\"},\"sort\":\"ASC\"}]}}"},
	{"build5", build5, "{\"sql\":null,\"stmt_type\":\"SELECT\",\"contents\":{\"distinct\":\"false\",\"fields\":[{\"expr\":{\"value\":\"id\",\"sqlident\":\"TRUE\"}}],\"from\":{\"targets\":[{\"expr\":{\"value\":\"mytable\",\"sqlident\":\"TRUE\"},\"table_name\":\"mytable\"}]},\"where\":{\"operation\":{\"operator\":\"AND\",\"operand0\":{\"operation\":{\"operator\":\"=\",\"operand0\":{\"value\":\"sesion\",\"sqlident\":\"TRUE\"},\"operand1\":{\"value\":null,\"param_spec\":{\"name\":\"session\",\"descr\":null,\"type\":\"int\",\"is_param\":true,\"nullok\":false}}}},\"operand1\":{\"operation\":{\"operator\":\"=\",\"operand0\":{\"value\":\"contents\",\"sqlident\":\"TRUE\"},\"operand1\":{\"value\":null,\"param_spec\":{\"name\":\"contents\",\"descr\":null,\"type\":\"int\",\"is_param\":true,\"nullok\":false}}}}}}}}"},
	{"build6", build6, "{\"sql\":null,\"stmt_type\":\"SELECT\",\"contents\":{\"distinct\":\"false\",\"fields\":[{\"expr\":{\"value\":\"fav_id\",\"sqlident\":\"TRUE\"}},{\"expr\":{\"value\":\"rank\",\"sqlident\":\"TRUE\"}}],\"from\":{\"targets\":[{\"expr\":{\"value\":\"mytable\",\"sqlident\":\"TRUE\"},\"table_name\":\"mytable\"}]},\"where\":{\"operation\":{\"operator\":\"AND\",\"operand0\":{\"operation\":{\"operator\":\"=\",\"operand0\":{\"value\":\"order_key\",\"sqlident\":\"TRUE\"},\"operand1\":{\"value\":null,\"param_spec\":{\"name\":\"orderkey\",\"descr\":null,\"type\":\"int\",\"is_param\":true,\"nullok\":false}}}},\"operand1\":{\"operation\":{\"operator\":\"!=\",\"operand0\":{\"value\":\"fav_id\",\"sqlident\":\"TRUE\"},\"operand1\":{\"value\":null,\"param_spec\":{\"name\":\"id\",\"descr\":null,\"type\":\"int\",\"is_param\":true,\"nullok\":false}}}}}}}}"},
	{"build7", build7, "{\"sql\":null,\"stmt_type\":\"UPDATE\",\"contents\":{\"table\":\"mytable\",\"fields\":[\"rank\"],\"expressions\":[{\"value\":null,\"param_spec\":{\"name\":\"newrank\",\"descr\":null,\"type\":\"int\",\"is_param\":true,\"nullok\":false}}],\"condition\":{\"operation\":{\"operator\":\"AND\",\"operand0\":{\"operation\":{\"operator\":\"=\",\"operand0\":{\"value\":\"fav_id\",\"sqlident\":\"TRUE\"},\"operand1\":{\"value\":null,\"param_spec\":{\"name\":\"id\",\"descr\":null,\"type\":\"int\",\"is_param\":true,\"nullok\":false}}}},\"operand1\":{\"operation\":{\"operator\":\"=\",\"operand0\":{\"value\":\"order_key\",\"sqlident\":\"TRUE\"},\"operand1\":{\"value\":null,\"param_spec\":{\"name\":\"orderkey\",\"descr\":null,\"type\":\"int\",\"is_param\":true,\"nullok\":false}}}},\"operand2\":{\"operation\":{\"operator\":\"=\",\"operand0\":{\"value\":\"rank\",\"sqlident\":\"TRUE\"},\"operand1\":{\"value\":null,\"param_spec\":{\"name\":\"rank\",\"descr\":null,\"type\":\"int\",\"is_param\":true,\"nullok\":false}}}}}}}}"},
	{"build8", build8, "{\"sql\":null,\"stmt_type\":\"DELETE\",\"contents\":{\"table\":\"mytable\",\"condition\":{\"operation\":{\"operator\":\"=\",\"operand0\":{\"value\":\"id\",\"sqlident\":\"TRUE\"},\"operand1\":{\"value\":null,\"param_spec\":{\"name\":\"id\",\"descr\":null,\"type\":\"int\",\"is_param\":true,\"nullok\":false}}}}}}"},
	{"build9", build9, "{\"sql\":null,\"stmt_type\":\"INSERT\",\"contents\":{\"table\":\"mytable\",\"fields\":[\"session\",\"name\"],\"values\":[[{\"value\":\"NULL\"},{\"value\":\"NULL\"}]]}}"},
	{"build10", build10, "{\"sql\":null,\"stmt_type\":\"SELECT\",\"contents\":{\"distinct\":\"true\",\"fields\":[{\"expr\":{\"value\":\"fav_id\",\"sqlident\":\"TRUE\"}},{\"expr\":{\"value\":\"rank\",\"sqlident\":\"TRUE\"}}],\"from\":{\"targets\":[{\"expr\":{\"value\":\"mytable\",\"sqlident\":\"TRUE\"},\"table_name\":\"mytable\"}]},\"limit\":{\"value\":\"5\"}}}"},
	{"build11", build11, "{\"sql\":null,\"stmt_type\":\"SELECT\",\"contents\":{\"distinct\":\"true\",\"distinct_on\":{\"value\":\"rank\",\"sqlident\":\"TRUE\"},\"fields\":[{\"expr\":{\"value\":\"fav_id\",\"sqlident\":\"TRUE\"}},{\"expr\":{\"value\":\"rank\",\"sqlident\":\"TRUE\"}}],\"from\":{\"targets\":[{\"expr\":{\"value\":\"mytable\",\"sqlident\":\"TRUE\"},\"table_name\":\"mytable\"}]},\"limit\":{\"value\":\"5\"},\"offset\":{\"value\":\"2\"}}}"},
	{"build12", build12, "{\"sql\":null,\"stmt_type\":\"SELECT\",\"contents\":{\"distinct\":\"false\",\"fields\":[{\"expr\":{\"value\":\"store_name\",\"sqlident\":\"TRUE\"}},{\"expr\":{\"func\":{\"function_name\":\"sum\",\"function_args\":[{\"value\":\"sales\",\"sqlident\":\"TRUE\"}]}}}],\"from\":{\"targets\":[{\"expr\":{\"value\":\"stores\",\"sqlident\":\"TRUE\"},\"table_name\":\"stores\"}]},\"group_by\":[{\"value\":\"store_name\",\"sqlident\":\"TRUE\"}],\"having\":{\"operation\":{\"operator\":\">\",\"operand0\":{\"func\":{\"function_name\":\"sum\",\"function_args\":[{\"value\":\"sales\",\"sqlident\":\"TRUE\"}]}},\"operand1\":{\"value\":\"10\"}}}}}"},
	{"build13", build13, "{\"sql\":null,\"stmt_type\":\"SELECT\",\"contents\":{\"distinct\":\"false\",\"fields\":[{\"expr\":{\"value\":\"'A''string'\"}},{\"expr\":{\"value\":\"234\"}},{\"expr\":{\"value\":\"TRUE\"}},{\"expr\":{\"value\":\"123.456789\"}},{\"expr\":{\"value\":\"1972-05-27\"}},{\"expr\":{\"value\":\"abc'de\\\\\\\\fgh\"}}]}}"}
};

int
main (G_GNUC_UNUSED int argc, G_GNUC_UNUSED char** argv)
{
	gda_init ();
	guint i, nfailed = 0;
	for (i = 0; i < G_N_ELEMENTS (tests); i++) {
		ATest *test = &(tests [i]);
		GdaSqlStatement *stmt;
		gboolean fail = FALSE;


		g_print ("TEST is: %s\n", test->name);

		stmt = test->build_func ();
		if (!stmt) {
			g_print ("Builder function did not create GdaSqlStatement\n");
			fail = TRUE;
		}
		else {
			gchar *result;
			result = gda_sql_statement_serialize (stmt);
			if (!result) {
				g_print ("Could not serialize GdaSqlStatement\n");
				fail = TRUE;
			}
			else {
				if (strcmp (result, test->expected_stmt)) {
					g_print ("Failed:\n\tEXP: %s\n\tGOT: %s\n",
						 test->expected_stmt, result);
					fail = TRUE;
				}
				g_free (result);
			}
			GdaStatement *rstmt;
			rstmt = g_object_new (GDA_TYPE_STATEMENT, "structure", stmt, NULL);
			gchar *sql;
			GError *lerror = NULL;
			sql = gda_statement_to_sql (rstmt, NULL, &lerror);
			if (sql) {
				g_print ("==>%s\n", sql);
				g_free (sql);
			}
			else {
				g_print ("Can't get SQL: %s\n", lerror && lerror->message ?
					 lerror->message : "No detail");
				if (lerror)
					g_error_free (lerror);
			}
			g_object_unref (rstmt);
			gda_sql_statement_free (stmt);
		}

		if (fail)
			nfailed++;
	}

	if (! builder_test_target_id ())
		nfailed++;

	g_print ("%d tests executed, ", i);
	if (nfailed > 0)
		g_print ("%d failed\n", nfailed);
	else
		g_print ("Ok\n");
	return EXIT_SUCCESS;
}

/*
 * SELECT * FROM mytable
 */
static GdaSqlStatement *
build0 (void)
{
	GdaSqlBuilder *builder;
	GdaSqlStatement *stmt;

	builder = gda_sql_builder_new (GDA_SQL_STATEMENT_SELECT);
	gda_sql_builder_add_field_value_id (builder,
				   gda_sql_builder_add_id (builder, "*"), 0);
	gda_sql_builder_select_add_target_id (builder,
					   gda_sql_builder_add_id (builder, "mytable"), NULL);
	stmt = gda_sql_statement_copy (gda_sql_builder_get_sql_statement (builder));
	g_object_unref (builder);
	return stmt;
}

/*
 * SELECT contents, descr, rank, name FROM mytable WHERE (session = ##session::string) AND ((type = 'TABLE') AND (name = 'alf'))
 */
static GdaSqlStatement *
build1 (void)
{
	GdaSqlBuilder *builder;
	GdaSqlBuilderId op_ids [3];
	gint index = 0;
	GdaSqlStatement *stmt;

	memset (op_ids, 0, sizeof (GdaSqlBuilderId) * 3);

	builder = gda_sql_builder_new (GDA_SQL_STATEMENT_SELECT);
	gda_sql_builder_select_add_target_id (builder,
					   gda_sql_builder_add_id (builder, "mytable"), NULL);
	gda_sql_builder_add_field_value_id (builder,
				   gda_sql_builder_add_id (builder, "contents"), 0);
	gda_sql_builder_add_field_value_id (builder,
				   gda_sql_builder_add_id (builder, "descr"), 0);
	gda_sql_builder_add_field_value_id (builder,
				   gda_sql_builder_add_id (builder, "rank"), 0);
	gda_sql_builder_add_field_value_id (builder,
				   gda_sql_builder_add_id (builder, "name"), 0);
	GdaSqlBuilderId id_cond1 = gda_sql_builder_add_cond (builder, GDA_SQL_OPERATOR_TYPE_EQ,
						   gda_sql_builder_add_id (builder, "session"),
						   gda_sql_builder_add_param (builder, "session", G_TYPE_STRING, FALSE), 0);

	op_ids [index] = gda_sql_builder_add_cond (builder, GDA_SQL_OPERATOR_TYPE_EQ,
						   gda_sql_builder_add_id (builder, "type"),
						   gda_sql_builder_add_expr (builder, NULL, G_TYPE_STRING, "TABLE"), 0);
	index++;

	op_ids [index] = gda_sql_builder_add_cond (builder, GDA_SQL_OPERATOR_TYPE_EQ,
						   gda_sql_builder_add_id (builder, "name"),
						   gda_sql_builder_add_expr (builder, NULL, G_TYPE_STRING, "alf"), 0);
	index++;

	GdaSqlBuilderId id_cond2 = gda_sql_builder_add_cond_v (builder, GDA_SQL_OPERATOR_TYPE_AND, op_ids, index);

	gda_sql_builder_set_where (builder, gda_sql_builder_add_cond (builder, GDA_SQL_OPERATOR_TYPE_AND, id_cond1, id_cond2, 0));

	stmt = gda_sql_statement_copy (gda_sql_builder_get_sql_statement (builder));
	g_object_unref (builder);
	return stmt;
}

/*
 * INSERT INTO mytable (session, type, name, contents, descr) VALUES (##session::int, ##type::int,
 *                      ##name::string::null, ##contents::string, ##descr::string::null)
 */
static GdaSqlStatement *
build2 (void)
{
	GdaSqlBuilder *builder;
	GdaSqlStatement *stmt;

	builder = gda_sql_builder_new (GDA_SQL_STATEMENT_INSERT);
	gda_sql_builder_set_table (builder, "mytable");

	gda_sql_builder_add_field_value_id (builder,
				   gda_sql_builder_add_id (builder, "session"),
				   gda_sql_builder_add_param (builder, "session", G_TYPE_INT, FALSE));
	gda_sql_builder_add_field_value_id (builder,
				   gda_sql_builder_add_id (builder, "type"),
				   gda_sql_builder_add_param (builder, "type", G_TYPE_INT, FALSE));
	gda_sql_builder_add_field_value_id (builder,
				   gda_sql_builder_add_id (builder, "name"),
				   gda_sql_builder_add_param (builder, "name", G_TYPE_STRING, TRUE));
	gda_sql_builder_add_field_value_id (builder,
				   gda_sql_builder_add_id (builder, "contents"),
				   gda_sql_builder_add_param (builder, "contents", G_TYPE_STRING, FALSE));
	gda_sql_builder_add_field_value_id (builder,
				   gda_sql_builder_add_id (builder, "descr"),
				   gda_sql_builder_add_param (builder, "descr", G_TYPE_STRING, TRUE));

	stmt = gda_sql_statement_copy (gda_sql_builder_get_sql_statement (builder));
	g_object_unref (builder);
	return stmt;
}

/*
 * UPDATE mytable set name = ##name::string::null, contents = ##contents::string, descr = ##descr::string::null
 *                WHERE id = ##id::int
 */
static GdaSqlStatement *
build3 (void)
{
	GdaSqlBuilder *builder;
	GdaSqlStatement *stmt;

	builder = gda_sql_builder_new (GDA_SQL_STATEMENT_UPDATE);
	gda_sql_builder_set_table (builder, "mytable");

	gda_sql_builder_add_field_value_id (builder,
				   gda_sql_builder_add_id (builder, "name"),
				   gda_sql_builder_add_param (builder, "name", G_TYPE_STRING, TRUE));
	gda_sql_builder_add_field_value_id (builder,
				   gda_sql_builder_add_id (builder, "contents"),
				   gda_sql_builder_add_param (builder, "contents", G_TYPE_STRING, FALSE));
	gda_sql_builder_add_field_value_id (builder,
				   gda_sql_builder_add_id (builder, "descr"),
				   gda_sql_builder_add_param (builder, "descr", G_TYPE_STRING, TRUE));

	gda_sql_builder_set_where (builder,
				   gda_sql_builder_add_cond (builder, GDA_SQL_OPERATOR_TYPE_EQ,
							 gda_sql_builder_add_id (builder, "id"),
							 gda_sql_builder_add_param (builder, "id", G_TYPE_INT, FALSE),
							 0));

	stmt = gda_sql_statement_copy (gda_sql_builder_get_sql_statement (builder));
	g_object_unref (builder);
	return stmt;
}

/*
 * SELECT fav.* FROM mytable fav LEFT JOIN fav_orders o ON (fav.id=o.fav_id)
 *              WHERE fav.session=##session::int AND o.order_key=##okey::int
 *              ORDER BY o.rank
 */
static GdaSqlStatement *
build4 (void)
{
	GdaSqlBuilder *b;
	GdaSqlStatement *stmt;
	GdaSqlBuilderId t1, t2;

	b = gda_sql_builder_new (GDA_SQL_STATEMENT_SELECT);
	gda_sql_builder_add_field_value_id (b,
				   gda_sql_builder_add_id (b, "fav.*"), 0);
	t1 = gda_sql_builder_select_add_target_id (b,
						gda_sql_builder_add_id (b, "mytable"),
						"fav");
	t2 = gda_sql_builder_select_add_target_id (b,
						gda_sql_builder_add_id (b, "fav_orders"),
						"o");
	gda_sql_builder_select_join_targets (b, t1, t2, GDA_SQL_SELECT_JOIN_LEFT,
					     gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_EQ,
								   gda_sql_builder_add_id (b, "fav.id"),
								   gda_sql_builder_add_id (b, "o.fav_id"),
								   0));

	gda_sql_builder_set_where (b,
	    gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_AND,
		  gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_EQ,
					gda_sql_builder_add_id (b, "fav.sesion"),
					gda_sql_builder_add_param (b, "session", G_TYPE_INT, FALSE), 0),
		  gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_EQ,
					gda_sql_builder_add_id (b, "o.order_key"),
					gda_sql_builder_add_param (b, "okey", G_TYPE_INT, FALSE), 0), 0));

	gda_sql_builder_select_order_by (b,
					 gda_sql_builder_add_id (b, "o.rank"), TRUE, NULL);
	stmt = gda_sql_statement_copy (gda_sql_builder_get_sql_statement (b));
	g_object_unref (b);
	return stmt;
}

/*
 * SELECT id FROM mytable
 *              WHERE session=##session::int AND contents=##contents::string
 */
static GdaSqlStatement *
build5 (void)
{
	GdaSqlBuilder *b;
	GdaSqlStatement *stmt;

	b = gda_sql_builder_new (GDA_SQL_STATEMENT_SELECT);
	gda_sql_builder_add_field_value_id (b,
				   gda_sql_builder_add_id (b, "id"), 0);
	gda_sql_builder_select_add_target_id (b,
					   gda_sql_builder_add_id (b, "mytable"),
					   NULL);

	gda_sql_builder_set_where (b,
	    gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_AND,
		  gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_EQ,
					gda_sql_builder_add_id (b, "sesion"),
					gda_sql_builder_add_param (b, "session", G_TYPE_INT, FALSE), 0),
		  gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_EQ,
					gda_sql_builder_add_id (b, "contents"),
					gda_sql_builder_add_param (b, "contents", G_TYPE_INT, FALSE), 0), 0));

	stmt = gda_sql_statement_copy (gda_sql_builder_get_sql_statement (b));
	g_object_unref (b);
	return stmt;
}

/*
 * SELECT fav_id, rank FROM mytable
 *              WHERE order_key=##orderkey::int AND fav_id!=##id::int
 */
static GdaSqlStatement *
build6 (void)
{
	GdaSqlBuilder *b;
	GdaSqlStatement *stmt;

	b = gda_sql_builder_new (GDA_SQL_STATEMENT_SELECT);
	gda_sql_builder_add_field_value_id (b, gda_sql_builder_add_id (b, "fav_id"), 0);
	gda_sql_builder_add_field_value_id (b, gda_sql_builder_add_id (b, "rank"), 0);

	gda_sql_builder_select_add_target_id (b,
					   gda_sql_builder_add_id (b, "mytable"),
					   NULL);
	GdaSqlBuilderId id_cond1 = gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_EQ,
			      gda_sql_builder_add_id (b, "order_key"),
			      gda_sql_builder_add_param (b, "orderkey", G_TYPE_INT, FALSE), 0);
	GdaSqlBuilderId id_cond2 = gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_DIFF,
			      gda_sql_builder_add_id (b, "fav_id"),
			      gda_sql_builder_add_param (b, "id", G_TYPE_INT, FALSE), 0);

	gda_sql_builder_set_where (b, gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_AND, id_cond1, id_cond2, 0));

	stmt = gda_sql_statement_copy (gda_sql_builder_get_sql_statement (b));
	g_object_unref (b);
	return stmt;
}

/*
 * UPDATE mytable SET rank=##newrank WHERE fav_id = ##id::int AND order_key=##orderkey::int AND rank = ##rank::int
 */
static GdaSqlStatement *
build7 (void)
{
	GdaSqlBuilder *b;
	GdaSqlStatement *stmt;

	b = gda_sql_builder_new (GDA_SQL_STATEMENT_UPDATE);
	gda_sql_builder_set_table (b, "mytable");
	gda_sql_builder_add_field_value_id (b,
				   gda_sql_builder_add_id (b, "rank"),
				   gda_sql_builder_add_param (b, "newrank", G_TYPE_INT, FALSE));
	GdaSqlBuilderId id_cond1 = gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_EQ,
			      gda_sql_builder_add_id (b, "fav_id"),
			      gda_sql_builder_add_param (b, "id", G_TYPE_INT, FALSE),
			      0);
	GdaSqlBuilderId id_cond2 = gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_EQ,
			      gda_sql_builder_add_id (b, "order_key"),
			      gda_sql_builder_add_param (b, "orderkey", G_TYPE_INT, FALSE),
			      0);
	GdaSqlBuilderId id_cond3 = gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_EQ,
			      gda_sql_builder_add_id (b, "rank"),
			      gda_sql_builder_add_param (b, "rank", G_TYPE_INT, FALSE),
			      0);
	gda_sql_builder_set_where (b, gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_AND, id_cond1, id_cond2, id_cond3));

	stmt = gda_sql_statement_copy (gda_sql_builder_get_sql_statement (b));
	g_object_unref (b);
	return stmt;
}

/*
 * DELETE FROM mytable SET WHERE fav_id = ##id::int
 */
static GdaSqlStatement *
build8 (void)
{
	GdaSqlBuilder *b;
	GdaSqlStatement *stmt;

	b = gda_sql_builder_new (GDA_SQL_STATEMENT_DELETE);
	gda_sql_builder_set_table (b, "mytable");

	gda_sql_builder_set_where (b, gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_EQ,
							    gda_sql_builder_add_id (b, "id"),
							    gda_sql_builder_add_param (b, "id", G_TYPE_INT, FALSE),
							    0));
	stmt = gda_sql_statement_copy (gda_sql_builder_get_sql_statement (b));
	g_object_unref (b);
	return stmt;
}

/*
 * INSERT INTO mytable (session, name) VALUES (NULL, NULL);
 */
static GdaSqlStatement *
build9 (void)
{
	GdaSqlBuilder *builder;
	GdaSqlStatement *stmt;
	GValue *value;

	builder = gda_sql_builder_new (GDA_SQL_STATEMENT_INSERT);
	gda_sql_builder_set_table (builder, "mytable");

	gda_sql_builder_add_field_value_as_gvalue (builder, "session", NULL);
	value = gda_value_new_null ();
	gda_sql_builder_add_field_value_as_gvalue (builder, "name", value);
	gda_value_free (value);

	stmt = gda_sql_statement_copy (gda_sql_builder_get_sql_statement (builder));
	g_object_unref (builder);

	return stmt;
}

/*
 * SELECT DISTINCT fav_id, rank FROM mytable LIMIT 5
 */
static GdaSqlStatement *
build10 (void)
{
	GdaSqlBuilder *b;
	GdaSqlStatement *stmt;

	b = gda_sql_builder_new (GDA_SQL_STATEMENT_SELECT);
	gda_sql_builder_add_field_value_id (b, gda_sql_builder_add_id (b, "fav_id"), 0);
	gda_sql_builder_add_field_value_id (b, gda_sql_builder_add_id (b, "rank"), 0);

	gda_sql_builder_select_add_target_id (b,
					   gda_sql_builder_add_id (b, "mytable"),
					   NULL);
	gda_sql_builder_select_set_distinct (b, TRUE, 0);
	gda_sql_builder_select_set_limit (b, gda_sql_builder_add_expr (b, NULL, G_TYPE_INT, 5), 0);

	{
		GdaStatement *st;
		st = gda_sql_builder_get_statement (b, FALSE);
		g_print ("[%s]\n", gda_statement_to_sql (st, NULL, NULL));
		g_object_unref (st);
	}

	stmt = gda_sql_statement_copy (gda_sql_builder_get_sql_statement (b));
	g_object_unref (b);
	return stmt;
}

/*
 * SELECT DISTINCT ON (rank) fav_id, rank FROM mytable LIMIT 5 OFFSET 2
 */
static GdaSqlStatement *
build11 (void)
{
	GdaSqlBuilder *b;
	GdaSqlStatement *stmt;

	b = gda_sql_builder_new (GDA_SQL_STATEMENT_SELECT);
	gda_sql_builder_add_field_value_id (b, gda_sql_builder_add_id (b, "fav_id"), 0);
	gda_sql_builder_add_field_value_id (b, gda_sql_builder_add_id (b, "rank"), 0);

	gda_sql_builder_select_add_target_id (b,
					   gda_sql_builder_add_id (b, "mytable"),
					   NULL);
	gda_sql_builder_select_set_distinct (b, TRUE,
					     gda_sql_builder_add_id (b, "rank"));
	gda_sql_builder_select_set_limit (b, gda_sql_builder_add_expr (b, NULL, G_TYPE_INT, 5),
					  gda_sql_builder_add_expr (b, NULL, G_TYPE_INT, 2));

#ifdef DEBUG_NO
	{
		GdaStatement *st;
		st = gda_sql_builder_get_statement (b, FALSE);
		g_print ("[%s]\n", gda_statement_to_sql (st, NULL, NULL));
		g_object_unref (st);
	}
#endif

	stmt = gda_sql_statement_copy (gda_sql_builder_get_sql_statement (b));
	g_object_unref (b);
	return stmt;
}

/*
 * SELECT store_name, sum (sales) FROM stores GROUP BY store_name HAVING sum (sales) > 10
 */
static GdaSqlStatement *
build12 (void)
{
	GdaSqlBuilder *b;
	GdaSqlStatement *stmt;

	b = gda_sql_builder_new (GDA_SQL_STATEMENT_SELECT);
	gda_sql_builder_add_field_value_id (b, gda_sql_builder_add_id (b, "store_name"), 0);
	const GdaSqlBuilderId id_func = gda_sql_builder_add_function (b, "sum",
				      gda_sql_builder_add_id (b, "sales"), 0);
	gda_sql_builder_add_field_value_id (b, id_func, 0);

	gda_sql_builder_select_add_target_id (b,
					   gda_sql_builder_add_id (b, "stores"),
					   NULL);
	gda_sql_builder_select_group_by (b, gda_sql_builder_add_id (b, "store_name"));
	gda_sql_builder_select_set_having (b,
					   gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_GT,
								     id_func,
								     gda_sql_builder_add_expr (b, NULL,
											       G_TYPE_INT, 10),
								     0));

#ifdef DEBUG
	{
		GdaStatement *st;
		st = gda_sql_builder_get_statement (b, FALSE);
		g_print ("[%s]\n", gda_statement_to_sql (st, NULL, NULL));
		g_object_unref (st);
	}
#endif

	stmt = gda_sql_statement_copy (gda_sql_builder_get_sql_statement (b));
	g_object_unref (b);
	return stmt;
}

/*
 *
 */
static gboolean
builder_test_target_id (void)
{
	GdaSqlBuilder *builder;
	GdaSqlBuilderId id1, id2;
	gboolean allok = TRUE;

	builder = gda_sql_builder_new (GDA_SQL_STATEMENT_SELECT);
	gda_sql_builder_add_field_value_id (builder,
				      gda_sql_builder_add_id (builder, "*"), 0);
	/* same target with aliases */
	id1 = gda_sql_builder_select_add_target_id (builder,
						    gda_sql_builder_add_id (builder, "mytable"), "alias");
	id2 = gda_sql_builder_select_add_target_id (builder,
						    gda_sql_builder_add_id (builder, "mytable"), "alias");
	if (id1 != id2) {
		g_print ("identical targets with an alias not recognized as same target.\n");
		allok = FALSE;
	}

	id2 = gda_sql_builder_select_add_target_id (builder,
						    gda_sql_builder_add_id (builder, "mytable"), "alias2");
	if (id1 == id2) {
		g_print ("identical tables with different alias recognized as same target.\n");
		allok = FALSE;
	}

	id2 = gda_sql_builder_select_add_target_id (builder,
						    gda_sql_builder_add_id (builder, "mytable"), NULL);
	if (id1 == id2) {
		g_print ("identical tables with no alias recognized as same target.\n");
		allok = FALSE;
	}

	id1 = gda_sql_builder_select_add_target_id (builder,
						    gda_sql_builder_add_id (builder, "mytable"), NULL);
	if (id1 != id2) {
		g_print ("identical tables with different alias not recognized as same target.\n");
		allok = FALSE;
	}

	g_object_unref (builder);

	return allok;
}

/*
 * SELECT store_name, sum (sales) FROM stores GROUP BY store_name HAVING sum (sales) > 10
 */
static GdaSqlStatement *
build13 (void)
{
	GdaSqlBuilder *b;
	GdaSqlStatement *stmt;

	b = gda_sql_builder_new (GDA_SQL_STATEMENT_SELECT);
	gda_sql_builder_add_field_value_id (b,
					    gda_sql_builder_add_expr (b, NULL, G_TYPE_STRING, "A'string"), 0);
	gda_sql_builder_add_field_value_id (b, 
					    gda_sql_builder_add_expr (b, NULL, G_TYPE_INT, 234), 0);
	gda_sql_builder_add_field_value_id (b,
					    gda_sql_builder_add_expr (b, NULL, G_TYPE_BOOLEAN, TRUE), 0);
	GdaNumeric *numval;
	numval = gda_numeric_new ();
	gda_numeric_set_from_string (numval, "123.456789");
	gda_sql_builder_add_field_value_id (b,
					    gda_sql_builder_add_expr (b, NULL, GDA_TYPE_NUMERIC, numval), 0);
	gda_numeric_free (numval);

	GDate *date = g_date_new_dmy (27, G_DATE_MAY, 1972);
	gda_sql_builder_add_field_value_id (b,
					    gda_sql_builder_add_expr (b, NULL, G_TYPE_DATE, date), 0);
	g_date_free (date);

	gchar *data = "abc'de\\fghijklm";
	GdaBinary *bin = gda_binary_new ();
	gda_binary_set_data (bin, (const guchar*) data, 10);
	gda_sql_builder_add_field_value_id (b,
					    gda_sql_builder_add_expr (b, NULL, GDA_TYPE_BINARY, bin), 0);

	gda_binary_free (bin);

#ifdef DEBUG
	{
		GdaStatement *st;
		st = gda_sql_builder_get_statement (b, FALSE);
		g_print ("[%s]\n", gda_statement_to_sql (st, NULL, NULL));
		g_object_unref (st);
	}
#endif

	stmt = gda_sql_statement_copy (gda_sql_builder_get_sql_statement (b));
	g_object_unref (b);
	return stmt;
}
