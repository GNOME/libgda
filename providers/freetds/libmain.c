/* GDA FreeTDS Provider
 * Copyright (C) 2002 The GNOME Foundation
 *
 * AUTHORS:
 *         Holger Thon <holger.thon@gnome-db.org>
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
#include "gda-freetds-provider.h"

const gchar *plugin_get_name (void);
const gchar *plugin_get_description (void);
GList *plugin_get_connection_params (void);
GdaServerProvider *plugin_create_provider (void);

const gchar *
plugin_get_name (void)
{
	return "FreeTDS";
}

const gchar *
plugin_get_description (void)
{
	return _("Provider for TDS-based databases (using FreeTDS)");
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
			      gda_provider_parameter_info_new_full ("HOST", _("Host"),
								    _("Host name of the machine where the database is located"),
								    GDA_VALUE_TYPE_STRING));
	list = g_list_append (list,
			      gda_provider_parameter_info_new_full ("HOSTADDR", _("Host Address"),
								    _("IP address of the host to connect to"),
								    GDA_VALUE_TYPE_STRING));
	list = g_list_append (list,
			      gda_provider_parameter_info_new_full ("OPTIONS", _("Connection Options"),
								    _("Extra options to use on the connection"),
								    GDA_VALUE_TYPE_STRING));
	list = g_list_append (list,
			      gda_provider_parameter_info_new_full ("PORT", _("Port"),
								    _("Port to use to connect to the database's host"),
								    GDA_VALUE_TYPE_INTEGER));

	/************************************/
	/* environment variables for sybase */
	list = g_list_append (list,
			      gda_provider_parameter_info_new_full ("SYBASE", _("Enable Sybase"),
								    _("Enable Sybase operative mode"),
								    GDA_VALUE_TYPE_BOOLEAN));

	/************************************/
	/* environment settings for freetds */
	/* location of freetds config file */
	list = g_list_append (list,
			      gda_provider_parameter_info_new_full ("TDS_FREETDSCONF", _("FreeTDS Config File"),
								    _("Location of the FreeTDS config file"),
								    GDA_VALUE_TYPE_STRING));
	/* Protocol version */
	list = g_list_append (list,
			      gda_provider_parameter_info_new_full ("TDS_MAJVER", _("TDS Major Version"),
								    _("TDS protocol major version"),
								    GDA_VALUE_TYPE_STRING));
	list = g_list_append (list,
			      gda_provider_parameter_info_new_full ("TDS_MINVER", _("TDS Minor Version"),
								    _("TDS protocol minor version"),
								    GDA_VALUE_TYPE_STRING));
	/* File for tds dumps */
	list = g_list_append (list,
			      gda_provider_parameter_info_new_full ("TDS_DUMP", _("Dump File"),
								    _("File for TDS protocol dumps"),
								    GDA_VALUE_TYPE_STRING));
	list = g_list_append (list,
			      gda_provider_parameter_info_new_full ("TDS_DUMPCONFIG", _("Dump Configuration File"),
								    _("File fpr TDS protocol dumps configuration"),
								    GDA_VALUE_TYPE_STRING));
	/* Same effect like PORT */
	/* list = g_list_append (list, g_strdup ("TDS_PORT")); */
	/* Same effect like HOST */
	/* list = g_list_append (list, g_strdup ("TDS_HOST")); */
        /* list = g_list_append (list, g_strdup ("TDS_QUERY")); */
	
	return list;
}

GdaServerProvider *
plugin_create_provider (void)
{
	return gda_freetds_provider_new ();
}
