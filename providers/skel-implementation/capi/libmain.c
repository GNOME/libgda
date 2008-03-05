/* GDA Capi Provider
 * Copyright (C) 2008 The GNOME Foundation
 *
 * AUTHORS:
 *      TO_ADD: your name and email
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
#include <libgda/gda-server-provider-extra.h>
#include <libgda/binreloc/gda-binreloc.h>
#include "gda-capi.h"
#include "gda-capi-provider.h"

static gchar      *module_path = NULL;
const gchar       *plugin_get_name (void);
const gchar       *plugin_get_description (void);
gchar             *plugin_get_dsn_spec (void);
GdaServerProvider *plugin_create_provider (void);

void
plugin_init (const gchar *real_path)
{
        if (real_path)
                module_path = g_strdup (real_path);
}

const gchar *
plugin_get_name (void)
{
	return CAPI_PROVIDER_NAME;
}

const gchar *
plugin_get_description (void)
{
	return _("Example provider for C API databases");
}

gchar *
plugin_get_dsn_spec (void)
{
	gchar *ret, *dir;

	dir = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, NULL);
	ret = gda_server_provider_load_file_contents (module_path, dir, "capi_specs_dsn.xml");
	g_free (dir);
	return ret;
}

GdaServerProvider *
plugin_create_provider (void)
{
	GdaServerProvider *prov;

	prov = (GdaServerProvider*) g_object_new (GDA_TYPE_CAPI_PROVIDER, NULL);
        g_object_set_data ((GObject *) prov, "GDA_PROVIDER_DIR", module_path);
        return prov;
}
