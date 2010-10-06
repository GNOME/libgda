/* gdaui-data-widget.c
 *
 * Copyright (C) 2004 - 2010 Vivien Malerba <malerba@gnome-db.org>
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <string.h>
#include <libgda/libgda.h>
#include <libgda/binreloc/gda-binreloc.h>
#include "gdaui-data-proxy.h"
#include  <glib/gi18n-lib.h>
#include <libgda-ui/gdaui-raw-form.h>
#include <libgda-ui/gdaui-raw-grid.h>

/* signals */
enum {
        PROXY_CHANGED,
        LAST_SIGNAL
};

static gint gdaui_data_proxy_signals[LAST_SIGNAL] = { 0 };

static void gdaui_data_proxy_iface_init (gpointer g_class);

GType
gdaui_data_proxy_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdauiDataProxyIface),
			(GBaseInitFunc) gdaui_data_proxy_iface_init,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) NULL,
			NULL,
			NULL,
			0,
			0,
			(GInstanceInitFunc) NULL
		};
		
		type = g_type_register_static (G_TYPE_INTERFACE, "GdauiDataProxy", &info, 0);
		g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
	}
	return type;
}


static void
gdaui_data_proxy_iface_init (gpointer g_class)
{
	static gboolean initialized = FALSE;

	if (! initialized) {
		gdaui_data_proxy_signals[PROXY_CHANGED] = 
			g_signal_new ("proxy-changed",
                                      GDAUI_TYPE_DATA_PROXY,
                                      G_SIGNAL_RUN_FIRST,
                                      G_STRUCT_OFFSET (GdauiDataProxyIface, proxy_changed),
                                      NULL, NULL,
                                      g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE,
                                      1, GDA_TYPE_DATA_PROXY);
		initialized = TRUE;
	}
}

/**
 * gdaui_data_proxy_get_proxy:
 * @iface: an object which implements the #GdauiDataProxy interface
 *
 * Get a pointer to the #GdaDataProxy being used by @iface
 *
 * Returns: (transfer none): a #GdaDataProxy pointer
 *
 * Since: 4.2
 */
GdaDataProxy *
gdaui_data_proxy_get_proxy (GdauiDataProxy *iface)
{
	g_return_val_if_fail (GDAUI_IS_DATA_PROXY (iface), NULL);
	
	if (GDAUI_DATA_PROXY_GET_IFACE (iface)->get_proxy)
		return (GDAUI_DATA_PROXY_GET_IFACE (iface)->get_proxy) (iface);
	else
		return NULL;
}

/**
 * gdaui_data_proxy_column_set_editable:
 * @iface: an object which implements the #GdauiDataProxy interface
 * @column: column number of the data
 * @editable: set to %TRUE to make the column editable
 *
 * Sets if the data entry in the @iface widget at @column (in the data model @iface operates on)
 * can be edited or not.
 *
 * Since: 4.2
 */
void 
gdaui_data_proxy_column_set_editable (GdauiDataProxy *iface, gint column, gboolean editable)
{
	g_return_if_fail (GDAUI_IS_DATA_PROXY (iface));

	if (GDAUI_DATA_PROXY_GET_IFACE (iface)->set_column_editable)
		(GDAUI_DATA_PROXY_GET_IFACE (iface)->set_column_editable) (iface, column, editable);	
}

/**
 * gdaui_data_proxy_column_show_actions:
 * @iface: an object which implements the #GdauiDataProxy interface
 * @column: column number of the data, or -1 to apply the setting to all the columns
 * @show_actions: set to %TRUE if the actions menu must be shown
 * 
 * Sets if the data entry in the @iface widget at @column (in the data model @iface operates on) must show its
 * actions menu or not.
 *
 * Since: 4.2
 */
void
gdaui_data_proxy_column_show_actions (GdauiDataProxy *iface, gint column, gboolean show_actions)
{
	g_return_if_fail (GDAUI_IS_DATA_PROXY (iface));

	if (GDAUI_DATA_PROXY_GET_IFACE (iface)->show_column_actions)
		(GDAUI_DATA_PROXY_GET_IFACE (iface)->show_column_actions) (iface, column, show_actions);
}

/**
 * gdaui_data_proxy_get_actions_group:
 * @iface: an object which implements the #GdauiDataProxy interface
 *
 * Each widget imlplementing the #GdauiDataProxy interface provides actions. Actions can be triggered
 * using the gdaui_data_proxy_perform_action() method, but using this method allows for the creation of
 * toolbars, menus, etc calling these actions.
 *
 * The actions are among: 
 * <itemizedlist><listitem><para>Data edition actions: "ActionNew", "ActionCommit", 
 *    "ActionDelete, "ActionUndelete, "ActionReset", </para></listitem>
 * <listitem><para>Record by record moving: "ActionFirstRecord", "ActionPrevRecord", 
 *    "ActionNextRecord", "ActionLastRecord",</para></listitem>
 * <listitem><para>Chuncks of records moving: "ActionFirstChunck", "ActionPrevChunck", 
 *     "ActionNextChunck", "ActionLastChunck".</para></listitem>
 * <listitem><para>Filtering: "ActionFilter"</para></listitem></itemizedlist>
 * 
 * Returns: (transfer none): the #GtkActionGroup with all the possible actions on the widget.
 *
 * Since: 4.2
 */
GtkActionGroup *
gdaui_data_proxy_get_actions_group (GdauiDataProxy *iface)
{
	g_return_val_if_fail (GDAUI_IS_DATA_PROXY (iface), NULL);

	if (GDAUI_DATA_PROXY_GET_IFACE (iface)->get_actions_group)
		return (GDAUI_DATA_PROXY_GET_IFACE (iface)->get_actions_group) (iface);
	return NULL;
}

/**
 * gdaui_data_proxy_perform_action:
 * @iface: an object which implements the #GdauiDataProxy interface
 * @action: a #GdauiAction action
 *
 * Forces the widget to perform the selected @action, as if the user
 * had pressed on the corresponding action button in the @iface widget,
 * if the corresponding action is possible and if the @iface widget
 * supports the action.
 *
 * Since: 4.2
 */
void
gdaui_data_proxy_perform_action (GdauiDataProxy *iface, GdauiAction action)
{
	gchar *action_name = NULL;
	GtkActionGroup *group;
	GtkAction *gtkaction;

	g_return_if_fail (GDAUI_IS_DATA_PROXY (iface));
	group = gdaui_data_proxy_get_actions_group (iface);
	if (!group) {
		g_warning ("Object class %s does not support the gdaui_data_proxy_get_actions_group() method",
			   G_OBJECT_TYPE_NAME (iface));
		return;
	}
	
	switch (action) {
	case GDAUI_ACTION_NEW_DATA:
		action_name = "ActionNew";
		break;
	case GDAUI_ACTION_WRITE_MODIFIED_DATA:
		action_name = "ActionCommit";
		break;
        case GDAUI_ACTION_DELETE_SELECTED_DATA:
		action_name = "ActionDelete";
		break;
        case GDAUI_ACTION_UNDELETE_SELECTED_DATA:
		action_name = "ActionUndelete";
		break;
        case GDAUI_ACTION_RESET_DATA:
		action_name = "ActionReset";
		break;
        case GDAUI_ACTION_MOVE_FIRST_RECORD:
		action_name = "ActionFirstRecord";
		break;
        case GDAUI_ACTION_MOVE_PREV_RECORD:
		action_name = "ActionPrevRecord";
		break;
        case GDAUI_ACTION_MOVE_NEXT_RECORD:
		action_name = "ActionNextRecord";
		break;
        case GDAUI_ACTION_MOVE_LAST_RECORD:
		action_name = "ActionLastRecord";
		break;
        case GDAUI_ACTION_MOVE_FIRST_CHUNCK:
		action_name = "ActionFirstChunck";
		break;
        case GDAUI_ACTION_MOVE_PREV_CHUNCK:
		action_name = "ActionPrevChunck";
		break;
        case GDAUI_ACTION_MOVE_NEXT_CHUNCK:
		action_name = "ActionNextChunck";
		break;
        case GDAUI_ACTION_MOVE_LAST_CHUNCK:
		action_name = "ActionLastChunck";
		break;
	default:
		g_assert_not_reached ();
	}

	gtkaction = gtk_action_group_get_action (group, action_name);
	if (gtkaction)
		gtk_action_activate (gtkaction);
}

/**
 * gdaui_data_proxy_set_write_mode:
 * @iface: an object which implements the #GdauiDataProxy interface
 * @mode: the requested #GdauiDataProxyWriteMode mode
 *
 * Specifies the way the modifications stored in the #GdaDataProxy used internally by @iface are written back to
 * the #GdaDataModel which holds the data displayed in @iface.
 *
 * Returns: TRUE if the proposed mode has been taken into account
 *
 * Since: 4.2
 */
gboolean
gdaui_data_proxy_set_write_mode (GdauiDataProxy *iface, GdauiDataProxyWriteMode mode)
{
	g_return_val_if_fail (GDAUI_IS_DATA_PROXY (iface), FALSE);
	
	if (GDAUI_DATA_PROXY_GET_IFACE (iface)->set_write_mode)
		return (GDAUI_DATA_PROXY_GET_IFACE (iface)->set_write_mode) (iface, mode);
	else 
		return FALSE;
}

/**
 * gdaui_data_proxy_get_write_mode:
 * @iface: an object which implements the #GdauiDataProxy interface
 *
 * Get the way the modifications stored in the #GdaDataProxy used internally by @iface are written back to
 * the #GdaDataModel which holds the data displayed in @iface.
 *
 * Returns: the write mode used by @iface
 *
 * Since: 4.2
 */
GdauiDataProxyWriteMode
gdaui_data_proxy_get_write_mode (GdauiDataProxy *iface)
{
	g_return_val_if_fail (GDAUI_IS_DATA_PROXY (iface), GDAUI_DATA_PROXY_WRITE_ON_DEMAND);
	
	if (GDAUI_DATA_PROXY_GET_IFACE (iface)->get_write_mode)
		return (GDAUI_DATA_PROXY_GET_IFACE (iface)->get_write_mode) (iface);
	else 
		return GDAUI_DATA_PROXY_WRITE_ON_DEMAND;
}
