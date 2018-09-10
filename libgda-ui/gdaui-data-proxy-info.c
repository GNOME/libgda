/*
 * Copyright (C) 2009 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2018 Daniel Espinosa <esodan@gmail.com>
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
#include <glib/gi18n-lib.h>
#include <libgda/libgda.h>
#include "gdaui-data-proxy.h"
#include "gdaui-data-selector.h"
#include "gdaui-raw-grid.h"
#include "gdaui-data-proxy-info.h"
#include "gdaui-data-filter.h"
#include "gdaui-enum-types.h"

static void gdaui_data_proxy_info_dispose (GObject *object);

static void gdaui_data_proxy_info_set_property (GObject *object,
						guint param_id,
						const GValue *value,
						GParamSpec *pspec);
static void gdaui_data_proxy_info_get_property (GObject *object,
						guint param_id,
						GValue *value,
						GParamSpec *pspec);

static void modif_buttons_make (GdauiDataProxyInfo *info);
static void modif_buttons_update (GdauiDataProxyInfo *info);

static void data_proxy_proxy_changed_cb (GdauiDataProxy *data_proxy, GdaDataProxy *proxy, GdauiDataProxyInfo *info);
static void proxy_changed_cb (GdaDataProxy *proxy, GdauiDataProxyInfo *info);
static void proxy_sample_changed_cb (GdaDataProxy *proxy, gint sample_start, gint sample_end, GdauiDataProxyInfo *info);
static void proxy_row_changed_cb (GdaDataProxy *proxy, gint row, GdauiDataProxyInfo *info);
static void proxy_reset_cb (GdaDataProxy *wid, GdauiDataProxyInfo *info);

static void raw_grid_selection_changed_cb (GdauiRawGrid *grid, GdauiDataProxyInfo *info);


typedef enum {
	/* row modification actions */
	ACTION_NEW,
	ACTION_COMMIT,
	ACTION_DELETE,
	ACTION_RESET,

	/* current row move actions */
	ACTION_FIRSTRECORD,
	ACTION_PREVRECORD,
	ACTION_NEXTRECORD,
	ACTION_LASTRECORD,

	/* chunk changes actions */
	ACTION_FIRSTCHUNK,
	ACTION_PREVCHUNK,
	ACTION_NEXTCHUNK,
	ACTION_LASTCHUNK,

	ACTION_FILTER, /* button to display a search GdaUiDataFilter */
	ACTION_CURRENT_ROW, /* indicates current row */

	ACTION_COUNT
} ActionType;

typedef struct {
	GdauiAction  action;
	GdauiAction  toggle_action; /* if != @action */
	const gchar *icon_name;
	const gchar *tip;
} UIAction;

UIAction uiactions[] = {
	{GDAUI_ACTION_NEW_DATA, GDAUI_ACTION_NEW_DATA, "list-add-symbolic", N_("Insert new data")},
	{GDAUI_ACTION_WRITE_MODIFIED_DATA, GDAUI_ACTION_WRITE_MODIFIED_DATA, "document-save-symbolic", N_("Commit the changes")},
	{GDAUI_ACTION_DELETE_SELECTED_DATA, GDAUI_ACTION_UNDELETE_SELECTED_DATA, "edit-delete-symbolic", N_("Delete the selected entry")}, /* undelete: "document-revert" */
	{GDAUI_ACTION_RESET_DATA, GDAUI_ACTION_RESET_DATA, "edit-undo-symbolic", N_("Cancel any modification")},

	{GDAUI_ACTION_MOVE_FIRST_RECORD, GDAUI_ACTION_MOVE_FIRST_RECORD, "go-first-symbolic", N_("Move to first entry")},
	{GDAUI_ACTION_MOVE_PREV_RECORD, GDAUI_ACTION_MOVE_PREV_RECORD, "go-previous-symbolic", N_("Move to previous entry")},
	{GDAUI_ACTION_MOVE_NEXT_RECORD, GDAUI_ACTION_MOVE_NEXT_RECORD, "go-next-symbolic", N_("Move to next entry")},
	{GDAUI_ACTION_MOVE_LAST_RECORD, GDAUI_ACTION_MOVE_LAST_RECORD, "go-last-symbolic", N_("Move to last entry")},

	{GDAUI_ACTION_MOVE_FIRST_CHUNK, GDAUI_ACTION_MOVE_FIRST_CHUNK, "go-first-symbolic", N_("Show first chunk of data")},
	{GDAUI_ACTION_MOVE_PREV_CHUNK, GDAUI_ACTION_MOVE_PREV_CHUNK, "go-previous-symbolic", N_("Show previous chunk of data")},
	{GDAUI_ACTION_MOVE_NEXT_CHUNK, GDAUI_ACTION_MOVE_NEXT_CHUNK, "go-next-symbolic", N_("Show next chunk of data")},
	{GDAUI_ACTION_MOVE_LAST_CHUNK, GDAUI_ACTION_MOVE_LAST_CHUNK, "go-last-symbolic", N_("Show last chunk of data")},
};

typedef struct
{
	GdauiDataProxy *data_proxy;
	GdaDataProxy      *proxy;
	GdaDataModelIter  *iter;
	GdauiDataProxyInfoFlag flags; /* ORed values, only modified when setting property */

	GtkWidget         *current_sample;
	GtkWidget         *row_spin;

	GtkWidget         *action_items [ACTION_COUNT];
	GtkWidget         *filter_popover;

	guint              idle_id;
} GdauiDataProxyInfoPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GdauiDataProxyInfo, gdaui_data_proxy_info, GTK_TYPE_TOOLBAR)

/* properties */
enum {
	PROP_0,
	PROP_DATA_PROXY,
	PROP_FLAGS
};

static void
gdaui_data_proxy_info_class_init (GdauiDataProxyInfoClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = gdaui_data_proxy_info_dispose;

	/* Properties */
        object_class->set_property = gdaui_data_proxy_info_set_property;
        object_class->get_property = gdaui_data_proxy_info_get_property;
	g_object_class_install_property (object_class, PROP_DATA_PROXY,
                                         g_param_spec_object ("data-proxy", NULL, NULL, GDAUI_TYPE_DATA_PROXY,
							      G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (object_class, PROP_FLAGS,
                                         g_param_spec_flags ("flags", NULL, NULL, GDAUI_TYPE_DATA_PROXY_INFO_FLAG,
							     GDAUI_DATA_PROXY_INFO_CURRENT_ROW,
							     G_PARAM_READABLE | G_PARAM_WRITABLE));
}

static void
gdaui_data_proxy_info_init (GdauiDataProxyInfo *info)
{
	GdauiDataProxyInfoPrivate *priv = gdaui_data_proxy_info_get_instance_private (info);
	priv->data_proxy = NULL;
	priv->proxy = NULL;
	priv->row_spin = NULL;
}

/**
 * gdaui_data_proxy_info_new:
 * @data_proxy: a widget implementing the #GdauiDataProxy interface
 * @flags: OR'ed values, specifying what to display in the new widget
 *
 * Creates a new #GdauiDataProxyInfo widget suitable to display information about @data_proxy
 *
 * Returns: (transfer full): the new widget
 *
 * Since: 4.2
 */
GtkWidget *
gdaui_data_proxy_info_new (GdauiDataProxy *data_proxy, GdauiDataProxyInfoFlag flags)
{
	GtkWidget *info;

	g_return_val_if_fail (!data_proxy || GDAUI_IS_DATA_PROXY (data_proxy), NULL);

	info = (GtkWidget *) g_object_new (GDAUI_TYPE_DATA_PROXY_INFO,
					   "toolbar-style", GTK_TOOLBAR_ICONS,
					   "icon-size", GTK_ICON_SIZE_MENU,
					   "data-proxy", data_proxy,
					   "flags", flags, NULL);
	return info;
}

static void
data_proxy_destroyed_cb (GdauiDataProxy *wid, GdauiDataProxyInfo *info)
{
	g_return_if_fail (GDAUI_IS_DATA_PROXY_INFO (info));
	GdauiDataProxyInfoPrivate *priv = gdaui_data_proxy_info_get_instance_private (info);
	g_assert (wid == priv->data_proxy);
	g_signal_handlers_disconnect_by_func (G_OBJECT (wid),
					      G_CALLBACK (data_proxy_destroyed_cb), info);
	g_signal_handlers_disconnect_by_func (G_OBJECT (wid),
					      G_CALLBACK (data_proxy_proxy_changed_cb), info);
	if (GDAUI_IS_RAW_GRID (priv->data_proxy))
		g_signal_handlers_disconnect_by_func (priv->data_proxy,
						      G_CALLBACK (raw_grid_selection_changed_cb), info);

	priv->data_proxy = NULL;
}

static void
release_proxy (GdauiDataProxyInfo *info)
{
	g_return_if_fail (GDAUI_IS_DATA_PROXY_INFO (info));
	GdauiDataProxyInfoPrivate *priv = gdaui_data_proxy_info_get_instance_private (info);
	g_signal_handlers_disconnect_by_func (G_OBJECT (priv->proxy),
					      G_CALLBACK (proxy_changed_cb), info);
	g_signal_handlers_disconnect_by_func (G_OBJECT (priv->proxy),
					      G_CALLBACK (proxy_sample_changed_cb), info);
	g_signal_handlers_disconnect_by_func (G_OBJECT (priv->proxy),
					      G_CALLBACK (proxy_row_changed_cb), info);
	g_signal_handlers_disconnect_by_func (G_OBJECT (priv->proxy),
					      G_CALLBACK (proxy_reset_cb), info);
	g_object_unref (priv->proxy);
	priv->proxy = NULL;
}

static void iter_row_changed_cb (GdaDataModelIter *iter, gint row, GdauiDataProxyInfo *info);
static void
release_iter (GdauiDataProxyInfo *info)
{
	g_return_if_fail (GDAUI_IS_DATA_PROXY_INFO (info));
	GdauiDataProxyInfoPrivate *priv = gdaui_data_proxy_info_get_instance_private (info);
	g_signal_handlers_disconnect_by_func (priv->iter,
					      G_CALLBACK (iter_row_changed_cb), info);
	g_object_unref (priv->iter);
	priv->iter = NULL;
}

static void
data_proxy_proxy_changed_cb (GdauiDataProxy *data_proxy, G_GNUC_UNUSED GdaDataProxy *proxy, GdauiDataProxyInfo *info)
{
	g_object_set (G_OBJECT (info), "data-proxy", data_proxy, NULL);
}

static void
gdaui_data_proxy_info_dispose (GObject *object)
{
	GdauiDataProxyInfo *info;

	info = GDAUI_DATA_PROXY_INFO (object);
	GdauiDataProxyInfoPrivate *priv = gdaui_data_proxy_info_get_instance_private (info);

	if (priv->proxy)
		release_proxy (info);
	if (priv->iter)
		release_iter (info);
	if (priv->data_proxy)
		data_proxy_destroyed_cb (priv->data_proxy, info);
	if (priv->idle_id)
		g_source_remove (priv->idle_id);

	/* for the parent class */
	G_OBJECT_CLASS (gdaui_data_proxy_info_parent_class)->dispose (object);
}

static void
gdaui_data_proxy_info_set_property (GObject *object,
				    guint param_id,
				    const GValue *value,
				    GParamSpec *pspec)
{
	GdauiDataProxyInfo *info;

	info = GDAUI_DATA_PROXY_INFO (object);
	GdauiDataProxyInfoPrivate *priv = gdaui_data_proxy_info_get_instance_private (info);
	switch (param_id) {
	case PROP_DATA_PROXY:
		if (priv->data_proxy)
			data_proxy_destroyed_cb (priv->data_proxy, info);
		if (priv->iter)
			release_iter (info);
		if (priv->proxy)
			release_proxy (info);

		priv->data_proxy = GDAUI_DATA_PROXY (g_value_get_object (value));
		if (priv->data_proxy) {
			GdaDataProxy *proxy;
			GdaDataModelIter *iter;

			/* data widget */
			g_signal_connect (priv->data_proxy, "destroy",
					  G_CALLBACK (data_proxy_destroyed_cb), info);
			g_signal_connect (priv->data_proxy, "proxy-changed",
					  G_CALLBACK (data_proxy_proxy_changed_cb), info);
			if (GDAUI_IS_RAW_GRID (priv->data_proxy))
				g_signal_connect (priv->data_proxy, "selection-changed",
						  G_CALLBACK (raw_grid_selection_changed_cb), info);

			/* proxy */
			proxy = gdaui_data_proxy_get_proxy (priv->data_proxy);
			if (proxy) {
				priv->proxy = proxy;
				g_object_ref (priv->proxy);
				g_signal_connect (G_OBJECT (proxy), "changed",
						  G_CALLBACK (proxy_changed_cb), info);
				g_signal_connect (G_OBJECT (proxy), "sample-changed",
						  G_CALLBACK (proxy_sample_changed_cb), info);
				g_signal_connect (G_OBJECT (proxy), "row-inserted",
						  G_CALLBACK (proxy_row_changed_cb), info);
				g_signal_connect (G_OBJECT (proxy), "row-removed",
						  G_CALLBACK (proxy_row_changed_cb), info);
				g_signal_connect (G_OBJECT (proxy), "reset",
						  G_CALLBACK (proxy_reset_cb), info);


				/* iter */
				iter = gdaui_data_selector_get_data_set (GDAUI_DATA_SELECTOR
										  (priv->data_proxy));
				priv->iter = iter;
				if (iter) {
					g_object_ref (G_OBJECT (iter));
					g_signal_connect (iter, "row-changed",
							  G_CALLBACK (iter_row_changed_cb), info);
				}
			}
			modif_buttons_update (info);
		}
                      break;
              case PROP_FLAGS:
		priv->flags = g_value_get_flags (value);
		modif_buttons_make (info);
		modif_buttons_update (info);
                      break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
gdaui_data_proxy_info_get_property (GObject *object,
				    guint param_id,
				    GValue *value,
				    GParamSpec *pspec)
{
	GdauiDataProxyInfo *info;

	info = GDAUI_DATA_PROXY_INFO (object);
	GdauiDataProxyInfoPrivate *priv = gdaui_data_proxy_info_get_instance_private (info);
	switch (param_id) {
		case PROP_DATA_PROXY:
			g_value_set_object (value, priv->data_proxy);
			break;
		case PROP_FLAGS:
			g_value_set_flags (value, priv->flags);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
	}
}


static void
proxy_changed_cb (G_GNUC_UNUSED GdaDataProxy *proxy, G_GNUC_UNUSED GdauiDataProxyInfo *info)
{
	modif_buttons_update (info);
}

static void
proxy_sample_changed_cb (G_GNUC_UNUSED GdaDataProxy *proxy, G_GNUC_UNUSED gint sample_start,
			 G_GNUC_UNUSED gint sample_end, G_GNUC_UNUSED GdauiDataProxyInfo *info)
{
	modif_buttons_update (info);
}

static void
proxy_row_changed_cb (G_GNUC_UNUSED GdaDataProxy *proxy, G_GNUC_UNUSED gint row, GdauiDataProxyInfo *info)
{
	modif_buttons_update (info);
}

static void
proxy_reset_cb (G_GNUC_UNUSED GdaDataProxy *wid, GdauiDataProxyInfo *info)
{
	modif_buttons_make (info);
	modif_buttons_update (info);
}

static void
iter_row_changed_cb (G_GNUC_UNUSED GdaDataModelIter *iter, G_GNUC_UNUSED gint row, GdauiDataProxyInfo *info)
{
	modif_buttons_update (info);
}

static void
raw_grid_selection_changed_cb (G_GNUC_UNUSED GdauiRawGrid *grid, GdauiDataProxyInfo *info)
{
	modif_buttons_update (info);
}

static void
filter_item_clicked_cb (GtkToolButton *titem, GdauiDataProxyInfo *info)
{
	g_return_if_fail (GDAUI_IS_DATA_PROXY_INFO (info));
	GdauiDataProxyInfoPrivate *priv = gdaui_data_proxy_info_get_instance_private (info);
	if (!priv->filter_popover) {
		/* create filter window */
		priv->filter_popover = gtk_popover_new (GTK_WIDGET (titem));

		/* add real filter widget */
		GtkWidget *filter;
		filter = gdaui_data_filter_new (priv->data_proxy);
		gtk_widget_show (filter);
		gtk_container_add (GTK_CONTAINER (priv->filter_popover), filter);
	}

	gtk_widget_show (priv->filter_popover);
}

static void
action_statefull_clicked_cb (GtkToggleToolButton *toggle_tool_button, GdauiDataProxyInfo *info)
{
	g_return_if_fail (GDAUI_IS_DATA_PROXY_INFO (info));
	GdauiDataProxyInfoPrivate *priv = gdaui_data_proxy_info_get_instance_private (info);
	UIAction *uiaction;;
	uiaction = g_object_get_data ((GObject*) toggle_tool_button, "uia");
	GdauiAction action;
	if (gtk_toggle_tool_button_get_active (toggle_tool_button))
		action = uiaction->action;
	else
		action = uiaction->toggle_action;
	gdaui_data_proxy_perform_action (priv->data_proxy, action);
}

static void
action_stateless_clicked_cb (GtkToolButton *tool_button, GdauiDataProxyInfo *info)
{
	g_return_if_fail (GDAUI_IS_DATA_PROXY_INFO (info));
	GdauiDataProxyInfoPrivate *priv = gdaui_data_proxy_info_get_instance_private (info);
	UIAction *uiaction;;
	uiaction = g_object_get_data ((GObject*) tool_button, "uia");
	gdaui_data_proxy_perform_action (priv->data_proxy, uiaction->action);
}

static void row_spin_changed_cb (GtkSpinButton *spin, GdauiDataProxyInfo *info);
static void
modif_buttons_make (GdauiDataProxyInfo *info)
{
	g_return_if_fail (GDAUI_IS_DATA_PROXY_INFO (info));
	GdauiDataProxyInfoPrivate *priv = gdaui_data_proxy_info_get_instance_private (info);
	GtkWidget *wid;
	GdauiDataProxyInfoFlag flags = priv->flags;
	GtkWidget **action_items = priv->action_items;

	if (! priv->data_proxy)
		return;

	/* row modification actions */
	if ((flags & GDAUI_DATA_PROXY_INFO_ROW_MODIFY_BUTTONS) && !action_items[ACTION_NEW]) {
		ActionType atype;
		for (atype = ACTION_NEW; atype <= ACTION_RESET; atype++) {
			UIAction *uiaction = &(uiactions[atype]);

			if (! gdaui_data_proxy_supports_action (priv->data_proxy, uiaction->action))
				continue;

			gboolean is_toggle;
			is_toggle = (uiaction->action == uiaction->toggle_action) ? FALSE : TRUE;
			if (is_toggle)
				action_items[atype] = (GtkWidget*) gtk_toggle_tool_button_new ();
			else	
				action_items[atype] = (GtkWidget*) gtk_tool_button_new (NULL, NULL);
			gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (action_items[atype]), uiaction->icon_name);
			g_object_set_data ((GObject*) action_items[atype], "uia", uiaction);
			gtk_widget_set_tooltip_text (action_items[atype], uiaction->tip);

			gtk_toolbar_insert (GTK_TOOLBAR (info), GTK_TOOL_ITEM (action_items[atype]), -1);
			gtk_widget_show (action_items[atype]);

			if (is_toggle)
				g_signal_connect (action_items[atype], "toggled",
						  G_CALLBACK (action_statefull_clicked_cb), info);
			else
				g_signal_connect (action_items[atype], "clicked",
						  G_CALLBACK (action_stateless_clicked_cb), info);
		}
	}
	else if (! (flags & GDAUI_DATA_PROXY_INFO_ROW_MODIFY_BUTTONS) && action_items[ACTION_NEW]) {
		ActionType atype;
		for (atype = ACTION_NEW; atype <= ACTION_RESET; atype++) {
			if (action_items[atype]) {
				gtk_widget_destroy (action_items[atype]);
				action_items[atype] = NULL;
			}
		}
	}

	/* current row move actions */
	if ((flags & GDAUI_DATA_PROXY_INFO_ROW_MOVE_BUTTONS) && !action_items[ACTION_FIRSTRECORD]) {
		ActionType atype;
		for (atype = ACTION_FIRSTRECORD; atype <= ACTION_LASTRECORD; atype++) {
			UIAction *uiaction = &(uiactions[atype]);

			if (! gdaui_data_proxy_supports_action (priv->data_proxy, uiaction->action))
				continue;

			gboolean is_toggle;
			is_toggle = (uiaction->action == uiaction->toggle_action) ? FALSE : TRUE;
			if (is_toggle)
				action_items[atype] = (GtkWidget*) gtk_toggle_tool_button_new ();
			else
				action_items[atype] = (GtkWidget*) gtk_tool_button_new (NULL, NULL);
			gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (action_items[atype]), uiaction->icon_name);
			g_object_set_data ((GObject*) action_items[atype], "uia", uiaction);
			gtk_widget_set_tooltip_text (action_items[atype], uiaction->tip);

			gtk_toolbar_insert (GTK_TOOLBAR (info), GTK_TOOL_ITEM (action_items[atype]), -1);
			gtk_widget_show (action_items[atype]);

			if (is_toggle)
				g_signal_connect (action_items[atype], "toggled",
						  G_CALLBACK (action_statefull_clicked_cb), info);
			else
				g_signal_connect (action_items[atype], "clicked",
						  G_CALLBACK (action_stateless_clicked_cb), info);
		}
	}
	else if (! (flags & GDAUI_DATA_PROXY_INFO_ROW_MOVE_BUTTONS) && action_items[ACTION_FIRSTRECORD]) {
		ActionType atype;
		for (atype = ACTION_FIRSTRECORD; atype <= ACTION_LASTRECORD; atype++) {
			if (action_items[atype]) {
				gtk_widget_destroy (action_items[atype]);
				action_items[atype] = NULL;
			}
		}
	}

	/* chunk changes actions */
	if ((flags & GDAUI_DATA_PROXY_INFO_CHUNK_CHANGE_BUTTONS) && !action_items[ACTION_FIRSTCHUNK]) {
		ActionType atype;
		for (atype = ACTION_FIRSTCHUNK; atype <= ACTION_LASTCHUNK; atype++) {
			UIAction *uiaction = &(uiactions[atype]);

			if (! gdaui_data_proxy_supports_action (priv->data_proxy, uiaction->action))
				continue;

			gboolean is_toggle;
			is_toggle = (uiaction->action == uiaction->toggle_action) ? FALSE : TRUE;
			if (is_toggle)
				action_items[atype] = (GtkWidget*) gtk_toggle_tool_button_new ();
			else	
				action_items[atype] = (GtkWidget*) gtk_tool_button_new (NULL, NULL);
			gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (action_items[atype]), uiaction->icon_name);
			g_object_set_data ((GObject*) action_items[atype], "uia", uiaction);
			gtk_widget_set_tooltip_text (action_items[atype], uiaction->tip);

			gtk_toolbar_insert (GTK_TOOLBAR (info), GTK_TOOL_ITEM (action_items[atype]), -1);
			gtk_widget_show (action_items[atype]);

			if (is_toggle)
				g_signal_connect (action_items[atype], "toggled",
						  G_CALLBACK (action_statefull_clicked_cb), info);
			else
				g_signal_connect (action_items[atype], "clicked",
						  G_CALLBACK (action_stateless_clicked_cb), info);
		}
	}
	else if (! (flags & GDAUI_DATA_PROXY_INFO_CHUNK_CHANGE_BUTTONS) && action_items[ACTION_FIRSTCHUNK]) {
		ActionType atype;
		for (atype = ACTION_FIRSTCHUNK; atype <= ACTION_LASTCHUNK; atype++) {
			if (action_items[atype]) {
				gtk_widget_destroy (action_items[atype]);
				action_items[atype] = NULL;
			}
		}
	}

	/* filter button */
	if (! (flags & GDAUI_DATA_PROXY_INFO_NO_FILTER) && ! action_items[ACTION_FILTER]) {
		GtkWidget *icon;
		icon = gtk_image_new_from_icon_name ("edit-find-symbolic", GTK_ICON_SIZE_MENU);
		action_items[ACTION_FILTER] = (GtkWidget*) gtk_tool_button_new (icon, NULL);
		gtk_toolbar_insert (GTK_TOOLBAR (info), GTK_TOOL_ITEM (action_items[ACTION_FILTER]), -1);
		gtk_widget_set_tooltip_text (action_items[ACTION_FILTER], _("Filter data"));
		gtk_widget_show (action_items[ACTION_FILTER]);
		g_signal_connect (action_items[ACTION_FILTER], "clicked",
				  G_CALLBACK (filter_item_clicked_cb), info);
	}
	else if ((flags & GDAUI_DATA_PROXY_INFO_NO_FILTER) && action_items[ACTION_FILTER]) {
		gtk_widget_destroy (action_items[ACTION_FILTER]);
		action_items[ACTION_FILTER] = NULL;
	}
	else if (action_items[ACTION_FILTER]) {
		/* reposition the tool item at the end of the toolbar */
		g_object_ref ((GObject*) action_items[ACTION_FILTER]);
		gtk_container_remove (GTK_CONTAINER (info), action_items[ACTION_FILTER]);
		gtk_toolbar_insert (GTK_TOOLBAR (info), GTK_TOOL_ITEM (action_items[ACTION_FILTER]), -1);
		g_object_unref ((GObject*) action_items[ACTION_FILTER]);
	}

	/* row indication */
	if (action_items[ACTION_CURRENT_ROW]) {
		gtk_widget_destroy (action_items[ACTION_CURRENT_ROW]);
		action_items[ACTION_CURRENT_ROW] = NULL;
	}
	if (flags & GDAUI_DATA_PROXY_INFO_CURRENT_ROW) {
		if (action_items[ACTION_CURRENT_ROW]) {
			/* remove the current contents */
			gtk_widget_destroy (gtk_bin_get_child (GTK_BIN (action_items[ACTION_CURRENT_ROW])));
			priv->row_spin = NULL;
			priv->current_sample = NULL;
			gtk_toolbar_insert (GTK_TOOLBAR (info),
					    GTK_TOOL_ITEM (action_items[ACTION_CURRENT_ROW]), -1);
		}
		else {
			GtkToolItem *ti;
			ti = gtk_tool_item_new  ();
			gtk_container_set_border_width (GTK_CONTAINER (ti), 4);
			gtk_toolbar_insert (GTK_TOOLBAR (info), ti, -1);
			action_items[ACTION_CURRENT_ROW] = GTK_WIDGET (g_object_ref (G_OBJECT (ti)));
		}

		GtkWidget *toolwid;
    GtkStyleContext *context;

		if (flags & GDAUI_DATA_PROXY_INFO_ROW_MOVE_BUTTONS) {
			toolwid = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
			
			/* read-write spin counter (mainly for forms) */
			wid = gtk_spin_button_new_with_range (0, 1, 1);
      context = gtk_widget_get_style_context (wid);
      gtk_style_context_add_class (context, "small-text");

			gtk_spin_button_set_digits (GTK_SPIN_BUTTON (wid), 0);
			gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (wid), TRUE);
			gtk_box_pack_start (GTK_BOX (toolwid), wid, FALSE, TRUE, 2);
			gtk_widget_set_sensitive (wid, FALSE);
			priv->row_spin = wid;
			g_signal_connect (G_OBJECT (wid), "value-changed",
					  G_CALLBACK (row_spin_changed_cb), info);

			/* rows counter */
			wid = gtk_label_new (" /?");
      context = gtk_widget_get_style_context (wid);
      gtk_style_context_add_class (context, "small-text");
			priv->current_sample = wid;
			gtk_box_pack_start (GTK_BOX (toolwid), wid, FALSE, FALSE, 2);
		}
		else {
			/* read-only counter (mainly for grids) */
			wid = gtk_label_new (" ? - ? /?");
      context = gtk_widget_get_style_context (wid);
      gtk_style_context_add_class (context, "small-text");
			priv->current_sample = wid;
			toolwid = wid;
		}

		gtk_container_add (GTK_CONTAINER (action_items[ACTION_CURRENT_ROW]), toolwid);
		gtk_widget_show_all (action_items[ACTION_CURRENT_ROW]);
	}
}

static void
row_spin_changed_cb (GtkSpinButton *spin, GdauiDataProxyInfo *info)
{
	g_return_if_fail (GDAUI_IS_DATA_PROXY_INFO (info));
	GdauiDataProxyInfoPrivate *priv = gdaui_data_proxy_info_get_instance_private (info);
	gint row, nrows;
	gint value = gtk_spin_button_get_value (spin);
	nrows = gda_data_model_get_n_rows (GDA_DATA_MODEL (priv->proxy));

	if ((value >= 1) && (value <= nrows))
		row = value - 1;
	else if (value > nrows)
		row = nrows - 1;
	else
		row = 0;
	gda_data_model_iter_move_to_row (gdaui_data_selector_get_data_set (GDAUI_DATA_SELECTOR (priv->data_proxy)),
					 row);
}

static gboolean idle_modif_buttons_update (GdauiDataProxyInfo *info);
static void
modif_buttons_update (GdauiDataProxyInfo *info)
{
	g_return_if_fail (GDAUI_IS_DATA_PROXY_INFO (info));
	GdauiDataProxyInfoPrivate *priv = gdaui_data_proxy_info_get_instance_private (info);
	if (priv->idle_id == 0)
		priv->idle_id = g_idle_add_full (G_PRIORITY_HIGH_IDLE,
						       (GSourceFunc) idle_modif_buttons_update,
						       g_object_ref (info), g_object_unref);
}


#define BLOCK_SPIN (g_signal_handlers_block_by_func (G_OBJECT (priv->row_spin), \
						     G_CALLBACK (row_spin_changed_cb), info))
#define UNBLOCK_SPIN (g_signal_handlers_unblock_by_func (G_OBJECT (priv->row_spin), \
							 G_CALLBACK (row_spin_changed_cb), info))
static gboolean
idle_modif_buttons_update (GdauiDataProxyInfo *info)
{
	g_return_val_if_fail (GDAUI_IS_DATA_PROXY_INFO (info), FALSE);
	GdauiDataProxyInfoPrivate *priv = gdaui_data_proxy_info_get_instance_private (info);
	GdaDataModelIter *model_iter;
	gboolean wrows, filtered_proxy = FALSE;
	gint row;

	gint proxy_rows = 0;
	gint proxied_rows = 0;
	gint all_rows = 0;

	gint sample_first_row = 0, sample_last_row = 0, sample_size = 0;
	GdauiDataProxyInfoFlag flags = 0;

	GtkWidget **action_items = priv->action_items;

	model_iter = gdaui_data_selector_get_data_set (GDAUI_DATA_SELECTOR (priv->data_proxy));
	if (priv->proxy) {
		filtered_proxy = gda_data_proxy_get_filter_expr (priv->proxy) ? TRUE : FALSE;
		proxy_rows = gda_data_model_get_n_rows (GDA_DATA_MODEL (priv->proxy));
		if (filtered_proxy) {
			proxied_rows = gda_data_proxy_get_filtered_n_rows (priv->proxy);
			all_rows = proxied_rows;
		}
		else {
			proxied_rows = gda_data_proxy_get_proxied_model_n_rows (priv->proxy);
			all_rows = proxied_rows + gda_data_proxy_get_n_new_rows (priv->proxy);
		}

		/* samples don't take into account the proxy's inserted rows */
		sample_first_row = gda_data_proxy_get_sample_start (priv->proxy);
		sample_last_row = gda_data_proxy_get_sample_end (priv->proxy);
		sample_size = gda_data_proxy_get_sample_size (priv->proxy);

		flags = gda_data_model_get_access_flags (GDA_DATA_MODEL (priv->proxy));
	}

	/* sensitiveness of the text indications and of the spin button */
	wrows = (proxy_rows <= 0) ? FALSE : TRUE;
	row = model_iter ? gda_data_model_iter_get_row (model_iter) : 0;
	if (priv->flags & GDAUI_DATA_PROXY_INFO_CURRENT_ROW) {
		if (proxy_rows < 0) {
			if (priv->flags & GDAUI_DATA_PROXY_INFO_ROW_MOVE_BUTTONS) {
				BLOCK_SPIN;
				gtk_spin_button_set_range (GTK_SPIN_BUTTON (priv->row_spin), 0, 1);
				gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->row_spin), 0);
				UNBLOCK_SPIN;
				gtk_label_set_text (GTK_LABEL (priv->current_sample), " /?");
			}
			else
				gtk_label_set_text (GTK_LABEL (priv->current_sample), " ? - ? /?");
		}
		else {
			gchar *str;
			gint total;

			total = sample_first_row + proxy_rows;
			if (priv->flags & GDAUI_DATA_PROXY_INFO_ROW_MOVE_BUTTONS) {
				if (total <= 0)
					str = g_strdup (" / 0");
				else {
					if (filtered_proxy)
						str = g_strdup_printf (" / (%d)", proxy_rows);
					else
						str = g_strdup_printf (" / %d", proxy_rows);
				}
				BLOCK_SPIN;
				gtk_spin_button_set_range (GTK_SPIN_BUTTON (priv->row_spin),
							   proxy_rows > 0 ? 1 : 0,
							   proxy_rows);
				if (row >= 0)
					if (gtk_spin_button_get_value (GTK_SPIN_BUTTON (priv->row_spin)) != row+1)
						gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->row_spin), row+1);
				UNBLOCK_SPIN;
			}
			else {
				if (total <= 0)
					str = g_strdup_printf (" 0 - 0 / 0");
				else {
					if (all_rows < 0)
						str = g_strdup_printf (" %d - %d /?", sample_first_row + 1, total);
					else {
						if (filtered_proxy)
							str = g_strdup_printf (" %d - %d / (%d)",
									       sample_first_row + 1, total, all_rows);
						else
							str = g_strdup_printf (" %d - %d / %d",
									       sample_first_row + 1, total, all_rows);
					}
				}
			}

			gtk_label_set_text (GTK_LABEL (priv->current_sample), str);
			g_free (str);
		}

		gtk_widget_set_sensitive (priv->current_sample, wrows);
		if (priv->row_spin)
			gtk_widget_set_sensitive (priv->row_spin, wrows && (row >= 0));
	}

	/* current row modifications */
	gboolean changed = FALSE;
	gboolean to_be_deleted = FALSE;
	gboolean is_inserted = FALSE;
	gboolean force_del_btn = FALSE;
	gboolean force_undel_btn = FALSE;
	gboolean has_selection;

	has_selection = (row >= 0) ? TRUE : FALSE;
	if (priv->proxy) {
		changed = gda_data_proxy_has_changed (priv->proxy);

		if (has_selection) {
			to_be_deleted = gda_data_proxy_row_is_deleted (priv->proxy, row);
			is_inserted = gda_data_proxy_row_is_inserted (priv->proxy, row);
		}
		else if (GDAUI_IS_RAW_GRID (priv->data_proxy)) {
			/* bad for encapsulation, but very useful... */
			GList *sel, *list;

			sel = _gdaui_raw_grid_get_selection ((GdauiRawGrid*) priv->data_proxy);
			if (sel) {
				list = sel;
				while (list && (!force_del_btn || !force_undel_btn)) {
					if ((GPOINTER_TO_INT (list->data) != -1) &&
					    gda_data_proxy_row_is_deleted (priv->proxy,
									   GPOINTER_TO_INT (list->data)))
						force_undel_btn = TRUE;
					else
						force_del_btn = TRUE;
					list = g_list_next (list);
				}
				list = sel;

				is_inserted = TRUE;
				while (list && is_inserted) {
					if (GPOINTER_TO_INT (list->data) != -1)
						is_inserted = FALSE;
					list = g_list_next (list);
				}
				g_list_free (sel);
			}
		}
	}

	if (priv->flags & GDAUI_DATA_PROXY_INFO_ROW_MODIFY_BUTTONS) {
		GdauiDataProxyWriteMode mode;
		mode = gdaui_data_proxy_get_write_mode (priv->data_proxy);

		if (priv->action_items [ACTION_COMMIT]) {
			gtk_widget_set_sensitive (priv->action_items [ACTION_COMMIT], changed ? TRUE : FALSE);
			if (mode == GDAUI_DATA_PROXY_WRITE_ON_VALUE_CHANGE)
				gtk_widget_hide (action_items [ACTION_COMMIT]);
			else if (! gtk_widget_get_no_show_all (action_items [ACTION_COMMIT]))
				gtk_widget_show (action_items [ACTION_COMMIT]);
		}

		if (priv->action_items [ACTION_RESET]) {
			gtk_widget_set_sensitive (priv->action_items [ACTION_RESET], changed ? TRUE : FALSE);
			if (mode == GDAUI_DATA_PROXY_WRITE_ON_VALUE_CHANGE)
				gtk_widget_hide (action_items [ACTION_RESET]);
			else if (! gtk_widget_get_no_show_all (action_items [ACTION_RESET]))
				gtk_widget_show (action_items [ACTION_RESET]);
		}

		if (priv->action_items [ACTION_NEW]) {
			gtk_widget_set_sensitive (priv->action_items [ACTION_NEW],
						  flags & GDA_DATA_MODEL_ACCESS_INSERT ? TRUE : FALSE);
		}

		if (priv->action_items [ACTION_DELETE]) {
			g_signal_handlers_block_by_func (action_items[ACTION_DELETE],
							 G_CALLBACK (action_statefull_clicked_cb), info);
			gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (action_items[ACTION_DELETE]),
							   to_be_deleted);
			g_signal_handlers_unblock_by_func (action_items[ACTION_DELETE],
							   G_CALLBACK (action_statefull_clicked_cb), info);

			if (to_be_deleted) {
				wrows = (flags & GDA_DATA_MODEL_ACCESS_DELETE) &&
					(force_undel_btn || has_selection);
				gtk_widget_set_tooltip_text (priv->action_items [ACTION_DELETE],
							     _("Undelete the selected entry"));
			}
			else {
				wrows = is_inserted ||
					((flags & GDA_DATA_MODEL_ACCESS_DELETE) &&
					 (force_del_btn || has_selection));
				gtk_widget_set_tooltip_text (priv->action_items [ACTION_DELETE],
							     _("Delete the selected entry"));
			}
			gtk_widget_set_sensitive (priv->action_items [ACTION_DELETE], wrows);
		}
	}

	/* current row moving */
	if (priv->flags & GDAUI_DATA_PROXY_INFO_ROW_MOVE_BUTTONS) {
		if (priv->action_items [ACTION_FIRSTRECORD])
			gtk_widget_set_sensitive (priv->action_items [ACTION_FIRSTRECORD], (row <= 0) ? FALSE : TRUE);
		if (priv->action_items [ACTION_PREVRECORD])
			gtk_widget_set_sensitive (priv->action_items [ACTION_PREVRECORD], (row <= 0) ? FALSE : TRUE);
		if (priv->action_items [ACTION_NEXTRECORD])
			gtk_widget_set_sensitive (priv->action_items [ACTION_NEXTRECORD],
						  (row == proxy_rows -1) || (row < 0) ? FALSE : TRUE);
		if (priv->action_items [ACTION_LASTRECORD])
			gtk_widget_set_sensitive (priv->action_items [ACTION_LASTRECORD],
						  (row == proxy_rows -1) || (row < 0) ? FALSE : TRUE);
	}

	/* chunk indications */
	if (priv->flags & GDAUI_DATA_PROXY_INFO_CHUNK_CHANGE_BUTTONS) {
		gboolean abool;
		wrows = (sample_size > 0) ? TRUE : FALSE;
		if (priv->action_items [ACTION_FIRSTCHUNK])
			gtk_widget_set_sensitive (priv->action_items [ACTION_FIRSTCHUNK],
						  wrows && (sample_first_row > 0) ? TRUE : FALSE);
		if (priv->action_items [ACTION_PREVCHUNK])
			gtk_widget_set_sensitive (priv->action_items [ACTION_PREVCHUNK],
						  wrows && (sample_first_row > 0) ? TRUE : FALSE);

		abool = (proxied_rows != -1) ? (wrows && (sample_last_row < proxied_rows - 1)) : TRUE;
		if (priv->action_items [ACTION_NEXTCHUNK])
			gtk_widget_set_sensitive (priv->action_items [ACTION_NEXTCHUNK], abool);
		if (priv->action_items [ACTION_LASTCHUNK])
			gtk_widget_set_sensitive (priv->action_items [ACTION_LASTCHUNK],
						  wrows && (sample_last_row < proxied_rows - 1));
	}

	/* remove IDLE */
	priv->idle_id = 0;
	return FALSE;
}

/**
 * gdaui_data_proxy_info_get_item:
 * @info: a #GdauiDataProxyInfo object
 * @action: a #GdauiAction action
 *
 * Get the #GtkToolItem corresponding to the @action action
 *
 * Returns: (transfer none): the #GtkToolItem, or %NULL on error
 *
 * Since: 6.0
 */
GtkToolItem *
gdaui_data_proxy_info_get_item (GdauiDataProxyInfo *info, GdauiAction action)
{
	g_return_val_if_fail (GDAUI_IS_DATA_PROXY_INFO (info), NULL);
	g_return_val_if_fail ((action >= GDAUI_ACTION_NEW_DATA) && (action <= GDAUI_ACTION_MOVE_LAST_CHUNK), NULL);
	GdauiDataProxyInfoPrivate *priv = gdaui_data_proxy_info_get_instance_private (info);

	ActionType type;
	for (type = ACTION_NEW; type < ACTION_COUNT; type++) {
		UIAction *act;
		act = &(uiactions[type]);
		if (act->action == action)
			return GTK_TOOL_ITEM (priv->action_items [type]);
	}
	return NULL;
}
