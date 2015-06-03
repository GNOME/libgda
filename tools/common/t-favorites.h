/*
 * Copyright (C) 2014 Vivien Malerba <malerba@gnome-db.org>
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


#ifndef __T_FAVORITES_H_
#define __T_FAVORITES_H_

#include <libgda/libgda.h>

G_BEGIN_DECLS

#define T_TYPE_FAVORITES          (t_favorites_get_type())
#define T_FAVORITES(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, t_favorites_get_type(), TFavorites)
#define T_FAVORITES_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, t_favorites_get_type (), TFavoritesClass)
#define T_IS_FAVORITES(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, t_favorites_get_type ())

typedef struct _TFavorites TFavorites;
typedef struct _TFavoritesClass TFavoritesClass;
typedef struct _TFavoritesPrivate TFavoritesPrivate;

/**
 * TFavoritesType:
 * @T_FAVORITES_TABLES: a database's table favorite
 * @T_FAVORITES_DIAGRAMS: a diagram favorite
 * @T_FAVORITES_QUERIES:
 * @T_FAVORITES_DATA_MANAGERS:
 * @T_FAVORITES_ACTIONS:
 *
 * Enum to identify favorite's types.
 */
typedef enum {
        T_FAVORITES_TABLES   = 1 << 0,
	T_FAVORITES_DIAGRAMS = 1 << 1,
	T_FAVORITES_QUERIES  = 1 << 2,
	T_FAVORITES_DATA_MANAGERS  = 1 << 3,
	T_FAVORITES_ACTIONS  = 1 << 4,
	T_FAVORITES_LDAP_DN  = 1 << 5,
	T_FAVORITES_LDAP_CLASS = 1 << 6
} TFavoritesType;
#define T_FAVORITES_NB_TYPES 7

#define ORDER_KEY_SCHEMA 1
#define ORDER_KEY_QUERIES 2
#define ORDER_KEY_DATA_MANAGERS 3
#define ORDER_KEY_LDAP 4

/**
 * TFavoritesAttributes:
 * @id: the favorite ID, or <0 if not saved
 * @type: the favorite's type
 * @name: the favorite's name
 * @descr: the favorite's description
 * @contents: the favorite's contents, depending on the favorite type
 */
typedef struct {
	gint               id;
	TFavoritesType     type;
	gchar             *name;
	gchar             *descr;
	gchar             *contents;
} TFavoritesAttributes;

/* struct for the object's data */
struct _TFavorites
{
	GObject            object;
	TFavoritesPrivate *priv;
};

/* struct for the object's class */
struct _TFavoritesClass
{
	GObjectClass              parent_class;

	void                    (*favorites_changed) (TFavorites *bfav);
};

/**
 * SECTION:tools-favorites
 * @short_description: Favorites management
 * @title: TFavorites
 * @stability: Stable
 * @see_also:
 *
 * Each connection uses a single #TFavorites object to manage its favorites,
 * see gda_tools_connection_get_favorites().
 */

GType               t_favorites_get_type               (void) G_GNUC_CONST;

TFavorites     *t_favorites_new                    (GdaMetaStore *store);
const gchar        *t_favorites_type_to_string (TFavoritesType type);
gboolean            t_favorites_add          (TFavorites *bfav, guint session_id,
						  TFavoritesAttributes *fav,
						  gint order_key, gint pos,
						  GError **error);
GSList             *t_favorites_list             (TFavorites *bfav, guint session_id, 
						      TFavoritesType type, gint order_key, GError **error);

gboolean            t_favorites_delete           (TFavorites *bfav, guint session_id,
						      TFavoritesAttributes *fav, GError **error);
void                t_favorites_free_list        (GSList *fav_list);
void                t_favorites_reset_attributes (TFavoritesAttributes *fav);

gint                t_favorites_find             (TFavorites *bfav, guint session_id, const gchar *contents,
						      TFavoritesAttributes *out_fav, GError **error);
gint                t_favorites_find_by_name     (TFavorites *bfav, guint session_id,
						      TFavoritesType type, const gchar *name,
						      TFavoritesAttributes *out_fav, GError **error);
gboolean            t_favorites_get              (TFavorites *bfav, gint fav_id,
						      TFavoritesAttributes *out_fav, GError **error);

G_END_DECLS

#endif
