/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDAUI_DATA_CELL_RENDERER_PICT_H__
#define __GDAUI_DATA_CELL_RENDERER_PICT_H__

#include <gtk/gtk.h>
#include <libgda-ui/gdaui-decl.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_DATA_CELL_RENDERER_PICT		(gdaui_data_cell_renderer_pict_get_type ())
#define GDAUI_DATA_CELL_RENDERER_PICT(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GDAUI_TYPE_DATA_CELL_RENDERER_PICT, GdauiDataCellRendererPict))
#define GDAUI_DATA_CELL_RENDERER_PICT_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), GDAUI_TYPE_DATA_CELL_RENDERER_PICT, GdauiDataCellRendererPictClass))
#define GDAUI_IS_DATA_CELL_RENDERER_PICT(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDAUI_TYPE_DATA_CELL_RENDERER_PICT))
#define GDAUI_IS_DATA_CELL_RENDERER_PICT_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GDAUI_TYPE_DATA_CELL_RENDERER_PICT))
#define GDAUI_DATA_CELL_RENDERER_PICT_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDAUI_TYPE_DATA_CELL_RENDERER_PICT, GdauiDataCellRendererPictClass))

typedef struct _GdauiDataCellRendererPict GdauiDataCellRendererPict;
typedef struct _GdauiDataCellRendererPictClass GdauiDataCellRendererPictClass;
typedef struct _GdauiDataCellRendererPictPrivate GdauiDataCellRendererPictPrivate;

struct _GdauiDataCellRendererPict
{
	GtkCellRendererPixbuf               parent;
	
	GdauiDataCellRendererPictPrivate *priv;
};

struct _GdauiDataCellRendererPictClass
{
	GtkCellRendererPixbufClass          parent_class;
	
	void (* changed) (GdauiDataCellRendererPict *cell_renderer,
			  const gchar                 *path,
			  const GValue                *new_value);
};

GType            gdaui_data_cell_renderer_pict_get_type  (void) G_GNUC_CONST;
GtkCellRenderer *gdaui_data_cell_renderer_pict_new       (GdaDataHandler *dh, GType type, const gchar *options);

G_END_DECLS

#endif /* __GDAUI_DATA_CELL_RENDERER_PICT_H__ */
