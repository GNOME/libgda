/* gdaui-data-cell-renderer-bin.h
 *
 * Copyright (C) 2009 Vivien Malerba <malerba@gnome-db.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GDAUI_DATA_CELL_RENDERER_BIN_H__
#define __GDAUI_DATA_CELL_RENDERER_BIN_H__

#include <gtk/gtk.h>
#include <libgda/gda-data-handler.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_DATA_CELL_RENDERER_BIN		(gdaui_data_cell_renderer_bin_get_type ())
#define GDAUI_DATA_CELL_RENDERER_BIN(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GDAUI_TYPE_DATA_CELL_RENDERER_BIN, GdauiDataCellRendererBin))
#define GDAUI_DATA_CELL_RENDERER_BIN_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), GDAUI_TYPE_DATA_CELL_RENDERER_BIN, GdauiDataCellRendererBinClass))
#define GDAUI_IS_DATA_CELL_RENDERER_BIN(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDAUI_TYPE_DATA_CELL_RENDERER_BIN))
#define GDAUI_IS_DATA_CELL_RENDERER_BIN_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GDAUI_TYPE_DATA_CELL_RENDERER_BIN))
#define GDAUI_DATA_CELL_RENDERER_BIN_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDAUI_TYPE_DATA_CELL_RENDERER_BIN, GdauiDataCellRendererBinClass))

typedef struct _GdauiDataCellRendererBin GdauiDataCellRendererBin;
typedef struct _GdauiDataCellRendererBinClass GdauiDataCellRendererBinClass;
typedef struct _GdauiDataCellRendererBinPrivate GdauiDataCellRendererBinPrivate;

struct _GdauiDataCellRendererBin
{
	GtkCellRendererText             parent;
	
	GdauiDataCellRendererBinPrivate *priv;
};

struct _GdauiDataCellRendererBinClass
{
	GtkCellRendererTextClass  parent_class;
	
	void (* changed) (GdauiDataCellRendererBin *cell_renderer,
			  const gchar              *path,
			  const GValue             *new_value);
};

GType            gdaui_data_cell_renderer_bin_get_type  (void) G_GNUC_CONST;
GtkCellRenderer *gdaui_data_cell_renderer_bin_new       (GdaDataHandler *dh, GType type);

G_END_DECLS

#endif
