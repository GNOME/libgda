/*
 * Copyright (C) 2007 - 2012 Vivien Malerba <malerba@gnome-db.org>
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

#include "common.h"
GdaDataModel *get_products_contents (GdaConnection *cnc);
gboolean      copy_products_1 (GdaConnection *source, GdaConnection *dest);
gboolean      copy_products_2 (GdaConnection *source, GdaConnection *dest);

int
main (int argc, char *argv[])
{
        gda_init ();

        GdaConnection *s_cnc, *d_cnc;

	/* open connections */
	s_cnc = open_source_connection ();
	d_cnc = open_destination_connection ();

	/* copy some contents of the 'products' table into the 'products_copied', method 1 */
	if (! copy_products_1 (s_cnc, d_cnc)) 
		exit (1);

	/* copy some contents of the 'products' table into the 'products_copied', method 2 */
	if (! copy_products_2 (s_cnc, d_cnc)) 
		exit (1);

        gda_connection_close (s_cnc);
        gda_connection_close (d_cnc);
        return 0;
}

/* 
 * get some of the contents of the 'products' table 
 */
GdaDataModel *
get_products_contents (GdaConnection *cnc)
{
	GdaStatement *stmt;
	gchar *sql = "SELECT ref, name, price, wh_stored FROM products LIMIT 10";
        GError *error = NULL;
        GdaDataModel *data_model;
        GdaSqlParser *parser;

        parser = gda_connection_create_parser (cnc);
        stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
        g_object_unref (parser);

        data_model = gda_connection_statement_execute_select (cnc, stmt, NULL, &error);
        g_object_unref (stmt);
        if (!data_model)
                g_error ("Could not get the contents of the 'products' table: %s\n",
                         error && error->message ? error->message : "no detail");
        return data_model;
}

/*
 * method 1 to copy the data model: 
 *
 * - Get the contents of the 'products' table as a 'source' data model, 
 * - create an INSERT statement with parameters, 
 * - execute the query as many times as there are rows in the source data model, each time setting the parameters' 
 * - values with the source's contents.
 *
 * The price of the products is increased by 5% in the process.
 */
gboolean
copy_products_1 (GdaConnection *s_cnc, GdaConnection *d_cnc)
{
        gchar *sql;
	GError *error = NULL;
	GdaDataModel *source = NULL;
	GdaSet *params = NULL;
	gint nrows, row;
	GdaSqlParser *parser;
	GdaStatement *stmt = NULL;
	gboolean retval = TRUE;

	source = get_products_contents (s_cnc);

	/* query creation */
	parser = gda_connection_create_parser (d_cnc);
	sql = "INSERT INTO products_copied1 (ref, name, price, wh_stored) VALUES ("
		"## /* name:ref type:gchararray */, "
		"## /* name:name type:gchararray */, "
		"## /* name:price type:gdouble */, "
		"## /* name:location type:gint */)";
	stmt = gda_sql_parser_parse_string (parser, sql, NULL, &error);
	g_object_unref (parser);
	if (!stmt) {
		g_print ("Error creating INSERT query: %s\n",
                         error && error->message ? error->message : "No detail");
		g_error_free (error);
		retval = FALSE;
		goto end;
	}

	/* actual execution */
	if (!gda_statement_get_parameters (stmt, &params, &error)) {
		g_print ("Error getting query's parameters: %s\n",
                         error && error->message ? error->message : "No detail");
		g_error_free (error);
		retval = FALSE;
		goto end;
	}
	nrows = gda_data_model_get_n_rows (source);
	for (row = 0; row < nrows; row++) {
		GdaHolder *p;
		GValue *value;
		gint res;

		/* REM: exit status for each function should be check but has been omitted for clarity */
		p = gda_set_get_holder (params, "ref");
		gda_holder_set_value (p, gda_data_model_get_value_at (source, 0, row, NULL), NULL);
		p = gda_set_get_holder (params, "name");
		gda_holder_set_value (p, gda_data_model_get_value_at (source, 1, row, NULL), NULL);
		p = gda_set_get_holder (params, "price");
		value = gda_value_new (G_TYPE_DOUBLE);
		g_value_set_double (value, g_value_get_double (gda_data_model_get_value_at (source, 2, row, NULL)) * 1.05);
		gda_holder_set_value (p, value, NULL);
		gda_value_free (value);
		p = gda_set_get_holder (params, "location");
		gda_holder_set_value (p, gda_data_model_get_value_at (source, 3, row, NULL), NULL);

		res = gda_connection_statement_execute_non_select (d_cnc, stmt, params, NULL, &error);
		if (res == -1) {
			g_print ("Could execute INSERT query: %s\n",
                         error && error->message ? error->message : "No detail");
			g_error_free (error);
			retval = FALSE;
			goto end;
		}
	}

 end:
	if (stmt)
		g_object_unref (stmt);
	if (params)
		g_object_unref (params);
	return retval;
}

/*
 * method 2 to copy the data model: 
 *
 * - Get the contents of the 'products' table as a 'source' data model, 
 * - create a GdaDataSelect data model, 
 * - directly copy the contents of the source data model into it.
 *
 * The price of the products is unchanged in the process.
 */
gboolean
copy_products_2 (GdaConnection *s_cnc, GdaConnection *d_cnc)
{
        gchar *sql;
	GError *error = NULL;
	GdaDataModel *source = NULL, *dest = NULL;
	GdaStatement *stmt = NULL;
	gboolean retval = TRUE;
	GdaSqlParser *parser;
	
	source = get_products_contents (s_cnc);

	/* create GdaDataSelect and set it up */
	parser = gda_connection_create_parser (d_cnc);
	sql = "SELECT ref, name, price, wh_stored FROM products_copied2";
	stmt = gda_sql_parser_parse_string (parser, sql, NULL, &error);
	g_assert (stmt);
	dest = gda_connection_statement_execute_select (d_cnc, stmt, NULL, NULL);
	g_object_unref (stmt);
	g_object_unref (parser);

	if (!gda_data_select_set_row_selection_condition_sql (GDA_DATA_SELECT (dest), "ref = ##-0::string", &error)) {
		g_print ("Error setting the GdaDataSelect's condition: %s\n",
                         error && error->message ? error->message : "No detail");
		g_error_free (error);
		retval = FALSE;
		goto end;
	}

	sql = "INSERT INTO products_copied2 (ref, name, price, wh_stored) VALUES (##+0::string, ##+1::string, ##+2::gdouble, ##+3::gint)";

	if (!gda_data_select_set_modification_statement_sql (GDA_DATA_SELECT (dest), sql, &error)) {
		g_print ("Error setting the INSERT query: %s\n",
                         error && error->message ? error->message : "No detail");
		g_error_free (error);
		retval = FALSE;
		goto end;
	}

	if (! gda_data_model_import_from_model (dest, source, TRUE, NULL, &error)) {
		g_print ("Error importing data: %s\n",
                         error && error->message ? error->message : "No detail");
		g_error_free (error);
		retval = FALSE;
		goto end;
	}

 end:
	if (source)
		g_object_unref (source);
	if (dest)
		g_object_unref (dest);
	return retval;
}
