/*
 * Copyright (C) 2011 The GNOME Foundation
 *
 * AUTHORS:
 *      Vivien Malerba <malerba@gnome-db.org>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __LDAP_CLASSES_PAGE_H__
#define __LDAP_CLASSES_PAGE_H__

#include <gtk/gtk.h>
#include "../browser-connection.h"

G_BEGIN_DECLS

#define LDAP_CLASSES_PAGE_TYPE            (ldap_classes_page_get_type())
#define LDAP_CLASSES_PAGE(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, LDAP_CLASSES_PAGE_TYPE, LdapClassesPage))
#define LDAP_CLASSES_PAGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, LDAP_CLASSES_PAGE_TYPE, LdapClassesPageClass))
#define IS_LDAP_CLASSES_PAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, LDAP_CLASSES_PAGE_TYPE))
#define IS_LDAP_CLASSES_PAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), LDAP_CLASSES_PAGE_TYPE))

typedef struct _LdapClassesPage        LdapClassesPage;
typedef struct _LdapClassesPageClass   LdapClassesPageClass;
typedef struct _LdapClassesPagePrivate LdapClassesPagePrivate;

struct _LdapClassesPage {
	GtkVBox                parent;
	LdapClassesPagePrivate *priv;
};

struct _LdapClassesPageClass {
	GtkVBoxClass           parent_class;
};

GType        ldap_classes_page_get_type       (void) G_GNUC_CONST;

GtkWidget   *ldap_classes_page_new            (BrowserConnection *bcnc, const gchar *dn);
const gchar *ldap_classes_page_get_current_class (LdapClassesPage *ldap_classes_page);
void         ldap_classes_page_set_current_class (LdapClassesPage *ldap_classes_page, const gchar *classname);

G_END_DECLS

#endif
