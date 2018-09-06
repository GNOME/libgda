/*
 * Copyright (C) 2007 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2015 Gergely Polonkai <gergely@polonkai.eu>
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

GdaConnection *open_connection (void);
void display_products_contents (GdaConnection *cnc);
void create_table (GdaConnection *cnc);
void insert_data (GdaConnection *cnc);
void update_data (GdaConnection *cnc);
void delete_data (GdaConnection *cnc);

void run_sql_non_select (GdaConnection *cnc, const gchar *sql);

int
main (int argc, char *argv[])
{
  gda_init ();

  GdaConnection *cnc;

  GError *error = NULL;

	/* open connections */
	cnc = open_connection ();
	create_table (cnc);

	insert_data (cnc);
	display_products_contents (cnc);

	update_data (cnc);
	display_products_contents (cnc);

	delete_data (cnc);
	display_products_contents (cnc);

  if (!gda_connection_close (cnc,&error))
    {
      g_print ("Could not close connection: %s\n",
               error && error->message ? error->message : "No detail");
    }

	g_object_unref (cnc);

  return 0;
}

/*
 * Open a connection to the example.db file
 */
GdaConnection *
open_connection ()
{
        GdaConnection *cnc;
        GError *error = NULL;
	GdaSqlParser *parser;

	/* open connection */
        cnc = gda_connection_open_from_string ("SQLite", "DB_DIR=.;DB_NAME=example_db", NULL,
					       GDA_CONNECTION_OPTIONS_NONE,
					       &error);
        if (!cnc) {
                g_print ("Could not open connection to SQLite database in example_db.db file: %s\n",
                         error && error->message ? error->message : "No detail");
                exit (1);
        }

	/* create an SQL parser */
	parser = gda_connection_create_parser (cnc);
	if (!parser) /* @cnc does not provide its own parser => use default one */
		parser = gda_sql_parser_new ();
	/* attach the parser object to the connection */
	g_object_set_data_full (G_OBJECT (cnc), "parser", parser, g_object_unref);

        return cnc;
}

/*
 * Create a "products" table
 */
void
create_table (GdaConnection *cnc)
{
	run_sql_non_select (cnc, "DROP table IF EXISTS products");
        run_sql_non_select (cnc, "CREATE table products (ref string not null primary key, "
                            "name string not null, price real)");
}

/*
 * Insert some data
 *
 * Even though it is possible to use SQL text which includes the values to insert into the
 * table, it's better to use variables (place holders), or as is done here, convenience functions
 * to avoid SQL injection problems.
 */
void
insert_data (GdaConnection *cnc)
{
	typedef struct {
		gchar *ref;
		gchar *name;

		gboolean price_is_null;
		gfloat price;
	} RowData;
	RowData data [] = {
		{"p1", "chair", FALSE, 2.0},
		{"p2", "table", FALSE, 5.0},
		{"p3", "glass", FALSE, 1.1},
		{"p1000", "???", TRUE, 0.},
		{"p1001", "???", TRUE, 0.},
	};
	gint i;

	gboolean res;
	GError *error = NULL;
	GValue *v1, *v2, *v3;

	for (i = 0; i < sizeof (data) / sizeof (RowData); i++) {
		v1 = gda_value_new_from_string (data[i].ref, G_TYPE_STRING);
		v2 = gda_value_new_from_string (data[i].name, G_TYPE_STRING);
		if (data[i].price_is_null)
			v3 = NULL;
		else {
			v3 = gda_value_new (G_TYPE_FLOAT);
			g_value_set_float (v3, data[i].price);
		}
		
		res = gda_connection_insert_row_into_table (cnc, "products", &error, "ref", v1, "name", v2, "price", v3, NULL);

		if (!res) {
			g_error ("Could not INSERT data into the 'products' table: %s\n",
				 error && error->message ? error->message : "No detail");
		}
		gda_value_free (v1);
		gda_value_free (v2);
		if (v3)
			gda_value_free (v3);
	}
}

/*
 * Update some data
 */
void
update_data (GdaConnection *cnc)
{
	gboolean res;
	GError *error = NULL;
	GValue *v1, *v2, *v3;

	/* update data where ref is 'p1000' */
	v1 = gda_value_new_from_string ("p1000", G_TYPE_STRING);
	v2 = gda_value_new_from_string ("flowers", G_TYPE_STRING);
	v3 = gda_value_new (G_TYPE_FLOAT);
	g_value_set_float (v3, 1.99);
		
	res = gda_connection_update_row_in_table (cnc, "products", "ref", v1, &error, "name", v2, "price", v3, NULL);

	if (!res) {
		g_error ("Could not UPDATE data in the 'products' table: %s\n",
			 error && error->message ? error->message : "No detail");
	}
	gda_value_free (v1);
	gda_value_free (v2);
	gda_value_free (v3);
}

/*
 * Delete some data
 */
void
delete_data (GdaConnection *cnc)
{
	gboolean res;
	GError *error = NULL;
	GValue *v;

	/* delete data where name is 'table' */
	v = gda_value_new_from_string ("table", G_TYPE_STRING);
	res = gda_connection_delete_row_from_table (cnc, "products", "name", v, &error);
	if (!res) {
		g_error ("Could not DELETE data from the 'products' table: %s\n",
			 error && error->message ? error->message : "No detail");
	}
	gda_value_free (v);

	/* delete data where price is NULL */
	res = gda_connection_delete_row_from_table (cnc, "products", "price", NULL, &error);
	if (!res) {
		g_error ("Could not DELETE data from the 'products' table: %s\n",
			 error && error->message ? error->message : "No detail");
	}
}

/* 
 * display the contents of the 'products' table 
 */
void
display_products_contents (GdaConnection *cnc)
{
	GdaDataModel *data_model;
	GdaSqlParser *parser;
	GdaStatement *stmt;
	gchar *sql = "SELECT ref, name, price FROM products";
	GError *error = NULL;

	parser = g_object_get_data (G_OBJECT (cnc), "parser");
	stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
	data_model = gda_connection_statement_execute_select (cnc, stmt, NULL, &error);
	g_object_unref (stmt);
        if (!data_model) 
                g_error ("Could not get the contents of the 'products' table: %s\n",
                         error && error->message ? error->message : "No detail");
	gda_data_model_dump (data_model, stdout);
	g_object_unref (data_model);
}

/*
 * run a non SELECT command and stops if an error occurs
 */
void
run_sql_non_select (GdaConnection *cnc, const gchar *sql)
{
        GdaStatement *stmt;
        GError *error = NULL;
        gint nrows;
	const gchar *remain;
	GdaSqlParser *parser;

	parser = g_object_get_data (G_OBJECT (cnc), "parser");
	stmt = gda_sql_parser_parse_string (parser, sql, &remain, &error);
	if (remain) 
		g_print ("REMAINS: %s\n", remain);

        nrows = gda_connection_statement_execute_non_select (cnc, stmt, NULL, NULL, &error);
        if (nrows == -1)
                g_error ("NON SELECT error: %s\n", error && error->message ? error->message : "no detail");
	g_object_unref (stmt);
}
