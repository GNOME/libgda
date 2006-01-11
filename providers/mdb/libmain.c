/* GDA MDB Provider
 * Copyright (C) 1998 - 2005 The GNOME Foundation
 *
 * AUTHORS:
 *         Rodrigo Moya <rodrigo@gnome-db.org>
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
#include "gda-mdb.h"

MdbSQL *mdb_SQL = NULL;

const gchar       *plugin_get_name (void);
const gchar       *plugin_get_description (void);
gchar             *plugin_get_dsn_spec (void);
GdaServerProvider *plugin_create_provider (void);

const gchar *
plugin_get_name (void)
{
	return "MS Access";
}

const gchar *
plugin_get_description (void)
{
	return _("Provider for Microsoft Access files");
}

gchar *
plugin_get_dsn_spec (void)
{
	gchar *specs, *file;
	gsize len;

	file = g_build_filename (LIBGDA_DATA_DIR, "mdb_specs_dsn.xml", NULL);
	if (g_file_get_contents (file, &specs, &len, NULL)) 
		return specs;
	else
		return NULL;
}

GdaServerProvider *
plugin_create_provider (void)
{
	return gda_mdb_provider_new ();
}

/*
 * GModule functions
 */

const gchar *
g_module_check_init (void)
{
	if (!mdb_SQL)
		mdb_SQL = mdb_sql_init ();

	return NULL;
}

void
g_module_unload (void)
{
	if (mdb_SQL) {
		mdb_sql_exit (mdb_SQL);
		mdb_SQL = NULL;
	}
}
