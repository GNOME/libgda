/* GDA Sybase Provider
 * Copyright (C) 2002 - 2005 The GNOME Foundation
 *
 * AUTHORS:
 *         Mike Wingert <wingert.3@postbox.acs.ohio-state.edu>
 *         Holger Thon <holger.thon@gnome-db.org>
 *         Vivien Malerba <malerba@gnome-db.org>
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
#include "gda-sybase-provider.h"

const gchar       *plugin_get_name (void);
const gchar       *plugin_get_description (void);
gchar             *plugin_get_dsn_spec (void);
GdaServerProvider *plugin_create_provider (void);

const gchar *
plugin_get_name (void)
{
	return "Sybase";
}

const gchar *
plugin_get_description (void)
{
	return _("GDA provider for sybase databases (using OpenClient)");
}

gchar *
plugin_get_dsn_spec (void)
{
	gchar *specs, *file;
	gsize len;

	file = g_build_filename (LIBGDA_DATA_DIR, "sybase_specs_dsn.xml", NULL);
	if (g_file_get_contents (file, &specs, &len, NULL)) 
		return specs;
	else
		return NULL;
}

GdaServerProvider *
plugin_create_provider (void)
{
	return gda_sybase_provider_new ();
}
