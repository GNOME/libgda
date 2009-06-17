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


#ifndef __BROWSER_CONNECTION_H_
#define __BROWSER_CONNECTION_H_

#include <libgda/libgda.h>
#include "decl.h"

G_BEGIN_DECLS

#define BROWSER_TYPE_CONNECTION          (browser_connection_get_type())
#define BROWSER_CONNECTION(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, browser_connection_get_type(), BrowserConnection)
#define BROWSER_CONNECTION_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, browser_connection_get_type (), BrowserConnectionClass)
#define BROWSER_IS_CONNECTION(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, browser_connection_get_type ())

typedef struct _BrowserConnectionClass BrowserConnectionClass;
typedef struct _BrowserConnectionPrivate BrowserConnectionPrivate;

/* struct for the object's data */
struct _BrowserConnection
{
	GObject                   object;
	BrowserConnectionPrivate *priv;
};

/* struct for the object's class */
struct _BrowserConnectionClass
{
	GObjectClass              parent_class;

	/* signals */
	void                    (*busy) (BrowserConnection *bcnc, gboolean is_busy, const gchar *reason);
	void                    (*meta_changed) (BrowserConnection *bcnc, GdaMetaStruct *mstruct);
	void                    (*favorites_changed) (BrowserConnection *bcnc);
};

GType               browser_connection_get_type               (void) G_GNUC_CONST;

BrowserConnection  *browser_connection_new                    (GdaConnection *cnc);
const gchar        *browser_connection_get_name               (BrowserConnection *bcnc);
const GdaDsnInfo   *browser_connection_get_information        (BrowserConnection *bcnc);

gboolean            browser_connection_is_busy                (BrowserConnection *bcnc, gchar **out_reason);
void                browser_connection_update_meta_data       (BrowserConnection *bcnc);
GdaMetaStruct      *browser_connection_get_meta_struct        (BrowserConnection *bcnc);
GdaMetaStore       *browser_connection_get_meta_store         (BrowserConnection *bcnc);
const gchar        *browser_connection_get_dictionary_file    (BrowserConnection *bcnc);

/*
 * statements's execution
 */


/*
 * Favorites management
 */
typedef struct {
	BrowserFavoritesType  type;
	gchar                *descr;
	gchar                *contents;
} BrowserConnectionFavorite;
gboolean            browser_connection_add_favorite           (BrowserConnection *bcnc, guint session_id,
							       BrowserConnectionFavorite *fav, gint pos,
							       GError **error);
GSList             *browser_connection_list_favorites         (BrowserConnection *bcnc, guint session_id, 
							       BrowserFavoritesType type, GError **error);
gboolean            browser_connection_delete_favorite        (BrowserConnection *bcnc, guint session_id,
							       BrowserConnectionFavorite *fav, GError **error);
void                browser_connection_free_favorites_list    (GSList *fav_list);
G_END_DECLS

#endif
