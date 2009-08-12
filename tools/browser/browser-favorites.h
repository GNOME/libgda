/* 
 * Copyright (C) 2009 Vivien Malerba
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
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


#ifndef __BROWSER_FAVORITES_H_
#define __BROWSER_FAVORITES_H_

#include <libgda/libgda.h>
#include "decl.h"

G_BEGIN_DECLS

#define BROWSER_TYPE_FAVORITES          (browser_favorites_get_type())
#define BROWSER_FAVORITES(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, browser_favorites_get_type(), BrowserFavorites)
#define BROWSER_FAVORITES_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, browser_favorites_get_type (), BrowserFavoritesClass)
#define BROWSER_IS_FAVORITES(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, browser_favorites_get_type ())

typedef struct _BrowserFavorites BrowserFavorites;
typedef struct _BrowserFavoritesClass BrowserFavoritesClass;
typedef struct _BrowserFavoritesPrivate BrowserFavoritesPrivate;

typedef enum {
        BROWSER_FAVORITES_TABLES   = 1 << 0,
	BROWSER_FAVORITES_DIAGRAMS = 1 << 1
} BrowserFavoritesType;
#define BROWSER_FAVORITES_NB_TYPES 2

typedef struct {
	gint                  id;
	BrowserFavoritesType  type;
	gchar                *name;
	gchar                *descr;
	gchar                *contents;
} BrowserFavoritesAttributes;

/* struct for the object's data */
struct _BrowserFavorites
{
	GObject                  object;
	BrowserFavoritesPrivate *priv;
};

/* struct for the object's class */
struct _BrowserFavoritesClass
{
	GObjectClass              parent_class;

	void                    (*favorites_changed) (BrowserFavorites *bfav);
};

GType               browser_favorites_get_type               (void) G_GNUC_CONST;

BrowserFavorites   *browser_favorites_new                    (GdaMetaStore *store);

gboolean            browser_favorites_add          (BrowserFavorites *bfav, guint session_id,
						    BrowserFavoritesAttributes *fav,
						    gint order_key, gint pos,
						    GError **error);
GSList             *browser_favorites_list         (BrowserFavorites *bfav, guint session_id, 
						    BrowserFavoritesType type, gint order_key, GError **error);
gboolean            browser_favorites_delete       (BrowserFavorites *bfav, guint session_id,
						    BrowserFavoritesAttributes *fav, GError **error);
void                browser_favorites_free_list    (GSList *fav_list);
void                browser_favorites_reset_attributes (BrowserFavoritesAttributes *fav);

gboolean            browser_favorites_get          (BrowserFavorites *bfav, gint fav_id,
						    BrowserFavoritesAttributes *out_fav, GError **error);
G_END_DECLS

#endif
