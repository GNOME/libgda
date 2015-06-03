/*
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
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

#ifndef __LDAP_SEARCH_PAGE_H__
#define __LDAP_SEARCH_PAGE_H__

#include <gtk/gtk.h>
#include "common/t-connection.h"

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
	GtkBox            parent;
	LdapSearchPagePrivate *priv;
};

struct _LdapSearchPageClass {
	GtkBoxClass       parent_class;
};

GType        ldap_search_page_get_type       (void) G_GNUC_CONST;

GtkWidget   *ldap_search_page_new            (TConnection *tcnc, const gchar *base_dn);

G_END_DECLS

#endif
