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

#ifndef __GDAUI_DATA_CELL_RENDERER_PASSWORD_H__
#define __GDAUI_DATA_CELL_RENDERER_PASSWORD_H__

#include <gtk/gtk.h>
#include <libgda-ui/gdaui-decl.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_DATA_CELL_RENDERER_PASSWORD		(gdaui_data_cell_renderer_password_get_type ())
#define GDAUI_DATA_CELL_RENDERER_PASSWORD(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GDAUI_TYPE_DATA_CELL_RENDERER_PASSWORD, GdauiDataCellRendererPassword))
#define GDAUI_DATA_CELL_RENDERER_PASSWORD_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), GDAUI_TYPE_DATA_CELL_RENDERER_PASSWORD, GdauiDataCellRendererPasswordClass))
#define GDAUI_IS_DATA_CELL_RENDERER_PASSWORD(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDAUI_TYPE_DATA_CELL_RENDERER_PASSWORD))
#define GDAUI_IS_DATA_CELL_RENDERER_PASSWORD_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GDAUI_TYPE_DATA_CELL_RENDERER_PASSWORD))
#define GDAUI_DATA_CELL_RENDERER_PASSWORD_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDAUI_TYPE_DATA_CELL_RENDERER_PASSWORD, GdauiDataCellRendererPasswordClass))

typedef struct _GdauiDataCellRendererPassword GdauiDataCellRendererPassword;
typedef struct _GdauiDataCellRendererPasswordClass GdauiDataCellRendererPasswordClass;
typedef struct _GdauiDataCellRendererPasswordPrivate GdauiDataCellRendererPasswordPrivate;

struct _GdauiDataCellRendererPassword
{
	GtkCellRendererText                     parent;
	
	GdauiDataCellRendererPasswordPrivate *priv;
};

struct _GdauiDataCellRendererPasswordClass
{
	GtkCellRendererTextClass                parent_class;
	
	void (* changed) (GdauiDataCellRendererPassword *cell_renderer,
			  const gchar                     *path,
			  const GValue                    *new_value);
};

GType            gdaui_data_cell_renderer_password_get_type  (void) G_GNUC_CONST;
GtkCellRenderer *gdaui_data_cell_renderer_password_new       (GdaDataHandler *dh, GType type, const gchar *options);

G_END_DECLS

#endif /* __GDAUI_DATA_CELL_RENDERER_PASSWORD_H__ */
