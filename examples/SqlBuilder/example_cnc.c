/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 Murray Cumming <murrayc@murrayc.com>
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

void render_as_sql (GdaSqlBuilder *b);

int
main (int argc, char *argv[])
{
	gda_init ();

	GdaSqlBuilder *b;

	/* INSERT INTO customers (e, f, g) VALUES (##p1::string, 15, 'joe') */
	b = gda_sql_builder_new (GDA_SQL_STATEMENT_INSERT);

	gda_sql_builder_set_table (b, "customers");

	gda_sql_builder_add_field_value_id (b,
				   gda_sql_builder_add_id (b, "e"),
				   gda_sql_builder_add_param (b, "p1", G_TYPE_STRING, FALSE));
	gda_sql_builder_add_field_value_id (b,
				   gda_sql_builder_add_id (b, "f"),
				   gda_sql_builder_add_expr (b, NULL, G_TYPE_INT, 15));
	gda_sql_builder_add_field_value_id (b, gda_sql_builder_add_id (b, "g"),
				   gda_sql_builder_add_expr (b, NULL, G_TYPE_STRING, "joe"));

	render_as_sql (b);
	g_object_unref (b);


	/* UPDATE products set ref='A0E''FESP' WHERE id = 14 */
	b = gda_sql_builder_new (GDA_SQL_STATEMENT_UPDATE);

	gda_sql_builder_set_table (b, "products");
	gda_sql_builder_add_field_value_id (b,
				   gda_sql_builder_add_id (b, "ref"),
				   gda_sql_builder_add_expr (b, NULL, G_TYPE_STRING, "A0E'FESP"));
	GdaSqlBuilderId id_field = gda_sql_builder_add_id (b, "id");
	GdaSqlBuilderId id_value = gda_sql_builder_add_expr (b, NULL, G_TYPE_INT, 14);
	GdaSqlBuilderId id_cond = gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_EQ, id_field, id_value, 0);
	gda_sql_builder_set_where (b, id_cond);

	render_as_sql (b);

	/* reuse the same GdaSqlBuilder object to change the WHERE condition to: WHERE id = ##theid::int */
	gda_sql_builder_set_where (b,
				   gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_EQ,
							 id_field,
							 gda_sql_builder_add_param (b, "theid", G_TYPE_INT, FALSE),
							 0));
	render_as_sql (b);
	g_object_unref (b);

	/* DELETE FROM items WHERE id = ##theid::int */
	b = gda_sql_builder_new (GDA_SQL_STATEMENT_DELETE);

	gda_sql_builder_set_table (b, "items");
	id_field = gda_sql_builder_add_id (b, "id");
	GdaSqlBuilderId id_param = gda_sql_builder_add_param (b, "theid", G_TYPE_INT, FALSE);
	id_cond = gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_EQ, id_field, id_param, 0);
	gda_sql_builder_set_where (b, id_cond);

	render_as_sql (b);
	g_object_unref (b);

	/*
	 * The next statement shows automatic quoting of reserved SQL keywords (DATE and SELECT here)
	 *
	 * SELECT c."date", name, date AS person FROM "select" as c, orders
	 */
	b = gda_sql_builder_new (GDA_SQL_STATEMENT_SELECT);

	GdaSqlBuilderId id_table = gda_sql_builder_add_id (b, "select"); /* SELECT is an SQL reserved keyword */
	GdaSqlBuilderId id_target1 = gda_sql_builder_select_add_target_id (b, id_table, "c");
	GdaSqlBuilderId id_target2 = gda_sql_builder_select_add_target_id (b,
					   gda_sql_builder_add_id (b, "orders"),
					   NULL);
	GdaSqlBuilderId id_join = gda_sql_builder_select_join_targets (b, id_target1, id_target2, GDA_SQL_SELECT_JOIN_INNER, 0);

	gda_sql_builder_add_field_value_id (b,
				   gda_sql_builder_add_id (b, "c.date"), 0); /* DATE is an SQL reserved keyword */
	gda_sql_builder_add_field_value_id (b,
				   gda_sql_builder_add_id (b, "name"),
				   gda_sql_builder_add_id (b, "person"));

	render_as_sql (b);

	/* reuse the same GdaSqlBuilder object to change the INNER join's condition */
	gda_sql_builder_join_add_field (b, id_join, "id");

	render_as_sql (b);
	g_object_unref (b);

	/* SELECT myfunc (a, 5, 'Joe') FROM mytable */
	b = gda_sql_builder_new (GDA_SQL_STATEMENT_SELECT);
	gda_sql_builder_select_add_target_id (b,
					   gda_sql_builder_add_id (b, "mytable"),
					   NULL);
	GdaSqlBuilderId id_function_myfunc = gda_sql_builder_add_function (b, "myfunc",
				      gda_sql_builder_add_id (b, "a"),
				      gda_sql_builder_add_expr (b, NULL, G_TYPE_INT, 5),
				      gda_sql_builder_add_expr (b, NULL, G_TYPE_STRING, "Joe"),
				      0);
	gda_sql_builder_add_field_value_id (b, id_function_myfunc, 0);
	render_as_sql (b);

	/* reuse the same GdaSqlBuilder object to have:
	 * SELECT myfunc (a, 5, 'Joe'), MAX (myfunc (a, 5, 'Joe'), b, 10) FROM mytable */
	GdaSqlBuilderId id_b = gda_sql_builder_add_id (b, "b");
	GdaSqlBuilderId id_b_value = gda_sql_builder_add_expr (b, NULL, G_TYPE_INT, 10);

	GdaSqlBuilderId args[] = {id_function_myfunc, id_b, id_b_value};
	GdaSqlBuilderId id_function_max = gda_sql_builder_add_function_v (b, "MAX", args, 3);
	gda_sql_builder_add_field_value_id (b, id_function_max, 0);

	render_as_sql (b);
	g_object_unref (b);

	/* testing identifiers which are SQL reserved keywords */
	b = gda_sql_builder_new (GDA_SQL_STATEMENT_UPDATE);

	gda_sql_builder_set_table (b, "select");
	gda_sql_builder_add_field_value_id (b,
				   gda_sql_builder_add_id (b, "date"),
				   gda_sql_builder_add_expr (b, NULL, G_TYPE_STRING, "2009-05-27"));
	id_field = gda_sql_builder_add_id (b, "id");
	id_value = gda_sql_builder_add_expr (b, NULL, G_TYPE_INT, 14);
	id_cond = gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_EQ, id_field, id_value, 0);
	gda_sql_builder_set_where (b, id_cond);

	render_as_sql (b);
	g_object_unref (b);

	/* testing identifiers which are SQL reserved keywords */
	b = gda_sql_builder_new (GDA_SQL_STATEMENT_SELECT);
	gda_sql_builder_select_add_target_id (b,
					   gda_sql_builder_add_id (b, "date"),
					   NULL);
	gda_sql_builder_select_add_target_id (b,
					   gda_sql_builder_add_id (b, "MyTable"),
					   NULL);
	GdaSqlBuilderId id_function = gda_sql_builder_add_function (b, "date",
				      gda_sql_builder_add_id (b, "a"),
				      gda_sql_builder_add_expr (b, NULL, G_TYPE_INT, 5),
				      gda_sql_builder_add_expr (b, NULL, G_TYPE_STRING, "Joe"),
				      0);
	gda_sql_builder_add_field_value_id (b, id_function, 0);
	render_as_sql (b);
	g_object_unref (b);

	/*
	 * SELECT people.firstname AS person, people.lastname, "date" AS birthdate, age FROM people
	 */
	b = gda_sql_builder_new (GDA_SQL_STATEMENT_SELECT);

	gda_sql_builder_select_add_field (b, "firstname", "people", "person");
	gda_sql_builder_select_add_field (b, "lastname", "people", NULL);
	gda_sql_builder_select_add_field (b, "date", NULL, "birthdate");
	gda_sql_builder_select_add_field (b, "age", NULL, NULL);
	gda_sql_builder_select_add_target_id (b,
					   gda_sql_builder_add_id (b, "people"),
					   NULL);

	render_as_sql (b);
	g_object_unref (b);

	/* INSERT INTO customers (f, g) VALUES (15, 'joe') */
	b = gda_sql_builder_new (GDA_SQL_STATEMENT_INSERT);

	gda_sql_builder_set_table (b, "customers");
	gda_sql_builder_add_field_value (b, "f", G_TYPE_INT, 15);
	gda_sql_builder_add_field_value (b, "g", G_TYPE_STRING, "joe");

	render_as_sql (b);
	g_object_unref (b);

	/* Create a WHERE clause in a statement and reuse it in another one:
	 *
	 * - the first SQL built is DELETE FROM items WHERE id = ##theid::int
	 * - the "id = ##theid::int" is exported from the first build and imported into the final build
	 * - the final SQL is: SELECT id FROM mytable WHERE (name = ##thename::string) AND (id = ##theid::int)
	 */
	GdaSqlExpr *expr;
	b = gda_sql_builder_new (GDA_SQL_STATEMENT_DELETE);

	gda_sql_builder_set_table (b, "items");
	id_field = gda_sql_builder_add_id (b, "id");
	id_param = gda_sql_builder_add_param (b, "theid", G_TYPE_INT, FALSE);
	id_cond = gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_EQ, id_field, id_param, 0);
	gda_sql_builder_set_where (b, id_cond);

	render_as_sql (b);
	expr = gda_sql_builder_export_expression (b, id_cond);
	g_object_unref (b);

	b = gda_sql_builder_new (GDA_SQL_STATEMENT_SELECT);
	gda_sql_builder_add_field_value_id (b, gda_sql_builder_add_id (b, "id"), 0);
	gda_sql_builder_select_add_target_id (b,
					      gda_sql_builder_add_id (b, "mytable"),
					      NULL);
	id_field = gda_sql_builder_add_id (b, "name");
	id_param = gda_sql_builder_add_param (b, "thename", G_TYPE_STRING, FALSE);
	id_cond = gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_EQ, id_field, id_param, 0);

	gda_sql_builder_set_where (b,
				   gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_AND, id_cond,
							     gda_sql_builder_import_expression (b, expr),
							     0));
	gda_sql_expr_free (expr);
	render_as_sql (b);
	g_object_unref (b);

	return 0;
}

void
render_as_sql (GdaSqlBuilder *b)
{
	GdaStatement *stmt;
	GError *error = NULL;
	static GdaConnection *cnc = NULL;

	if (!cnc) {
		cnc = gda_connection_open_from_string ("SQLite", "DB_DIR=.;DB_NAME=test", NULL,
						       GDA_CONNECTION_OPTIONS_SQL_IDENTIFIERS_CASE_SENSITIVE, NULL);
		g_assert (cnc);
	}

	stmt = gda_sql_builder_get_statement (b, &error);
	if (!stmt) {
		g_print ("Statement error: %s\n",
			 error && error->message ? error->message : "No detail");
		if (error)
			g_error_free (error);
	}
	else {
		gchar *sql;
		sql = gda_statement_to_sql_extended (stmt, cnc, NULL, GDA_STATEMENT_SQL_PARAMS_SHORT, NULL, &error);
		if (!sql) {
			g_print ("SQL rendering error: %s\n",
				 error && error->message ? error->message : "No detail");
			if (error)
				g_error_free (error);
		}
		else {
			g_print ("SQL: %s\n", sql);
			g_free (sql);
		}
		g_object_unref (stmt);
	}
}
