/*
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2011 - 2012 Vivien Malerba <malerba@gnome-db.org>
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


#ifndef __LDAP_BROWSER_PERSPECTIVE_H_
#define __LDAP_BROWSER_PERSPECTIVE_H_

#include <glib-object.h>
#include "../browser-perspective.h"

G_BEGIN_DECLS

#define TYPE_LDAP_BROWSER_PERSPECTIVE          (ldap_browser_perspective_get_type())
#define LDAP_BROWSER_PERSPECTIVE(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, ldap_browser_perspective_get_type(), LdapBrowserPerspective)
#define LDAP_BROWSER_PERSPECTIVE_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, ldap_browser_perspective_get_type (), LdapBrowserPerspectiveClass)
#define IS_LDAP_BROWSER_PERSPECTIVE(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, ldap_browser_perspective_get_type ())

typedef struct _LdapBrowserPerspective LdapBrowserPerspective;
typedef struct _LdapBrowserPerspectiveClass LdapBrowserPerspectiveClass;
typedef struct _LdapBrowserPerspectivePrivate LdapBrowserPerspectivePrivate;

/* struct for the object's data */
struct _LdapBrowserPerspective
{
	GtkBox              object;
	LdapBrowserPerspectivePrivate *priv;
};

/* struct for the object's class */
struct _LdapBrowserPerspectiveClass
{
	GtkBoxClass         parent_class;
};

/**
 * SECTION:ldap-browser-perspective
 * @short_description: Perspective to analyse the database's ldap
 * @title: Ldap Browser perspective
 * @stability: Stable
 * @see_also:
 */

GType                ldap_browser_perspective_get_type               (void) G_GNUC_CONST;
BrowserPerspective  *ldap_browser_perspective_new                    (BrowserWindow *bwin);

void                 ldap_browser_perspective_display_ldap_entry     (LdapBrowserPerspective *bpers,
								      const gchar *dn);
void                 ldap_browser_perspective_display_ldap_class     (LdapBrowserPerspective *bpers,
								      const gchar *classname);

G_END_DECLS

#endif
