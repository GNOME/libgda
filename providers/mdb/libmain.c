/* GDA MDB Provider
 * Copyright (C) 1998-2002 The GNOME Foundation
 *
 * AUTHORS:
 *         Rodrigo Moya <rodrigo@gnome-db.org>
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
#include "gda-mdb.h"

MdbSQL *mdb_SQL = NULL;

const gchar *
plugin_get_name (void)
{
	return "MS Access";
}

const gchar *
plugin_get_description (void)
{
	return _("Provider for Microsoft Access files");
}

GList *
plugin_get_connection_params (void)
{
	GList *list = NULL;

	list = g_list_append (list,
			      gda_provider_parameter_info_new_full ("FILENAME", _("File Name"),
								    _("Full path of the file containing the database"),
								    GDA_VALUE_TYPE_STRING));

	return list;
}

GdaServerProvider *
plugin_create_provider (void)
{
	return gda_mdb_provider_new ();
}

/*
 * GModule functions
 */

const gchar *
g_module_check_init (void)
{
	if (!mdb_SQL)
		mdb_SQL = mdb_sql_init ();

	return NULL;
}

void
g_module_unload (void)
{
	if (mdb_SQL) {
		mdb_sql_exit (mdb_SQL);
		mdb_SQL = NULL;
	}
}
