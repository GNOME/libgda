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

#ifndef __OBJECTS_INDEX_H__
#define __OBJECTS_INDEX_H__

#include <gtk/gtk.h>
#include "common/t-connection.h"

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
	GtkBox               parent;
	ObjectsIndexPrivate  *priv;
};

struct _ObjectsIndexClass {
	GtkBoxClass          parent_class;

	/* signals */
	void                (*selection_changed) (ObjectsIndex *sel,
						  TFavoritesType fav_type, const gchar *fav_contents);
};

GType                    objects_index_get_type (void) G_GNUC_CONST;

GtkWidget               *objects_index_new      (TConnection *tcnc);
void                     objects_index_update   (ObjectsIndex *index);

G_END_DECLS

#endif
