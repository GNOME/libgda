#include <libgda/libgda.h>

#include "common.h"
GdaDataModel *get_products_contents (GdaConnection *cnc);
gboolean      copy_products_1 (GdaConnection *source, GdaConnection *dest);
gboolean      copy_products_2 (GdaConnection *source, GdaConnection *dest);

int
main (int argc, char *argv[])
{
        gda_init ("LibgdaCopyTable", "1.0", argc, argv);

        GdaClient *client;
        GdaConnection *s_cnc, *d_cnc;

        /* Create a GdaClient object which is the central object which manages all connections */
        client = gda_client_new ();

	/* open connections */
	s_cnc = open_source_connection (client);
	d_cnc = open_destination_connection (client);

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
	GdaDataModel *data_model;
	GdaCommand *command;
	gchar *sql = "SELECT ref, name, price, wh_stored FROM products LIMIT 10";
	GError *error = NULL;

	command = gda_command_new (sql, GDA_COMMAND_TYPE_SQL, GDA_COMMAND_OPTION_STOP_ON_ERRORS);
	data_model = gda_connection_execute_select_command (cnc, command, NULL, &error);
	gda_command_free (command);
        if (!data_model) 
                g_error ("Could not get the contents of the 'products' table: %s\n",
                         error && error->message ? error->message : "No detail");
	return data_model;
}

/*
 * method 1 to copy the data model: 
 *
 * - Get the contents of the 'products' table as a 'source' data model, 
 * - create an INSERT GdaQuery with parameters, 
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
	GdaParameterList *params = NULL;
	gint nrows, row;
	GdaQuery *query = NULL;
	gboolean retval = TRUE;

	source = get_products_contents (s_cnc);

	/* instruct that queries referencing the default dictionary will be executed using the d_cnc connection */
	gda_dict_set_connection (default_dict, d_cnc);

	/* query creation */
	sql = "INSERT INTO products_copied1 (ref, name, price, wh_stored) VALUES ("
		"## /* name:ref type:gchararray */, "
		"## /* name:name type:gchararray */, "
		"## /* name:price type:gdouble */, "
		"## /* name:location type:gint */)";
	query = gda_query_new_from_sql (NULL, sql, &error);
	if (error) {
		g_print ("Error creating INSERT query: %s\n",
                         error && error->message ? error->message : "No detail");
		g_error_free (error);
		retval = FALSE;
		goto end;
	}

	/* actual execution */
	params = gda_query_get_parameter_list (query);
	nrows = gda_data_model_get_n_rows (source);
	for (row = 0; row < nrows; row++) {
		GdaParameter *p;
		GValue *value;
		GdaObject *res;

		p = gda_parameter_list_find_param (params, "ref");
		gda_parameter_set_value (p, gda_data_model_get_value_at (source, 0, row));
		p = gda_parameter_list_find_param (params, "name");
		gda_parameter_set_value (p, gda_data_model_get_value_at (source, 1, row));
		p = gda_parameter_list_find_param (params, "price");
		value = gda_value_new (G_TYPE_DOUBLE);
		g_value_set_double (value, g_value_get_double (gda_data_model_get_value_at (source, 2, row)) * 1.05);
		gda_parameter_set_value (p, value);
		gda_value_free (value);
		p = gda_parameter_list_find_param (params, "location");
		gda_parameter_set_value (p, gda_data_model_get_value_at (source, 3, row));

		res = gda_query_execute (query, params, FALSE, &error);
		if (!res) {
			g_print ("Could execute INSERT query: %s\n",
                         error && error->message ? error->message : "No detail");
			g_error_free (error);
			retval = FALSE;
			goto end;
		}
		else
			g_object_unref (res);
	}

 end:
	if (query)
		g_object_unref (query);
	if (params)
		g_object_unref (params);
	return retval;
}

/*
 * method 2 to copy the data model: 
 *
 * - Get the contents of the 'products' table as a 'source' data model, 
 * - create a GdaDataModelQuery data model, 
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
	GdaQuery *query = NULL;
	gboolean retval = TRUE;

	source = get_products_contents (s_cnc);

	/* instruct that queries referencing the default dictionary will be executed using the d_cnc connection */
	gda_dict_set_connection (default_dict, d_cnc);

	/* create GdaDataModelQuery and set it up */
	query = gda_query_new_from_sql (NULL, "SELECT ref, name, price, wh_stored FROM products_copied2", &error);
	dest = gda_data_model_query_new (query);
	g_object_unref (query);

	sql = "INSERT INTO products_copied2 (ref, name, price, wh_stored) VALUES ("
		"## /* name:+0 type:gchararray */, "
		"## /* name:+1 type:gchararray */, "
		"## /* name:+2 type:gdouble */, "
		"## /* name:+3 type:gint */)";
	if (!gda_data_model_query_set_modification_query (GDA_DATA_MODEL_QUERY (dest), sql, &error)) {
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
