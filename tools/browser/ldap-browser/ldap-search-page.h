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

#ifndef __LDAP_SEARCH_PAGE_H__
#define __LDAP_SEARCH_PAGE_H__

#include <gtk/gtk.h>
#include "../browser-connection.h"

G_BEGIN_DECLS

#define LDAP_SEARCH_PAGE_TYPE            (ldap_search_page_get_type())
#define LDAP_SEARCH_PAGE(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, LDAP_SEARCH_PAGE_TYPE, LdapSearchPage))
#define LDAP_LDAP_SEARCH_PAGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, LDAP_SEARCH_PAGE_TYPE, LdapSearchPageClass))
#define IS_LDAP_LDAP_SEARCH_PAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, LDAP_SEARCH_PAGE_TYPE))
#define IS_LDAP_LDAP_LDAP_SEARCH_PAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), LDAP_SEARCH_PAGE_TYPE))

typedef struct _LdapSearchPage        LdapSearchPage;
typedef struct _LdapSearchPageClass   LdapSearchPageClass;
typedef struct _LdapSearchPagePrivate LdapSearchPagePrivate;

struct _LdapSearchPage {
	GtkVBox            parent;
	LdapSearchPagePrivate *priv;
};

struct _LdapSearchPageClass {
	GtkVBoxClass       parent_class;
};

GType        ldap_search_page_get_type       (void) G_GNUC_CONST;

GtkWidget   *ldap_search_page_new            (BrowserConnection *bcnc, const gchar *base_dn);

G_END_DECLS

#endif
