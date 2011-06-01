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

#ifndef __LDAP_FAVORITE_SELECTOR_H__
#define __LDAP_FAVORITE_SELECTOR_H__

#include <gtk/gtk.h>
#include "../browser-connection.h"

G_BEGIN_DECLS

#define LDAP_FAVORITE_SELECTOR_TYPE            (ldap_favorite_selector_get_type())
#define LDAP_FAVORITE_SELECTOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, LDAP_FAVORITE_SELECTOR_TYPE, LdapFavoriteSelector))
#define LDAP_FAVORITE_SELECTOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, LDAP_FAVORITE_SELECTOR_TYPE, LdapFavoriteSelectorClass))
#define IS_LDAP_FAVORITE_SELECTOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, LDAP_FAVORITE_SELECTOR_TYPE))
#define IS_LDAP_FAVORITE_SELECTOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), LDAP_FAVORITE_SELECTOR_TYPE))

typedef struct _LdapFavoriteSelector        LdapFavoriteSelector;
typedef struct _LdapFavoriteSelectorClass   LdapFavoriteSelectorClass;
typedef struct _LdapFavoriteSelectorPrivate LdapFavoriteSelectorPrivate;

struct _LdapFavoriteSelector {
	GtkVBox               parent;
	LdapFavoriteSelectorPrivate *priv;
};

struct _LdapFavoriteSelectorClass {
	GtkVBoxClass          parent_class;

	void                (*selection_changed) (LdapFavoriteSelector *sel, gint fav_id,
						  BrowserFavoritesType fav_type, const gchar *fav_contents);
};

GType                    ldap_favorite_selector_get_type (void) G_GNUC_CONST;

GtkWidget               *ldap_favorite_selector_new      (BrowserConnection *bcnc);

G_END_DECLS

#endif
