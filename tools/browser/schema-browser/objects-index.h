/*
 * Copyright (C) 2009 The GNOME Foundation
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

#ifndef __OBJECTS_INDEX_H__
#define __OBJECTS_INDEX_H__

#include <gtk/gtkvbox.h>
#include "../browser-connection.h"

G_BEGIN_DECLS

#define OBJECTS_INDEX_TYPE            (objects_index_get_type())
#define OBJECTS_INDEX(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, OBJECTS_INDEX_TYPE, ObjectsIndex))
#define OBJECTS_INDEX_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, OBJECTS_INDEX_TYPE, ObjectsIndexClass))
#define IS_OBJECTS_INDEX(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, OBJECTS_INDEX_TYPE))
#define IS_OBJECTS_INDEX_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), OBJECTS_INDEX_TYPE))

typedef struct _ObjectsIndex        ObjectsIndex;
typedef struct _ObjectsIndexClass   ObjectsIndexClass;
typedef struct _ObjectsIndexPrivate ObjectsIndexPrivate;

struct _ObjectsIndex {
	GtkVBox               parent;
	ObjectsIndexPrivate  *priv;
};

struct _ObjectsIndexClass {
	GtkVBoxClass          parent_class;

	/* signals */
	void                (*selection_changed) (ObjectsIndex *sel,
						  BrowserFavoritesType fav_type, const gchar *fav_contents);
};

GType                    objects_index_get_type (void) G_GNUC_CONST;

GtkWidget               *objects_index_new      (BrowserConnection *bcnc);

G_END_DECLS

#endif
