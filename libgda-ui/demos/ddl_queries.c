/* Widgets/DDL queries
 *
 * A DDL query is a query defining or modifying a database structure and properties.
 * In Libgda, each type of DDL query corresponds to an "operation".
 * Examples of "operation"s include creating a database, creating a table,
 * adding a field to a table.
 *
 * Because the SQL corresponding to DDL queries varies widely between the
 * DBMS, the information required to perform an operation is provided by
 * each DBMS adapter (server provider), and specified as a set of named
 * parameters.
 *
 * For each type of operation and for each type of provider, a GdaServerOperation 
 * object can be requested; this objects holds all the named parameters required
 * or optional to perform the operation. Once values have been set to the
 * named parameters, that object is passed back to the provider which can render
 * it as SQL or perform the requested operation.
 *
 * The GdauiServerOperation widget can be used to display
 * and assign value to the named parameters of a GdaServerOperation object.
 */

#include <libgda-ui/libgda-ui.h>
#include <string.h>

static GtkWidget *window = NULL;

typedef struct {
	GdaServerOperation      *op;
	GtkWidget               *op_container;
	GtkWidget               *op_form;
	GdauiProviderSelector *prov_sel;
	GtkWidget               *op_combo;
	GdaServerProvider       *prov;
	
	GtkWidget               *top_window;
	GtkWidget               *sql_button;
	GtkWidget               *show_button;
} DemoData;

static void tested_provider_changed_cb (GdauiProviderSelector *prov_sel, DemoData *data);
static void tested_operation_changed_cb (GdauiCombo *combo, DemoData *data);
static void update_possible_operations (DemoData *data);

static void show_named_parameters (GtkButton *button, DemoData *data);
static void show_sql (GtkButton *button, DemoData *data);

static GdaServerProvider *get_provider_obj (DemoData *data);

GtkWidget *
do_ddl_queries (GtkWidget *do_widget)
{  
	if (!window) {
		GtkWidget *grid;
		GtkWidget *label;
		GtkWidget *wid;
		DemoData *data;
		GtkWidget *bbox;
		GtkWidget *sw, *vp;

                data = g_new0 (DemoData, 1);

		window = gtk_dialog_new_with_buttons ("DDL queries",
						      GTK_WINDOW (do_widget),
						      0,
						      "Close", GTK_RESPONSE_NONE,
						      NULL);
		data->top_window = window;
		
		g_signal_connect (window, "response",
				  G_CALLBACK (gtk_widget_destroy), NULL);
		g_signal_connect (window, "destroy",
				  G_CALLBACK (gtk_widget_destroyed), &window);
		
		grid = gtk_grid_new ();
		gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (window))),
				    grid, TRUE, TRUE, 0);
		gtk_container_set_border_width (GTK_CONTAINER (grid), 5);
		
		label = gtk_label_new ("<b>Tested provider and operation:</b>");
		gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
		gtk_widget_set_halign (label, GTK_ALIGN_START);
		gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 2, 1);
		
		/* provider selection */
		label = gtk_label_new ("Tested provider:");
		gtk_widget_set_halign (label, GTK_ALIGN_START);
		gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);

		wid = gdaui_provider_selector_new ();
		gdaui_provider_selector_set_provider (GDAUI_PROVIDER_SELECTOR (wid),
								  "SQLite");
		gtk_grid_attach (GTK_GRID (grid), wid, 1, 1, 1, 1);
		data->prov_sel = GDAUI_PROVIDER_SELECTOR (wid);
		g_signal_connect (G_OBJECT (data->prov_sel), "changed", 
				  G_CALLBACK (tested_provider_changed_cb), data);

		/* operation selection */
		label = gtk_label_new ("Tested operation:");
		gtk_widget_set_halign (label, GTK_ALIGN_START);
		gtk_grid_attach (GTK_GRID (grid), label, 0, 2, 1, 1);
		
		wid = gdaui_combo_new ();
		gtk_grid_attach (GTK_GRID (grid), wid, 1, 2, 1, 1);
		g_signal_connect (G_OBJECT (wid), "changed",
				  G_CALLBACK (tested_operation_changed_cb), data);
		data->op_combo = wid;

		/* container for GdauiServerOperation */
		label = gtk_label_new ("<b>GdauiServerOperation widget:</b>");
		gtk_widget_set_halign (label, GTK_ALIGN_START);
		gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
		gtk_grid_attach (GTK_GRID (grid), label, 0, 3, 2, 1);

		sw = gtk_scrolled_window_new (FALSE, 0);
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
						GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
		gtk_widget_set_size_request (sw, 600, 450);
		gtk_grid_attach (GTK_GRID (grid), sw, 0, 4, 2, 1);
		vp = gtk_viewport_new (NULL, NULL);
		gtk_widget_set_name (vp, "gdaui-transparent-background");
		gtk_viewport_set_shadow_type (GTK_VIEWPORT (vp), GTK_SHADOW_NONE);
		gtk_container_add (GTK_CONTAINER (sw), vp);
		data->op_container = vp;
		
		/* bottom buttons */
		bbox = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
		gtk_grid_attach (GTK_GRID (grid), bbox, 0, 5, 2, 1);

		wid = gtk_button_new_with_label ("Show named parameters");
		data->show_button = wid;
		gtk_box_pack_start (GTK_BOX (bbox), wid, TRUE, TRUE, 0);
		g_signal_connect (G_OBJECT (wid), "clicked",
				  G_CALLBACK (show_named_parameters), data);

		wid = gtk_button_new_with_label ("Show SQL");
		data->sql_button = wid;
		gtk_box_pack_start (GTK_BOX (bbox), wid, TRUE, TRUE, 0);
		g_signal_connect (G_OBJECT (wid), "clicked",
				  G_CALLBACK (show_sql), data);

		tested_provider_changed_cb (data->prov_sel, data);
		gtk_combo_box_set_active (GTK_COMBO_BOX (data->op_combo), 1);
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

static void
tested_provider_changed_cb (G_GNUC_UNUSED GdauiProviderSelector *prov_sel, DemoData *data)
{
	if (data->prov) {
		g_object_unref (data->prov);
		data->prov = NULL;
	}
	update_possible_operations (data);
}

static void
update_possible_operations (DemoData *data)
{
	GdaServerOperationType type;
	GdaDataModel *model;
	
	model = gdaui_data_selector_get_model (GDAUI_DATA_SELECTOR (data->op_combo));
	if (!model) {
		gint columns[] = {1};
		model = gda_data_model_array_new_with_g_types (2, G_TYPE_INT, G_TYPE_STRING);
		gdaui_combo_set_data (GDAUI_COMBO (data->op_combo), model, 1, columns);
	}
	else
		gda_data_model_array_clear (GDA_DATA_MODEL_ARRAY (model));

	for (type = 0; type < GDA_SERVER_OPERATION_LAST; type ++)
		if (gda_server_provider_supports_operation (get_provider_obj (data),
							    NULL, type, NULL)) {
			gint row;

			row = gda_data_model_append_row (model, NULL);
			if (row < 0)
				g_error ("Cant' append data to a GdaDataModelArray");
			else {
				GValue value;

				memset (&value, 0, sizeof (GValue));
				g_value_init (&value, G_TYPE_INT);
				g_value_set_int (&value, type);
				gda_data_model_set_value_at (model, 0, row, &value, NULL);

				memset (&value, 0, sizeof (GValue));
				g_value_init (&value, G_TYPE_STRING);
				g_value_set_string (&value, gda_server_operation_op_type_to_string (type));
				gda_data_model_set_value_at (model, 1, row, &value, NULL);
			}
		}
}

static GdaServerProvider *
get_provider_obj (DemoData *data)
{
	GdaServerProvider *prov = NULL;

	if (data->prov)
		prov = data->prov;
	else {
		/* create the GdaServerProvider object */
		data->prov = gdaui_provider_selector_get_provider_obj (data->prov_sel);
		prov = data->prov;
	}

	return prov;
}

static void
tested_operation_changed_cb (G_GNUC_UNUSED GdauiCombo *combo, DemoData *data)
{
	GdaServerProvider *prov = NULL;
	GdaServerOperationType type;
	GError *error = NULL;
	GdaDataModelIter *iter;
	const GValue *cvalue = NULL;

	if (data->op) {
		g_object_unref (data->op);
		data->op = NULL;
	}
	if (data->op_form) 
		gtk_widget_destroy (data->op_form);

	gtk_widget_set_sensitive (data->show_button, FALSE);
	gtk_widget_set_sensitive (data->sql_button, FALSE);

	iter = gdaui_data_selector_get_data_set (GDAUI_DATA_SELECTOR (data->op_combo));
	if (iter)
		cvalue = gda_data_model_iter_get_value_at (iter, 0);
	if (!cvalue || !gda_value_isa ((GValue *) cvalue, G_TYPE_INT)) {
		GtkWidget *label;

		label = gtk_label_new ("Select an operation to perform");
		gtk_container_add (GTK_CONTAINER (data->op_container), label);
		data->op_form = label;
		gtk_widget_show (data->op_form);
		return;
	}
	type = g_value_get_int ((GValue *) cvalue);
	
	prov = get_provider_obj (data);
	if (prov)
		data->op = gda_server_provider_create_operation (prov, NULL, type, NULL, &error);

	if (!data->op) {
		GtkWidget *label;
		gchar *str;

		str = g_strdup_printf ("Can't create GdaServerOperation widget: %s",
				       error && error->message ? error->message : "No detail");
		label = gtk_label_new (str);
		g_free (str);
		gtk_container_add (GTK_CONTAINER (data->op_container), label);
		data->op_form = label;
	} 
	else {
		GtkWidget *wid;

		wid = gdaui_server_operation_new (data->op);
		gtk_container_add (GTK_CONTAINER (data->op_container), wid);
		data->op_form = wid;
		gtk_widget_set_sensitive (data->show_button, TRUE);
		gtk_widget_set_sensitive (data->sql_button, TRUE);
	}
	gtk_widget_show (data->op_form);
}

static void
extract_named_parameters (GdaServerOperation *op, const gchar *root_path, GtkTextBuffer *tbuffer)
{
	GdaServerOperationNode *node;
	GtkTextIter iter;
	gchar *str;

	node = gda_server_operation_get_node_info (op, root_path);
	g_return_if_fail (node);

	gtk_text_buffer_get_end_iter (tbuffer, &iter);

	gtk_text_buffer_insert (tbuffer, &iter, "  * ", -1);
	if (node->status == GDA_SERVER_OPERATION_STATUS_REQUIRED)
		gtk_text_buffer_insert_with_tags_by_name (tbuffer, &iter, root_path, -1, "req_pathname", NULL);
	else
		gtk_text_buffer_insert_with_tags_by_name (tbuffer, &iter, root_path, -1, "opt_pathname", NULL);
	gtk_text_buffer_insert (tbuffer, &iter, " (", -1);

	switch (node->type) {
	case GDA_SERVER_OPERATION_NODE_PARAMLIST: {
		GSList *params;

		str = g_strdup_printf ("GdaSet @%p)\n", node->plist);
		gtk_text_buffer_insert (tbuffer, &iter, str, -1);
		g_free (str);
		
		for (params = gda_set_get_holders (node->plist); params; params = params->next) {
			gchar *npath;
			npath = g_strdup_printf ("%s/%s", root_path, gda_holder_get_id (GDA_HOLDER (params->data)));
			extract_named_parameters (op, npath, tbuffer);
			g_free (npath);
		}
		
		break;
	}
	case GDA_SERVER_OPERATION_NODE_DATA_MODEL: {
		gint i, ncols;

		str = g_strdup_printf ("GdaDataModel @%p)\n", node->model);
		gtk_text_buffer_insert (tbuffer, &iter, str, -1);
		g_free (str);

		ncols = gda_data_model_get_n_columns (node->model);
		for (i = 0; i < ncols; i++) {
			GdaColumn *col = gda_data_model_describe_column (node->model, i);
			gchar *npath, *str;

			g_object_get (G_OBJECT (col), "id", &str, NULL);
			npath = g_strdup_printf ("%s/@%s", root_path, str);
			g_free (str);
			extract_named_parameters (op, npath, tbuffer);
			g_free (npath);
		}
		break;
	}
	case GDA_SERVER_OPERATION_NODE_PARAM: {
		gchar *str;
		const GValue *value;

		gtk_text_buffer_insert (tbuffer, &iter, "GdaHolder) = ", -1);

		value = gda_holder_get_value (node->param);
		str = gda_value_stringify (value);
		gtk_text_buffer_insert (tbuffer, &iter, str, -1);
		gtk_text_buffer_insert (tbuffer, &iter, "\n", -1);
		g_free (str);
		break;
	}
	case GDA_SERVER_OPERATION_NODE_SEQUENCE: {
		gtk_text_buffer_insert (tbuffer, &iter, "Sequence)\n", -1);
		guint i, size = gda_server_operation_get_sequence_size (op, root_path);
		for (i = 0; i < size; i++) {
			gchar **names;
			names = gda_server_operation_get_sequence_item_names (op, root_path);
			guint n;
			for (n = 0; names [n]; n++) {
				gchar *npath;
				npath = g_strdup_printf ("%s/%u%s", root_path, i, names [n]);
				extract_named_parameters (op, npath, tbuffer);
				g_free (npath);
			}
			g_strfreev (names);
		}
		break;
	}

	case GDA_SERVER_OPERATION_NODE_SEQUENCE_ITEM:
		gtk_text_buffer_insert (tbuffer, &iter, "Sequence item)\n", -1);
		break;

	case GDA_SERVER_OPERATION_NODE_DATA_MODEL_COLUMN: {
		gint j, nrows;

		gtk_text_buffer_insert (tbuffer, &iter, "Model column)\n", -1);

		nrows = gda_data_model_get_n_rows (node->model);
		for (j = 0; j < nrows; j++) {
			gchar *npath, *str;
			const GValue *value;
			
			npath = g_strdup_printf ("%s/%d", root_path, j);
			value = gda_data_model_get_value_at (node->model, gda_column_get_position  (node->column), j, NULL);
			if (value)
				str = gda_value_stringify (value);
			else
				str = g_strdup ("Error: could not read data model's value");
			gtk_text_buffer_insert (tbuffer, &iter, "  * ", -1);
			gtk_text_buffer_insert_with_tags_by_name (tbuffer, &iter, npath, -1, "opt_pathname", NULL);
			g_free (npath);
			gtk_text_buffer_insert (tbuffer, &iter, " (GValue) = ", -1);
			gtk_text_buffer_insert (tbuffer, &iter, str, -1);
			gtk_text_buffer_insert (tbuffer, &iter, "\n", -1);
			g_free (str);
		}

		break;
	}
	default:
		gtk_text_buffer_insert (tbuffer, &iter, "???", -1);
		break;
	}
}

static void
show_named_parameters (G_GNUC_UNUSED GtkButton *button, DemoData *data)
{
	GtkWidget *dlg, *label;
	gchar **root_nodes;
	gint i;
	GtkWidget *view;
	GtkWidget *sw;
	GtkTextBuffer *buffer;
	GtkTextIter iter;

	if (!data->op || !data->op_form || ! GDAUI_IS_SERVER_OPERATION (data->op_form))
		return;

	/* dialog box */
	dlg = gtk_dialog_new_with_buttons ("Named parameters",
					   GTK_WINDOW (data->top_window),
					   GTK_DIALOG_MODAL,
					   "Close", GTK_RESPONSE_REJECT, NULL);

	label = gtk_label_new ("<b>Named parameters:</b>\n");
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dlg))),
			    label, FALSE, FALSE, 0);
	gtk_widget_show (label);
	
	/* text area */
	view = gtk_text_view_new ();
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
	gtk_text_buffer_get_start_iter (buffer, &iter);
	gtk_text_buffer_create_tag (buffer, "opt_pathname",
                                    "weight", PANGO_WEIGHT_BOLD,
                                    "foreground", "grey", NULL);
	gtk_text_buffer_create_tag (buffer, "req_pathname",
                                    "weight", PANGO_WEIGHT_BOLD,
                                    "foreground", "blue", NULL);

	xmlNodePtr xml;
	xml = gda_server_operation_save_data_to_xml (data->op, NULL);
	if (xml) {
		g_print ("XML rendering of the GdaServerOperation is:\n");
                xmlBufferPtr buffer;
                buffer = xmlBufferCreate ();
                xmlNodeDump (buffer, NULL, xml, 0, 1);
                xmlFreeNode (xml);
                xmlBufferDump (stdout, buffer);
                xmlBufferFree (buffer);
		g_print ("\n");
	}
	else {
		g_print ("XML rendering ERROR\n");
	}

	root_nodes = gda_server_operation_get_root_nodes (data->op);
	for (i = 0; root_nodes && root_nodes[i]; i++)
		extract_named_parameters (data->op, root_nodes[i], buffer);
	g_strfreev (root_nodes);

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (sw), view);
	gtk_widget_show_all (sw);

	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dlg))),
			    sw, TRUE, TRUE, 0);
	gtk_widget_set_size_request (dlg, 530, 350);

	gtk_dialog_run (GTK_DIALOG (dlg));
	gtk_widget_destroy (dlg);
}

static void
show_sql (G_GNUC_UNUSED GtkButton *button, DemoData *data)
{
	GdaServerProvider *prov;

	if (!data->op)
		return;

	prov = get_provider_obj (data);
	if (prov) {
		gchar *sql, *msg;
		GtkMessageType msg_type = GTK_MESSAGE_INFO;
		GtkWidget *dlg;
		GError *error = NULL;

		sql = gda_server_provider_render_operation (prov, NULL, data->op, &error);
		if (!sql) {
			msg_type = GTK_MESSAGE_ERROR;
			msg = g_strdup_printf ("<b>Can't render operation as SQL:</b>\n%s\n", 
					       error && error->message ? error->message : 
					       "No detail (This operation may not be accessible using SQL)");
			if (error)
				g_error_free (error);
		}
		else
			msg = g_strdup_printf ("<b>SQL:</b>\n%s", sql);

		dlg = gtk_message_dialog_new (GTK_WINDOW (data->top_window),
					      GTK_DIALOG_MODAL, msg_type, GTK_BUTTONS_CLOSE, NULL);
		gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dlg), msg);
		g_free (sql);
		g_free (msg);

		gtk_dialog_run (GTK_DIALOG (dlg));
		gtk_widget_destroy (dlg);
	}
	else 
		g_warning ("Could not get provider object");
}
