/* gdaui-data-widget.h
 *
 * Copyright (C) 2004 - 2009 Vivien Malerba <malerba@gnome-db.org>
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


#ifndef __GDAUI_DATA_WIDGET_H_
#define __GDAUI_DATA_WIDGET_H_

#include <glib-object.h>
#include <gtk/gtk.h>
#include <libgda/gda-decl.h>
#include "gdaui-decl.h"
#include "gdaui-enums.h"

G_BEGIN_DECLS

#define GDAUI_TYPE_DATA_WIDGET          (gdaui_data_widget_get_type())
#define GDAUI_DATA_WIDGET(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, GDAUI_TYPE_DATA_WIDGET, GdauiDataWidget)
#define GDAUI_IS_DATA_WIDGET(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, GDAUI_TYPE_DATA_WIDGET)
#define GDAUI_DATA_WIDGET_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), GDAUI_TYPE_DATA_WIDGET, GdauiDataWidgetIface))

typedef enum {
	GDAUI_DATA_WIDGET_WRITE_ON_DEMAND           = 0, /* write only when explicitely requested */
	GDAUI_DATA_WIDGET_WRITE_ON_ROW_CHANGE       = 1, /* write when the current selected row changes */
	GDAUI_DATA_WIDGET_WRITE_ON_VALUE_ACTIVATED  = 2, /* write when user activates a value change */
	GDAUI_DATA_WIDGET_WRITE_ON_VALUE_CHANGE     = 3  /* write when a parameters's value changes */
} GdauiDataWidgetWriteMode;

/* struct for the interface */
struct _GdauiDataWidgetIface
{
	GTypeInterface           g_iface;

	/* virtual table */
	GdaDataProxy        *(* get_proxy)           (GdauiDataWidget *iface);
	void                 (* col_set_show)        (GdauiDataWidget *iface, gint column, gboolean shown);
	void                 (* set_column_editable) (GdauiDataWidget *iface, gint column, gboolean editable);
	void                 (* show_column_actions) (GdauiDataWidget *iface, gint column, gboolean show_actions);
	GtkActionGroup      *(* get_actions_group)   (GdauiDataWidget *iface);
	GdaDataModelIter    *(* get_data_set)        (GdauiDataWidget *iface);

	GdaDataModel        *(* get_gda_model)       (GdauiDataWidget *iface);
	void                 (* set_gda_model)       (GdauiDataWidget *iface, GdaDataModel *model);
	gboolean             (* set_write_mode)      (GdauiDataWidget *iface, GdauiDataWidgetWriteMode mode);
	GdauiDataWidgetWriteMode (* get_write_mode)(GdauiDataWidget *iface);
	void                 (* set_data_layout)     (GdauiDataWidget *iface, const gpointer data);

	/* signals */
	void                 (* proxy_changed)       (GdauiDataWidget *iface, GdaDataProxy *proxy);
	void                 (* iter_changed)        (GdauiDataWidget *iface, GdaDataModelIter *iter);
};

GType             gdaui_data_widget_get_type                  (void) G_GNUC_CONST;

GdaDataProxy     *gdaui_data_widget_get_proxy                 (GdauiDataWidget *iface);
GtkActionGroup   *gdaui_data_widget_get_actions_group         (GdauiDataWidget *iface);
void              gdaui_data_widget_perform_action            (GdauiDataWidget *iface, GdauiAction action);

void              gdaui_data_widget_column_show               (GdauiDataWidget *iface, gint column);
void              gdaui_data_widget_column_hide               (GdauiDataWidget *iface, gint column);
void              gdaui_data_widget_column_set_editable       (GdauiDataWidget *iface, gint column, gboolean editable);
void              gdaui_data_widget_column_show_actions       (GdauiDataWidget *iface, gint column, gboolean show_actions);

GdaDataModel     *gdaui_data_widget_get_gda_model             (GdauiDataWidget *iface);
void              gdaui_data_widget_set_gda_model             (GdauiDataWidget *iface, 
								  GdaDataModel *model);
GdaDataModelIter *gdaui_data_widget_get_current_data          (GdauiDataWidget *iface);

gboolean          gdaui_data_widget_set_write_mode            (GdauiDataWidget *iface, GdauiDataWidgetWriteMode mode);
GdauiDataWidgetWriteMode gdaui_data_widget_get_write_mode   (GdauiDataWidget *iface);

void              gdaui_data_widget_set_data_layout_from_file (GdauiDataWidget *iface, const gchar *file_name,
							       const gchar  *parent_table);

G_END_DECLS

#endif
