/* gdaui-data-widget-filter.h
 *
 * Copyright (C) 2007 Vivien Malerba
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

#ifndef __GDAUI_DATA_WIDGET_FILTER__
#define __GDAUI_DATA_WIDGET_FILTER__

#include <gtk/gtk.h>
#include <libgda/gda-decl.h>
#include <libgda-ui/gdaui-decl.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_DATA_WIDGET_FILTER          (gdaui_data_widget_filter_get_type())
#define GDAUI_DATA_WIDGET_FILTER(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gdaui_data_widget_filter_get_type(), GdauiDataWidgetFilter)
#define GDAUI_DATA_WIDGET_FILTER_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gdaui_data_widget_filter_get_type (), GdauiDataWidgetFilterClass)
#define GDAUI_IS_DATA_WIDGET_FILTER(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gdaui_data_widget_filter_get_type ())


typedef struct _GdauiDataWidgetFilter      GdauiDataWidgetFilter;
typedef struct _GdauiDataWidgetFilterClass GdauiDataWidgetFilterClass;
typedef struct _GdauiDataWidgetFilterPriv  GdauiDataWidgetFilterPriv;

/* struct for the object's data */
struct _GdauiDataWidgetFilter
{
	GtkVBox                      object;

	GdauiDataWidgetFilterPriv *priv;
};

/* struct for the object's class */
struct _GdauiDataWidgetFilterClass
{
	GtkVBoxClass                 parent_class;
};

/* 
 * Generic widget's methods 
 */
GType             gdaui_data_widget_filter_get_type                  (void) G_GNUC_CONST;

GtkWidget        *gdaui_data_widget_filter_new                       (GdauiDataWidget *data_widget);

G_END_DECLS

#endif



