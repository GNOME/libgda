/*
 * Copyright (C) 2011 - 2014 Vivien Malerba <malerba@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __MGR_LDAP_ENTRIES_H__
#define __MGR_LDAP_ENTRIES_H__

#include "common/t-connection.h"
#include <libgda/gda-tree-manager.h>

G_BEGIN_DECLS

#define MGR_TYPE_LDAP_ENTRIES            (mgr_ldap_entries_get_type())
#define MGR_LDAP_ENTRIES(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, MGR_TYPE_LDAP_ENTRIES, MgrLdapEntries))
#define MGR_LDAP_ENTRIES_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, MGR_TYPE_LDAP_ENTRIES, MgrLdapEntriesClass))
#define MGR_IS_LDAP_ENTRIES(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, MGR_TYPE_LDAP_ENTRIES))
#define MGR_IS_LDAP_ENTRIES_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), MGR_TYPE_LDAP_ENTRIES))
#define MGR_LDAP_ENTRIES_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), MGR_TYPE_LDAP_ENTRIES, MgrLdapEntriesClass))

typedef struct _MgrLdapEntries MgrLdapEntries;
typedef struct _MgrLdapEntriesPriv MgrLdapEntriesPriv;
typedef struct _MgrLdapEntriesClass MgrLdapEntriesClass;

struct _MgrLdapEntries {
	GdaTreeManager          object;
	MgrLdapEntriesPriv *priv;
};

struct _MgrLdapEntriesClass {
	GdaTreeManagerClass     object_class;
};

GType           mgr_ldap_entries_get_type  (void) G_GNUC_CONST;
GdaTreeManager* mgr_ldap_entries_new       (TConnection *tcnc, const gchar *dn);

G_END_DECLS

#endif
