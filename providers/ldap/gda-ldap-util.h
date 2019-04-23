/*
 * Copyright (C) 2011 - 2014 Vivien Malerba <malerba@gnome-db.org>
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

/*
 * Some data copied from GQ's sources and transformed
 */

#ifndef __GDA_LDAP_UTIL_H__
#define __GDA_LDAP_UTIL_H__

#include <glib.h>
#include "gda-ldap.h"
#include "gda-ldap-connection.h"

/*
 * Attributes
 */
typedef struct {
	gchar *oid;
	gchar *descr;
	GType  gtype;
  gint  gtype_custom;
} LdapAttrType;

typedef struct {
	gchar        *name;
	LdapAttrType *type; /* never NULL */
	gboolean      single_value;
} LdapAttribute;

LdapAttrType  *gda_ldap_get_type_info (const gchar *oid);
LdapAttribute *gda_ldap_get_attr_info (GdaLdapConnection *cnc, LdapConnectionData *cdata, const gchar *attribute);
GType          gda_ldap_get_g_type    (GdaLdapConnection *cnc, LdapConnectionData *cdata, const gchar *attribute, const gchar *specified_gtype);

/*
 * Misc.
 */
GValue        *gda_ldap_attr_value_to_g_value (LdapConnectionData *cdata, GType type, BerValue *bv);
BerValue      *gda_ldap_attr_g_value_to_value (LdapConnectionData *cdata, const GValue *cvalue);
void           gda_ldap_attr_value_free (LdapConnectionData *cdata, BerValue *bvalue);

gboolean       gda_ldap_parse_dn (const char *attr, gchar **out_userdn);

gboolean       gdaprov_ldap_is_dn (const gchar *dn);

/*
 * Debug purposes
 */
void           gda_ldap_execution_slowdown (GdaLdapConnection *cnc);

#endif
