/* Forms/Read-write form
 *
 * The GdauiForm widget displays data stored in a GdaDataModel.
 */

#include <libgdaui/libgdaui.h>
#include <sql-parser/gda-sql-parser.h>

extern GdaConnection *demo_cnc;
extern GdaSqlParser *demo_parser;
static GtkWidget *window = NULL;

GtkWidget *
do_form_rw (GtkWidget *do_widget)
{  
	if (!window) {
		GdaStatement *stmt;
		GtkWidget *vbox;
		GtkWidget *label;
		GdaDataModel *model;
		GtkWidget *form;
		
		window = gtk_dialog_new_with_buttons ("GdauiForm (RW)",
						      GTK_WINDOW (do_widget),
						      0,
						      GTK_STOCK_CLOSE,
						      GTK_RESPONSE_NONE,
						      NULL);
		
		g_signal_connect (window, "response",
				  G_CALLBACK (gtk_widget_destroy), NULL);
		g_signal_connect (window, "destroy",
				  G_CALLBACK (gtk_widget_destroyed), &window);
		
		vbox = gtk_vbox_new (FALSE, 5);
		gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), vbox, TRUE, TRUE, 0);
		gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
		
		label = gtk_label_new ("The following GdauiForm widget displays data from the 'products' table.\n\n"
				       "As modification queries are provided, the data is read-write\n(except for the 'price' "
				       "field as these queries voluntarily omit that field).");
		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
		
		/* Create the demo widget */
		stmt = gda_sql_parser_parse_string (demo_parser, 
						    "SELECT ref, category, name, price, wh_stored FROM products", 
						    NULL, NULL);
		model = gda_connection_statement_execute_select (demo_cnc, stmt, NULL, NULL);
		g_object_unref (stmt);
		GError *error = NULL;
		if (!gda_data_select_compute_modification_statements (GDA_DATA_SELECT (model), &error)) {
			g_print ("===> %s\n", error && error->message ? error->message : "No detail");
		}
		form = gdaui_form_new (model);
		g_object_unref (model);

		gtk_box_pack_start (GTK_BOX (vbox), form, TRUE, TRUE, 0);
	}

	if (!GTK_WIDGET_VISIBLE (window))
		gtk_widget_show_all (window);
	else
		gtk_widget_destroy (window);

	return window;
}


