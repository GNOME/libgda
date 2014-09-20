/*
 * Copyright (C) 2009 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
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

#ifndef __FAVORITE_SELECTOR_H__
#define __FAVORITE_SELECTOR_H__

#include <gtk/gtk.h>
#include <t-connection.h>
#include <t-favorites.h>

G_BEGIN_DECLS

#define FAVORITE_SELECTOR_TYPE            (favorite_selector_get_type())
#define FAVORITE_SELECTOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, FAVORITE_SELECTOR_TYPE, FavoriteSelector))
#define FAVORITE_SELECTOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, FAVORITE_SELECTOR_TYPE, FavoriteSelectorClass))
#define IS_FAVORITE_SELECTOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, FAVORITE_SELECTOR_TYPE))
#define IS_FAVORITE_SELECTOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FAVORITE_SELECTOR_TYPE))

typedef struct _FavoriteSelector        FavoriteSelector;
typedef struct _FavoriteSelectorClass   FavoriteSelectorClass;
typedef struct _FavoriteSelectorPrivate FavoriteSelectorPrivate;

struct _FavoriteSelector {
	GtkBox                   parent;
	FavoriteSelectorPrivate *priv;
};

struct _FavoriteSelectorClass {
	GtkBoxClass              parent_class;

	void                   (*selection_changed) (FavoriteSelector *sel, gint fav_id,
						     TFavoritesType fav_type, const gchar *fav_contents);
};

GType                    favorite_selector_get_type (void) G_GNUC_CONST;

GtkWidget               *favorite_selector_new      (TConnection *tcnc);

G_END_DECLS

#endif
