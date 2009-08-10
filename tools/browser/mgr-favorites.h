/* 
 * Copyright (C) 2009 The GNOME Foundation.
 *
 * AUTHORS:
 *      Vivien Malerba <malerba@gnome-db.org>
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

#ifndef ___MGR_FAVORITES_H__
#define ___MGR_FAVORITES_H__

#include <libgda/gda-tree-manager.h>
#include "browser-connection.h"

G_BEGIN_DECLS

#define MGR_FAVORITES_TYPE            (mgr_favorites_get_type())
#define MGR_FAVORITES(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, MGR_FAVORITES_TYPE, MgrFavorites))
#define MGR_FAVORITES_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, MGR_FAVORITES_TYPE, MgrFavoritesClass))
#define IS_MGR_FAVORITES(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, MGR_FAVORITES_TYPE))
#define IS_MGR_FAVORITES_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), MGR_FAVORITES_TYPE))

typedef struct _MgrFavorites MgrFavorites;
typedef struct _MgrFavoritesPriv MgrFavoritesPriv;
typedef struct _MgrFavoritesClass MgrFavoritesClass;

struct _MgrFavorites {
	GdaTreeManager      object;
	MgrFavoritesPriv     *priv;
};

struct _MgrFavoritesClass {
	GdaTreeManagerClass object_class;
};

GType           mgr_favorites_get_type                 (void) G_GNUC_CONST;
GdaTreeManager* mgr_favorites_new                      (BrowserConnection *bcnc, BrowserFavoritesType type);

/* name of the attribute which stores the favorite's contents */
#define MGR_FAVORITES_CONTENTS_ATT_NAME "fav_contents"
/* name of the attribute which stores the favorite's id */
#define MGR_FAVORITES_ID_ATT_NAME "fav_id"
/* name of the attribute which stores the favorite type */
#define MGR_FAVORITES_TYPE_ATT_NAME "fav_type"

G_END_DECLS

#endif
