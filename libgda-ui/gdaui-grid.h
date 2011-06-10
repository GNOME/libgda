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

#ifndef __GDAUI_GRID__
#define __GDAUI_GRID__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_GRID          (gdaui_grid_get_type())
#define GDAUI_GRID(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gdaui_grid_get_type(), GdauiGrid)
#define GDAUI_GRID_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gdaui_grid_get_type (), GdauiGridClass)
#define GDAUI_IS_GRID(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gdaui_grid_get_type ())
#define GDAUI_IS_GRID_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDAUI_TYPE_GRID))

typedef struct _GdauiGrid      GdauiGrid;
typedef struct _GdauiGridClass GdauiGridClass;
typedef struct _GdauiGridPriv  GdauiGridPriv;

/* struct for the object's data */
struct _GdauiGrid
{
	GtkVBox             object;

	GdauiGridPriv     *priv;
};

/* struct for the object's class */
struct _GdauiGridClass
{
	GtkVBoxClass       parent_class;
};

/**
 * SECTION:gdaui-grid
 * @short_description: Grid widget to manipulate data in a #GdaDataModel, with decorations
 * @title: GdauiGrid
 * @stability: Stable
 * @Image:
 * @see_also: The #GdauiRawGrid widget which is used by the #GdaGrid widget.
 */

GType             gdaui_grid_get_type            (void) G_GNUC_CONST;

GtkWidget        *gdaui_grid_new                 (GdaDataModel *model);
void              gdaui_grid_set_sample_size     (GdauiGrid *grid, gint sample_size);

G_END_DECLS

#endif



