/* GDA FireBird Provider
 * Copyright (C) 1998-2002 The GNOME Foundation
 *
 * AUTHORS:
 *         Rodrigo Moya <rodrigo@gnome-db.org>
 *         Albi Jeronimo <jeronimoalbi@yahoo.com.ar>
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
#include "gda-firebird-provider.h"

const gchar       *plugin_get_name (void);
const gchar       *plugin_get_description (void);
GList             *plugin_get_connection_params (void);
GdaServerProvider *plugin_create_provider (void);

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

GList *
plugin_get_connection_params (void)
{
	GList *list = NULL;

	list = g_list_append (list,
			      gda_provider_parameter_info_new_full ("DATABASE", _("Database Name"),
								    _("Name of the database to be used"),
								    GDA_VALUE_TYPE_STRING));
	list = g_list_append (list,
			      gda_provider_parameter_info_new_full ("CHARACTER_SET", _("Character Set"),
								    _("Character set to be used on the connection"),
								    GDA_VALUE_TYPE_STRING));
	list = g_list_append (list,
			      gda_provider_parameter_info_new_full ("SQL_DIALECT", _("SQL Dialect"),
								    _("SQL dialect to use on the connection"),
								    GDA_VALUE_TYPE_INTEGER));
	list = g_list_append (list,
			      gda_provider_parameter_info_new_full ("PAGE_SIZE", _("Page Size"),
								    _("Page size to use on the database"),
								    GDA_VALUE_TYPE_INTEGER));

	return list;
}

GdaServerProvider *
plugin_create_provider (void)
{
	return gda_firebird_provider_new ();
}
