/*
 * Copyright (C) 2009 The GNOME Foundation
 *
 * AUTHORS:
 *      Vivien Malerba <malerba@gnome-db.org>
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <glib/gi18n-lib.h>
#include <gmodule.h>
#include "perspective-main.h"
#include "schema-browser-perspective.h"

static BrowserPerspectiveFactory bfact;

BrowserPerspectiveFactory *
schema_browser_perspective_get_factory (void)
{
	bfact.perspective_name = _("Schema browser");
	bfact.menu_shortcut = "<control>B";
	bfact.perspective_create = schema_browser_perspective_new;

	return &bfact;
}
