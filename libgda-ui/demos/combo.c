/* Selector widgets/Combo widget
 *
 * The GdauiCombo widget displays data stored in a GdaDataModel in a GtkComboBox
 */

#include <libgda-ui/libgda-ui.h>
#include <sql-parser/gda-sql-parser.h>

extern GdaConnection *demo_cnc;
extern GdaSqlParser *demo_parser;
static GtkWidget *window = NULL;

static void
null_entry_changed_cb (GtkCheckButton *cbutton, GdauiCombo *combo)
{
	gdaui_combo_add_null (combo, gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (cbutton)));
}

static void
popup_as_list_changed_cb (GtkCheckButton *cbutton, GdauiCombo *combo)
{
	g_object_set ((GObject*) combo, "as-list", gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (cbutton)), NULL);
}

static void
column_show_changed_cb (GtkCheckButton *cbutton, GdauiCombo *combo)
{
	gint column;
	column = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (cbutton), "column"));
	gdaui_data_selector_set_column_visible (GDAUI_DATA_SELECTOR (combo), column,
						gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (cbutton)));
	g_print ("Column %d %s\n", column,
		 gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (cbutton)) ? "visible" : "invisible");
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
force_select (GtkButton *button, GdauiCombo *combo)
{
	GtkEntry *entry;
	gint row;
	entry = g_object_get_data (G_OBJECT (button), "entry");
	row = atoi (gtk_entry_get_text (GTK_ENTRY (entry)));
	g_print ("Row %d selected: %s\n", row, 
		 gdaui_data_selector_select_row (GDAUI_DATA_SELECTOR (combo), row) ? "OK": "Error");
}

static void
force_unselect (GtkButton *button, GdauiCombo *combo)
{
	GtkEntry *entry;
	gint row;
	entry = g_object_get_data (G_OBJECT (button), "entry");
	row = atoi (gtk_entry_get_text (GTK_ENTRY (entry)));
	gdaui_data_selector_unselect_row (GDAUI_DATA_SELECTOR (combo), row);
	g_print ("Row %d UNselected\n", row);
}

GtkWidget *
do_combo (GtkWidget *do_widget)
{  
	if (!window) {
                GdaStatement *stmt;
		GtkWidget *vbox;
		GtkWidget *label;
		GdaDataModel *model;
		GtkWidget *combo;
		
		window = gtk_dialog_new_with_buttons ("GdauiCombo",
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
		
		label = gtk_label_new ("The following GdauiCombo widget displays customers");
		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
		
		/* Create the demo widget */
		gint cols_index[] = {1};
		stmt = gda_sql_parser_parse_string (demo_parser, "select c.id, c.name, count (o.id) as weight from customers c left join orders o on (c.id=o.customer) group by c.name order by c.name", NULL, NULL);
		model = gda_connection_statement_execute_select (demo_cnc, stmt, NULL, NULL);
		g_object_unref (stmt);
		combo = gdaui_combo_new_with_model (model, 1, cols_index);
		g_object_unref (model);

		gtk_box_pack_start (GTK_BOX (vbox), combo, FALSE, FALSE, 0);

		/* options */
		GtkWidget *option;

		label = gtk_label_new ("");
		gtk_label_set_markup (GTK_LABEL (label), "<b>Options:</b>");
		gtk_widget_set_halign (label, GTK_ALIGN_START);
		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

		option = gtk_check_button_new_with_label ("Add NULL entry");
		gtk_box_pack_start (GTK_BOX (vbox), option, FALSE, FALSE, 0);
		g_signal_connect (option, "toggled",
				  G_CALLBACK (null_entry_changed_cb), combo);
		
		option = gtk_check_button_new_with_label ("Popup as list");
		gtk_box_pack_start (GTK_BOX (vbox), option, FALSE, FALSE, 0);
		g_signal_connect (option, "toggled",
				  G_CALLBACK (popup_as_list_changed_cb), combo);
		

		/* selection */
		GdaDataModelIter *sel;
		GtkWidget *form;
		label = gtk_label_new ("");
		gtk_label_set_markup (GTK_LABEL (label), "<b>Current selection is:</b>");
		gtk_widget_set_halign (label, GTK_ALIGN_START);
		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

		sel = gdaui_data_selector_get_data_set (GDAUI_DATA_SELECTOR (combo));
		form = gdaui_basic_form_new (GDA_SET (sel));
		gtk_box_pack_start (GTK_BOX (vbox), form, FALSE, FALSE, 0);

		g_signal_connect (combo, "selection-changed",
				  G_CALLBACK (selection_changed_cb), NULL);

		/* force selection */
		GtkWidget *hbox, *entry, *button;
		label = gtk_label_new ("");
		gtk_label_set_markup (GTK_LABEL (label), "<b>Selection forcing:</b>");
		gtk_widget_set_halign (label, GTK_ALIGN_START);
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
				  G_CALLBACK (force_select), combo);
		button = gtk_button_new_with_label ("Force UNselect");
		gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
		g_object_set_data (G_OBJECT (button), "entry", entry);
		g_signal_connect (button, "clicked",
				  G_CALLBACK (force_unselect), combo);

		/* show/hide columns */
		gint i, ncols;
		label = gtk_label_new ("");
		gtk_label_set_markup (GTK_LABEL (label), "<b>Show columns:</b>");
		gtk_widget_set_halign (label, GTK_ALIGN_START);
		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
		ncols = gda_data_model_get_n_columns (model);
		for (i = 0; i < ncols; i++) {
			gchar *str;
			str = g_strdup_printf ("Column %d", i);
			option = gtk_check_button_new_with_label (str);
			g_free (str);
			gtk_box_pack_start (GTK_BOX (vbox), option, FALSE, FALSE, 0);
			g_object_set_data (G_OBJECT (option), "column", GINT_TO_POINTER (i));
			if (i == 1)
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (option), TRUE);
			g_signal_connect (option, "toggled",
					  G_CALLBACK (column_show_changed_cb), combo);
		}
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

