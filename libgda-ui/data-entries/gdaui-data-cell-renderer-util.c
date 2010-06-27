/*
 * Copyright (C) 2010 Vivien Malerba <malerba@gnome-db.org>
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

#include "gdaui-data-cell-renderer-util.h"

void
gdaui_data_cell_renderer_draw_invalid_area (GdkWindow *window, GdkRectangle *cell_area)
{
	cairo_t *cr;
	cr = gdk_cairo_create (window);
	cairo_set_source_rgba (cr, .5, .5, .5, .4);
	cairo_rectangle (cr, cell_area->x, cell_area->y, cell_area->width,  cell_area->height);
	cairo_clip (cr);
	
	gint i;
	for (i = 0; ; i++) {
		gint x = 10 * i;
		if (x > cell_area->width + cell_area->height)
			break;
		cairo_move_to (cr, x + cell_area->x, cell_area->y);
		cairo_line_to (cr, x + cell_area->x - cell_area->height,
			       cell_area->y + cell_area->height);
		cairo_stroke (cr);
	}
	cairo_destroy (cr);
}
