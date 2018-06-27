/* Data widgets linking/Same data
 *
 * Shows a form and a grid synchronized to display the same data and
 * optionally to have the same selected row.
 */

#include <libgda-ui/libgda-ui.h>
#include <sql-parser/gda-sql-parser.h>

extern GdaConnection *demo_cnc;
extern GdaSqlParser *demo_parser;
static GtkWidget *window = NULL;

typedef struct {
	GdaDataModelIter *grid_iter;
	GdaDataModelIter *form_iter;
	gboolean          keep_sync;
} DemoData;

static void
restrict_default_served_by_field (GdauiDataSelector *selector, GdaDataModel *restrict_with, gint restrict_col)
{
	GdaDataModelIter *iter;
	GdaHolder *param;

	iter = gdaui_data_selector_get_data_set (selector);
	param = GDA_HOLDER (g_slist_nth_data (gda_set_get_holders (GDA_SET (iter)), 2));

	g_assert (gda_holder_set_source_model (param, restrict_with, restrict_col, NULL));
}

static void
iter_row_changed_cb (GdaDataModelIter *iter, gint row, DemoData *data)
{
        GdaDataModelIter *other;

        if (iter == data->grid_iter)
                other = data->form_iter;
        else
                other = data->grid_iter;

        if (data->keep_sync) {
		g_signal_handlers_block_by_func (other,
						 G_CALLBACK (iter_row_changed_cb),
						 data);
		gda_data_model_iter_move_to_row (other, row);
		g_signal_handlers_unblock_by_func (other,
						   G_CALLBACK (iter_row_changed_cb),
						   data);
		/* REM: other method would be to do:
                        GtkTreePath *path = gtk_tree_path_new_from_indices (row, -1);
                        gtk_tree_view_set_cursor (raw_grid, path, NULL, FALSE);
                        gtk_tree_path_free (path);
		*/
        }
}

static void
sync_selections_cb (GtkToggleButton *toggle, DemoData *data)
{
        data->keep_sync = gtk_toggle_button_get_active (toggle);
}

GtkWidget *
do_linked_grid_form (GtkWidget *do_widget)
{  
	if (!window) {
                GdaStatement *stmt;
		GtkWidget *vbox;
		GtkWidget *label;
		GtkWidget *cb;		
		GdaDataModel *cust_model, *sr_model;
		GtkWidget *form, *grid;
		GdaDataProxy *proxy;
		DemoData *data;

		data = g_new0 (DemoData, 1);
		
		window = gtk_dialog_new_with_buttons ("Linked grid and form on the same data",
						      GTK_WINDOW (do_widget),
						      0,
						      "Close", GTK_RESPONSE_NONE,
						      NULL);
		
		g_signal_connect (window, "response",
				  G_CALLBACK (gtk_widget_destroy), NULL);
		g_signal_connect (window, "destroy",
				  G_CALLBACK (gtk_widget_destroyed), &window);
		g_object_set_data_full (G_OBJECT (window), "demodata", data, g_free);
		
		vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
		gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (window))),
				    vbox, TRUE, TRUE, 0);
		gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
		
		label = gtk_label_new ("The following GdauiForm and GdauiGrid widgets\n"
				       "display data from the 'customers' and 'salesrep' tables.");
		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
		
		/* create a data model for the customers */
		stmt = gda_sql_parser_parse_string (demo_parser, 
						    "SELECT c.id, c.name, c.default_served_by as \"SalesRep\""
						    "FROM customers c "
						    "LEFT JOIN salesrep s ON (s.id=c.default_served_by)", NULL, NULL);
		cust_model = gda_connection_statement_execute_select (demo_cnc, stmt, NULL, NULL);
		g_object_unref (stmt);
		gda_data_select_compute_modification_statements (GDA_DATA_SELECT (cust_model), NULL);
			  
		/* create a data model for the salesrep */
		stmt = gda_sql_parser_parse_string (demo_parser, "SELECT id, name FROM salesrep", NULL, NULL);
		sr_model = gda_connection_statement_execute_select (demo_cnc, stmt, NULL, NULL);
		g_object_unref (stmt);

		/* create grid widget */
		label = gtk_label_new ("<b>GdauiGrid:</b>");
		gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
		gtk_widget_set_halign (label, GTK_ALIGN_START);
		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, TRUE, 0);
		gtk_widget_show (label);

		grid = gdaui_grid_new (cust_model);
		gtk_box_pack_start (GTK_BOX (vbox), grid, TRUE, TRUE, 0);
		gtk_widget_show (grid);

		/* restrict the c.default_served_by field in the grid to be within the sr_model */
		restrict_default_served_by_field (GDAUI_DATA_SELECTOR (grid), sr_model, 0);
		data->grid_iter = gdaui_data_selector_get_data_set (GDAUI_DATA_SELECTOR (grid));
		g_signal_connect (data->grid_iter, "row-changed",
				  G_CALLBACK (iter_row_changed_cb), data);

		/* create form widget which uses the same data model as the grid */
		label = gtk_label_new ("<b>GdauiForm:</b>");
		gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
		gtk_widget_set_halign (label, GTK_ALIGN_START);
		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, TRUE, 0);
		gtk_widget_show (label);

		proxy = gdaui_data_proxy_get_proxy (GDAUI_DATA_PROXY (grid));
		form = gdaui_form_new (GDA_DATA_MODEL (proxy));
		gtk_box_pack_start (GTK_BOX (vbox), form, TRUE, TRUE, 0);
		gtk_widget_show (form);

		/* restrict the c.default_served_by field in the form to be within the sr_model */
		restrict_default_served_by_field (GDAUI_DATA_SELECTOR (form), sr_model, 0);
		data->form_iter = gdaui_data_selector_get_data_set (GDAUI_DATA_SELECTOR (form));
		g_signal_connect (data->form_iter, "row-changed",
				  G_CALLBACK (iter_row_changed_cb), data);

		g_object_unref (cust_model);
		g_object_unref (sr_model);

		/* optional synchronization of the selections */
		label = gtk_label_new ("<b>Selected rows synchronization option:</b>\n"
				       "<small>Effective only at the next selected row change</small>");
		gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
		gtk_widget_set_halign (label, GTK_ALIGN_START);
		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, TRUE, 0);
		gtk_widget_show (label);
		
		cb = gtk_check_button_new_with_label ("Keep selected rows synchroniezd");
		gtk_box_pack_start (GTK_BOX (vbox), cb, FALSE, TRUE, 0);
		gtk_widget_show (cb);
		g_signal_connect (G_OBJECT (cb), "toggled",
				  G_CALLBACK (sync_selections_cb), data);
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
