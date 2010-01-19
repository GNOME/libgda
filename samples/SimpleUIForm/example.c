#include <libgda-ui/libgda-ui.h>
#include <sql-parser/gda-sql-parser.h>

static GdaConnection *open_connection (void);
static GdaDataModel *get_customers (GdaConnection *cnc);
static GdaDataModel *get_salesrep (GdaConnection *cnc);

static void destroy (GtkWidget *widget, gpointer data)
{
	gtk_main_quit ();
}

int
main (int argc, char *argv[])
{
	gtk_init (&argc, &argv);
	gdaui_init ();

	/* open connection */
        GdaConnection *cnc;
	cnc = open_connection ();

	/* create data models */
	GdaDataModel *customers, *salesrep;
	customers = get_customers (cnc);
	salesrep = get_salesrep (cnc);
	
	/* create UI */
	GtkWidget *window, *vbox, *button, *form;
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	g_signal_connect_swapped (window, "destroy",
				  G_CALLBACK (destroy),
				  window);
	gtk_container_set_border_width (GTK_CONTAINER (window), 10);

	vbox = gtk_vbox_new (FALSE, 5);
	gtk_container_add (GTK_CONTAINER (window), vbox);

	/* main form to list customers */
	form = gdaui_form_new (customers);
	gtk_box_pack_start (GTK_BOX (vbox), form, TRUE, TRUE, 0);

	GdaDataModelIter *iter;
	GdaHolder *holder;
	iter = gdaui_data_selector_get_data_set (GDAUI_DATA_SELECTOR (form));
	holder = gda_data_model_iter_get_holder_for_field (iter, 2);
	gda_holder_set_source_model (holder, salesrep, 0, NULL);

	/* button to quit */
	button = gtk_button_new_with_label ("Quit");
	gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
	g_signal_connect_swapped (button, "clicked",
				  G_CALLBACK (gtk_widget_destroy),
				  window);

	gtk_widget_show_all (window);
	gtk_main ();

        gda_connection_close (cnc);
	g_object_unref (cnc);

        return 0;
}

/*
 * Open a connection to the example.db file
 */
static GdaConnection *
open_connection ()
{
        GdaConnection *cnc;
        GError *error = NULL;
	GdaSqlParser *parser;

	/* open connection */
        cnc = gda_connection_open_from_dsn ("SalesTest", NULL,
					    GDA_CONNECTION_OPTIONS_NONE,
					    &error);
        if (!cnc) {
                g_print ("Could not open connection to SalesTest DSN: %s\n",
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

static GdaDataModel *
get_customers (GdaConnection *cnc)
{
	GdaDataModel *data_model;
	GdaSqlParser *parser;
	GdaStatement *stmt;
	gchar *sql = "SELECT id, name, default_served_by FROM customers ORDER BY name";
	GError *error = NULL;

	parser = g_object_get_data (G_OBJECT (cnc), "parser");
	stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
	data_model = gda_connection_statement_execute_select (cnc, stmt, NULL, &error);
	g_object_unref (stmt);
        if (!data_model) 
                g_error ("Could not get the contents of the 'products' table: %s\n",
                         error && error->message ? error->message : "No detail");
	gda_data_model_dump (data_model, stdout);
	return data_model;
}

static GdaDataModel *
get_salesrep (GdaConnection *cnc)
{
	GdaDataModel *data_model;
	GdaSqlParser *parser;
	GdaStatement *stmt;
	gchar *sql = "SELECT id, name FROM salesrep ORDER BY name";
	GError *error = NULL;

	parser = g_object_get_data (G_OBJECT (cnc), "parser");
	stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
	data_model = gda_connection_statement_execute_select (cnc, stmt, NULL, &error);
	g_object_unref (stmt);
        if (!data_model) 
                g_error ("Could not get the contents of the 'products' table: %s\n",
                         error && error->message ? error->message : "No detail");
	gda_data_model_dump (data_model, stdout);
	return data_model;
}
