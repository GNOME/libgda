/*
 * Copyright (C) 2000 Reinhard Müller <reinhard@src.gnome.org>
 * Copyright (C) 2000 - 2002 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2001 Carlos Perelló Marín <carlos@gnome-db.org>
 * Copyright (C) 2001 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2002 Gonzalo Paniagua Javier <gonzalo@src.gnome.org>
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

#ifndef __GDAUI_EASY_H__
#define __GDAUI_EASY_H__

#include <libgda-ui/gdaui-data-entry.h>

G_BEGIN_DECLS

/**
 * SECTION:gdaui-easy
 * @short_description: Set of UI related functions
 * @title: UI Utility functions
 * @stability: Stable
 * @Image:
 * @see_also:
 */

GdauiDataEntry  *gdaui_new_data_entry    (GType type, const gchar *plugin_name);
GtkCellRenderer *_gdaui_new_cell_renderer (GType type, const gchar *plugin_name);

G_END_DECLS

#endif
