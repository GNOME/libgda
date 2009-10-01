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

	gda_sql_builder_add_field (b,
				   gda_sql_builder_add_id (b, 0, "e"),
				   gda_sql_builder_add_param (b, 0, "p1", G_TYPE_STRING, FALSE));
	gda_sql_builder_add_field (b,
				   gda_sql_builder_add_id (b, 0, "f"),
				   gda_sql_builder_add_expr (b, 0, NULL, G_TYPE_INT, 15));
	gda_sql_builder_add_field (b, gda_sql_builder_add_id (b, 0, "g"),
				   gda_sql_builder_add_expr (b, 0, NULL, G_TYPE_STRING, "joe"));
	
	render_as_sql (b);
	g_object_unref (b);


	/* UPDATE products set ref='A0E''FESP' WHERE id = 14 */
	b = gda_sql_builder_new (GDA_SQL_STATEMENT_UPDATE);

	gda_sql_builder_set_table (b, "products");
	gda_sql_builder_add_field (b,
				   gda_sql_builder_add_id (b, 0, "ref"),
				   gda_sql_builder_add_expr (b, 10, NULL, G_TYPE_STRING, "A0E'FESP"));
	gda_sql_builder_add_id (b, 1, "id");
	gda_sql_builder_add_expr (b, 2, NULL, G_TYPE_INT, 14);
	gda_sql_builder_add_cond (b, 3, GDA_SQL_OPERATOR_TYPE_EQ, 1, 2, 0);
	gda_sql_builder_set_where (b, 3);

	render_as_sql (b);

	/* reuse the same GdaSqlBuilder object to change the WHERE condition to: WHERE id = ##theid::int */
	gda_sql_builder_set_where (b,
				   gda_sql_builder_add_cond (b, 0, GDA_SQL_OPERATOR_TYPE_EQ,
							 1,
							 gda_sql_builder_add_param (b, 0, "theid", G_TYPE_INT, FALSE),
							 0));
	render_as_sql (b);
	g_object_unref (b);

	/* DELETE FROM items WHERE id = ##theid::int */
	b = gda_sql_builder_new (GDA_SQL_STATEMENT_DELETE);

	gda_sql_builder_set_table (b, "items");
	gda_sql_builder_add_id (b, 1, "id");
	gda_sql_builder_add_param (b, 2, "theid", G_TYPE_INT, FALSE);
	gda_sql_builder_add_cond (b, 3, GDA_SQL_OPERATOR_TYPE_EQ, 1, 2, 0);
	gda_sql_builder_set_where (b, 3);

	render_as_sql (b);
	g_object_unref (b);
	
	/*
	 * The next statement shows automatic quoting of reserved SQL keywords (DATE and SELECT here)
	 *
	 * SELECT c."date", name, date AS person FROM "select" as c, orders
	 */
	b = gda_sql_builder_new (GDA_SQL_STATEMENT_SELECT);
	
	gda_sql_builder_add_id (b, 1, "select"); /* SELECT is an SQL reserved keyword */
	gda_sql_builder_select_add_target (b, 1, 1, "c");
	gda_sql_builder_select_add_target (b, 2,
					   gda_sql_builder_add_id (b, 0, "orders"),
					   NULL);
	gda_sql_builder_select_join_targets (b, 5, 1, 2, GDA_SQL_SELECT_JOIN_INNER, 0);

	gda_sql_builder_add_field (b,
				   gda_sql_builder_add_id (b, 0, "c.date"), 0); /* DATE is an SQL reserved keyword */
	gda_sql_builder_add_field (b,
				   gda_sql_builder_add_id (b, 0, "name"),
				   gda_sql_builder_add_id (b, 0, "person"));

	render_as_sql (b);

	/* reuse the same GdaSqlBuilder object to change the INNER join's condition */
	gda_sql_builder_join_add_field (b, 5, "id");

	render_as_sql (b);
	g_object_unref (b);

	/* SELECT myfunc (a, 5, 'Joe') FROM mytable */
	b = gda_sql_builder_new (GDA_SQL_STATEMENT_SELECT);
	gda_sql_builder_select_add_target (b, 0,
					   gda_sql_builder_add_id (b, 0, "mytable"),
					   NULL);
	gda_sql_builder_add_function (b, 1, "myfunc",
				      gda_sql_builder_add_id (b, 0, "a"),
				      gda_sql_builder_add_expr (b, 0, NULL, G_TYPE_INT, 5),
				      gda_sql_builder_add_expr (b, 0, NULL, G_TYPE_STRING, "Joe"),
				      0);
	gda_sql_builder_add_field (b, 1, 0);
	render_as_sql (b);

	/* reuse the same GdaSqlBuilder object to have:
	 * SELECT myfunc (a, 5, 'Joe'), MAX (myfunc (a, 5, 'Joe'), b, 10) FROM mytable */
	guint args[] = {1, 3, 4};
	gda_sql_builder_add_id (b, 3, "b");
	gda_sql_builder_add_expr (b, 4, NULL, G_TYPE_INT, 10);

	gda_sql_builder_add_function_v (b, 5, "MAX", args, 3);
	gda_sql_builder_add_field (b, 5, 0);

	render_as_sql (b);
	g_object_unref (b);	

	return 0;
}

void
render_as_sql (GdaSqlBuilder *b)
{
	GdaStatement *stmt;
	GError *error = NULL;

	stmt = gda_sql_builder_get_statement (b, &error);
	if (!stmt) {
		g_print ("Statement error: %s\n",
			 error && error->message ? error->message : "No detail");
		if (error)
			g_error_free (error);
	}
	else {
		gchar *sql;
		sql = gda_statement_to_sql (stmt, NULL, &error);
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
