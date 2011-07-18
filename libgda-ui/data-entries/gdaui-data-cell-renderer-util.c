/*
 * Copyright (C) 2010 - 2011 Vivien Malerba <malerba@gnome-db.org>
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

#include "gdaui-data-cell-renderer-util.h"
#include <libgda-ui/gdaui-decl.h>

void
gdaui_data_cell_renderer_draw_invalid_area (cairo_t *cr, const GdkRectangle *cell_area)
{
	cairo_set_source_rgba (cr, .5, .5, .5, .4);
	cairo_rectangle (cr, cell_area->x, cell_area->y, cell_area->width,  cell_area->height);
	cairo_clip (cr);
	
	cairo_set_source_rgba (cr, GDAUI_COLOR_UNKNOWN_MASK);
	cairo_rectangle (cr, cell_area->x, cell_area->y,
			 cell_area->width, cell_area->height);
	cairo_fill (cr);
}
