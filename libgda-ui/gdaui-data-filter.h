/*
 * Copyright (C) 2009 - 2011 Vivien Malerba <malerba@gnome-db.org>
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
#include <libgda/gda-decl.h>
#include <libgda-ui/gdaui-decl.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_DATA_FILTER          (gdaui_data_filter_get_type())
#define GDAUI_DATA_FILTER(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gdaui_data_filter_get_type(), GdauiDataFilter)
#define GDAUI_DATA_FILTER_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gdaui_data_filter_get_type (), GdauiDataFilterClass)
#define GDAUI_IS_DATA_FILTER(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gdaui_data_filter_get_type ())


typedef struct _GdauiDataFilter      GdauiDataFilter;
typedef struct _GdauiDataFilterClass GdauiDataFilterClass;
typedef struct _GdauiDataFilterPriv  GdauiDataFilterPriv;

/* struct for the object's data */
struct _GdauiDataFilter
{
	GtkBox                      object;

	GdauiDataFilterPriv *priv;
};

/* struct for the object's class */
struct _GdauiDataFilterClass
{
	GtkBoxClass                 parent_class;
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

GType             gdaui_data_filter_get_type                  (void) G_GNUC_CONST;

GtkWidget        *gdaui_data_filter_new                       (GdauiDataProxy *data_widget);

G_END_DECLS

#endif



