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

#ifndef __GDAUI_DATA_CELL_RENDERER_INFO_H__
#define __GDAUI_DATA_CELL_RENDERER_INFO_H__

#include <gtk/gtk.h>
#include <libgda/gda-enums.h>
#include <libgda-ui/gdaui-data-store.h>
#include <libgda-ui/gdaui-set.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_DATA_CELL_RENDERER_INFO			(gdaui_data_cell_renderer_info_get_type ())
G_DECLARE_DERIVABLE_TYPE (GdauiDataCellRendererInfo, gdaui_data_cell_renderer_info, GDAUI, DATA_CELL_RENDERER_INFO, GtkCellRenderer)
struct _GdauiDataCellRendererInfoClass
{
	GtkCellRendererClass parent_class;
	void (* status_changed) (GdauiDataCellRendererInfo *cell_renderer_info,
				 const gchar                 *path,
				 GdaValueAttribute            requested_action);
};

GtkCellRenderer *gdaui_data_cell_renderer_info_new       (GdauiDataStore *store,
							  GdaDataModelIter *iter, 
							  GdauiSetGroup *group);
G_END_DECLS

#endif
