#include <libgda/libgda.h>
#include <virtual/libgda-virtual.h>
#include <sql-parser/gda-sql-parser.h>

#define OUTFILE "results.csv"

static gboolean run_sql_non_select (GdaConnection *cnc, const gchar *sql);

int 
main (int argc, char **argv)
{
	GError *error = NULL;	
	GdaConnection *cnc;
	GdaVirtualProvider *provider;
	GdaDataModel *rw_model;
	
	gda_init ("SQlite virtual test", "1.0", argc, argv);

	provider = gda_vprovider_data_model_new ();
	cnc = gda_virtual_connection_open (provider, NULL);
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
