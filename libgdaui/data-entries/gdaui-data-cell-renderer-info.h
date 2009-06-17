/* gdaui-data-cell-renderer-info.h
 *
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
 * Copyright (C) 2003 - 2009 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDAUI_DATA_CELL_RENDERER_INFO_H__
#define __GDAUI_DATA_CELL_RENDERER_INFO_H__

#include <gtk/gtk.h>
#include <libgda/gda-enums.h>
#include <libgdaui/gdaui-data-store.h>
#include <libgdaui/gdaui-set.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_DATA_CELL_RENDERER_INFO			(gdaui_data_cell_renderer_info_get_type ())
#define GDAUI_DATA_CELL_RENDERER_INFO(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), GDAUI_TYPE_DATA_CELL_RENDERER_INFO, GdauiDataCellRendererInfo))
#define GDAUI_DATA_CELL_RENDERER_INFO_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), GDAUI_TYPE_DATA_CELL_RENDERER_INFO, GdauiDataCellRendererInfoClass))
#define GDAUI_IS_DATA_CELL_RENDERER_INFO(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDAUI_TYPE_DATA_CELL_RENDERER_INFO))
#define GDAUI_IS_DATA_CELL_RENDERER_INFO_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GDAUI_TYPE_DATA_CELL_RENDERER_INFO))
#define GDAUI_DATA_CELL_RENDERER_INFO_GET_CLASS(obj)         (G_TYPE_INSTANCE_GET_CLASS ((obj), GDAUI_TYPE_DATA_CELL_RENDERER_INFO, GdauiDataCellRendererInfoClass))

typedef struct _GdauiDataCellRendererInfo GdauiDataCellRendererInfo;
typedef struct _GdauiDataCellRendererInfoClass GdauiDataCellRendererInfoClass;
typedef struct _GdauiDataCellRendererInfoPriv GdauiDataCellRendererInfoPriv;

struct _GdauiDataCellRendererInfo
{
	GtkCellRenderer             parent;

	GdauiDataCellRendererInfoPriv *priv;
};

struct _GdauiDataCellRendererInfoClass
{
	GtkCellRendererClass parent_class;
	
	void (* status_changed) (GdauiDataCellRendererInfo *cell_renderer_info,
				 const gchar                 *path,
				 GdaValueAttribute            requested_action);
};

GType            gdaui_data_cell_renderer_info_get_type  (void) G_GNUC_CONST;
GtkCellRenderer *gdaui_data_cell_renderer_info_new       (GdauiDataStore *store,
							  GdaDataModelIter *iter, 
							  GdauiSetGroup *group);
G_END_DECLS

#endif
