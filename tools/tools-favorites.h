/*
 * Copyright (C) 2012 Vivien Malerba <malerba@gnome-db.org>
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


#ifndef __GDA_TOOLS_FAVORITES_H_
#define __GDA_TOOLS_FAVORITES_H_

#include <libgda/libgda.h>

G_BEGIN_DECLS

#define GDA_TOOLS_TYPE_FAVORITES          (gda_tools_favorites_get_type())
#define GDA_TOOLS_FAVORITES(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_tools_favorites_get_type(), ToolsFavorites)
#define GDA_TOOLS_FAVORITES_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_tools_favorites_get_type (), ToolsFavoritesClass)
#define GDA_TOOLS_IS_FAVORITES(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_tools_favorites_get_type ())

typedef struct _ToolsFavorites ToolsFavorites;
typedef struct _ToolsFavoritesClass ToolsFavoritesClass;
typedef struct _ToolsFavoritesPrivate ToolsFavoritesPrivate;

/**
 * ToolsFavoritesType:
 * @GDA_TOOLS_FAVORITES_TABLES: a database's table favorite
 * @GDA_TOOLS_FAVORITES_DIAGRAMS: a diagram favorite
 * @GDA_TOOLS_FAVORITES_QUERIES:
 * @GDA_TOOLS_FAVORITES_DATA_MANAGERS:
 * @GDA_TOOLS_FAVORITES_ACTIONS:
 *
 * Enum to identify favorite's types.
 */
typedef enum {
        GDA_TOOLS_FAVORITES_TABLES   = 1 << 0,
	GDA_TOOLS_FAVORITES_DIAGRAMS = 1 << 1,
	GDA_TOOLS_FAVORITES_QUERIES  = 1 << 2,
	GDA_TOOLS_FAVORITES_DATA_MANAGERS  = 1 << 3,
	GDA_TOOLS_FAVORITES_ACTIONS  = 1 << 4,
	GDA_TOOLS_FAVORITES_LDAP_DN  = 1 << 5,
	GDA_TOOLS_FAVORITES_LDAP_CLASS = 1 << 6
} ToolsFavoritesType;
#define GDA_TOOLS_FAVORITES_NB_TYPES 7

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
 * see gda_tools_connection_get_favorites().
 */

GType               gda_tools_favorites_get_type               (void) G_GNUC_CONST;

ToolsFavorites     *gda_tools_favorites_new                    (GdaMetaStore *store);
const gchar        *gda_tools_favorites_type_to_string (ToolsFavoritesType type);
gboolean            gda_tools_favorites_add          (ToolsFavorites *bfav, guint session_id,
						  ToolsFavoritesAttributes *fav,
						  gint order_key, gint pos,
						  GError **error);
GSList             *gda_tools_favorites_list             (ToolsFavorites *bfav, guint session_id, 
						      ToolsFavoritesType type, gint order_key, GError **error);

gboolean            gda_tools_favorites_delete           (ToolsFavorites *bfav, guint session_id,
						      ToolsFavoritesAttributes *fav, GError **error);
void                gda_tools_favorites_free_list        (GSList *fav_list);
void                gda_tools_favorites_reset_attributes (ToolsFavoritesAttributes *fav);

gint                gda_tools_favorites_find             (ToolsFavorites *bfav, guint session_id, const gchar *contents,
						      ToolsFavoritesAttributes *out_fav, GError **error);
gint                gda_tools_favorites_find_by_name     (ToolsFavorites *bfav, guint session_id,
						      ToolsFavoritesType type, const gchar *name,
						      ToolsFavoritesAttributes *out_fav, GError **error);
gboolean            gda_tools_favorites_get              (ToolsFavorites *bfav, gint fav_id,
						      ToolsFavoritesAttributes *out_fav, GError **error);

G_END_DECLS

#endif
