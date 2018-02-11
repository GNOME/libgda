/*
 * Copyright (C) 2001 - 2002 Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 * Copyright (C) 2001 - 2004 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2002 Carlos Perelló Marín <carlos@gnome-db.org>
 * Copyright (C) 2005 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2005 Stanislav Brabec <sbrabec@suse.de>
 * Copyright (C) 2005 - 2015 Vivien Malerba <malerba@gnome-db.org>
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

#include <glib/gi18n-lib.h>
#include <gmodule.h>
#include <libgda/gda-config.h>
#include "gda-sqlite-provider.h"
#include <libgda/gda-server-provider-extra.h>
#include <libgda/binreloc/gda-binreloc.h>
#include <libgda/sqlite/gda-symbols-util.h>

static gchar      *module_path = NULL;
const gchar       *plugin_get_name (void);
const gchar       *plugin_get_description (void);
gchar             *plugin_get_dsn_spec (void);
const gchar       *plugin_get_icon_id (void);
GdaServerProvider *plugin_create_provider (void);
GModule           *bdbsql_module = NULL;

/*
 * Functions executed when calling g_module_open() and g_module_close()
 */
const gchar *
g_module_check_init (GModule *module)
{
	if (! bdbsql_module) {
		bdbsql_module = find_sqlite_library ("libdb_sql-5");
		if (! bdbsql_module)
			bdbsql_module = find_sqlite_library ("libdb_sql-6");
	}
	if (bdbsql_module)
		load_symbols (bdbsql_module);
	if (! s3r)
		return _("Can't find libdb_sql-5." G_MODULE_SUFFIX " or libdb_sql-6." G_MODULE_SUFFIX "file, or any dependent library");

        /*g_module_make_resident (module);*/
        return NULL;
}

void
g_module_unload (GModule *module)
{
	if (bdbsql_module) {
		if (g_module_close (bdbsql_module))
			bdbsql_module = NULL;
	}
	g_free (s3r);
	s3r = NULL;
        g_free (module_path);
        module_path = NULL;
}

/*
 * Normal plugin functions 
 */
void
plugin_init (const gchar *real_path)
{
        if (real_path)
                module_path = g_strdup (real_path);
}

const gchar *
plugin_get_name (void)
{
	return "BDBSql";
}

const gchar *
plugin_get_description (void)
{
	return _("Provider for Berkeley DB SQL databases");
}

gchar *
plugin_get_dsn_spec (void)
{
	gchar *ret, *dir;

	dir = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, NULL);
	ret = gda_server_provider_load_file_contents (module_path, dir, "bdbsql_specs_dsn.xml");
	g_free (dir);
	if (ret)
		return ret;
	else
		return gda_server_provider_load_resource_contents ("bdbsql", "bdbsql_specs_dsn.raw.xml");
}

gchar *
plugin_get_auth_spec (void)
{
#define AUTH "<?xml version=\"1.0\"?>" \
	     "<data-set-spec>" \
	     "  <parameters/>" \
	     "</data-set-spec>"

	return g_strdup (AUTH);
}

const gchar *
plugin_get_icon_id (void)
{
	return "bdb";
}

GdaServerProvider *
plugin_create_provider (void)
{
	GdaServerProvider *prov;

	prov = (GdaServerProvider*) g_object_new (GDA_TYPE_SQLITE_PROVIDER, NULL);
        g_object_set_data ((GObject *) prov, "GDA_PROVIDER_DIR", module_path);
        return prov;
}
