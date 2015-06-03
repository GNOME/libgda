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

#ifndef __MGR_LDAP_CLASSES_H__
#define __MGR_LDAP_CLASSES_H__

#include "common/t-connection.h"
#include "gda-tree-manager.h"

G_BEGIN_DECLS

#define MGR_TYPE_LDAP_CLASSES            (mgr_ldap_classes_get_type())
#define MGR_LDAP_CLASSES(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, MGR_TYPE_LDAP_CLASSES, MgrLdapClasses))
#define MGR_LDAP_CLASSES_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, MGR_TYPE_LDAP_CLASSES, MgrLdapClassesClass))
#define MGR_IS_LDAP_CLASSES(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, MGR_TYPE_LDAP_CLASSES))
#define MGR_IS_LDAP_CLASSES_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), MGR_TYPE_LDAP_CLASSES))
#define MGR_LDAP_CLASSES_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), MGR_TYPE_LDAP_CLASSES, MgrLdapClassesClass))

typedef struct _MgrLdapClasses MgrLdapClasses;
typedef struct _MgrLdapClassesPriv MgrLdapClassesPriv;
typedef struct _MgrLdapClassesClass MgrLdapClassesClass;

struct _MgrLdapClasses {
	GdaTreeManager      object;
	MgrLdapClassesPriv *priv;
};

struct _MgrLdapClassesClass {
	GdaTreeManagerClass object_class;
};

GType              mgr_ldap_classes_get_type  (void) G_GNUC_CONST;
GdaTreeManager*    mgr_ldap_classes_new       (TConnection *tcnc, gboolean flat, const gchar *classname);

G_END_DECLS

#endif
