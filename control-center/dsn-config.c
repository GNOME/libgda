/*
 * Copyright (C) 2009 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <glib/gi18n-lib.h>
#include <libgda/binreloc/gda-binreloc.h>
#include <libgda/gda-config.h>
#include <libgda/gda-data-model-array.h>
#include <libgda-ui/libgda-ui.h>
#include "dsn-config.h"
#include "gdaui-bar.h"
#include "gdaui-dsn-editor.h"
#include <libgda-ui/internal/utility.h>

#define DSN_CONFIG_DATA "DSN_ConfigData"
#define ST_NOPROP "noprop"
#define ST_PROP "prop"

typedef struct {
	GtkWidget *title;
	GtkWidget *dsn_list;
	GtkWidget *dialog;
	GtkWidget *stack;
	GdauiDsnEditor *dsn_editor;
	GtkToggleButton *view_buttons[GDAUI_DSN_EDITOR_PANE_AUTH + 1];
	GtkWidget *commit_button;
} DsnConfigPrivate;

static void
free_private_data (gpointer data)
{
	DsnConfigPrivate *priv = (DsnConfigPrivate *) data;
	g_free (priv);
}

static void
list_popup_delete_cb (G_GNUC_UNUSED GtkWidget *menu, gpointer user_data)
{
	dsn_config_delete (GTK_WIDGET (user_data));
}

static GtkWidget *
new_menu_item (const gchar *label,
	       GCallback cb_func,
	       gpointer user_data)
{
	GtkWidget *item;

	item = gtk_menu_item_new_with_mnemonic (label);
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (cb_func), user_data);

	return item;
}

static void
list_popup_cb (GdauiRawGrid *grid, GtkMenu *menu, gpointer user_data)
{
	GtkWidget *item_delete;
	gboolean ok;
	GArray *selection;

	item_delete = new_menu_item (_("_Delete"),
				     G_CALLBACK (list_popup_delete_cb),
				     user_data);

	selection = gdaui_data_selector_get_selected_rows (GDAUI_DATA_SELECTOR (grid));
	ok = (selection != NULL);
	if (selection)
		g_array_free (selection, TRUE);
	gtk_widget_set_sensitive (item_delete, ok);

	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), gtk_separator_menu_item_new ());
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item_delete);
}

static void
list_selection_changed_cb (GdauiRawGrid *grid, gpointer user_data)
{
	DsnConfigPrivate *priv;
	GdaDataModel *model;
	GArray *selection;
	gchar *str;
	const GValue *cvalue;
	GtkWidget *win = gtk_widget_get_toplevel (GTK_WIDGET (grid));
	if (gtk_widget_is_toplevel (win)) {
		g_simple_action_set_enabled (G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (win),
											  "DatasourceDelete")), FALSE);
	}

	priv = g_object_get_data (G_OBJECT (user_data), DSN_CONFIG_DATA);

	selection = gdaui_data_selector_get_selected_rows (GDAUI_DATA_SELECTOR (priv->dsn_list));
	if (!selection) {
		gtk_stack_set_visible_child_name (GTK_STACK (priv->stack), ST_NOPROP);
		return;
	}

	model = gdaui_data_selector_get_model (GDAUI_DATA_SELECTOR (priv->dsn_list));
	if (!GDA_IS_DATA_MODEL (model)) {
		g_array_free (selection, TRUE);
		gtk_stack_set_visible_child_name (GTK_STACK (priv->stack), ST_NOPROP);
		return;
	}

	cvalue = gda_data_model_get_value_at (model, 0, g_array_index (selection, gint, 0), NULL);
	g_array_free (selection, TRUE);
	if (!cvalue) {
		gtk_stack_set_visible_child_name (GTK_STACK (priv->stack), ST_NOPROP);
		return;
	}

	str = gda_value_stringify ((GValue *) cvalue);
	g_print ("==> %s\n", str);

	GdaDsnInfo *dsn_info;
	dsn_info = gda_config_get_dsn_info (str);
	g_free (str);
	if (!dsn_info) {
		/* something went wrong here... */
		gtk_stack_set_visible_child_name (GTK_STACK (priv->stack), ST_NOPROP);
		return;
	}

	gdaui_dsn_editor_set_dsn (priv->dsn_editor, dsn_info);

	if (gdaui_dsn_editor_need_authentication (priv->dsn_editor))
		gtk_widget_show (GTK_WIDGET (priv->view_buttons [GDAUI_DSN_EDITOR_PANE_AUTH]));
	else
		gtk_widget_hide (GTK_WIDGET (priv->view_buttons [GDAUI_DSN_EDITOR_PANE_AUTH]));
	gtk_toggle_button_set_active (priv->view_buttons [GDAUI_DSN_EDITOR_PANE_DEFINITION], TRUE);

	gtk_stack_set_visible_child_name (GTK_STACK (priv->stack), ST_PROP);
	if (gtk_widget_is_toplevel (win)) {
		g_simple_action_set_enabled (G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (win),
											  "DatasourceDelete")), TRUE);
	}
}

/*
 * Public functions
 */
static void dsn_editor_changed_cb (G_GNUC_UNUSED GdauiDsnEditor *editor, GtkWidget *dsn);
static void view_toggled_cb (GtkToggleButton *button, GtkWidget *dsn);
static void save_cb (GtkButton *button, GtkWidget *dsn);

GtkWidget *
dsn_config_new (void)
{
	DsnConfigPrivate *priv;
	GtkWidget *dsn;
	GtkWidget *label;
	GtkWidget *sw;
	gchar *title;
	GdaDataModel *model;

	priv = g_new0 (DsnConfigPrivate, 1);
	dsn = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_show (dsn);
        gtk_container_set_border_width (GTK_CONTAINER (dsn), 6);
	g_object_set_data_full (G_OBJECT (dsn), DSN_CONFIG_DATA, priv,
				(GDestroyNotify) free_private_data);

	/* title */
	title = g_strdup_printf ("<b>%s</b>\n%s", _("Data Sources"),
				 _("Data sources are the means by which database "
				 "connections are identified: all "
				 "the information needed to open a connection to "
				 "a specific database using a 'provider' is referenced using "
				 "a unique name."));
	priv->title = gdaui_bar_new (title);
	g_free (title);

	gdaui_bar_set_icon_from_resource (GDAUI_BAR (priv->title), "/images/gdaui-generic.png");

	gtk_box_pack_start (GTK_BOX (dsn), priv->title, FALSE, FALSE, 0);
	gtk_widget_show (priv->title);

	/* horizontal box for the provider list and its properties */
	GtkWidget *hbox;
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start (GTK_BOX (dsn), hbox, TRUE, TRUE, 0);

	/* left part */
	GtkWidget *vbox;
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_size_request (vbox, 150, -1);
	gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);

	/* create the data source list */
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);
	gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);

	model = gda_config_list_dsn ();
	priv->dsn_list = gdaui_raw_grid_new (model);
	gtk_tree_view_move_column_after (GTK_TREE_VIEW (priv->dsn_list),
					 gtk_tree_view_get_column (GTK_TREE_VIEW (priv->dsn_list), 1),
					 gtk_tree_view_get_column (GTK_TREE_VIEW (priv->dsn_list), 2));

	g_object_unref (model);
	g_object_set_data (G_OBJECT (dsn), "grid", priv->dsn_list);
	gdaui_data_proxy_column_set_editable (GDAUI_DATA_PROXY (priv->dsn_list), 0, FALSE);
	gdaui_data_selector_set_column_visible (GDAUI_DATA_SELECTOR (priv->dsn_list), -1, FALSE);
	gdaui_data_selector_set_column_visible (GDAUI_DATA_SELECTOR (priv->dsn_list), 0, TRUE);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (priv->dsn_list), FALSE);
	g_object_set (priv->dsn_list, "info-cell-visible", FALSE, NULL);

	gtk_container_add (GTK_CONTAINER (sw), priv->dsn_list);

	g_signal_connect (priv->dsn_list, "selection-changed",
			  G_CALLBACK (list_selection_changed_cb), dsn);
	g_signal_connect (priv->dsn_list, "populate-popup",
			  G_CALLBACK (list_popup_cb), dsn);

	/* add/delete buttons */
	GtkWidget *toolbar;
	toolbar = gtk_toolbar_new ();
	gtk_toolbar_set_icon_size (GTK_TOOLBAR (toolbar), GTK_ICON_SIZE_SMALL_TOOLBAR);
	gtk_style_context_add_class (gtk_widget_get_style_context (toolbar), "inline-toolbar");
	gtk_box_pack_start (GTK_BOX (vbox), toolbar, FALSE, FALSE, 0);
	GtkToolItem *titem;
	titem = gtk_tool_button_new (NULL, NULL);
	gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (titem), "list-add-symbolic");
	gtk_actionable_set_action_name (GTK_ACTIONABLE (titem), "win.DatasourceNew");
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), titem, -1);
	titem = gtk_tool_button_new (NULL, NULL);
	gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (titem), "list-remove-symbolic");
	gtk_actionable_set_action_name (GTK_ACTIONABLE (titem), "win.DatasourceDelete");
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), titem, -1);

	/* create the data source's properties */
	GtkWidget *stack;
	stack = gtk_stack_new ();
	priv->stack = stack;
	gtk_box_pack_start (GTK_BOX (hbox), stack, TRUE, TRUE, 10);

	label = gtk_label_new (_("No data source selected."));
	gtk_stack_add_named (GTK_STACK (stack), label, ST_NOPROP);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_stack_add_named (GTK_STACK (stack), vbox, ST_PROP);

	GtkWidget *form;
	form = gdaui_dsn_editor_new ();
	priv->dsn_editor = GDAUI_DSN_EDITOR (form);
	gtk_box_pack_start (GTK_BOX (vbox), form, TRUE, TRUE, 0);
	g_signal_connect (priv->dsn_editor, "changed",
			  G_CALLBACK (dsn_editor_changed_cb), dsn);

	/* action buttons */
	GtkWidget *hbox2;
	hbox2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start (GTK_BOX (vbox), hbox2, FALSE, FALSE, 6);

	GtkWidget *bbox;
	bbox = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_widget_set_hexpand (bbox, TRUE);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), GTK_BUTTONBOX_CENTER);
	gtk_box_pack_start (GTK_BOX (hbox2), bbox, FALSE, FALSE, 6);

	GtkWidget *button;
	button = gtk_toggle_button_new_with_label (_("Definition"));
	gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);
	priv->view_buttons [GDAUI_DSN_EDITOR_PANE_DEFINITION] = GTK_TOGGLE_BUTTON (button);
	g_signal_connect (button, "toggled",
			  G_CALLBACK (view_toggled_cb), dsn);

	button = gtk_toggle_button_new_with_label (_("Parameters"));
	gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);
	priv->view_buttons [GDAUI_DSN_EDITOR_PANE_PARAMS] = GTK_TOGGLE_BUTTON (button);
	g_signal_connect (button, "toggled",
			  G_CALLBACK (view_toggled_cb), dsn);

	button = gtk_toggle_button_new_with_label (_("Authentication"));
	gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);
	priv->view_buttons [GDAUI_DSN_EDITOR_PANE_AUTH] = GTK_TOGGLE_BUTTON (button);
	g_signal_connect (button, "toggled",
			  G_CALLBACK (view_toggled_cb), dsn);

	button = gtk_button_new_from_icon_name ("document-save-symbolic", GTK_ICON_SIZE_BUTTON);
	gtk_box_pack_start (GTK_BOX (hbox2), button, FALSE, FALSE, 0);
	priv->commit_button = button;
	gtk_widget_set_sensitive (button, FALSE);
	gtk_widget_set_tooltip_text (button, _("Write changes made to the DSN"));
	g_signal_connect (button, "clicked",
			  G_CALLBACK (save_cb), dsn);

	gtk_widget_show_all (hbox);
	return dsn;
}

static void
view_toggled_cb (GtkToggleButton *button, GtkWidget *dsn)
{
	if (! gtk_toggle_button_get_active (button))
		return;

	DsnConfigPrivate *priv;
	priv = g_object_get_data (G_OBJECT (dsn), DSN_CONFIG_DATA);
	if (button == priv->view_buttons [GDAUI_DSN_EDITOR_PANE_DEFINITION]) {
		gtk_toggle_button_set_active (priv->view_buttons [GDAUI_DSN_EDITOR_PANE_PARAMS], FALSE);
		gtk_toggle_button_set_active (priv->view_buttons [GDAUI_DSN_EDITOR_PANE_AUTH], FALSE);
		gdaui_dsn_editor_show_pane (priv->dsn_editor, GDAUI_DSN_EDITOR_PANE_DEFINITION);
	}
	else if (button == priv->view_buttons [GDAUI_DSN_EDITOR_PANE_PARAMS]) {
		gtk_toggle_button_set_active (priv->view_buttons [GDAUI_DSN_EDITOR_PANE_DEFINITION], FALSE);
		gtk_toggle_button_set_active (priv->view_buttons [GDAUI_DSN_EDITOR_PANE_AUTH], FALSE);
		gdaui_dsn_editor_show_pane (priv->dsn_editor, GDAUI_DSN_EDITOR_PANE_PARAMS);
	}
	else if (button == priv->view_buttons [GDAUI_DSN_EDITOR_PANE_AUTH]) {
		gtk_toggle_button_set_active (priv->view_buttons [GDAUI_DSN_EDITOR_PANE_DEFINITION], FALSE);
		gtk_toggle_button_set_active (priv->view_buttons [GDAUI_DSN_EDITOR_PANE_PARAMS], FALSE);
		gdaui_dsn_editor_show_pane (priv->dsn_editor, GDAUI_DSN_EDITOR_PANE_AUTH);
	}
	else
		g_assert_not_reached ();
}

static void
dsn_editor_changed_cb (GdauiDsnEditor *editor, GtkWidget *dsn)
{
	DsnConfigPrivate *priv;
	priv = g_object_get_data (G_OBJECT (dsn), DSN_CONFIG_DATA);

	gboolean can_save = FALSE;
	can_save = gdaui_dsn_editor_has_been_changed (editor);

	gtk_widget_set_sensitive (priv->commit_button, can_save);

	if (gdaui_dsn_editor_need_authentication (priv->dsn_editor))
		gtk_widget_show (GTK_WIDGET (priv->view_buttons [GDAUI_DSN_EDITOR_PANE_AUTH]));
	else
		gtk_widget_hide (GTK_WIDGET (priv->view_buttons [GDAUI_DSN_EDITOR_PANE_AUTH]));
}

static void
save_cb (G_GNUC_UNUSED GtkButton *button,
         GtkWidget *dsn)
{
	DsnConfigPrivate *priv;
	priv = g_object_get_data (G_OBJECT (dsn), DSN_CONFIG_DATA);

	const GdaDsnInfo *newdsn;
	newdsn = gdaui_dsn_editor_get_dsn (priv->dsn_editor);

	GError *error = NULL;
	if (! gda_config_define_dsn (newdsn, &error)) {
		_gdaui_utility_show_error (NULL, _("Could not save DSN definition: %s"), error ? error->message : _("No detail"));
		g_clear_error (&error);
	}

	/* update reference DSN and UI */
	GdaDsnInfo *dsn_info;
	dsn_info = gda_config_get_dsn_info (newdsn->name);
	gdaui_dsn_editor_set_dsn (priv->dsn_editor, dsn_info);
}

void
dsn_config_delete (GtkWidget *dsn)
{
	DsnConfigPrivate *priv;
	GtkTreeSelection *sel;
	GList *list, *sel_rows;
	GList *sel_dsn = NULL;
	GdaDataModel *model;

	priv = g_object_get_data (G_OBJECT (dsn), DSN_CONFIG_DATA);

	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->dsn_list));
	sel_rows = gtk_tree_selection_get_selected_rows (sel, NULL);
	model = gdaui_data_selector_get_model (GDAUI_DATA_SELECTOR (priv->dsn_list));
	g_assert (GDA_IS_DATA_MODEL (model));

	/* make a list of DSN to remove */
	for (list = sel_rows; list; list = list->next) {
		gchar *dsn;
		gint *row;
		const GValue *cvalue;
		
		row = gtk_tree_path_get_indices ((GtkTreePath *) list->data);
		cvalue = gda_data_model_get_value_at (model, 0, *row, NULL);
		if (cvalue) {
			dsn = gda_value_stringify ((GValue *) cvalue);
			sel_dsn = g_list_prepend (sel_dsn, dsn);
		}
		gtk_tree_path_free ((GtkTreePath *) list->data);
	}
	g_list_free (sel_rows);

	/* actually remove the DSN listed */
	for (list = sel_dsn; list; list = list->next) {
		gchar *str, *dsn;
		GtkWidget *dialog;
		dsn = (gchar *) list->data;

		str = g_strdup_printf (_("Are you sure you want to remove the data source '%s'?"), dsn);
		dialog = gtk_message_dialog_new_with_markup (GTK_WINDOW (gtk_widget_get_toplevel (priv->dsn_list)),
							     GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION,
							     GTK_BUTTONS_YES_NO, 
							     "<b>%s:</b>\n\n%s",
							     _("Data source removal confirmation"), str);
		g_free (str);
		gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_NO);
		gtk_widget_show (dialog);
		
		if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_YES) 
			gda_config_remove_dsn (dsn, NULL);
		gtk_widget_destroy (dialog);
		
		g_free (dsn);
	}
}
