/* Grids/Automatic data_layout
 *
 * A GdauiGrid widget where automatic 'data_layout' is used to display
 * 
 */

#include <libgda-ui/libgda-ui.h>
#include <sql-parser/gda-sql-parser.h>

extern GdaConnection *demo_cnc;
extern GdaSqlParser *demo_parser;
static GtkWidget *window = NULL;

GtkWidget *
do_grid_data_layout (GtkWidget *do_widget)
{  
	if (!window) {
                GdaStatement *stmt;
		GtkWidget *vbox;
		GtkWidget *label;
		GdaDataModel *model;
		GtkWidget *grid;
		GdauiRawGrid *raw_grid;
		GdaSet *data_set;
		GdaHolder *param;
		GValue *value;
		
		window = gtk_dialog_new_with_buttons ("Grid with the automatic 'data_layout'",
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
				       "\n"
				       "\n"
				       ".");
		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
		
		/* Create the demo widget */
		//stmt = gda_sql_parser_parse_string (demo_parser, "SELECT ref, wh_stored, category, name, price FROM products", NULL, NULL);
		stmt = gda_sql_parser_parse_string (demo_parser, "SELECT p.ref, p.wh_stored, p.category, p.name, p.price, w.name FROM products p LEFT JOIN warehouses w ON (w.id=p.wh_stored)", NULL, NULL);
                model = gda_connection_statement_execute_select (demo_cnc, stmt, NULL, NULL);
                g_object_unref (stmt);
                gda_data_select_compute_modification_statements (GDA_DATA_SELECT (model), NULL);
		grid = gdaui_grid_new (model);
		g_object_unref (model);

		/* specify that we want to use the 'data_layout' plugin */
		g_object_get (G_OBJECT (grid), "raw_grid", &raw_grid, NULL);
		/* data_set = GDA_SET (gdaui_data_widget_get_current_data (GDAUI_DATA_WIDGET (raw_grid))); */
		/* param = gda_set_get_holder (data_set, "pict"); */

		/* value = gda_value_new_from_string ("data_layout", G_TYPE_STRING); */
		/* gda_holder_set_attribute (param, GDAUI_ATTRIBUTE_PLUGIN, value); */
		/* gda_value_free (value); */
		//
		//gpointer d[2];
		//d[0] = "./example_automatic_layout_full.xml";
		//d[1] = "products";

		//g_object_set (G_OBJECT (raw_grid), "data_layout", d, NULL);
		//
		gchar *filename;
		filename = demo_find_file ("example_automatic_layout.xml", NULL);
		gdaui_data_widget_set_data_layout_from_file (GDAUI_DATA_WIDGET (raw_grid),
							     filename, "products");
		g_free (filename);
		//

		gtk_box_pack_start (GTK_BOX (vbox), grid, TRUE, TRUE, 0);

		gtk_widget_set_size_request (window, 500, 500);
	}

	if (!GTK_WIDGET_VISIBLE (window))
		gtk_widget_show_all (window);
	else
		gtk_widget_destroy (window);

	return window;
}


