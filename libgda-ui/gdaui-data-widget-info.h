/* gdaui-data-widget-info.h
 *
 * Copyright (C) 2006 Vivien Malerba
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

#ifndef __GDAUI_DATA_WIDGET_INFO__
#define __GDAUI_DATA_WIDGET_INFO__

#include <gtk/gtk.h>
#include <libgda/gda-decl.h>
#include <libgda-ui/gdaui-decl.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_DATA_WIDGET_INFO          (gdaui_data_widget_info_get_type())
#define GDAUI_DATA_WIDGET_INFO(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gdaui_data_widget_info_get_type(), GdauiDataWidgetInfo)
#define GDAUI_DATA_WIDGET_INFO_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gdaui_data_widget_info_get_type (), GdauiDataWidgetInfoClass)
#define GDAUI_IS_DATA_WIDGET_INFO(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gdaui_data_widget_info_get_type ())


typedef struct _GdauiDataWidgetInfo      GdauiDataWidgetInfo;
typedef struct _GdauiDataWidgetInfoClass GdauiDataWidgetInfoClass;
typedef struct _GdauiDataWidgetInfoPriv  GdauiDataWidgetInfoPriv;

typedef enum 
{
	GDAUI_DATA_WIDGET_INFO_NONE = 0,
	GDAUI_DATA_WIDGET_INFO_CURRENT_ROW    = 1 << 0,
	GDAUI_DATA_WIDGET_INFO_ROW_MODIFY_BUTTONS = 1 << 2,
	GDAUI_DATA_WIDGET_INFO_ROW_MOVE_BUTTONS = 1 << 3,
	GDAUI_DATA_WIDGET_INFO_CHUNCK_CHANGE_BUTTONS = 1 << 4,
	GDAUI_DATA_WIDGET_INFO_NO_FILTER = 1 << 5
} GdauiDataWidgetInfoFlag;

/* struct for the object's data */
struct _GdauiDataWidgetInfo
{
	GtkHBox                    object;

	GdauiDataWidgetInfoPriv *priv;
};

/* struct for the object's class */
struct _GdauiDataWidgetInfoClass
{
	GtkHBoxClass               parent_class;
};

/* 
 * Generic widget's methods 
 */
GType             gdaui_data_widget_info_get_type                  (void) G_GNUC_CONST;

GtkWidget        *gdaui_data_widget_info_new                       (GdauiDataWidget *data_widget, GdauiDataWidgetInfoFlag flags);

G_END_DECLS

#endif



