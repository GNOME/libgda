/* Grids/Read-only grid
 *
 * The GdauiGrid widget displays data stored in a GdaDataModel.
 */

#include <libgda-ui/libgda-ui.h>
#include <sql-parser/gda-sql-parser.h>

extern GdaConnection *demo_cnc;
extern GdaSqlParser *demo_parser;
static GtkWidget *window = NULL;

GtkWidget *
do_grid (GtkWidget *do_widget)
{  
	if (!window) {
		GdaStatement *stmt;
		GtkWidget *vbox;
		GtkWidget *label;
		GdaDataModel *model;
		GtkWidget *grid;
		
		window = gtk_dialog_new_with_buttons ("GdauiGrid (RO)",
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
		
		label = gtk_label_new ("The following GdauiGrid widget displays data from the 'products' table.\n\n"
				       "As no modification query is provided, the data is read-only.");
		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
		
		/* Create the demo widget */
		stmt = gda_sql_parser_parse_string (demo_parser, "SELECT * FROM products", NULL, NULL);
		model = gda_connection_statement_execute_select (demo_cnc, stmt, NULL, NULL);
		g_object_unref (stmt);
		grid = gdaui_grid_new (model);
		gtk_widget_set_size_request (grid, -1, 350);
		g_object_unref (model);
		gdaui_grid_set_sample_size (GDAUI_GRID (grid), 100);
		g_object_set (G_OBJECT (grid), "info-flags",
			      GDAUI_DATA_PROXY_INFO_CURRENT_ROW | GDAUI_DATA_PROXY_INFO_CHUNK_CHANGE_BUTTONS, NULL);
		gdaui_data_selector_set_column_visible (GDAUI_DATA_SELECTOR (grid), 1, FALSE);

		gtk_box_pack_start (GTK_BOX (vbox), grid, TRUE, TRUE, 0);
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


