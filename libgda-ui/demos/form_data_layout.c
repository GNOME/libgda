/* Forms/Automatic data_layout
 *
 * A GdauiForm widget where automatic 'data_layout' is used to display
 * 
 */

#include <libgda-ui/libgda-ui.h>
#include <sql-parser/gda-sql-parser.h>
#include "demo-common.h"

extern GdaConnection *demo_cnc;
extern GdaSqlParser *demo_parser;
static GtkWidget *window = NULL;

GtkWidget *
do_form_data_layout (GtkWidget *do_widget)
{  
	if (!window) {
		GdaStatement *stmt;
		GtkWidget *vbox;
		GtkWidget *label;
		GdaDataModel *model;
		GtkWidget *form;
		GdauiRawForm *raw_form;
		GdaSet *data_set;
		GdaHolder *param;
		GValue *value;
		
		window = gtk_dialog_new_with_buttons ("Form with automatic 'data_layout'",
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
#if GTK_CHECK_VERSION(2,18,0)
		gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (window))),
				    vbox, TRUE, TRUE, 0);
#else
		gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), vbox, TRUE, TRUE, 0);
#endif
		gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
		
		label = gtk_label_new ("The following GdauiForm widget displays data from the 'products' table.\n\n"
				       "\n "
				       ".");
		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
		
		/* Create the demo widget */
		stmt = gda_sql_parser_parse_string (demo_parser, 
						    "SELECT ref, category, name, price, wh_stored FROM products", 
						    NULL, NULL);
		model = gda_connection_statement_execute_select (demo_cnc, stmt, NULL, NULL);
		g_object_unref (stmt)
;
		GError *error = NULL;
		if (!gda_data_select_compute_modification_statements (GDA_DATA_SELECT (model), &error)) {
			g_print ("===> %s\n", error && error->message ? error->message : "No detail");
		}
		form = gdaui_form_new (model);
		g_object_unref (model);

		g_object_get (G_OBJECT (form), "raw_form", &raw_form, NULL);
		gchar *filename;
		filename = demo_find_file ("example_automatic_layout.xml", NULL);
		gdaui_data_widget_set_data_layout_from_file (GDAUI_DATA_WIDGET (raw_form),
							     filename, "products");
		g_free (filename);

		gtk_box_pack_start (GTK_BOX (vbox), form, TRUE, TRUE, 0);

		gtk_widget_set_size_request (window, 500, 500);
	}

	gboolean visible;
	g_object_get (G_OBJECT (window), "visible", &visible, NULL);
	if (!visible)
		gtk_widget_show_all (window);
	else {
		gtk_widget_destroy (window);
		window = NULL;
	}

	return window;
}


