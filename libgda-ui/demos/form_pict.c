/* Forms/Using the picture plugin
 *
 * A GdauiForm widget where the 'picture' plugin is used to display an image
 * internally stored as a BLOB
 */

#include <libgda-ui/libgda-ui.h>
#include <sql-parser/gda-sql-parser.h>

extern GdaConnection *demo_cnc;
extern GdaSqlParser *demo_parser;
static GtkWidget *window = NULL;

GtkWidget *
do_form_pict (GtkWidget *do_widget)
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
		
		window = gtk_dialog_new_with_buttons ("Form with the 'picture' plugin",
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
		
		label = gtk_label_new ("The following GdauiForm widget displays data from the 'pictures' table.\n\n"
				       "The pictures are stored as BLOB inside the database and\n"
				       "are displayed using the 'picture' plugin (right click to \n"
				       "open a menu, or double click to load an image).");
		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
		
		/* Create the demo widget */
		stmt = gda_sql_parser_parse_string (demo_parser, "SELECT id, pict FROM pictures", NULL, NULL);
		model = gda_connection_statement_execute_select (demo_cnc, stmt, NULL, NULL);
		g_object_unref (stmt);
		gda_data_select_compute_modification_statements (GDA_DATA_SELECT (model), NULL);
		form = gdaui_form_new (model);
		g_object_unref (model);

		/* specify that we want to use the 'picture' plugin */
		g_object_get (G_OBJECT (form), "raw_form", &raw_form, NULL);
		data_set = gdaui_basic_form_get_data_set (GDAUI_BASIC_FORM (raw_form));
		param = gda_set_get_holder (data_set, "pict");

		value = gda_value_new_from_string ("picture", G_TYPE_STRING);
		gda_holder_set_attribute_static (param, GDAUI_ATTRIBUTE_PLUGIN, value);
		gda_value_free (value);

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


