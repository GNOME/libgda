/* gdaui-raw-grid.h
 *
 * Copyright (C) 2002 - 2009 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDAUI_RAW_GRID__
#define __GDAUI_RAW_GRID__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_RAW_GRID          (gdaui_raw_grid_get_type())
#define GDAUI_RAW_GRID(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gdaui_raw_grid_get_type(), GdauiRawGrid)
#define GDAUI_RAW_GRID_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gdaui_raw_grid_get_type (), GdauiRawGridClass)
#define GDAUI_IS_RAW_GRID(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gdaui_raw_grid_get_type ())


typedef struct _GdauiRawGrid      GdauiRawGrid;
typedef struct _GdauiRawGridClass GdauiRawGridClass;
typedef struct _GdauiRawGridPriv  GdauiRawGridPriv;

/* struct for the object's data */
struct _GdauiRawGrid
{
	GtkTreeView         object;

	GdauiRawGridPriv   *priv;
};

/* struct for the object's class */
struct _GdauiRawGridClass
{
	GtkTreeViewClass    parent_class;

	void             (* double_clicked)    (GdauiRawGrid *grid, gint row);
        void             (* populate_popup)    (GdauiRawGrid *grid, GtkMenu *menu);
};

/* 
 * Generic widget's methods 
 */
GType      gdaui_raw_grid_get_type              (void) G_GNUC_CONST;

GtkWidget *gdaui_raw_grid_new                   (GdaDataModel *model);

void       gdaui_raw_grid_set_sample_size       (GdauiRawGrid *grid, gint sample_size);
void       gdaui_raw_grid_set_sample_start      (GdauiRawGrid *grid, gint sample_start);

void       gdaui_raw_grid_set_layout_from_file  (GdauiRawGrid *grid, const gchar *file_name, const gchar *grid_name);

/* private API */
GList     *_gdaui_raw_grid_get_selection        (GdauiRawGrid *grid);

G_END_DECLS

#endif



