/*
 * Copyright (C) 2009 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
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

#ifndef __GDAUI_DATA_PROXY_INFO__
#define __GDAUI_DATA_PROXY_INFO__

#include <gtk/gtk.h>
#include <libgda/gda-decl.h>
#include <libgda-ui/gdaui-data-proxy.h>
#include <libgda-ui/gdaui-enums.h>

G_BEGIN_DECLS

/**
 * GdauiDataProxyInfoFlag:
 * @GDAUI_DATA_PROXY_INFO_NONE: 
 * @GDAUI_DATA_PROXY_INFO_CURRENT_ROW: 
 * @GDAUI_DATA_PROXY_INFO_ROW_MODIFY_BUTTONS: 
 * @GDAUI_DATA_PROXY_INFO_ROW_MOVE_BUTTONS: 
 * @GDAUI_DATA_PROXY_INFO_CHUNK_CHANGE_BUTTONS: 
 * @GDAUI_DATA_PROXY_INFO_NO_FILTER: 
 */
typedef enum 
{
	GDAUI_DATA_PROXY_INFO_NONE = 0,
	GDAUI_DATA_PROXY_INFO_CURRENT_ROW    = 1 << 0,
	GDAUI_DATA_PROXY_INFO_ROW_MODIFY_BUTTONS = 1 << 2,
	GDAUI_DATA_PROXY_INFO_ROW_MOVE_BUTTONS = 1 << 3,
	GDAUI_DATA_PROXY_INFO_CHUNK_CHANGE_BUTTONS = 1 << 4,
	GDAUI_DATA_PROXY_INFO_NO_FILTER = 1 << 5
} GdauiDataProxyInfoFlag;

#define GDAUI_TYPE_DATA_PROXY_INFO          (gdaui_data_proxy_info_get_type())
G_DECLARE_DERIVABLE_TYPE (GdauiDataProxyInfo, gdaui_data_proxy_info, GDAUI, DATA_PROXY_INFO, GtkToolbar)


/* struct for the object's class */
struct _GdauiDataProxyInfoClass
{
	GtkToolbarClass     parent_class;
	gpointer            padding[12];
};

/**
 * SECTION:gdaui-data-proxy-info
 * @short_description: Shows information &amp; actions about a #GdauiDataProxy widget
 * @title: GdauiDataProxyInfo
 * @stability: Stable
 * @Image: vi-info.png
 * @see_also:
 *
 * The #GdauiDataProxyInfo widget is a container widget which, depending on how it is configured:
 * <itemizedlist>
 *   <listitem><para>proposes action buttons to change the currently displayed row, add new row, ...</para></listitem>
 *   <listitem><para>displays information about the number of rows in a #GdauiDataProxy</para></listitem>
 * </itemizedlist>
 */

GtkWidget        *gdaui_data_proxy_info_new      (GdauiDataProxy *data_proxy, GdauiDataProxyInfoFlag flags);
GtkToolItem      *gdaui_data_proxy_info_get_item (GdauiDataProxyInfo *info, GdauiAction action);
G_END_DECLS

#endif



