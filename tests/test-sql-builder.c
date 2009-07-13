/*
 * Copyright (C) 2009 The GNOME Foundation.
 *
 * AUTHORS:
 *      Vivien Malerba <malerba@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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

ATest tests[] = {
	{"build0", build0, "{\"sql\":null,\"stmt_type\":\"SELECT\",\"contents\":{\"distinct\":\"false\",\"fields\":[{\"expr\":{\"value\":\"*\"}}],\"from\":{\"targets\":[{\"expr\":{\"value\":\"mytable\"}}]}}}"},
	{"build1", build1, "{\"sql\":null,\"stmt_type\":\"SELECT\",\"contents\":{\"distinct\":\"false\",\"fields\":[{\"expr\":{\"value\":\"contents\"}},{\"expr\":{\"value\":\"descr\"}},{\"expr\":{\"value\":\"rank\"}},{\"expr\":{\"value\":\"name\"}}],\"from\":{\"targets\":[{\"expr\":{\"value\":\"mytable\"}}]},\"where\":{\"operation\":{\"operator\":\"AND\",\"operand0\":{\"operation\":{\"operator\":\"=\",\"operand0\":{\"value\":\"session\"},\"operand1\":{\"value\":null,\"param_spec\":{\"name\":\"session\",\"descr\":null,\"type\":\"string\",\"is_param\":true,\"nullok\":false}}}},\"operand1\":{\"operation\":{\"operator\":\"AND\",\"operand0\":{\"operation\":{\"operator\":\"=\",\"operand0\":{\"value\":\"type\"},\"operand1\":{\"value\":\"'TABLE'\"}}},\"operand1\":{\"operation\":{\"operator\":\"=\",\"operand0\":{\"value\":\"name\"},\"operand1\":{\"value\":\"'alf'\"}}}}}}}}}"},
	{"build2", build2, "{\"sql\":null,\"stmt_type\":\"INSERT\",\"contents\":{\"table\":\"mytable\",\"fields\":[\"session\",\"type\",\"name\",\"contents\",\"descr\"],\"values\":[[{\"value\":null,\"param_spec\":{\"name\":\"session\",\"descr\":null,\"type\":\"int\",\"is_param\":true,\"nullok\":false}},{\"value\":null,\"param_spec\":{\"name\":\"type\",\"descr\":null,\"type\":\"int\",\"is_param\":true,\"nullok\":false}},{\"value\":null,\"param_spec\":{\"name\":\"name\",\"descr\":null,\"type\":\"string\",\"is_param\":true,\"nullok\":true}},{\"value\":null,\"param_spec\":{\"name\":\"contents\",\"descr\":null,\"type\":\"string\",\"is_param\":true,\"nullok\":false}},{\"value\":null,\"param_spec\":{\"name\":\"descr\",\"descr\":null,\"type\":\"string\",\"is_param\":true,\"nullok\":true}}]]}}"},
	{"build3", build3, "{\"sql\":null,\"stmt_type\":\"UPDATE\",\"contents\":{\"table\":\"mytable\",\"fields\":[\"name\",\"contents\",\"descr\"],\"expressions\":[{\"value\":null,\"param_spec\":{\"name\":\"name\",\"descr\":null,\"type\":\"string\",\"is_param\":true,\"nullok\":true}},{\"value\":null,\"param_spec\":{\"name\":\"contents\",\"descr\":null,\"type\":\"string\",\"is_param\":true,\"nullok\":false}},{\"value\":null,\"param_spec\":{\"name\":\"descr\",\"descr\":null,\"type\":\"string\",\"is_param\":true,\"nullok\":true}}],\"condition\":{\"operation\":{\"operator\":\"=\",\"operand0\":{\"value\":\"id\"},\"operand1\":{\"value\":null,\"param_spec\":{\"name\":\"id\",\"descr\":null,\"type\":\"int\",\"is_param\":true,\"nullok\":false}}}}}}"},
	{"build4", build4, "{\"sql\":null,\"stmt_type\":\"SELECT\",\"contents\":{\"distinct\":\"false\",\"fields\":[{\"expr\":{\"value\":\"fav.*\"}}],\"from\":{\"targets\":[{\"expr\":{\"value\":\"mytable\"},\"as\":\"fav\"},{\"expr\":{\"value\":\"fav_orders\"},\"as\":\"o\"}],\"joins\":[{\"join_type\":\"LEFT\",\"join_pos\":\"1\",\"on_cond\":{\"operation\":{\"operator\":\"=\",\"operand0\":{\"value\":\"fav.id\"},\"operand1\":{\"value\":\"o.fav_id\"}}}}]},\"where\":{\"operation\":{\"operator\":\"AND\",\"operand0\":{\"operation\":{\"operator\":\"=\",\"operand0\":{\"value\":\"fav.sesion\"},\"operand1\":{\"value\":null,\"param_spec\":{\"name\":\"session\",\"descr\":null,\"type\":\"int\",\"is_param\":true,\"nullok\":false}}}},\"operand1\":{\"operation\":{\"operator\":\"=\",\"operand0\":{\"value\":\"o.order_key\"},\"operand1\":{\"value\":null,\"param_spec\":{\"name\":\"okey\",\"descr\":null,\"type\":\"int\",\"is_param\":true,\"nullok\":false}}}}}},\"order_by\":[{\"expr\":{\"value\":\"o.rank\"},\"sort\":\"ASC\"}]}}"},
	{"build5", build5, "{\"sql\":null,\"stmt_type\":\"SELECT\",\"contents\":{\"distinct\":\"false\",\"fields\":[{\"expr\":{\"value\":\"id\"}}],\"from\":{\"targets\":[{\"expr\":{\"value\":\"mytable\"}}]},\"where\":{\"operation\":{\"operator\":\"AND\",\"operand0\":{\"operation\":{\"operator\":\"=\",\"operand0\":{\"value\":\"sesion\"},\"operand1\":{\"value\":null,\"param_spec\":{\"name\":\"session\",\"descr\":null,\"type\":\"int\",\"is_param\":true,\"nullok\":false}}}},\"operand1\":{\"operation\":{\"operator\":\"=\",\"operand0\":{\"value\":\"contents\"},\"operand1\":{\"value\":null,\"param_spec\":{\"name\":\"contents\",\"descr\":null,\"type\":\"int\",\"is_param\":true,\"nullok\":false}}}}}}}}"},
	{"build6", build6, "{\"sql\":null,\"stmt_type\":\"SELECT\",\"contents\":{\"distinct\":\"false\",\"fields\":[{\"expr\":{\"value\":\"fav_id\"}},{\"expr\":{\"value\":\"rank\"}}],\"from\":{\"targets\":[{\"expr\":{\"value\":\"mytable\"}}]},\"where\":{\"operation\":{\"operator\":\"AND\",\"operand0\":{\"operation\":{\"operator\":\"=\",\"operand0\":{\"value\":\"order_key\"},\"operand1\":{\"value\":null,\"param_spec\":{\"name\":\"orderkey\",\"descr\":null,\"type\":\"int\",\"is_param\":true,\"nullok\":false}}}},\"operand1\":{\"operation\":{\"operator\":\"!=\",\"operand0\":{\"value\":\"fav_id\"},\"operand1\":{\"value\":null,\"param_spec\":{\"name\":\"id\",\"descr\":null,\"type\":\"int\",\"is_param\":true,\"nullok\":false}}}}}}}}"},
	{"build7", build7, "{\"sql\":null,\"stmt_type\":\"UPDATE\",\"contents\":{\"table\":\"mytable\",\"fields\":[\"rank\"],\"expressions\":[{\"value\":null,\"param_spec\":{\"name\":\"newrank\",\"descr\":null,\"type\":\"int\",\"is_param\":true,\"nullok\":false}}],\"condition\":{\"operation\":{\"operator\":\"AND\",\"operand0\":{\"operation\":{\"operator\":\"=\",\"operand0\":{\"value\":\"fav_id\"},\"operand1\":{\"value\":null,\"param_spec\":{\"name\":\"id\",\"descr\":null,\"type\":\"int\",\"is_param\":true,\"nullok\":false}}}},\"operand1\":{\"operation\":{\"operator\":\"=\",\"operand0\":{\"value\":\"order_key\"},\"operand1\":{\"value\":null,\"param_spec\":{\"name\":\"orderkey\",\"descr\":null,\"type\":\"int\",\"is_param\":true,\"nullok\":false}}}},\"operand2\":{\"operation\":{\"operator\":\"=\",\"operand0\":{\"value\":\"rank\"},\"operand1\":{\"value\":null,\"param_spec\":{\"name\":\"rank\",\"descr\":null,\"type\":\"int\",\"is_param\":true,\"nullok\":false}}}}}}}}"},
	{"build8", build8, "{\"sql\":null,\"stmt_type\":\"DELETE\",\"contents\":{\"table\":\"mytable\",\"condition\":{\"operation\":{\"operator\":\"=\",\"operand0\":{\"value\":\"id\"},\"operand1\":{\"value\":null,\"param_spec\":{\"name\":\"id\",\"descr\":null,\"type\":\"int\",\"is_param\":true,\"nullok\":false}}}}}}"}
};

int
main (int argc, char** argv)
{
	gda_init ();
	gint i, nfailed = 0;
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
	gda_sql_builder_add_field (builder,
				   gda_sql_builder_literal (builder, 0, "*"), 0);
	gda_sql_builder_select_add_target (builder, 0,
					   gda_sql_builder_literal (builder, 0, "mytable"), NULL);
	stmt = gda_sql_builder_get_sql_statement (builder, FALSE);
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
	guint op_ids [3];
	gint index = 0;
	GdaSqlStatement *stmt;

	memset (op_ids, 0, sizeof (guint) * 3);

	builder = gda_sql_builder_new (GDA_SQL_STATEMENT_SELECT);
	gda_sql_builder_select_add_target (builder, 0,
					   gda_sql_builder_literal (builder, 0, "mytable"), NULL);
	gda_sql_builder_add_field (builder,
				   gda_sql_builder_literal (builder, 0, "contents"), 0);
	gda_sql_builder_add_field (builder,
				   gda_sql_builder_literal (builder, 0, "descr"), 0);
	gda_sql_builder_add_field (builder,
				   gda_sql_builder_literal (builder, 0, "rank"), 0);
	gda_sql_builder_add_field (builder,
				   gda_sql_builder_literal (builder, 0, "name"), 0);
	gda_sql_builder_cond (builder, 1, GDA_SQL_OPERATOR_TYPE_EQ,
			      gda_sql_builder_literal (builder, 0, "session"),
			      gda_sql_builder_param (builder, 0, "session", G_TYPE_STRING, FALSE), 0);

	op_ids [index] = gda_sql_builder_cond (builder, 0, GDA_SQL_OPERATOR_TYPE_EQ,
					       gda_sql_builder_literal (builder, 0, "type"),
					       gda_sql_builder_expr (builder, 0, NULL, G_TYPE_STRING, "TABLE"), 0);
	index++;

	op_ids [index] = gda_sql_builder_cond (builder, 0, GDA_SQL_OPERATOR_TYPE_EQ,
					       gda_sql_builder_literal (builder, 0, "name"),
					       gda_sql_builder_expr (builder, 0, NULL, G_TYPE_STRING, "alf"), 0);
	index++;

	gda_sql_builder_cond_v (builder, 2, GDA_SQL_OPERATOR_TYPE_AND, op_ids, index);

	gda_sql_builder_set_where (builder, gda_sql_builder_cond (builder, 0, GDA_SQL_OPERATOR_TYPE_AND, 1, 2, 0));

	stmt = gda_sql_builder_get_sql_statement (builder, FALSE);
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

	gda_sql_builder_add_field (builder,
				   gda_sql_builder_literal (builder, 0, "session"),
				   gda_sql_builder_param (builder, 0, "session", G_TYPE_INT, FALSE));
	gda_sql_builder_add_field (builder,
				   gda_sql_builder_literal (builder, 0, "type"),
				   gda_sql_builder_param (builder, 0, "type", G_TYPE_INT, FALSE));
	gda_sql_builder_add_field (builder,
				   gda_sql_builder_literal (builder, 0, "name"),
				   gda_sql_builder_param (builder, 0, "name", G_TYPE_STRING, TRUE));
	gda_sql_builder_add_field (builder,
				   gda_sql_builder_literal (builder, 0, "contents"),
				   gda_sql_builder_param (builder, 0, "contents", G_TYPE_STRING, FALSE));
	gda_sql_builder_add_field (builder,
				   gda_sql_builder_literal (builder, 0, "descr"),
				   gda_sql_builder_param (builder, 0, "descr", G_TYPE_STRING, TRUE));

	stmt = gda_sql_builder_get_sql_statement (builder, FALSE);
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

	gda_sql_builder_add_field (builder,
				   gda_sql_builder_literal (builder, 0, "name"),
				   gda_sql_builder_param (builder, 0, "name", G_TYPE_STRING, TRUE));
	gda_sql_builder_add_field (builder,
				   gda_sql_builder_literal (builder, 0, "contents"),
				   gda_sql_builder_param (builder, 0, "contents", G_TYPE_STRING, FALSE));
	gda_sql_builder_add_field (builder,
				   gda_sql_builder_literal (builder, 0, "descr"),
				   gda_sql_builder_param (builder, 0, "descr", G_TYPE_STRING, TRUE));

	gda_sql_builder_set_where (builder,
				   gda_sql_builder_cond (builder, 0, GDA_SQL_OPERATOR_TYPE_EQ,
							 gda_sql_builder_literal (builder, 0, "id"),
							 gda_sql_builder_param (builder, 0, "id", G_TYPE_INT, FALSE),
							 0));

	stmt = gda_sql_builder_get_sql_statement (builder, FALSE);
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
	guint t1, t2;

	b = gda_sql_builder_new (GDA_SQL_STATEMENT_SELECT);
	gda_sql_builder_add_field (b,
				   gda_sql_builder_literal (b, 0, "fav.*"), 0);
	t1 = gda_sql_builder_select_add_target (b, 0,
						gda_sql_builder_literal (b, 0, "mytable"),
						"fav");
	t2 = gda_sql_builder_select_add_target (b, 0,
						gda_sql_builder_literal (b, 0, "fav_orders"),
						"o");
	gda_sql_builder_select_join_targets (b, 0, t1, t2, GDA_SQL_SELECT_JOIN_LEFT,
					     gda_sql_builder_cond (b, 0, GDA_SQL_OPERATOR_TYPE_EQ,
								   gda_sql_builder_literal (b, 0, "fav.id"),
								   gda_sql_builder_literal (b, 0, "o.fav_id"),
								   0));

	gda_sql_builder_set_where (b,
	    gda_sql_builder_cond (b, 0, GDA_SQL_OPERATOR_TYPE_AND,
		  gda_sql_builder_cond (b, 0, GDA_SQL_OPERATOR_TYPE_EQ,
					gda_sql_builder_literal (b, 0, "fav.sesion"),
					gda_sql_builder_param (b, 0, "session", G_TYPE_INT, FALSE), 0),
		  gda_sql_builder_cond (b, 0, GDA_SQL_OPERATOR_TYPE_EQ,
					gda_sql_builder_literal (b, 0, "o.order_key"),
					gda_sql_builder_param (b, 0, "okey", G_TYPE_INT, FALSE), 0), 0));

	gda_sql_builder_select_order_by (b,
					 gda_sql_builder_literal (b, 0, "o.rank"), TRUE, NULL);
	stmt = gda_sql_builder_get_sql_statement (b, FALSE);
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
	gda_sql_builder_add_field (b,
				   gda_sql_builder_literal (b, 0, "id"), 0);
	gda_sql_builder_select_add_target (b, 0,
					   gda_sql_builder_literal (b, 0, "mytable"),
					   NULL);

	gda_sql_builder_set_where (b,
	    gda_sql_builder_cond (b, 0, GDA_SQL_OPERATOR_TYPE_AND,
		  gda_sql_builder_cond (b, 0, GDA_SQL_OPERATOR_TYPE_EQ,
					gda_sql_builder_literal (b, 0, "sesion"),
					gda_sql_builder_param (b, 0, "session", G_TYPE_INT, FALSE), 0),
		  gda_sql_builder_cond (b, 0, GDA_SQL_OPERATOR_TYPE_EQ,
					gda_sql_builder_literal (b, 0, "contents"),
					gda_sql_builder_param (b, 0, "contents", G_TYPE_INT, FALSE), 0), 0));

	stmt = gda_sql_builder_get_sql_statement (b, FALSE);
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
	gda_sql_builder_add_field (b, gda_sql_builder_literal (b, 0, "fav_id"), 0);
	gda_sql_builder_add_field (b, gda_sql_builder_literal (b, 0, "rank"), 0);

	gda_sql_builder_select_add_target (b, 0,
					   gda_sql_builder_literal (b, 0, "mytable"),
					   NULL);
	gda_sql_builder_cond (b, 1, GDA_SQL_OPERATOR_TYPE_EQ,
			      gda_sql_builder_literal (b, 0, "order_key"),
			      gda_sql_builder_param (b, 0, "orderkey", G_TYPE_INT, FALSE), 0);
	gda_sql_builder_cond (b, 2, GDA_SQL_OPERATOR_TYPE_DIFF,
			      gda_sql_builder_literal (b, 0, "fav_id"),
			      gda_sql_builder_param (b, 0, "id", G_TYPE_INT, FALSE), 0);

	gda_sql_builder_set_where (b, gda_sql_builder_cond (b, 0, GDA_SQL_OPERATOR_TYPE_AND, 1, 2, 0));

	stmt = gda_sql_builder_get_sql_statement (b, FALSE);
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
	gda_sql_builder_add_field (b,
				   gda_sql_builder_literal (b, 0, "rank"),
				   gda_sql_builder_param (b, 0, "newrank", G_TYPE_INT, FALSE));
	gda_sql_builder_cond (b, 1, GDA_SQL_OPERATOR_TYPE_EQ,
			      gda_sql_builder_literal (b, 0, "fav_id"),
			      gda_sql_builder_param (b, 0, "id", G_TYPE_INT, FALSE),
			      0);
	gda_sql_builder_cond (b, 2, GDA_SQL_OPERATOR_TYPE_EQ,
			      gda_sql_builder_literal (b, 0, "order_key"),
			      gda_sql_builder_param (b, 0, "orderkey", G_TYPE_INT, FALSE),
			      0);
	gda_sql_builder_cond (b, 3, GDA_SQL_OPERATOR_TYPE_EQ,
			      gda_sql_builder_literal (b, 0, "rank"),
			      gda_sql_builder_param (b, 0, "rank", G_TYPE_INT, FALSE),
			      0);
	gda_sql_builder_set_where (b, gda_sql_builder_cond (b, 0, GDA_SQL_OPERATOR_TYPE_AND, 1, 2, 3));

	stmt = gda_sql_builder_get_sql_statement (b, FALSE);
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
	
	gda_sql_builder_set_where (b, gda_sql_builder_cond (b, 0, GDA_SQL_OPERATOR_TYPE_EQ,
							    gda_sql_builder_literal (b, 0, "id"),
							    gda_sql_builder_param (b, 0, "id", G_TYPE_INT, FALSE),
							    0));
	stmt = gda_sql_builder_get_sql_statement (b, FALSE);
	g_object_unref (b);
	return stmt;
}

