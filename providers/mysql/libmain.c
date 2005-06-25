/* GDA MySQL Provider
 * Copyright (C) 1998 - 2005 The GNOME Foundation
 *
 * AUTHORS:
 * 	   Michael Lausch <michael@lausch.at>
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

#include <libgda/gda-intl.h>
#include "gda-mysql-provider.h"

const gchar       *plugin_get_name (void);
const gchar       *plugin_get_description (void);
GList             *plugin_get_connection_params (void);
gchar             *plugin_get_dsn_spec (void);
GdaServerProvider *plugin_create_provider (void);

const gchar *
plugin_get_name (void)
{
	return "MySQL";
}

const gchar *
plugin_get_description (void)
{
	return _("Provider for MySQL databases");
}

GList *
plugin_get_connection_params (void)
{
	GList *list = NULL;

	list = g_list_append (list,
			      gda_provider_parameter_info_new_full ("DATABASE", _("Database Name"),
								    _("Name of the database to connect to"),
								    GDA_VALUE_TYPE_STRING));
	list = g_list_append (list,
			      gda_provider_parameter_info_new_full ("HOST", _("Host Name"),
								    _("Name of the host to connect to"),
								    GDA_VALUE_TYPE_STRING));
	list = g_list_append (list,
			      gda_provider_parameter_info_new_full ("PORT", _("Port"),
								    _("Port number to use for connecting"),
								    GDA_VALUE_TYPE_INTEGER));
	list = g_list_append (list,
			      gda_provider_parameter_info_new_full ("UNIX_SOCKET", _("UNIX Socket"),
								    _("Full path of the UNIX socket to use for connecting locally"),
								    GDA_VALUE_TYPE_STRING));
	list = g_list_append (list,
			      gda_provider_parameter_info_new_full ("USE_SSL", _("Use Secure Connection"),
								    _("Whether to use or not SSL for establishing the connection"),
								    GDA_VALUE_TYPE_BOOLEAN));

	return list;
}

gchar *
plugin_get_dsn_spec (void)
{
	gchar *specs, *file;
	gsize len;

	file = g_build_filename (LIBGDA_DATA_DIR, "mysql_specs_dsn.xml", NULL);
	if (g_file_get_contents (file, &specs, &len, NULL)) 
		return specs;
	else
		return NULL;
}

GdaServerProvider *
plugin_create_provider (void)
{
	return gda_mysql_provider_new ();
}
