/*
 * Copyright (C) 2009 - 2011 Vivien Malerba <malerba@gnome-db.org>
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


#ifndef __TOOLS_FAVORITES_H_
#define __TOOLS_FAVORITES_H_

#include <libgda/libgda.h>

G_BEGIN_DECLS

#define TOOLS_TYPE_FAVORITES          (tools_favorites_get_type())
#define TOOLS_FAVORITES(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, tools_favorites_get_type(), ToolsFavorites)
#define TOOLS_FAVORITES_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, tools_favorites_get_type (), ToolsFavoritesClass)
#define TOOLS_IS_FAVORITES(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, tools_favorites_get_type ())

typedef struct _ToolsFavorites ToolsFavorites;
typedef struct _ToolsFavoritesClass ToolsFavoritesClass;
typedef struct _ToolsFavoritesPrivate ToolsFavoritesPrivate;

/**
 * ToolsFavoritesType:
 * @TOOLS_FAVORITES_TABLES: a database's table favorite
 * @TOOLS_FAVORITES_DIAGRAMS: a diagram favorite
 * @TOOLS_FAVORITES_QUERIES:
 * @TOOLS_FAVORITES_DATA_MANAGERS:
 * @TOOLS_FAVORITES_ACTIONS:
 *
 * Enum to identify favorite's types.
 */
typedef enum {
        TOOLS_FAVORITES_TABLES   = 1 << 0,
	TOOLS_FAVORITES_DIAGRAMS = 1 << 1,
	TOOLS_FAVORITES_QUERIES  = 1 << 2,
	TOOLS_FAVORITES_DATA_MANAGERS  = 1 << 3,
	TOOLS_FAVORITES_ACTIONS  = 1 << 4,
	TOOLS_FAVORITES_LDAP_DN  = 1 << 5,
	TOOLS_FAVORITES_LDAP_CLASS = 1 << 6
} ToolsFavoritesType;
#define TOOLS_FAVORITES_NB_TYPES 7

#define ORDER_KEY_SCHEMA 1
#define ORDER_KEY_QUERIES 2
#define ORDER_KEY_DATA_MANAGERS 3
#define ORDER_KEY_LDAP 4

/**
 * ToolsFavoritesAttributes:
 * @id: the favorite ID, or <0 if not saved
 * @type: the favorite's type
 * @name: the favorite's name
 * @descr: the favorite's description
 * @contents: the favorite's contents, depending on the favorite type
 */
typedef struct {
	gint                  id;
	ToolsFavoritesType    type;
	gchar                *name;
	gchar                *descr;
	gchar                *contents;
} ToolsFavoritesAttributes;

/* struct for the object's data */
struct _ToolsFavorites
{
	GObject                  object;
	ToolsFavoritesPrivate *priv;
};

/* struct for the object's class */
struct _ToolsFavoritesClass
{
	GObjectClass              parent_class;

	void                    (*favorites_changed) (ToolsFavorites *bfav);
};

/**
 * SECTION:tools-favorites
 * @short_description: Favorites management
 * @title: ToolsFavorites
 * @stability: Stable
 * @see_also:
 *
 * Each connection uses a single #ToolsFavorites object to manage its favorites,
 * see tools_connection_get_favorites().
 */

GType               tools_favorites_get_type               (void) G_GNUC_CONST;

ToolsFavorites     *tools_favorites_new                    (GdaMetaStore *store);
const gchar        *tools_favorites_type_to_string (ToolsFavoritesType type);
gboolean            tools_favorites_add          (ToolsFavorites *bfav, guint session_id,
						  ToolsFavoritesAttributes *fav,
						  gint order_key, gint pos,
						  GError **error);
GSList             *tools_favorites_list             (ToolsFavorites *bfav, guint session_id, 
						      ToolsFavoritesType type, gint order_key, GError **error);

gboolean            tools_favorites_delete           (ToolsFavorites *bfav, guint session_id,
						      ToolsFavoritesAttributes *fav, GError **error);
void                tools_favorites_free_list        (GSList *fav_list);
void                tools_favorites_reset_attributes (ToolsFavoritesAttributes *fav);

gint                tools_favorites_find             (ToolsFavorites *bfav, guint session_id, const gchar *contents,
						      ToolsFavoritesAttributes *out_fav, GError **error);
gint                tools_favorites_find_by_name     (ToolsFavorites *bfav, guint session_id,
						      ToolsFavoritesType type, const gchar *name,
						      ToolsFavoritesAttributes *out_fav, GError **error);
gboolean            tools_favorites_get              (ToolsFavorites *bfav, gint fav_id,
						      ToolsFavoritesAttributes *out_fav, GError **error);

G_END_DECLS

#endif
