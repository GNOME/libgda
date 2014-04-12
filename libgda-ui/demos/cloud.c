/* Selector widgets/Cloud widget
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

static void
selection_changed_cb (GdauiDataSelector *selector, G_GNUC_UNUSED gpointer data)
{
	GString *string = NULL;
	GArray *sel;
	gsize i;

	sel = gdaui_data_selector_get_selected_rows (selector);
	if (sel) {
		for (i = 0; i < sel->len; i++) {
			if (!string)
				string = g_string_new ("");
			else
				g_string_append (string, ", ");
			g_string_append_printf (string, "%d", g_array_index (sel, gint, i));
		}
		g_array_free (sel, TRUE);
	}
	g_print ("Selection changed: %s\n", string ? string->str: "none");
}

static void
force_select (GtkButton *button, GdauiCloud *cloud)
{
	GtkEntry *entry;
	gint row;
	entry = g_object_get_data (G_OBJECT (button), "entry");
	row = atoi (gtk_entry_get_text (GTK_ENTRY (entry)));
	g_print ("Row %d selected: %s\n", row, 
		 gdaui_data_selector_select_row (GDAUI_DATA_SELECTOR (cloud), row) ? "OK": "Error");
}

static void
force_unselect (GtkButton *button, GdauiCloud *cloud)
{
	GtkEntry *entry;
	gint row;
	entry = g_object_get_data (G_OBJECT (button), "entry");
	row = atoi (gtk_entry_get_text (GTK_ENTRY (entry)));
	gdaui_data_selector_unselect_row (GDAUI_DATA_SELECTOR (cloud), row);
	g_print ("Row %d UNselected\n", row);
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
		
		label = gtk_label_new ("The following GdauiCloud widget displays customers,\n"
				       "appearing bigger if they made more purchases.");
		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
		
		/* Create the demo widget */
		stmt = gda_sql_parser_parse_string (demo_parser, "select c.id, c.name, count (o.id) as weight from customers c left join orders o on (c.id=o.customer) group by c.name order by c.name", NULL, NULL);
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

		/* selection */
		GdaDataModelIter *sel;
		GtkWidget *form;
		label = gtk_label_new ("Current selection is:");
		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

		sel = gdaui_data_selector_get_data_set (GDAUI_DATA_SELECTOR (cloud));
		form = gdaui_basic_form_new (GDA_SET (sel));
		gtk_box_pack_start (GTK_BOX (vbox), form, FALSE, FALSE, 0);

		g_signal_connect (cloud, "selection-changed",
				  G_CALLBACK (selection_changed_cb), NULL);

		/* force selection */
		GtkWidget *hbox, *entry, *button;
		label = gtk_label_new ("Selection forcing:");
		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

		hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
		gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
		label = gtk_label_new ("row number:");
		gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
		entry = gtk_entry_new ();
		gtk_box_pack_start (GTK_BOX (hbox), entry, FALSE, FALSE, 0);

		hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
		gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);		
		button = gtk_button_new_with_label ("Force select");
		gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
		g_object_set_data (G_OBJECT (button), "entry", entry);
		g_signal_connect (button, "clicked",
				  G_CALLBACK (force_select), cloud);
		button = gtk_button_new_with_label ("Force UNselect");
		gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
		g_object_set_data (G_OBJECT (button), "entry", entry);
		g_signal_connect (button, "clicked",
				  G_CALLBACK (force_unselect), cloud);
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
