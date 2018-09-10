/*
 * Copyright (C) 2009 - 2011 Vivien Malerba <malerba@gnome-db.org>
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


#ifndef __GDAUI_DATA_SELECTOR_H_
#define __GDAUI_DATA_SELECTOR_H_

#include <glib-object.h>
#include <gtk/gtk.h>
#include <libgda/libgda.h>
#include "gdaui-decl.h"

G_BEGIN_DECLS

#define GDAUI_TYPE_DATA_SELECTOR          (gdaui_data_selector_get_type())
G_DECLARE_INTERFACE (GdauiDataSelector, gdaui_data_selector, GDAUI, DATA_SELECTOR, GObject)
/* struct for the interface */
struct _GdauiDataSelectorInterface
{
	GTypeInterface           g_iface;

	/* virtual table */
	GdaDataModel     *(*get_model)             (GdauiDataSelector *iface);
	void              (*set_model)             (GdauiDataSelector *iface, GdaDataModel *model);
	GArray           *(*get_selected_rows)     (GdauiDataSelector *iface);
	GdaDataModelIter *(*get_data_set)          (GdauiDataSelector *iface);
	gboolean          (*select_row)            (GdauiDataSelector *iface, gint row);
	void              (*unselect_row)          (GdauiDataSelector *iface, gint row);
	void              (*set_column_visible)    (GdauiDataSelector *iface, gint column, gboolean visible);

	/* signals */
	void              (* selection_changed)    (GdauiDataSelector *iface);
	gpointer            padding[12];
};

/**
 * SECTION:gdaui-data-selector
 * @short_description: Selecting data in a #GdaDataModel
 * @title: GdauiDataSelector
 * @stability: Stable
 * @Image:
 * @see_also:
 *
 * The #GdauiDataSelector interface is implemented by widgets which allow the user
 * to select some data from a #GdaDataModel. Depending on the actual widget, the selection
 * can be a single row or more than one row.
 *
 * This interface allows one to set and get the #GdaDataModel from which data is to be selected
 * and offers a few other common behaviours.
 *
 * Please note that any row number in this interface is in reference to the #GdaDataModel returned by
 * the gdaui_data_selector_get_model() method.
 */

GdaDataModel     *gdaui_data_selector_get_model             (GdauiDataSelector *iface);
void              gdaui_data_selector_set_model             (GdauiDataSelector *iface, GdaDataModel *model);
GArray           *gdaui_data_selector_get_selected_rows     (GdauiDataSelector *iface);
GdaDataModelIter *gdaui_data_selector_get_data_set          (GdauiDataSelector *iface);
gboolean          gdaui_data_selector_select_row            (GdauiDataSelector *iface, gint row);
void              gdaui_data_selector_unselect_row          (GdauiDataSelector *iface, gint row);
void              gdaui_data_selector_set_column_visible    (GdauiDataSelector *iface, gint column, gboolean visible);

G_END_DECLS

#endif
