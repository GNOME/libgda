/* GDA LDAP Provider
 * Copyright (C) 1998-2003 The GNOME Foundation
 *
 * AUTHORS:
 * 	   Michael Lausch <michael@lausch.at>
 *     Rodrigo Moya <rodrigo@gnome-db.org>
 *     Vivien Malerba <malerba@gnome-db.org>
 *     Germán Poo-Caamaño <gpoo@ubiobio.cl>
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

#include <libgda/gda-config.h>
#include <libgda/gda-intl.h>
#include "gda-ldap-provider.h"

const gchar *plugin_get_name (void);
const gchar *plugin_get_description (void);
GList *plugin_get_connection_params (void);
GdaServerProvider *plugin_create_provider (void);

/*
 * This functions provides information of this provider
 *
 */

const gchar *
plugin_get_name (void)
{
	return "LDAP";
}

const gchar *
plugin_get_description (void)
{
	return _("Provider for LDAP directory");
}

GList *
plugin_get_connection_params (void)
{
	GList *list = NULL;

	list = g_list_append (list,
			      gda_provider_parameter_info_new_full ("FLAGS", _("LDAP Flags"),
								    _("LDAP flags"),
								    GDA_VALUE_TYPE_STRING));
	list = g_list_append (list,
			      gda_provider_parameter_info_new_full ("HOST", _("Host Name"),
								    _("Host name of the LDAP server"),
								    GDA_VALUE_TYPE_STRING));
	list = g_list_append (list,
			      gda_provider_parameter_info_new_full ("PORT", _("Port"),
								    _("Port to use for connecting to the LDAP server"),
								    GDA_VALUE_TYPE_INTEGER));
	list = g_list_append (list,
			      gda_provider_parameter_info_new_full ("BINDDN", _("Bind Domain"),
								    _("LDAP bind domain to use for authentication"),
								    GDA_VALUE_TYPE_STRING));
	list = g_list_append (list,
			      gda_provider_parameter_info_new_full ("AUTHMETHOD", _("Authentication Method"),
								    _("Authentication method to use for connecting"),
								    GDA_VALUE_TYPE_STRING));

	return list;
}

GdaServerProvider *
plugin_create_provider (void)
{
	return gda_ldap_provider_new ();
}
