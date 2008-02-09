<![CDATA[#include <libgda/libgda.h>
#include <sql-parser/gda-sql-parser.h>

GdaConnection *open_connection (GdaClient *client);
void display_products_contents (GdaConnection *cnc);
void create_table (GdaConnection *cnc);
void run_sql_non_select (GdaConnection *cnc, const gchar *sql);

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
	GdaSqlParser *parser;

	/* open connection */
        cnc = gda_client_open_connection_from_string (client, "SQLite", "DB_DIR=.;DB_NAME=example_db", NULL, NULL,
						      GDA_CONNECTION_OPTIONS_DONT_SHARE,
						      &error);
        if (!cnc) {
                g_print ("Could not open connection to SQLite database in example_db.db file: %s\n",
                         error && error->message ? error->message : "No detail");
                exit (1);
        }

	/* create an SQL parser */
	parser = gda_connection_create_parser (cnc);
	if (!parser) /* @cnc doe snot provide its own parser => use default one */
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
	run_sql_non_select (cnc, "INSERT INTO products VALUES ('p1', 'chair', 2.0)");
	run_sql_non_select (cnc, "INSERT INTO products VALUES ('p2', 'table', 5.0)");
	run_sql_non_select (cnc, "INSERT INTO products VALUES ('p3', 'glass', 1.1)");
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
]]>
