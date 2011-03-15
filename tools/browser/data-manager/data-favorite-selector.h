/*
 * Copyright (C) 2009 - 2010 The GNOME Foundation
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

#ifndef __DATA_FAVORITE_SELECTOR_H__
#define __DATA_FAVORITE_SELECTOR_H__

#include <gtk/gtk.h>
#include "../browser-connection.h"

G_BEGIN_DECLS

#define DATA_FAVORITE_SELECTOR_TYPE            (data_favorite_selector_get_type())
#define DATA_FAVORITE_SELECTOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, DATA_FAVORITE_SELECTOR_TYPE, DataFavoriteSelector))
#define DATA_FAVORITE_SELECTOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, DATA_FAVORITE_SELECTOR_TYPE, DataFavoriteSelectorClass))
#define IS_DATA_FAVORITE_SELECTOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, DATA_FAVORITE_SELECTOR_TYPE))
#define IS_DATA_FAVORITE_SELECTOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), DATA_FAVORITE_SELECTOR_TYPE))

typedef struct _DataFavoriteSelector        DataFavoriteSelector;
typedef struct _DataFavoriteSelectorClass   DataFavoriteSelectorClass;
typedef struct _DataFavoriteSelectorPrivate DataFavoriteSelectorPrivate;

struct _DataFavoriteSelector {
	GtkVBox               parent;
	DataFavoriteSelectorPrivate *priv;
};

struct _DataFavoriteSelectorClass {
	GtkVBoxClass          parent_class;

	void                (*selection_changed) (DataFavoriteSelector *sel, gint fav_id,
						  BrowserFavoritesType fav_type, const gchar *fav_contents);
};

GType                    data_favorite_selector_get_type (void) G_GNUC_CONST;

GtkWidget               *data_favorite_selector_new      (BrowserConnection *bcnc);

G_END_DECLS

#endif
