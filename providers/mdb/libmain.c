/*
 * Copyright (C) 2001 - 2002 Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 * Copyright (C) 2001 - 2004 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2006 - 2015 Vivien Malerba <malerba@gnome-db.org>
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
#include <libgda/gda-server-provider-extra.h>
#include <libgda/binreloc/gda-binreloc.h>
#include "gda-mdb.h"
#include "gda-mdb-provider.h"

static gchar *module_path = NULL;

const gchar       *plugin_get_name (void);
const gchar       *plugin_get_description (void);
gchar             *plugin_get_dsn_spec (void);
GdaServerProvider *plugin_create_provider (void);
const gchar       *plugin_get_icon_id (void);

/*
 * Functions executed when calling g_module_open() and g_module_close()
 */
const gchar *
g_module_check_init (GModule *module)
{
        /*g_module_make_resident (module);*/
        return NULL;
}

void
g_module_unload (GModule *module)
{
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
	return MDB_PROVIDER_NAME;
}

const gchar *
plugin_get_description (void)
{
	return _("Provider for Microsoft Access files");
}

gchar *
plugin_get_dsn_spec (void)
{
	gchar *ret, *dir;

        dir = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, NULL);
        ret = gda_server_provider_load_file_contents (module_path, dir, "mdb_specs_dsn.xml");
        g_free (dir);
	if (ret)
		return ret;
	else
		return gda_server_provider_load_resource_contents ("mdb", "mdb_specs_dsn.raw.xml");
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
	return "mdb";
}

GdaServerProvider *
plugin_create_provider (void)
{
	GdaServerProvider *prov;

        prov = gda_mdb_provider_new ();
        g_object_set_data (G_OBJECT (prov), "GDA_PROVIDER_DIR", module_path);
        return prov;
}
