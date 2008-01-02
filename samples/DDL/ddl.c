#include <libgda/libgda.h>
#include <sql-parser/gda-sql-parser.h>

GdaConnection *open_connection (GdaClient *client);
void display_products_contents (GdaConnection *cnc);
void create_table (GdaConnection *cnc);

int
main (int argc, char *argv[])
{
        gda_init ("SimpleExample", "1.0", argc, argv);

        GdaClient *client;
        GdaConnection *cnc;

        /* Create a GdaClient object which is the central object which manages all connections */
        client = gda_client_new ();

	/* open connections */
	cnc = open_connection (client);
	create_table (cnc);
	display_products_contents (cnc);
        gda_connection_close (cnc);

        return 0;
}

/*
 * Open a connection to the example.db file
 */
GdaConnection *
open_connection (GdaClient *client)
{
        GdaConnection *cnc;
        GError *error = NULL;
        cnc = gda_client_open_connection_from_string (client, "SQLite", "DB_DIR=.;DB_NAME=ddl_db", NULL, NULL,
						      GDA_CONNECTION_OPTIONS_DONT_SHARE,
						      &error);
        if (!cnc) {
                g_print ("Could not open connection to SQLite database in example_db.db file: %s\n",
                         error && error->message ? error->message : "No detail");
                exit (1);
        }
        return cnc;
}

/*
 * Create a "products" table using a GdaServerOperation object
 */
void
create_table (GdaConnection *cnc)
{
	GError *error = NULL;
	GdaServerProvider *provider;
	GdaServerOperation *op;
	gchar *sql;
	gint i;

	/* create a new GdaServerOperation object */
	provider = gda_connection_get_provider_obj (cnc);
	op = gda_server_provider_create_operation (provider, cnc, GDA_SERVER_OPERATION_CREATE_TABLE, NULL, &error);
	if (!op) {
		g_print ("CREATE TABLE operation is not supported by the provider: %s\n",
			 error && error->message ? error->message : "No detail");
		exit (1);
	}

	/* Set parameter's values */
	/* table name */
	if (!gda_server_operation_set_value_at (op, "products", &error, "/TABLE_DEF_P/TABLE_NAME")) goto on_set_error;

	/* "id' field */
	i = 0;
	if (!gda_server_operation_set_value_at (op, "id", &error, "/FIELDS_A/@COLUMN_NAME/%d", i)) goto on_set_error;
	if (!gda_server_operation_set_value_at (op, "integer", &error, "/FIELDS_A/@COLUMN_TYPE/%d", i)) goto on_set_error;
	if (!gda_server_operation_set_value_at (op, "TRUE", &error, "/FIELDS_A/@COLUMN_AUTOINC/%d", i)) goto on_set_error;
	if (!gda_server_operation_set_value_at (op, "TRUE", &error, "/FIELDS_A/@COLUMN_PKEY/%d", i)) goto on_set_error;
	
	/* 'product_name' field */
	i++;
	if (!gda_server_operation_set_value_at (op, "product_name", &error, "/FIELDS_A/@COLUMN_NAME/%d", i)) goto on_set_error;
	if (!gda_server_operation_set_value_at (op, "varchar", &error, "/FIELDS_A/@COLUMN_TYPE/%d", i)) goto on_set_error;
	if (!gda_server_operation_set_value_at (op, "50", &error, "/FIELDS_A/@COLUMN_SIZE/%d", i)) goto on_set_error;
	if (!gda_server_operation_set_value_at (op, "TRUE", &error, "/FIELDS_A/@COLUMN_NNUL/%d", i)) goto on_set_error;


	/* Show the SQL to execute
	 * This does not always work since some operations may not be accessible through SQL
	 */
	sql = gda_server_provider_render_operation (provider, cnc, op, &error);
	if (!sql) {
		g_print ("Error rendering SQL: %s\n",
			 error && error->message ? error->message : "No detail");
		exit (1);
	}
	g_print ("SQL to execute: %s\n", sql);
	g_free (sql);

	/* Actually execute the operation */
	if (! gda_server_provider_perform_operation (provider, cnc, op, &error)) {
		g_print ("Error executing the operation: %s\n",
			 error && error->message ? error->message : "No detail");
		exit (1);
	}
	g_object_unref (op);
	return;

 on_set_error:
	g_print ("Error setting value in GdaSererOperation: %s\n",
		 error && error->message ? error->message : "No detail");
	exit (1);
}


/* 
 * display the contents of the 'products' table 
 */
void
display_products_contents (GdaConnection *cnc)
{
	GdaDataModel *data_model;
	GdaStatement *stmt;
	gchar *sql = "SELECT * FROM products";
	GError *error = NULL;
	GdaSqlParser *parser;

	parser = gda_connection_create_parser (cnc);
        if (!parser) /* @cnc doe snot provide its own parser => use default one */
                parser = gda_sql_parser_new ();

	stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
	data_model = gda_connection_statement_execute_select (cnc, stmt, NULL, 
							      GDA_STATEMENT_MODEL_RANDOM_ACCESS, &error);
	g_object_unref (stmt);
        if (!data_model) 
                g_error ("Could not get the contents of the 'products' table: %s\n",
                         error && error->message ? error->message : "No detail");
	gda_data_model_dump (data_model, stdout);
	g_object_unref (data_model);
}
