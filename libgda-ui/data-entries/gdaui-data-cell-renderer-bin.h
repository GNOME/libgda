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

#ifndef __GDAUI_DATA_CELL_RENDERER_BIN_H__
#define __GDAUI_DATA_CELL_RENDERER_BIN_H__

#include <gtk/gtk.h>
#include <libgda/gda-data-handler.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_DATA_CELL_RENDERER_BIN		(gdaui_data_cell_renderer_bin_get_type ())
G_DECLARE_DERIVABLE_TYPE (GdauiDataCellRendererBin, gdaui_data_cell_renderer_bin, GDAUI, DATA_CELL_RENDERER_BIN, GtkCellRendererPixbuf)
struct _GdauiDataCellRendererBinClass
{
	GtkCellRendererPixbufClass  parent_class;
	
	void (* changed) (GdauiDataCellRendererBin *cell_renderer,
			  const gchar              *path,
			  const GValue             *new_value);
};

GtkCellRenderer *gdaui_data_cell_renderer_bin_new       (GdaDataHandler *dh, GType type);

G_END_DECLS

#endif
