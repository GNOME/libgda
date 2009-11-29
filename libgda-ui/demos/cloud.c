/* Widgets/Cloud widget
 *
 * The GdauiCloud widget displays data stored in a GdaDataModel in a cloud fashion
 */

#include <libgda-ui/libgda-ui.h>
#include <sql-parser/gda-sql-parser.h>

extern GdaConnection *demo_cnc;
extern GdaSqlParser *demo_parser;
static GtkWidget *window = NULL;

static void
mode_changed_cb (GtkToggleButton *button, GdauiCloud *cloud)
{
	GtkSelectionMode mode;
	if (gtk_toggle_button_get_active (button)) {
		mode = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "mode"));
		gdaui_cloud_set_selection_mode (cloud, mode);
	}
}

GtkWidget *
do_cloud (GtkWidget *do_widget)
{  
	if (!window) {
                GdaStatement *stmt;
		GtkWidget *vbox;
		GtkWidget *label;
		GdaDataModel *model;
		GtkWidget *cloud, *search;
		
		window = gtk_dialog_new_with_buttons ("GdauiCloud",
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
		
		label = gtk_label_new ("The following GdauiCloud widget displays customers,\n"
				       "appearing bigger if they made more purshases.");
		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
		
		/* Create the demo widget */
		stmt = gda_sql_parser_parse_string (demo_parser, "select c.id, c.name, count (o.id) from customers c left join orders o on (c.id=o.customer) group by c.name order by c.name", NULL, NULL);
		model = gda_connection_statement_execute_select (demo_cnc, stmt, NULL, NULL);
		g_object_unref (stmt);
		cloud = gdaui_cloud_new (model, 1, 2);
		g_object_unref (model);

		gtk_box_pack_start (GTK_BOX (vbox), cloud, TRUE, TRUE, 0);

		/* create a search box */
		search = gdaui_cloud_create_filter_widget (GDAUI_CLOUD (cloud));
		gtk_box_pack_start (GTK_BOX (vbox), search, FALSE, FALSE, 0);

		/* selection modes part */
		GtkWidget *rb;
		label = gtk_label_new ("Selection mode:");
		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);


		rb = gtk_radio_button_new_with_label (NULL, "GTK_SELECTION_NONE");
		gtk_box_pack_start (GTK_BOX (vbox), rb, FALSE, FALSE, 0);
		g_object_set_data (G_OBJECT (rb),"mode", GINT_TO_POINTER (GTK_SELECTION_NONE));
		g_signal_connect (rb, "toggled",
				  G_CALLBACK (mode_changed_cb), cloud);
		rb = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (rb),
								  "GTK_SELECTION_SINGLE");
		gtk_box_pack_start (GTK_BOX (vbox), rb, FALSE, FALSE, 0);
		g_object_set_data (G_OBJECT (rb),"mode", GINT_TO_POINTER (GTK_SELECTION_SINGLE));
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb), TRUE);
		g_signal_connect (rb, "toggled",
				  G_CALLBACK (mode_changed_cb), cloud);
		rb = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (rb),
								  "GTK_SELECTION_BROWSE");
		gtk_box_pack_start (GTK_BOX (vbox), rb, FALSE, FALSE, 0);
		g_object_set_data (G_OBJECT (rb),"mode", GINT_TO_POINTER (GTK_SELECTION_BROWSE));
		g_signal_connect (rb, "toggled",
				  G_CALLBACK (mode_changed_cb), cloud);
		rb = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (rb),
								  "GTK_SELECTION_MULTIPLE");
		gtk_box_pack_start (GTK_BOX (vbox), rb, FALSE, FALSE, 0);
		g_object_set_data (G_OBJECT (rb),"mode", GINT_TO_POINTER (GTK_SELECTION_MULTIPLE));
		g_signal_connect (rb, "toggled",
				  G_CALLBACK (mode_changed_cb), cloud);
	}

	if (!GTK_WIDGET_VISIBLE (window))
		gtk_widget_show_all (window);
	else
		gtk_widget_destroy (window);

	return window;
}


