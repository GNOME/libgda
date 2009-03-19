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

	gda_sql_builder_add_field_value (b, "e", gda_sql_builder_param (b, 0, "p1", G_TYPE_STRING, FALSE));
	gda_sql_builder_add_field_value (b, "f", gda_sql_builder_expr (b, 0, NULL, G_TYPE_INT, 15));
	gda_sql_builder_add_field_value (b, "g", gda_sql_builder_expr (b, 0, NULL, G_TYPE_STRING, "joe"));
	
	render_as_sql (b);
	g_object_unref (b);


	/* UPDATE products set ref='A0E''FESP' WHERE id = 14 */
	b = gda_sql_builder_new (GDA_SQL_STATEMENT_UPDATE);

	gda_sql_builder_set_table (b, "products");
	gda_sql_builder_add_field_value (b, "ref", gda_sql_builder_expr (b, 10, NULL, G_TYPE_STRING, "A0E'FESP"));
	gda_sql_builder_literal (b, 1, "id");
	gda_sql_builder_expr (b, 2, NULL, G_TYPE_INT, 14);
	gda_sql_builder_cond2 (b, 3, GDA_SQL_OPERATOR_TYPE_EQ, 1, 2);
	gda_sql_builder_set_where (b, 3);

	render_as_sql (b);

	/* reuse the same GdaSqlBuilder object to change the WHERE condition to: WHERE id = ##theid::int */
	gda_sql_builder_set_where (b,
				   gda_sql_builder_cond2 (b, 0, GDA_SQL_OPERATOR_TYPE_EQ,
							  1,
							  gda_sql_builder_param (b, 0, "theid", G_TYPE_INT, FALSE)));
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
