/*
 * Copyright (C) 2011 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_TREE_MGR_LDAP_H__
#define __GDA_TREE_MGR_LDAP_H__

#include <libgda/gda-connection.h>
#include "gda-tree-manager.h"

G_BEGIN_DECLS

#define GDA_TYPE_TREE_MGR_LDAP            (gda_tree_mgr_ldap_get_type())
G_DECLARE_DERIVABLE_TYPE (GdaTreeMgrLdap, gda_tree_mgr_ldap, GDA, TREE_MGR_LDAP, GdaTreeManager)
struct _GdaTreeMgrLdapClass {
	GdaTreeManagerClass object_class;
};

/**
 * SECTION:gda-tree-mgr-ldap
 * @short_description: A tree manager which creates a node for each child entry of an LDAP entry
 * @title: GdaTreeMgrLdap
 * @stability: Stable
 * @see_also:
 *
 * The #GdaTreeMgrLdap is a #GdaTreeManager object which creates a node for
 * each child entry of an LDAP entry.
 *
 * Note: this type of tree manager is available only if the LDAP library was found at compilation time and
 * if the LDAP provider is correctly installed.
 */

GdaTreeManager*    gda_tree_mgr_ldap_new       (GdaConnection *cnc, const gchar *dn);

G_END_DECLS

#endif
