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

#ifndef __GDAUI_DATA_CELL_RENDERER_COMBO_H__
#define __GDAUI_DATA_CELL_RENDERER_COMBO_H__

#include <gtk/gtk.h>
#include <libgda-ui/gdaui-set.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_DATA_CELL_RENDERER_COMBO		(gdaui_data_cell_renderer_combo_get_type ())
G_DECLARE_DERIVABLE_TYPE (GdauiDataCellRendererCombo, gdaui_data_cell_renderer_combo, GDAUI, DATA_CELL_RENDERER_COMBO, GtkCellRendererText)
struct _GdauiDataCellRendererComboClass
{
	GtkCellRendererTextClass parent_class;
	
	void (* changed) (GdauiDataCellRendererCombo *cell_renderer_combo,
			  const gchar *path,
			  GSList *new_values, GSList *all_new_values);
};

GtkCellRenderer *gdaui_data_cell_renderer_combo_new      (GdauiSet *paramlist, GdauiSetSource *source);

G_END_DECLS

#endif
