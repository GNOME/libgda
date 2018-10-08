/*
 * Copyright (C) 2015 Vivien Malerba <malerba@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "support.h"
#include <libgda-ui/libgda-ui.h>

/**
 * support_create_pixbuf_for_provider:
 * @pinfo: (nullable): a #GdaProviderInfo, or %NULL
 *
 * Creates a new #GdkPixbuf using @pinfo's icon. The new pixbuf will at most be SUPPORT_ICON_SIZE pixels high.
 *
 * Returns: (transfer full): a new #GdkPixbuf
 */
GdkPixbuf *
support_create_pixbuf_for_provider (GdaProviderInfo *pinfo)
{
	if (!pinfo)
		return NULL;

	GdkPixbuf *pix, *pix2;

	pix = gdaui_get_icon_for_db_engine (pinfo->icon_id);
	if (pix) {
		gfloat scale;
		scale = (gfloat) SUPPORT_ICON_SIZE / (gfloat) gdk_pixbuf_get_height (pix);
		if (scale > 1.)
			scale = 1.;
		pix2 = gdk_pixbuf_scale_simple (pix, scale * gdk_pixbuf_get_width (pix),
						scale * gdk_pixbuf_get_height (pix),
						GDK_INTERP_HYPER);
		return pix2;
	}
	else
		return NULL;
}

