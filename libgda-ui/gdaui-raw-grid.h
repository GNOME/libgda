/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDAUI_RAW_GRID__
#define __GDAUI_RAW_GRID__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_RAW_GRID          (gdaui_raw_grid_get_type())
G_DECLARE_DERIVABLE_TYPE(GdauiRawGrid, gdaui_raw_grid, GDAUI, RAW_GRID, GtkTreeView)

/* struct for the object's class */
struct _GdauiRawGridClass
{
	GtkTreeViewClass    parent_class;

	void             (* double_clicked)    (GdauiRawGrid *grid, gint row);
	void             (* populate_popup)    (GdauiRawGrid *grid, GtkMenu *menu);
};

/**
 * SECTION:gdaui-raw-grid
 * @short_description: Grid widget to manipulate data in a #GdaDataModel
 * @title: GdauiRawGrid
 * @stability: Stable
 * @Image: vi-raw-grid.png
 * @see_also: the #GdauiGrid widget which uses the #GdauiRawGrid and adds decorations such as information about data model size, and features searching.
 *
 * The #GdauiGrid widget which uses the #GdauiRawGrid and adds decorations such as
 * information about data model size, and features searching.
 */

GtkWidget *gdaui_raw_grid_new                   (GdaDataModel *model);

void       gdaui_raw_grid_set_sample_size       (GdauiRawGrid *grid, gint sample_size);
void       gdaui_raw_grid_set_sample_start      (GdauiRawGrid *grid, gint sample_start);

void       gdaui_raw_grid_set_layout_from_file  (GdauiRawGrid *grid, const gchar *file_name, const gchar *grid_name);

typedef void (*GdauiRawGridFormatFunc) (GtkCellRenderer *cell, GtkTreeViewColumn *column, gint column_pos, GdaDataModel *model, gint row, gpointer data);
void       gdaui_raw_grid_add_formatting_function (GdauiRawGrid *grid, GdauiRawGridFormatFunc func,
						   gpointer data, GDestroyNotify dnotify);
void       gdaui_raw_grid_remove_formatting_function (GdauiRawGrid *grid, GdauiRawGridFormatFunc func);

/* private API */
GList     *_gdaui_raw_grid_get_selection        (GdauiRawGrid *grid);

G_END_DECLS

#endif



