/* Grids/Using the picture plugin
 *
 * A GdauiGrid widget where the 'picture' plugin is used to display an image
 * internally stored as a BLOB
 */

#include <libgda-ui/libgda-ui.h>
#include <sql-parser/gda-sql-parser.h>

extern GdaConnection *demo_cnc;
extern GdaSqlParser *demo_parser;
static GtkWidget *window = NULL;

GtkWidget *
do_grid_pict (GtkWidget *do_widget)
{  
	if (!window) {
                GdaStatement *stmt;
		GtkWidget *vbox;
		GtkWidget *label;
		GdaDataModel *model;
		GtkWidget *grid;
		GdaSet *data_set;
		GdaHolder *param;

		window = gtk_dialog_new_with_buttons ("Grid with the 'picture' plugin",
						      GTK_WINDOW (do_widget),
						      0,
						      "Close", GTK_RESPONSE_NONE,
						      NULL);
		
		g_signal_connect (window, "response",
				  G_CALLBACK (gtk_widget_destroy), NULL);
		g_signal_connect (window, "destroy",
				  G_CALLBACK (gtk_widget_destroyed), &window);
		
		vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
		gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (window))),
				    vbox, TRUE, TRUE, 0);
		gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
		
		label = gtk_label_new ("The following GdauiGrid widget displays data from the 'pictures' table.\n\n"
				       "The pictures are stored as BLOB inside the database and\n"
				       "are displayed using the 'picture' plugin (right click to \n"
				       "open a menu, or double click to load an image).");
		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
		
		/* Create the demo widget */
		stmt = gda_sql_parser_parse_string (demo_parser, "SELECT id, pict FROM pictures", NULL, NULL);
                model = gda_connection_statement_execute_select (demo_cnc, stmt, NULL, NULL);
                g_object_unref (stmt);
                gda_data_select_compute_modification_statements (GDA_DATA_SELECT (model), NULL);
		grid = gdaui_grid_new (model);
		g_object_unref (model);
		g_object_set (G_OBJECT (grid), "info-flags",
			      GDAUI_DATA_PROXY_INFO_CURRENT_ROW | GDAUI_DATA_PROXY_INFO_ROW_MODIFY_BUTTONS, NULL);

		/* specify that we want to use the 'picture' plugin */
		data_set = GDA_SET (gdaui_data_selector_get_data_set (GDAUI_DATA_SELECTOR (grid)));
		param = gda_set_get_holder (data_set, "pict");
		g_object_set (param, "plugin", "picture", NULL);

		gtk_box_pack_start (GTK_BOX (vbox), grid, TRUE, TRUE, 0);

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


