#include <libgda/libgda.h>
#include <virtual/libgda-virtual.h>
#include <stdlib.h>
#include <string.h>

static gboolean run_sql_non_select (GdaConnection *cnc, const gchar *sql);

int 
main (int argc, char **argv)
{
	GError *error = NULL;	
	GdaConnection *cnc;
	GdaVirtualProvider *provider;
	GdaDataModel *rw_model;
	gchar *file;
	
	gda_init ("SQlite virtual test", "1.0", argc, argv);

	provider = gda_vprovider_data_model_new ();
	cnc = gda_server_provider_create_connection (NULL, GDA_SERVER_PROVIDER (provider), NULL, NULL, NULL, 0);
	g_assert (gda_connection_open (cnc, NULL));

	/* create RW data model to store results */
	rw_model = gda_data_model_array_new_with_g_types (2, G_TYPE_STRING, G_TYPE_INT);
	gda_data_model_set_column_name (rw_model, 0, "country");
	gda_data_model_set_column_name (rw_model, 1, "population");

	/* load CSV data models */
	GdaDataModel *country_model, *city_model;
	GdaParameterList *plist = gda_parameter_list_new_inline (NULL, "TITLE_AS_FIRST_LINE", G_TYPE_BOOLEAN, TRUE, NULL);
	file = g_build_filename (CHECK_FILES, "tests", "data-models", "city.csv", NULL);
	city_model = gda_data_model_import_new_file (file, TRUE, plist);
	g_free (file);
	file = g_build_filename (CHECK_FILES, "tests", "data-models", "country.csv", NULL);
	country_model = gda_data_model_import_new_file (file, TRUE, plist);
	g_free (file);
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
	gchar *export, *expected;
	export = gda_data_model_export_to_string (rw_model, GDA_DATA_MODEL_IO_TEXT_SEPARATED,
						  NULL, 0, NULL, 0, NULL);
	file = g_build_filename (CHECK_FILES, "tests", "data-models", "check_virtual.csv", NULL);
	if (!g_file_get_contents (file, &expected, NULL, NULL))
		return EXIT_FAILURE;
	g_free (file);
	if (strcmp (export, expected))
		return EXIT_FAILURE;

	g_free (export);
	g_free (expected);
	g_object_unref (city_model);
	g_object_unref (country_model);
	g_object_unref (cnc);
	g_object_unref (provider);

	return EXIT_SUCCESS;
}


static gboolean
run_sql_non_select (GdaConnection *cnc, const gchar *sql)
{
	GdaCommand *command;
	gint nrows;

	command = gda_command_new (sql, GDA_COMMAND_TYPE_SQL, 0);
	nrows = gda_connection_execute_non_select_command (cnc, command, NULL, NULL);
	gda_command_free (command);
	if (nrows == -1) 
		return FALSE;
	else
		return TRUE;
}
