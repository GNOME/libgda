/* Forms/Changing data model
 *
 * This example shows how to set and change the data model displayed by a GdauiForm widget.
 *
 * You can choose between 3 data models, the first two share some rows, and the 3rd one is completely
 * different.
 *
 * Because the "cache-changes" property for the GdaDataProxy internally used is set to TRUE, modifications
 * are kept and shared between the two first data models.
 */

#include <libgda-ui/libgda-ui.h>
#include <sql-parser/gda-sql-parser.h>

extern GdaConnection *demo_cnc;
extern GdaSqlParser *demo_parser;
static GtkWidget *window = NULL;
static GtkWidget *form = NULL;

static void
model_toggled_cb (GtkToggleButton *rb, GdaDataModel *model)
{
	if (gtk_toggle_button_get_active (rb))
		gdaui_data_selector_set_model (GDAUI_DATA_SELECTOR (form), model);
}

GtkWidget *
do_form_model_change (GtkWidget *do_widget)
{  
	if (!window) {
                GdaStatement *stmt;
		GtkWidget *vbox;
		GtkWidget *label;
		GdaDataModel *models [3];
		
		window = gtk_dialog_new_with_buttons ("Changing data in a GdauiForm",
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
		
		label = gtk_label_new ("The data in the same GdauiForm widget can be change on the fly.");
		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

		/* creating data models */
		stmt = gda_sql_parser_parse_string (demo_parser, "SELECT * FROM products ORDER BY ref, category LIMIT 15", NULL, NULL);
		models[0] = gda_connection_statement_execute_select (demo_cnc, stmt, NULL, NULL);
		gda_data_select_compute_modification_statements (GDA_DATA_SELECT (models[0]), NULL);
		g_object_unref (stmt);

		stmt = gda_sql_parser_parse_string (demo_parser, "SELECT * FROM products WHERE price > 20.2 ORDER BY ref, category LIMIT 10", NULL, NULL);
		models[1] = gda_connection_statement_execute_select (demo_cnc, stmt, NULL, NULL);
		gda_data_select_compute_modification_statements (GDA_DATA_SELECT (models[1]), NULL);
		g_object_unref (stmt);

		stmt = gda_sql_parser_parse_string (demo_parser, "SELECT name, price, ref, category FROM products WHERE price > 20.2 ORDER BY name LIMIT 30", NULL, NULL);
		models[2] = gda_connection_statement_execute_select (demo_cnc, stmt, NULL, NULL);
		gda_data_select_compute_modification_statements (GDA_DATA_SELECT (models[2]), NULL);
		g_object_unref (stmt);

		
		/* allow choosing which data model to display */
		label = gtk_label_new ("");
		gtk_widget_set_halign (label, GTK_ALIGN_START);
                gtk_label_set_markup (GTK_LABEL (label), "<b>Choose which data model to display:</b>");
		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

		GtkWidget *layout, *rb;
		GSList *group = NULL;
		gint i;
		layout = gtk_grid_new ();
		gtk_box_pack_start (GTK_BOX (vbox), layout, FALSE, FALSE, 0);
		
		for (i = 0; i < 3; i++) {
			gchar *str;
			str = g_strdup_printf ("%d columns x %d rows", gda_data_model_get_n_columns (models[i]),
					       gda_data_model_get_n_rows (models[i]));
			rb = gtk_radio_button_new_with_label (group, str);
			g_free (str);
			gtk_grid_attach (GTK_GRID (layout), rb, i, 0, 1, 1);
			g_signal_connect (rb, "toggled", G_CALLBACK (model_toggled_cb), models[i]);
			g_object_set_data_full (G_OBJECT (rb), "model", models[i], g_object_unref);
			group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (rb));
		}

		/* Create the form widget */
		label = gtk_label_new ("");
		gtk_widget_set_halign (label, GTK_ALIGN_START);
                gtk_label_set_markup (GTK_LABEL (label), "<b>GdauiForm:</b>");
		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

		form = gdaui_form_new (models[0]);
		g_object_set (G_OBJECT (form), "info-flags",
                              GDAUI_DATA_PROXY_INFO_CURRENT_ROW |
                              GDAUI_DATA_PROXY_INFO_ROW_MOVE_BUTTONS |
                              GDAUI_DATA_PROXY_INFO_ROW_MODIFY_BUTTONS, NULL);
		gtk_box_pack_start (GTK_BOX (vbox), form, TRUE, TRUE, 0);

		GdaDataProxy *proxy;
		proxy = gdaui_data_proxy_get_proxy (GDAUI_DATA_PROXY (form));
		g_object_set (proxy, "cache-changes", TRUE, NULL);
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


