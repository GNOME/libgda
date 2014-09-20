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

#ifndef __QUERY_FAVORITE_SELECTOR_H__
#define __QUERY_FAVORITE_SELECTOR_H__

#include <gtk/gtk.h>
#include "../common/t-connection.h"

G_BEGIN_DECLS

#define QUERY_FAVORITE_SELECTOR_TYPE            (query_favorite_selector_get_type())
#define QUERY_FAVORITE_SELECTOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, QUERY_FAVORITE_SELECTOR_TYPE, QueryFavoriteSelector))
#define QUERY_FAVORITE_SELECTOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, QUERY_FAVORITE_SELECTOR_TYPE, QueryFavoriteSelectorClass))
#define IS_QUERY_FAVORITE_SELECTOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, QUERY_FAVORITE_SELECTOR_TYPE))
#define IS_QUERY_FAVORITE_SELECTOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), QUERY_FAVORITE_SELECTOR_TYPE))

typedef struct _QueryFavoriteSelector        QueryFavoriteSelector;
typedef struct _QueryFavoriteSelectorClass   QueryFavoriteSelectorClass;
typedef struct _QueryFavoriteSelectorPrivate QueryFavoriteSelectorPrivate;

struct _QueryFavoriteSelector {
	GtkBox               parent;
	QueryFavoriteSelectorPrivate *priv;
};

struct _QueryFavoriteSelectorClass {
	GtkBoxClass          parent_class;

	void                (*selection_changed) (QueryFavoriteSelector *sel, gint fav_id,
						  TFavoritesType fav_type, const gchar *fav_contents);
};

GType                    query_favorite_selector_get_type (void) G_GNUC_CONST;

GtkWidget               *query_favorite_selector_new      (TConnection *tcnc);

G_END_DECLS

#endif
