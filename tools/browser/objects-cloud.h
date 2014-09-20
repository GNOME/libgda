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

#ifndef __OBJECTS_CLOUD_H__
#define __OBJECTS_CLOUD_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OBJECTS_CLOUD_TYPE            (objects_cloud_get_type())
#define OBJECTS_CLOUD(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, OBJECTS_CLOUD_TYPE, ObjectsCloud))
#define OBJECTS_CLOUD_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, OBJECTS_CLOUD_TYPE, ObjectsCloudClass))
#define IS_OBJECTS_CLOUD(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, OBJECTS_CLOUD_TYPE))
#define IS_OBJECTS_CLOUD_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), OBJECTS_CLOUD_TYPE))

typedef struct _ObjectsCloud        ObjectsCloud;
typedef struct _ObjectsCloudClass   ObjectsCloudClass;
typedef struct _ObjectsCloudPrivate ObjectsCloudPrivate;

typedef enum {
	OBJECTS_CLOUD_TYPE_TABLE,
	OBJECTS_CLOUD_TYPE_VIEW,
} ObjectsCloudObjType;

struct _ObjectsCloud {
	GtkBox               parent;
	ObjectsCloudPrivate  *priv;
};

struct _ObjectsCloudClass {
	GtkBoxClass          parent_class;

	/* signals */
	void                (*selected) (ObjectsCloud *sel, ObjectsCloudObjType sel_type, 
					 const gchar *sel_contents);
};

GType                    objects_cloud_get_type (void) G_GNUC_CONST;

GtkWidget               *objects_cloud_new             (GdaMetaStruct *mstruct, ObjectsCloudObjType type);
void                     objects_cloud_set_meta_struct (ObjectsCloud *cloud, GdaMetaStruct *mstruct);
void                     objects_cloud_show_schemas    (ObjectsCloud *cloud, gboolean show_schemas);
void                     objects_cloud_filter          (ObjectsCloud *cloud, const gchar *filter);
GtkWidget               *objects_cloud_create_filter   (ObjectsCloud *cloud);

G_END_DECLS

#endif
