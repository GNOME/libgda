/*
 * Copyright (C) 2007 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2018 Daniel Espinosa <esodan@gmail.com>
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
#include <stdlib.h>
#include <string.h>

static GdaDataModel *run_sql_select (GdaConnection *cnc, const gchar *sql);
static gboolean run_sql_non_select (GdaConnection *cnc, const gchar *sql);
static gboolean test1 (void);
static gboolean test2 (void);

int 
main (G_GNUC_UNUSED int argc, G_GNUC_UNUSED char **argv)
{
	gint nfailed = 0;
	gda_init ();

	if (! test1 ())
		nfailed++;
	if (! test2 ())
		nfailed++;

	if (nfailed == 0) {
		g_print ("Ok, all tests passed\n");
		return EXIT_SUCCESS;
	}
	else {
		g_print ("%d failed\n", nfailed);
		return EXIT_FAILURE;
	} 
}

static gboolean
test1 (void)
{
	GError *error = NULL;	
	GdaConnection *cnc;
	GdaVirtualProvider *provider;
	GdaDataModel *rw_model;
	gchar *file;
	gboolean retval = FALSE;

	g_print ("===== %s() =====\n", __FUNCTION__);
	provider = gda_vprovider_data_model_new ();
	cnc = gda_virtual_connection_open (provider, GDA_CONNECTION_OPTIONS_NONE, NULL);
	g_object_unref (provider);
	g_assert (cnc);

	/* create RW data model to store results */
	rw_model = gda_data_model_array_new_with_g_types (2, G_TYPE_STRING, G_TYPE_INT);
	gda_data_model_set_column_name (rw_model, 0, "country");
	gda_data_model_set_column_name (rw_model, 1, "population");

	/* load CSV data models */
	GdaDataModel *country_model, *city_model;
	GdaSet *options = gda_set_new_inline (1, "TITLE_AS_FIRST_LINE", G_TYPE_BOOLEAN, TRUE);
	file = g_build_filename (CHECK_FILES, "tests", "data-models", "city.csv", NULL);
	city_model = gda_data_model_import_new_file (file, TRUE, options);
	g_free (file);
	file = g_build_filename (CHECK_FILES, "tests", "data-models", "country.csv", NULL);
	country_model = gda_data_model_import_new_file (file, TRUE, options);
	g_free (file);
	file = NULL;
	g_object_unref (options);

	/* Add data models to connection */
	if (!gda_vconnection_data_model_add_model (GDA_VCONNECTION_DATA_MODEL (cnc), rw_model, "results", &error)) 
		g_error ("Add RW model error: %s\n", error && error->message ? error->message : "no detail");
	if (!gda_vconnection_data_model_add_model (GDA_VCONNECTION_DATA_MODEL (cnc), city_model, "city", &error)) 
		g_error ("Add city model error: %s\n", error && error->message ? error->message : "no detail");
	if (!gda_vconnection_data_model_add_model (GDA_VCONNECTION_DATA_MODEL (cnc), country_model, "country", &error)) 
		g_error ("Add country model error: %s\n", error && error->message ? error->message : "no detail");
	g_object_unref (city_model);
	g_object_unref (country_model);

	/* test */
	GdaDataModel *model;
	model = run_sql_select (cnc, "SELECT * FROM city");
	gda_data_model_dump (model, stdout);
	g_message ("Removing City Data Model from select: 97");
	g_object_unref (model);
	model = run_sql_select (cnc, "SELECT __gda_row_nb, * FROM city");
	gda_data_model_dump (model, stdout);
	g_message ("Removing second City Data Model from select :102");
	g_object_unref (model);
	

	/* compute list of countries where population >= 1M */
	if (! run_sql_non_select (cnc, "INSERT INTO results SELECT co.name, sum (ci.population) as pop FROM country co INNER JOIN city ci ON (co.code=ci.countrycode) GROUP BY co.name HAVING sum (ci.population) > 10000000 ORDER BY co.name DESC"))
		g_error ("Could not run computation query");

	/* save resulting data model to CSV */
	gchar *export, *expected;
	export = gda_data_model_export_to_string (rw_model, GDA_DATA_MODEL_IO_TEXT_SEPARATED,
						  NULL, 0, NULL, 0, NULL);
	file = g_build_filename (CHECK_FILES, "tests", "data-models", "check_virtual.csv", NULL);
	if (!g_file_get_contents (file, &expected, NULL, NULL))
		goto out;
	if (strcmp (export, expected))
		goto out;

	retval = TRUE;
 out:

	g_free (file);
	g_free (export);
	g_free (expected);
	g_object_unref (cnc);

	g_print ("%s() is %s\n", __FUNCTION__, retval ? "Ok" : "NOT Ok");
	return retval;
}

static gboolean
test2 (void)
{
	GError *error = NULL;	
	GdaConnection *cnc;
	GdaVirtualProvider *provider;
	gchar *file;
	gboolean retval = FALSE;

	g_print ("===== %s () =====\n", __FUNCTION__);
	provider = gda_vprovider_data_model_new ();
	cnc = gda_virtual_connection_open (provider, GDA_CONNECTION_OPTIONS_NONE, NULL);
	g_object_unref (provider);
	g_assert (cnc);

	/* load CSV data models */
	GdaDataModel *country_model, *city_model;
	GdaSet *options = gda_set_new_inline (1, "TITLE_AS_FIRST_LINE", G_TYPE_BOOLEAN, TRUE);
	file = g_build_filename (CHECK_FILES, "tests", "data-models", "city.csv", NULL);
	city_model = gda_data_model_import_new_file (file, TRUE, options);
	g_free (file);
	file = g_build_filename (CHECK_FILES, "tests", "data-models", "country.csv", NULL);
	country_model = gda_data_model_import_new_file (file, TRUE, options);
	g_free (file);
	g_object_unref (options);

	/* Add data models to connection */
	if (!gda_vconnection_data_model_add_model (GDA_VCONNECTION_DATA_MODEL (cnc), city_model, "city", &error)) 
		g_error ("Add city model error: %s\n", error && error->message ? error->message : "no detail");
	if (!gda_vconnection_data_model_add_model (GDA_VCONNECTION_DATA_MODEL (cnc), country_model, "country", &error)) 
		g_error ("Add country model error: %s\n", error && error->message ? error->message : "no detail");
	g_object_unref (city_model);
	g_object_unref (country_model);

	/* test */
	GdaDataModel *model;
	model = run_sql_select (cnc, "SELECT c.*, o.name FROM city c INNER JOIN country o ON (c.countrycode = o.code)");
	gda_data_model_dump (model, stdout);
	g_message ("Removing Data Model from select : 170");
	g_object_unref (model);

	retval = TRUE;

	g_object_unref (cnc);

	g_print ("%s() is %s\n", __FUNCTION__, retval ? "Ok" : "NOT Ok");
	return retval;
}

static GdaDataModel *
run_sql_select (GdaConnection *cnc, const gchar *sql)
{
	GError *error = NULL;
	GdaDataModel *res = NULL;

	g_print ("EXECUTING [%s]\n", sql);
	res = gda_connection_execute_select_command (cnc, sql, &error);
	if (res == NULL) {
		g_print ("Could not execute query: %s\n",
			 error && error->message ? error->message : "no detail");
		if (error != NULL) {
			g_error_free (error);
		}
	}
	return res;
}


static gboolean
run_sql_non_select (GdaConnection *cnc, const gchar *sql)
{
	GError *error = NULL;

	g_print ("EXECUTING [%s]\n", sql);
	gda_connection_execute_non_select_command (cnc, sql, &error);
	if (error != NULL) {
		g_print ("NON SELECT error: %s\n", error->message ? error->message : "no detail");
		g_error_free (error);
		return FALSE;
	}

	return TRUE;
}
