/*
 * Copyright (C) 2001 - 2002 Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 * Copyright (C) 2001 - 2004 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2002 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2005 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2008 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
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
#include "gda-postgres.h"
#include "gda-postgres-provider.h"

const gchar       *plugin_get_name (void);
const gchar       *plugin_get_description (void);
gchar             *plugin_get_dsn_spec (void);
GdaServerProvider *plugin_create_provider (void);

/*
 * Functions executed when calling g_module_open() and g_module_close()
 */
const gchar *
g_module_check_init (G_GNUC_UNUSED GModule *module)
{
        /*g_module_make_resident (module);*/
        return NULL;
}

void
g_module_unload (G_GNUC_UNUSED GModule *module)
{
}

/*
 * Normal plugin functions 
 */
void
plugin_init (const gchar *real_path)
{
}

const gchar *
plugin_get_name (void)
{
	return POSTGRES_PROVIDER_NAME;
}

const gchar *
plugin_get_description (void)
{
	return _("Provider for PostgreSQL databases");
}

gchar *
plugin_get_dsn_spec (void)
{
  return gda_server_provider_load_resource_contents ("postgres", "postgres_specs_dsn.raw.xml");
}

GdaServerProvider *
plugin_create_provider (void)
{
	GdaServerProvider *prov;

	prov = (GdaServerProvider*) g_object_new (GDA_TYPE_POSTGRES_PROVIDER, NULL);
  return prov;
}
