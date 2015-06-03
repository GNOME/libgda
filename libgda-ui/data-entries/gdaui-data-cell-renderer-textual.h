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

#ifndef __GDAUI_DATA_CELL_RENDERER_TEXTUAL_H__
#define __GDAUI_DATA_CELL_RENDERER_TEXTUAL_H__

#include <gtk/gtk.h>
#include <pango/pango.h>
#include <libgda/gda-data-handler.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_DATA_CELL_RENDERER_TEXTUAL		(gdaui_data_cell_renderer_textual_get_type ())
#define GDAUI_DATA_CELL_RENDERER_TEXTUAL(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GDAUI_TYPE_DATA_CELL_RENDERER_TEXTUAL, GdauiDataCellRendererTextual))
#define GDAUI_DATA_CELL_RENDERER_TEXTUAL_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), GDAUI_TYPE_DATA_CELL_RENDERER_TEXTUAL, GdauiDataCellRendererTextualClass))
#define GDAUI_IS_DATA_CELL_RENDERER_TEXTUAL(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDAUI_TYPE_DATA_CELL_RENDERER_TEXTUAL))
#define GDAUI_IS_DATA_CELL_RENDERER_TEXTUAL_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GDAUI_TYPE_DATA_CELL_RENDERER_TEXTUAL))
#define GDAUI_DATA_CELL_RENDERER_TEXTUAL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GDAUI_TYPE_DATA_CELL_RENDERER_TEXTUAL, GdauiDataCellRendererTextualClass))

typedef struct _GdauiDataCellRendererTextual        GdauiDataCellRendererTextual;
typedef struct _GdauiDataCellRendererTextualClass   GdauiDataCellRendererTextualClass;
typedef struct _GdauiDataCellRendererTextualPrivate GdauiDataCellRendererTextualPrivate;

struct _GdauiDataCellRendererTextual
{
	GtkCellRendererText                    parent;

	GdauiDataCellRendererTextualPrivate *priv;
};

struct _GdauiDataCellRendererTextualClass
{
	GtkCellRendererTextClass               parent_class;
	
	void (* changed) (GdauiDataCellRendererTextual  *cell_renderer_textual,
			  const gchar         *path,
			  const GValue      *new_value);
};

GType            gdaui_data_cell_renderer_textual_get_type (void) G_GNUC_CONST;
GtkCellRenderer *gdaui_data_cell_renderer_textual_new      (GdaDataHandler *dh, GType type, 
							    const gchar *options);


G_END_DECLS

#endif
