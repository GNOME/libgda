/* GDA FireBird Provider
 * Copyright (C) 1998 - 2007 The GNOME Foundation
 *
 * AUTHORS:
 *         Rodrigo Moya <rodrigo@gnome-db.org>
 *         Albi Jeronimo <jeronimoalbi@yahoo.com.ar>
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
#include "gda-firebird-provider.h"
#include <libgda/gda-server-provider-extra.h>

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
	return "Firebird";
}

const gchar *
plugin_get_description (void)
{
	return _("Provider for Firebird databases");
}

gchar *
plugin_get_dsn_spec (void)
{
	return gda_server_provider_load_file_contents (module_path, LIBGDA_DATA_DIR, "firebird_specs_dsn.xml");
}

GdaServerProvider *
plugin_create_provider (void)
{
	GdaServerProvider *prov;

        prov = gda_firebird_provider_new ();
        g_object_set_data (prov, "GDA_PROVIDER_DIR", module_path);
        return prov;
}
