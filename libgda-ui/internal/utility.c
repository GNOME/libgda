/*
 * Copyright (C) 2009 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2012 Snicksie <none@none.com>
 * Copyright (C) 2013 Daniel Espinosa <esodan@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <string.h>
#include <glib/gprintf.h>
#include "utility.h"
#include <libgda/libgda.h>
#include <glib/gi18n-lib.h>
#include <libgda-ui/gdaui-data-entry.h>
#include <libgda-ui/libgda-ui.h>
#include <libgda-ui/gdaui-decl.h>


/*
 * _gdaui_utility_entry_build_actions_menu
 * @obj_data:
 * @attrs:
 * @function:
 *
 * Creates a GtkMenu widget which contains the possible actions on a data entry which 
 * attributes are @attrs. After the menu has been displayed, and when an action is selected,
 * the @function callback is called with the 'user_data' being @obj_data.
 *
 * The menu item which emits the signal has a "action" attribute which describes what action the
 * user has requested.
 *
 * Returns: the new menu
 */
GtkWidget *
_gdaui_utility_entry_build_actions_menu (GObject *obj_data, guint attrs, GCallback function)
{
	GtkWidget *menu, *mitem;
	gchar *str;
	gboolean nullact = FALSE;
	gboolean defact = FALSE;
	gboolean reset = FALSE;

	gboolean value_is_null;
	gboolean value_is_modified;
	gboolean value_is_default;

	menu = gtk_menu_new ();

	/* which menu items to make sensitive ? */
	value_is_null = attrs & GDA_VALUE_ATTR_IS_NULL;
	value_is_modified = ! (attrs & GDA_VALUE_ATTR_IS_UNCHANGED);
	value_is_default = attrs & GDA_VALUE_ATTR_IS_DEFAULT;

	if (! (attrs & GDA_VALUE_ATTR_NO_MODIF)) {
		if ((attrs & GDA_VALUE_ATTR_CAN_BE_NULL) && 
		    !(attrs & GDA_VALUE_ATTR_IS_NULL))
			nullact = TRUE;
		if ((attrs & GDA_VALUE_ATTR_CAN_BE_DEFAULT) && 
		    !(attrs & GDA_VALUE_ATTR_IS_DEFAULT))
			defact = TRUE;
		if (!(attrs & GDA_VALUE_ATTR_IS_UNCHANGED)) {
			if (attrs & GDA_VALUE_ATTR_HAS_VALUE_ORIG) 
				reset = TRUE;
		}
	}

	/* set to NULL item */
	str = g_strdup (_("Unset"));
	mitem = gtk_check_menu_item_new_with_label (str);
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (mitem),
					value_is_null);
	gtk_widget_show (mitem);
	g_object_set_data (G_OBJECT (mitem), "action", GUINT_TO_POINTER (GDA_VALUE_ATTR_IS_NULL));
	g_signal_connect (G_OBJECT (mitem), "activate",
			  G_CALLBACK (function), obj_data);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), mitem);
	g_free (str);
	gtk_widget_set_sensitive (mitem, nullact);

	/* default value item */
	str = g_strdup (_("Set to default value"));
	mitem = gtk_check_menu_item_new_with_label (str);
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (mitem), 
					value_is_default);
	gtk_widget_show (mitem);
	g_object_set_data (G_OBJECT (mitem), "action", GUINT_TO_POINTER (GDA_VALUE_ATTR_IS_DEFAULT));
	g_signal_connect (G_OBJECT (mitem), "activate",
			  G_CALLBACK (function), obj_data);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), mitem);
	g_free (str);
	gtk_widget_set_sensitive (mitem, defact);
		
	/* reset to original value item */
	str = g_strdup (_("Reset to original value"));
	mitem = gtk_check_menu_item_new_with_label (str);
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (mitem), 
					!value_is_modified);
	gtk_widget_show (mitem);
	g_object_set_data (G_OBJECT (mitem), "action", GUINT_TO_POINTER (GDA_VALUE_ATTR_IS_UNCHANGED));
	g_signal_connect (G_OBJECT (mitem), "activate",
			  G_CALLBACK (function), obj_data);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), mitem);
	g_free (str);
	gtk_widget_set_sensitive (mitem, reset);

	return menu;
}

/*
 * _gdaui_utility_entry_build_info_colors_array_a
 * 
 * Creates an array of colors for the different states of an entry:
 *    Valid   <-> No special color
 *    Null    <-> Green
 *    Default <-> Blue
 *    Invalid <-> Red
 * Each status (except Valid) is represented by two colors for GTK_STATE_NORMAL and
 * GTK_STATE_PRELIGHT.
 *
 * Returns: a new array of 6 colors
 */
GdkRGBA **
_gdaui_utility_entry_build_info_colors_array_a (void)
{
	GdkRGBA **colors;
	GdkRGBA *color;
	
	colors = g_new0 (GdkRGBA *, 6);
	
	/* Green color */
	color = g_new0 (GdkRGBA, 1);
	g_assert (gdk_rgba_parse (color, GDAUI_COLOR_NORMAL_NULL));
	colors[0] = color;
	
	color = g_new0 (GdkRGBA, 1);
	g_assert (gdk_rgba_parse (color, GDAUI_COLOR_PRELIGHT_NULL));
	colors[1] = color;
	
	/* Blue color */
	color = g_new0 (GdkRGBA, 1);
	g_assert (gdk_rgba_parse (color, GDAUI_COLOR_NORMAL_DEFAULT));
	colors[2] = color;
	
	color = g_new0 (GdkRGBA, 1);
	g_assert (gdk_rgba_parse (color, GDAUI_COLOR_PRELIGHT_DEFAULT));
	colors[3] = color;
	
	/* Red color */
	color = g_new0 (GdkRGBA, 1);
	g_assert (gdk_rgba_parse (color, GDAUI_COLOR_NORMAL_INVALID));
	colors[4] = color;
	
	color = g_new0 (GdkRGBA, 1);
	g_assert (gdk_rgba_parse (color, GDAUI_COLOR_PRELIGHT_INVALID));
	colors[5] = color;

	return colors;
}



/*
 * _gdaui_utility_markup_title
 */
gchar *
_gdaui_utility_markup_title (const gchar *title, gboolean optional)
{
	if (!optional)
		return g_markup_printf_escaped ("%s <span foreground='red' weight='bold'>*</span>:", title);
	else
		return g_markup_printf_escaped ("%s:", title);
}

/*
 * _gdaui_utility_proxy_compute_attributes_for_group
 *
 * Computes an attributes from the individual attributes of the values stored in @store and
 * corresponding to the columns for the parameters in @group (as described by @model_iter), 
 * at row pointed by @iter
 *
 * Returns: the attributes
 */
guint
_gdaui_utility_proxy_compute_attributes_for_group (GdauiSetGroup *group, GdauiDataStore *store, 
						     GdaDataModelIter *model_iter, GtkTreeIter *tree_iter, 
						     gboolean *to_be_deleted)
{
	guint attributes = GDA_VALUE_ATTR_IS_NULL | GDA_VALUE_ATTR_CAN_BE_NULL |
                GDA_VALUE_ATTR_IS_DEFAULT | GDA_VALUE_ATTR_CAN_BE_DEFAULT |
                GDA_VALUE_ATTR_IS_UNCHANGED | GDA_VALUE_ATTR_HAS_VALUE_ORIG;
        gboolean to_del = TRUE, local_to_del;
        GSList *list;
        gint col;
        gint offset;
        guint localattr;
	GdaDataProxy *proxy;

	proxy = gdaui_data_store_get_proxy (store);
        offset = gda_data_proxy_get_proxied_model_n_cols (proxy);

        /* list the values in proxy_model for each param in GDA_SET_NODE (group->group->nodes->data)->params */
	attributes = 0;
        for (list = gda_set_group_get_nodes (gdaui_set_group_get_group (group)); list; list = list->next) {
		col = g_slist_index (gda_set_get_holders ((GdaSet*)model_iter), gda_set_node_get_holder (GDA_SET_NODE (list->data)));
                gtk_tree_model_get (GTK_TREE_MODEL (store), tree_iter,
                                    GDAUI_DATA_STORE_COL_TO_DELETE, &local_to_del,
                                    offset + col, &localattr, -1);
		if (list == gda_set_group_get_nodes (gdaui_set_group_get_group (group)))
			attributes = localattr;
		else
			attributes &= localattr;
                to_del = to_del && local_to_del;
        }

	if (to_be_deleted)
                *to_be_deleted = to_del;

        return attributes;
}

/*
 * _gdaui_utility_proxy_compute_values_for_group:
 *
 * Computes a list of values containing the individual values stored in @store and
 * corresponding to the columns for the parameters if @group, at row pointed by @tree_iter.
 * 
 * If @model_values is TRUE, then the values in the returned list are the values of the
 * @group->nodes_source->data_model, at the @group->nodes_source->shown_cols_index columns
 *
 * For both performances reasons and because the contents of the @group->nodes_source->data_model
 * may change, this function uses the gda_data_proxy_get_model_row_value() function.
 *
 * The returned list must be freed by the caller, but the values in the list must not be modified or
 * freed.
 */
GList *
_gdaui_utility_proxy_compute_values_for_group (GdauiSetGroup *group, GdauiDataStore *store, 
					       GdaDataModelIter *model_iter, 
					       GtkTreeIter *tree_iter, gboolean model_values)
{
	GList *retval = NULL;
#ifdef PROXY_STORE_EXTRA_VALUES
	GdaDataProxy *proxy;
	proxy = gdaui_data_store_get_proxy (store);
#endif
	if (!model_values) {
		GSList *list;
		GValue *value;

		for (list = gda_set_group_get_nodes (gdaui_set_group_get_group (group)); list; list = list->next) {
			gint col;

			col = g_slist_index (gda_set_get_holders ((GdaSet*)model_iter), gda_set_node_get_holder (GDA_SET_NODE (list->data)));
			gtk_tree_model_get (GTK_TREE_MODEL (store), tree_iter, col, &value, -1);
			retval = g_list_append (retval, value);
		}
	}
	else {
		gint col, i;
#ifdef PROXY_STORE_EXTRA_VALUES
		gint proxy_row;
#endif
		GdauiSetSource *source;
		const GValue *value;
		gboolean slow_way = FALSE;
		gboolean ret_null = FALSE;
#ifdef PROXY_STORE_EXTRA_VALUES
		proxy_row = gdaui_data_store_get_row_from_iter (store, tree_iter);
#endif
		source = gdaui_set_group_get_source (group);
		for (i = 0 ; (i < gdaui_set_source_get_shown_n_cols (source))  && !ret_null; i++) {
			col = (gdaui_set_source_get_shown_columns (source))[i];
#ifdef PROXY_STORE_EXTRA_VALUES
			if (!slow_way) {
				value = gda_data_proxy_get_model_row_value (proxy, source->data_model, proxy_row, col);
				if (value)
					retval = g_list_append (retval, (GValue *) value);
				else {
					if (gda_data_proxy_get_assigned_model_col (proxy, source->data_model, col) < 0)
						slow_way = TRUE;
					else
						retval = g_list_append (retval, NULL);
				}
			}
#endif
			slow_way = TRUE;
			
			if (slow_way) {
				/* may be a bit slow: try to find the values in the model, it's better if the user
				 * uses gda_data_proxy_assign_model_col()!  */
				GSList *key_values = NULL;
				gint row, *cols_index;
				GSList *list;
				gint j;
				
				cols_index = g_new0 (gint, gda_set_group_get_n_nodes (gdaui_set_group_get_group (group)));
				for (list = gda_set_group_get_nodes (gdaui_set_group_get_group (group)), j = 0; list; list = list->next, j++) {
					gint colno;
					colno = g_slist_index (gda_set_get_holders ((GdaSet*)model_iter),
							       gda_set_node_get_holder (GDA_SET_NODE (list->data)));
					cols_index [j] = gda_set_node_get_source_column (GDA_SET_NODE (list->data));
					gtk_tree_model_get (GTK_TREE_MODEL (store), tree_iter,
							    colno, &value, -1);
					key_values = g_slist_append (key_values, (GValue *) value);
				}
				
				row = gda_data_model_get_row_from_values (gda_set_source_get_data_model (gdaui_set_source_get_source (source)), 
									  key_values, cols_index);
				if (row >= 0) {
					value = gda_data_model_get_value_at (gda_set_source_get_data_model (gdaui_set_source_get_source (source)),
									     col, row, NULL);
					retval = g_list_append (retval, (GValue *) value);
				}
				else {
#ifdef DEBUG_WARNING
					g_warning ("Could not find requested value in restricting data model");
					g_print ("Requested: ");
					list = key_values;
					j = 0;
					while (list) {
						gchar *str;
						
						if (value) {
							str = gda_value_stringify ((GValue *) list->data);
							g_print ("/%s @col %d", str, cols_index [j]);
							g_free (str);
						}
						else
							g_print ("/NULL @col %d", cols_index [j]);
						list = g_slist_next (list);
						j++;
					}
					g_print (" in data model: \n");
					gda_data_model_dump (source->data_model, stdout);
#endif
					ret_null = TRUE;
				}
				g_slist_free (key_values);
			}
		}

		if (ret_null) {
			g_list_free (retval);
			retval = NULL;
		}
	}

	return retval;
}

/*
 * Errors reporting
 */
static GtkWidget *
create_data_error_dialog (GdauiDataProxy *form, gboolean with_question, gboolean can_discard, GError *filled_error)
{
	GtkWidget *dlg;
	const gchar *msg1 = NULL, *msg2 = NULL;

	if (can_discard) {
		msg1 = _("Current modified data is invalid");
		if (with_question)
			msg2 =_("You may now choose to correct it, or to discard "
				"the modifications.\n\n"
				"What do you want to do?");
		else
			msg2 = _("please correct it and try again, or discard "
				 "the modifications.");
	}
	else {
		if (with_question)
			g_warning ("Incoherence problem...\n");
		else {
			msg1 = _("Part of the current modified data was invalid");
			msg2 = _("As no transaction was used, only a part of the valid data\n"
				 "has been written, and the remaining modification have been discarded.");
		}
	}
	dlg = gtk_message_dialog_new_with_markup ((GtkWindow *) gtk_widget_get_toplevel (GTK_WIDGET (form)),
						  GTK_DIALOG_MODAL,
						  GTK_MESSAGE_ERROR, 
						  with_question ? GTK_BUTTONS_NONE : GTK_BUTTONS_CLOSE,
						  "<b>%s:</b>\n%s", msg1, msg2);

	if (filled_error && filled_error->message) {
		GtkWidget *label;

		label = gtk_label_new (filled_error->message);
		gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dlg))),
				    label, TRUE, TRUE, 0);
		gtk_widget_show (label);
	}

	return dlg;
}

/*
 * _gdaui_utility_display_error_with_keep_or_discard_choice
 * @form: a #GdauiDataProxy
 * @filled_error: a #GError containing the error to display
 *
 * Displays a dialog showing @filled_error's message and asks the user to either keep the data which
 * led to the error, or to discard it.
 *
 * Returns: TRUE if current data can be discarded
 */
gboolean
_gdaui_utility_display_error_with_keep_or_discard_choice (GdauiDataProxy *form, GError *filled_error)
{
	GtkWidget *dlg;
	gint res;

	if (filled_error && (filled_error->domain == GDA_DATA_PROXY_ERROR) &&
	    (filled_error->code == GDA_DATA_PROXY_COMMIT_CANCELLED))
		return FALSE;

	dlg = create_data_error_dialog (form, TRUE, TRUE, filled_error);
	gtk_dialog_add_buttons (GTK_DIALOG (dlg),
				_("Discard modified data"), GTK_RESPONSE_REJECT,
				_("Correct data first"), GTK_RESPONSE_NONE, NULL);
	res = gtk_dialog_run (GTK_DIALOG (dlg));
	gtk_widget_destroy (dlg);
	return (res == GTK_RESPONSE_REJECT) ? TRUE : FALSE;
}

/*
 * _gdaui_utility_display_error
 * @form: a #GdauiDataProxy
 * @filled_error: a #GError containing the error to display
 *
 * Displays a dialog showing @filled_error's message and asks the user to either keep the data which
 * led to the error.
 */
void
_gdaui_utility_display_error (GdauiDataProxy *form, gboolean can_discard, GError *filled_error)
{
	GtkWidget *dlg;
	
	if (filled_error && (filled_error->domain == GDA_DATA_PROXY_ERROR) &&
	    (filled_error->code == GDA_DATA_PROXY_COMMIT_CANCELLED))
		return;

	dlg = create_data_error_dialog (form, FALSE, can_discard, filled_error);
	gtk_dialog_run (GTK_DIALOG (dlg));
	gtk_widget_destroy (dlg);

}

/*
 * _gdaui_utility_show_error
 * @format:
 * @...:
 *
 */
void
_gdaui_utility_show_error (GtkWindow *parent, const gchar *format, ...)
{
	va_list args;
        gchar sz[2048];
	GtkWidget *dialog;

	/* build the message string */
        va_start (args, format);
        vsnprintf (sz, sizeof sz, format, args);
        va_end (args);

	/* find a parent if none provided */
	if (parent && !GTK_IS_WINDOW (parent))
		parent = (GtkWindow*) gtk_widget_get_toplevel ((GtkWidget*) parent);

	if (!parent) {
		GApplication *app;
		app = g_application_get_default ();
		if (app && GTK_IS_APPLICATION (app))
			parent = gtk_application_get_active_window (GTK_APPLICATION (app));
	}
	/* create the error message dialog */
	dialog = gtk_message_dialog_new_with_markup (parent,
						     GTK_DIALOG_DESTROY_WITH_PARENT |
						     GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
						     GTK_BUTTONS_CLOSE,
						     "<b>%s:</b>\n%s",
						     _("Error"), sz);
	gtk_dialog_add_action_widget (GTK_DIALOG (dialog),
				      gtk_button_new_with_label (_("Ok")),
				      GTK_RESPONSE_OK);
	gtk_widget_show_all (dialog);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

/*
 * Compares 2 GdaDataModelIter
 *
 * Returns: %TRUE if they have at least one difference
 */
gboolean
_gdaui_utility_iter_differ (GdaDataModelIter *iter1, GdaDataModelIter *iter2)
{
	GSList *list1, *list2;
        gboolean retval = TRUE;

	for (list1 = gda_set_get_holders (GDA_SET (iter1)), list2 = gda_set_get_holders (GDA_SET (iter2));
             list1 && list2;
             list1 = list1->next, list2 = list2->next) {
                GdaHolder *oh, *nh;
                oh = GDA_HOLDER (list1->data);
                nh = GDA_HOLDER (list2->data);

                if (gda_holder_get_not_null (oh) != gda_holder_get_not_null (nh))
                        goto out;

                GType ot, nt;
                ot = gda_holder_get_g_type (oh);
                nt = gda_holder_get_g_type (nh);
                if (ot && (ot != nt))
                        goto out;
                if (ot != nt)
                        g_object_set (oh, "g-type", nt, NULL);

                const gchar *oid, *nid;
                oid = gda_holder_get_id (oh);
                nid = gda_holder_get_id (nh);
                if ((oid && !nid) || (!oid && nid) ||
                    (oid && strcmp (oid, nid)))
                        goto out;
        }

        if (list1 || list2)
                goto out;
        retval = FALSE; /* iters don't differ */

 out:
        return retval;
}


static gboolean tree_view_button_pressed_cb (GtkWidget *widget, GdkEventButton *event, gpointer unuseddata);
/*
 * Setup a callback on the treeview to, on right click:
 *   - if there is no row under the cursor, then force an empty selection
 *   OR
 *   - if the row under the cursor is not selected, then force the selection of only that row
 *   OR
 *   - otherwise don't change anything
 */
void
_gdaui_setup_right_click_selection_on_treeview (GtkTreeView *tview)
{
	g_return_if_fail (GTK_IS_TREE_VIEW (tview));
	g_signal_connect (G_OBJECT (tview), "button-press-event",
                          G_CALLBACK (tree_view_button_pressed_cb), NULL);
}

static gboolean
tree_view_button_pressed_cb (GtkWidget *widget, GdkEventButton *event, G_GNUC_UNUSED gpointer data)
{
	GtkTreeView *tree_view;
	GtkTreeSelection *selection;

	if (event->button != 3)
		return FALSE;

	tree_view = GTK_TREE_VIEW (widget);
	selection = gtk_tree_view_get_selection (tree_view);

	/* force selection of row on which clicked occurred */
	if (event->window == gtk_tree_view_get_bin_window (tree_view)) {
		GtkTreePath *path;
		if (gtk_tree_view_get_path_at_pos (tree_view, event->x, event->y, &path,
						   NULL, NULL, NULL)) {
			if (! gtk_tree_selection_path_is_selected (selection, path)) {
				gtk_tree_selection_unselect_all (selection);
				gtk_tree_selection_select_path (selection, path);
			}
			gtk_tree_path_free (path);
		}
		else
			gtk_tree_selection_unselect_all (selection);
	}

	return FALSE;
}
