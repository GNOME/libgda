/*
 * Copyright (C) 2001 - 2002 Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 * Copyright (C) 2001 - 2002 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2003 Danilo Schoeneberg <dj@starfire-programming.net>
 * Copyright (C) 2005 - 2014 Vivien Malerba <malerba@gnome-db.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef __GDA_LDAP_H__
#define __GDA_LDAP_H__

/*
 * Provider name
 */
#define LDAP_PROVIDER_NAME "Ldap"

#include <ldap.h>
#include <ldap_schema.h>
#include <glib.h>
#include <libgda/libgda.h>
#include <libgda/gda-connection-private.h>
#include "gda-ldap-connection.h"

/*
 * Note about thread safety:
 * the doc says: "libldap is thread safe in the sense that multiple
 * threads can use separate LDAP* handles without running into concurrency
 * issues; except for library initialization, all accesses to common data
 * (i.e. global variables) is read-only."
 *
 * Here each LDAP handle is treated in its own thread, and function like ldap_str2dn(), ldap_dnfree(),
 * are considered thread safe.
 */

/*
 * Provider's specific connection data
 */
typedef struct {
	GdaServerProviderConnectionData parent;
	guint         keep_bound_count; /* set to >0 if connection must remain opened */
	LDAP         *handle;

	gchar        *base_dn;
	gchar        *server_version;
	gchar        *url;
	GdaQuarkList *auth;

	int           time_limit;
	int           size_limit;

	GHashTable   *attributes_hash; /* key = attribute name, value = a #LdapAttribute */
	gchar        *attributes_cache_file;

	GSList       *top_classes; /* list of #LdapClass (no ref held) which have no parent */
	GHashTable   *classes_hash; /* key = class name, value = a #LdapClass */
} LdapConnectionData;

void     gda_ldap_may_unbind (GdaLdapConnection *cnc);
gboolean gda_ldap_ensure_bound (GdaLdapConnection *cnc, GError **error);
gboolean gda_ldap_rebind (GdaLdapConnection *cnc, GError **error);

#endif
