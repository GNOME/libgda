/* GDA Berkeley-DB Provider
 * Copyright (C) 1998-2002 The GNOME Foundation
 *
 * AUTHORS:
 *         Laurent Sansonetti <lrz@gnome.org>
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
#include "gda-bdb.h"

const gchar *plugin_get_name (void);
const gchar *plugin_get_description (void);
GList *plugin_get_connection_params (void);
GdaServerProvider *plugin_create_provider (void);

const gchar *
plugin_get_name (void)
{
	return "Berkeley-DB";
}

const gchar *
plugin_get_description (void)
{
	return _("Provider for Berkeley databases");
}

GList *
plugin_get_connection_params (void)
{
	GList *list = NULL;

	list = g_list_append (list,
			      gda_provider_parameter_info_new_full ("FILE", _("Database File"),
								    _("Berkeley DB database file to be used"),
								    GDA_VALUE_TYPE_STRING));
	list = g_list_append (list,
			      gda_provider_parameter_info_new_full ("DATABASE", _("Database name"),
								    _("Name of the database in the database file to be used"),
								    GDA_VALUE_TYPE_STRING));

	return list;
}

GdaServerProvider *
plugin_create_provider (void)
{
	return gda_bdb_provider_new ();
}
