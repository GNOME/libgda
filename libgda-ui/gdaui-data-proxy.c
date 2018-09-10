/*
 * Copyright (C) 2009 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
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
#include <libgda/libgda.h>
#include <libgda/binreloc/gda-binreloc.h>
#include "gdaui-data-proxy.h"
#include  <glib/gi18n-lib.h>
#include <libgda-ui/gdaui-raw-form.h>
#include <libgda-ui/gdaui-raw-grid.h>
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

/* signals */
enum {
        PROXY_CHANGED,
        LAST_SIGNAL
};

static gint gdaui_data_proxy_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_INTERFACE(GdauiDataProxy, gdaui_data_proxy, GTK_TYPE_WIDGET)

static void
gdaui_data_proxy_default_init (GdauiDataProxyInterface *iface)
{
	static gboolean initialized = FALSE;

	if (! initialized) {
		/**
		 * GdauiDataProxy::proxy-changed:
		 * @gdauidataproxy: the #GdauiDataProxy
		 * @arg1: the GdaDataProxy which would be returned by gdaui_data_proxy_get_proxy()
		 *
		 * The ::proxy-changed signal is emitted each time the #GdaDataProxy which would be
		 * returned by gdaui_data_proxy_get_proxy() changes. This is generally the result
		 * of changes in the structure of the proxied data model (different number and/or type
		 * of columns for example).
		 */
		gdaui_data_proxy_signals[PROXY_CHANGED] = 
			g_signal_new ("proxy-changed",
                                      GDAUI_TYPE_DATA_PROXY,
                                      G_SIGNAL_RUN_FIRST,
                                      G_STRUCT_OFFSET (GdauiDataProxyInterface, proxy_changed),
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
 * gdaui_data_proxy_supports_action:
 * @iface: an object which implements the #GdauiDataProxy interface
 * @action: a #GdauiAction action
 *
 * Determines if @action can be used on @iface (using gdaui_data_proxy_perform_action()).
 *
 * Returns: %TRUE if the requested action is supported, %FALSE otherwise
 *
 * Since: 6.0
 */
gboolean
gdaui_data_proxy_supports_action (GdauiDataProxy *iface, GdauiAction action)
{
	g_return_val_if_fail (GDAUI_IS_DATA_PROXY (iface), FALSE);
	g_return_val_if_fail ((action >= GDAUI_ACTION_NEW_DATA) && (action <= GDAUI_ACTION_MOVE_LAST_CHUNK), FALSE);

	if (GDAUI_DATA_PROXY_GET_IFACE (iface)->supports_action)
		return (GDAUI_DATA_PROXY_GET_IFACE (iface)->supports_action) (iface, action);
	else
		return FALSE;
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
	g_return_if_fail (GDAUI_IS_DATA_PROXY (iface));
	g_return_if_fail ((action >= GDAUI_ACTION_NEW_DATA) && (action <= GDAUI_ACTION_MOVE_LAST_CHUNK));

	if (gdaui_data_proxy_supports_action (iface, action)) {
		if (GDAUI_DATA_PROXY_GET_IFACE (iface)->perform_action)
			(GDAUI_DATA_PROXY_GET_IFACE (iface)->perform_action) (iface, action);
	}
	else
		g_warning (_("GdauiAction is not supported by this GdauiDataProxy interface"));
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
