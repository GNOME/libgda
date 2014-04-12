/* Forms/Custom layout
 *
 * A GdauiForm widget where the layout is customized
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
		
		window = gtk_dialog_new_with_buttons ("Form with custom data layout",
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
		
		label = gtk_label_new ("The following GdauiForm widget displays information about customers,\n"
				       "using a paned container where the right part is used to display\n"
				       "a picture of the customer.\n");
		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
		
		/* Create the demo widget: select all the customers and computes the total
		 * amount of orders they have spent */
		stmt = gda_sql_parser_parse_string (demo_parser, 
						    "select c.id, c.name, c.country, c.city, c.photo, c.comments, sum (od.quantity * (1 - od.discount/100) * p.price) as total_orders from customers c left join orders o on (c.id=o.customer) left join order_contents od on (od.order_id=o.id) left join products p on (p.ref = od.product_ref) group by c.id order by total_orders desc",
						    NULL, NULL);
		model = gda_connection_statement_execute_select (demo_cnc, stmt, NULL, NULL);
		g_object_unref (stmt);
		form = gdaui_form_new (model);
		g_object_unref (model);

		/* hide the ID data entry */
		g_object_get (G_OBJECT (form), "raw-form", &raw_form, NULL);
		gdaui_basic_form_entry_set_visible (GDAUI_BASIC_FORM (raw_form),
						    gda_set_get_holder (gdaui_basic_form_get_data_set (GDAUI_BASIC_FORM (raw_form)),
									"id"), FALSE);

		/* request custom layout:
		   <gdaui_form name="customers" container="hpaned">
		     <gdaui_section title="Summary">
		       <gdaui_column>
		         <gdaui_entry name="id"/>
			 <gdaui_entry name="name" label="Customer name"/>
			 <gdaui_entry name="comments" label="Comments" plugin="text"/>
			 <gdaui_entry name="total_orders" label="Total ordered" plugin="number:NB_DECIMALS=2;CURRENCY=€"/>
		       </gdaui_column>
		     </gdaui_section>
		     <gdaui_section title="Photo">
		       <gdaui_column>
		         <gdaui_entry name="photo" plugin="picture"/>
		       </gdaui_column>
		     </gdaui_section>
		   </gdaui_form>
		 */
		gchar *filename;
		filename = demo_find_file ("custom_layout.xml", NULL);
		gdaui_basic_form_set_layout_from_file (GDAUI_BASIC_FORM (raw_form), filename, "customers");
		g_free (filename);

		/* we don't need the raw_form's reference anymore */
		g_object_unref (G_OBJECT (raw_form));

		gtk_box_pack_start (GTK_BOX (vbox), form, TRUE, TRUE, 0);
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


