/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDAUI_DATA_FILTER__
#define __GDAUI_DATA_FILTER__

#include <gtk/gtk.h>
#include <libgda-ui/gdaui-data-proxy.h>
#include <libgda-ui/gdaui-data-filter.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_DATA_FILTER          (gdaui_data_filter_get_type())
G_DECLARE_DERIVABLE_TYPE(GdauiDataFilter, gdaui_data_filter, GDAUI, DATA_FILTER, GtkBox)
/* struct for the object's class */
struct _GdauiDataFilterClass
{
	GtkBoxClass    parent_class;
	gpointer       padding[12];
};

/**
 * SECTION:gdaui-data-filter
 * @short_description: Entrer rules to filter the rows in a #GdauiDataProxy
 * @title: GdauiDataFilter
 * @stability: Stable
 * @Image: vi-filter.png
 * @see_also:
 *
 * The #GdauiDataFilter widget can be used as a standalone widget, but is also
 * used internally by the #GdauiDataProxyInfo widget for its search option.
 */


GtkWidget        *gdaui_data_filter_new                       (GdauiDataProxy *data_widget);

G_END_DECLS

#endif



