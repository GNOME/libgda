#include <libgda/libgda.h>
#include <virtual/libgda-virtual.h>
#include <sql-parser/gda-sql-parser.h>
#include <stdlib.h>
#include <string.h>

static GdaDataModel *run_sql_select (GdaConnection *cnc, const gchar *sql);
static gboolean run_sql_non_select (GdaConnection *cnc, const gchar *sql);

int 
main (int argc, char **argv)
{
	GError *error = NULL;	
	GdaConnection *cnc;
	GdaVirtualProvider *provider;
	GdaDataModel *rw_model;
	gchar *file;
	
	gda_init ();

	provider = gda_vprovider_data_model_new ();
	cnc = gda_virtual_connection_open (provider, NULL);
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
	g_object_unref (options);

	/* Add data models to connection */
	if (!gda_vconnection_data_model_add_model (GDA_VCONNECTION_DATA_MODEL (cnc), rw_model, "results", &error)) 
		g_error ("Add RW model error: %s\n", error && error->message ? error->message : "no detail");
	if (!gda_vconnection_data_model_add_model (GDA_VCONNECTION_DATA_MODEL (cnc), city_model, "city", &error)) 
		g_error ("Add city model error: %s\n", error && error->message ? error->message : "no detail");
	if (!gda_vconnection_data_model_add_model (GDA_VCONNECTION_DATA_MODEL (cnc), country_model, "country", &error)) 
		g_error ("Add country model error: %s\n", error && error->message ? error->message : "no detail");

	/* test */
	GdaDataModel *model;
	model = run_sql_select (cnc, "SELECT * FROM city");
	gda_data_model_dump (model, stdout);
	g_object_unref (model);
	model = run_sql_select (cnc, "SELECT __gda_row_nb, * FROM city");
	gda_data_model_dump (model, stdout);
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

	g_print ("Ok.\n");
	return EXIT_SUCCESS;
}

static GdaDataModel *
run_sql_select (GdaConnection *cnc, const gchar *sql)
{
	GdaStatement *stmt;
	GError *error = NULL;
	GdaDataModel *res;
	GdaSqlParser *parser;

	parser = gda_connection_create_parser (cnc);
	stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
	g_object_unref (parser);

        res = gda_connection_statement_execute_select (cnc, stmt, NULL, &error);
        g_object_unref (stmt);
	if (!res) 
		g_error ("Could not execute query: %s\n", 
			 error && error->message ? error->message : "no detail");
	return res;
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
		if (error)
			g_error_free (error);
		return FALSE;
	}

	return TRUE;
}
