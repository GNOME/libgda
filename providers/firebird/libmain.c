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

	list = g_list_append (list, g_strdup ("DATABASE"));
	list = g_list_append (list, g_strdup ("PASSWORD"));
	list = g_list_append (list, g_strdup ("USER"));
	list = g_list_append (list, g_strdup ("CHARACTER_SET"));	/* Default: NONE */
	list = g_list_append (list, g_strdup ("SQL_DIALECT"));  	/* Default: 3 */
	list = g_list_append (list, g_strdup ("PAGE_SIZE"));		/* Default: 4096 */

	return list;
}

GdaServerProvider *
plugin_create_provider (void)
{
	return gda_firebird_provider_new ();
}
