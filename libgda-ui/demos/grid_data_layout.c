/* Grids/Custom layout
 *
 * A GdauiGrid widget where the layout is customized
 * 
 */

#include <libgda-ui/libgda-ui.h>
#include <sql-parser/gda-sql-parser.h>
#include "demo-common.h"

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
		
		window = gtk_dialog_new_with_buttons ("Grid with custom data layout",
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
		
		label = gtk_label_new ("The following GdauiGrid widget displays information about customers,\n"
				       "using a picture of the customer.\n");
		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
		
		/* Create the demo widget: select all the customers and computes the total
		 * amount of orders they have spent */
		stmt = gda_sql_parser_parse_string (demo_parser, 
						    "select c.id, c.name, c.country, c.city, c.photo, c.comments, sum (od.quantity * (1 - od.discount/100) * p.price) as total_orders from customers c left join orders o on (c.id=o.customer) left join order_contents od on (od.order_id=o.id) left join products p on (p.ref = od.product_ref) group by c.id order by total_orders desc",
						    NULL, NULL);

                model = gda_connection_statement_execute_select (demo_cnc, stmt, NULL, NULL);
                g_object_unref (stmt);
 		grid = gdaui_grid_new (model);
		g_object_unref (model);

		/* request custom layout:
		   <gdaui_grid name="customers">
		     <gdaui_entry name="name"/>
		     <gdaui_entry name="total_orders" label="Total ordered" plugin="number:NB_DECIMALS=2;CURRENCY=€"/>
		     <gdaui_entry name="photo" plugin="picture"/>
		   </gdaui_grid>
		 */
		g_object_get (G_OBJECT (grid), "raw-grid", &raw_grid, NULL);
		gchar *filename;
		filename = demo_find_file ("custom_layout.xml", NULL);
		gdaui_raw_grid_set_layout_from_file (GDAUI_RAW_GRID (raw_grid), filename, "customers");
		g_free (filename);

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


