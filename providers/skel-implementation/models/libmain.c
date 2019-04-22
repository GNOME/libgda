/*
 * Copyright (C) YEAR The GNOME Foundation
 *
 * AUTHORS:
 *      TO_ADD: your name and email
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
#include <libgda/gda-config.h>
#include <libgda/gda-server-provider-extra.h>
#include <libgda/binreloc/gda-binreloc.h>
#include "gda-models.h"
#include "gda-models-provider.h"

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
	return MODELS_PROVIDER_NAME;
}

const gchar *
plugin_get_description (void)
{
	return _("Example provider for database where tables are based on data models");
}

gchar *
plugin_get_dsn_spec (void)
{
	return gda_server_provider_load_resource_contents ("models", "models_specs_dsn.raw.xml");
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

GdaServerProvider *
plugin_create_provider (void)
{
	GdaServerProvider *prov;

  prov = (GdaServerProvider *) g_object_new (GDA_TYPE_MODELS_PROVIDER, NULL);
  return prov;
}
