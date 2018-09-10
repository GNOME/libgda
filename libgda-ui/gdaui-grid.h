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

#ifndef __GDAUI_GRID__
#define __GDAUI_GRID__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_GRID          (gdaui_grid_get_type())
G_DECLARE_DERIVABLE_TYPE(GdauiGrid, gdaui_grid, GDAUI, GRID, GtkBox)
/* struct for the object's class */
struct _GdauiGridClass
{
	GtkBoxClass       parent_class;
	gpointer            padding[12];
};

/**
 * SECTION:gdaui-grid
 * @short_description: Grid widget to manipulate data in a #GdaDataModel, with decorations
 * @title: GdauiGrid
 * @stability: Stable
 * @Image:
 * @see_also: The #GdauiRawGrid widget which is used by the #GdaGrid widget.
 */

GtkWidget        *gdaui_grid_new                 (GdaDataModel *model);
void              gdaui_grid_set_sample_size     (GdauiGrid *grid, gint sample_size);

G_END_DECLS

#endif



