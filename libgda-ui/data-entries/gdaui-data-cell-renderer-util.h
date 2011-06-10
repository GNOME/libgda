/*
 * Copyright (C) 2001 - 2002 Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 * Copyright (C) 2001 - 2002 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2003 Danilo Schoeneberg <dj@starfire-programming.net>
 * Copyright (C) 2003 Laurent Sansonetti <lrz@gnome.org>
 * Copyright (C) 2005 - 2011 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDAUI_DATA_CELL_RENDERER_UTIL_H__
#define __GDAUI_DATA_CELL_RENDERER_UTIL_H__

#include <gtk/gtk.h>

void gdaui_data_cell_renderer_draw_invalid_area (cairo_t *cr, const GdkRectangle *cell_area);

#endif
