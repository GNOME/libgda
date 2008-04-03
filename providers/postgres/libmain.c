/* GDA Postgres Provider
 * Copyright (C) 1998 - 2007 The GNOME Foundation
 *
 * AUTHORS:
 *         Vivien Malerba <malerba@gnome-db.org>
 *         Rodrigo Moya <rodrigo@gnome-db.org>
 *         Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
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
#include <libgda/gda-config.h>
#include "gda-postgres-provider.h"
#include <libgda/gda-server-provider-extra.h>
#include <libgda/binreloc/gda-binreloc.h>

static gchar      *module_path = NULL;
const gchar       *plugin_get_name (void);
const gchar       *plugin_get_description (void);
gchar             *plugin_get_dsn_spec (void);
GdaServerProvider *plugin_create_provider (void);

void
plugin_init (const gchar *real_path)
{
	/* This is never freed, but that is OK. It is only called once. */
	/* But it would be nice to have some cleanup function just to shut valgrind up. murrayc. */
	if (real_path)
		module_path = g_strdup (real_path);
}

const gchar *
plugin_get_name (void)
{
	return "PostgreSQL";
}

const gchar *
plugin_get_description (void)
{
	return _("Provider for PostgreSQL databases");
}

gchar *
plugin_get_dsn_spec (void)
{
	gchar *ret, *dir;

	dir = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, NULL);
	ret = gda_server_provider_load_file_contents (module_path, dir, "postgres_specs_dsn.xml");
	g_free (dir);
	return ret;
}

GdaServerProvider *
plugin_create_provider (void)
{
	GdaServerProvider *prov;

	prov = gda_postgres_provider_new ();
	g_object_set_data ((GObject *) prov, "GDA_PROVIDER_DIR", module_path);
	return prov;
}
