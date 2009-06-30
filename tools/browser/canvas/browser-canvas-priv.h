/* browser-canvas-priv.h
 *
 * Copyright (C) 2009 Vivien Malerba
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifndef __BROWSER_CANVAS_PRIV__
#define __BROWSER_CANVAS_PRIV__

#include <goocanvas.h>
#include "browser-canvas-decl.h"

G_BEGIN_DECLS

struct _BrowserCanvasPrivate
{
	GooCanvas          *goocanvas;
	GSList             *items; /* BrowserCanvasItem objects, non ordered */

	gboolean            canvas_moving;

	BrowserCanvasItem  *current_selected_item;
};

G_END_DECLS

#endif
