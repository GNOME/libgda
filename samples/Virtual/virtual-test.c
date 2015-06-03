/*
 * Copyright (C) 2007 - 2014 Vivien Malerba <malerba@gnome-db.org>
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
#include <virtual/libgda-virtual.h>
#include <sql-parser/gda-sql-parser.h>

#define OUTFILE "results.csv"

static gboolean run_sql_non_select (GdaConnection *cnc, const gchar *sql);

int 
main ()
{
	GError *error = NULL;	
	GdaConnection *cnc;
	GdaVirtualProvider *provider;
	GdaDataModel *rw_model;
	
	gda_init ();

	provider = gda_vprovider_data_model_new ();
	cnc = gda_virtual_connection_open (provider, GDA_CONNECTION_OPTIONS_NONE, NULL);
	g_assert (cnc);

	/* create RW data model to store results */
	rw_model = gda_data_model_array_new_with_g_types (2, G_TYPE_STRING, G_TYPE_INT);
	gda_data_model_set_column_name (rw_model, 0, "country");
	gda_data_model_set_column_name (rw_model, 1, "population");

	/* load CSV data models */
	GdaDataModel *country_model, *city_model;
	GdaSet *plist = gda_set_new_inline (1, "TITLE_AS_FIRST_LINE", G_TYPE_BOOLEAN, TRUE);
	city_model = gda_data_model_import_new_file ("city.csv", TRUE, plist);
	country_model = gda_data_model_import_new_file ("country.csv", TRUE, plist);
	g_object_unref (plist);

	/* Add data models to connection */
	if (!gda_vconnection_data_model_add_model (GDA_VCONNECTION_DATA_MODEL (cnc), rw_model, "results", &error)) 
		g_error ("Add RW model error: %s\n", error && error->message ? error->message : "no detail");
	if (!gda_vconnection_data_model_add_model (GDA_VCONNECTION_DATA_MODEL (cnc), city_model, "city", &error)) 
		g_error ("Add city model error: %s\n", error && error->message ? error->message : "no detail");
	if (!gda_vconnection_data_model_add_model (GDA_VCONNECTION_DATA_MODEL (cnc), country_model, "country", &error)) 
		g_error ("Add country model error: %s\n", error && error->message ? error->message : "no detail");

	/* compute list of countries where population >= 1M */
	if (! run_sql_non_select (cnc, "INSERT INTO results SELECT co.name, sum (ci.population) as pop FROM country co INNER JOIN city ci ON (co.code=ci.countrycode) GROUP BY co.name HAVING sum (ci.population) > 10000000;"))
		g_error ("Could not run computation query");

	/* save resulting data model to CSV */
	plist = gda_set_new_inline (1, "OVERWRITE", G_TYPE_BOOLEAN, TRUE);
	gda_data_model_export_to_file (rw_model, GDA_DATA_MODEL_IO_TEXT_SEPARATED, OUTFILE,
				       NULL, 0, NULL, 0, plist, NULL);
	g_object_unref (plist);

	g_object_unref (city_model);
	g_object_unref (country_model);
	g_object_unref (cnc);
	g_object_unref (provider);

	return 0;
}


static gboolean
run_sql_non_select (GdaConnection *cnc, const gchar *sql)
{
	GdaStatement *stmt;
        GError *error = NULL;
        gint nrows;
        GdaSqlParser *parser;

        parser = gda_connection_create_parser (cnc);
        stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
        g_object_unref (parser);

        nrows = gda_connection_statement_execute_non_select (cnc, stmt, NULL, NULL, &error);
        g_object_unref (stmt);
        if (nrows == -1) {
		g_print ("NON SELECT error: %s\n", error && error->message ? error->message : "no detail");
                g_error_free (error);
                error = NULL;
                return FALSE;
	}

	return TRUE;
}
