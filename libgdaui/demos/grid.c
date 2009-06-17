/* Grids/Read-only grid
 *
 * The GdauiGrid widget displays data stored in a GdaDataModel.
 */

#include <libgdaui/libgdaui.h>
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
		
		label = gtk_label_new ("The following GdauiGrid widget displays data from the 'products' table.\n\n"
				       "As no modification query is provided, the data is read-only.");
		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
		
		/* Create the demo widget */
		stmt = gda_sql_parser_parse_string (demo_parser, "SELECT * FROM products", NULL, NULL);
		model = gda_connection_statement_execute_select (demo_cnc, stmt, NULL, NULL);
		g_object_unref (stmt);
		grid = gdaui_grid_new (model);
		g_object_unref (model);

		gtk_box_pack_start (GTK_BOX (vbox), grid, TRUE, TRUE, 0);
	}

	if (!GTK_WIDGET_VISIBLE (window))
		gtk_widget_show_all (window);
	else
		gtk_widget_destroy (window);

	return window;
}


