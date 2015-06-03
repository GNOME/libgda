/*
 * Copyright (C) 2010 - 2012 Vivien Malerba <malerba@gnome-db.org>
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


#ifndef __DATA_MANAGER_PERSPECTIVE_H_
#define __DATA_MANAGER_PERSPECTIVE_H_

#include <glib-object.h>
#include "../browser-perspective.h"

G_BEGIN_DECLS

#define TYPE_DATA_MANAGER_PERSPECTIVE          (data_manager_perspective_get_type())
#define DATA_MANAGER_PERSPECTIVE(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, data_manager_perspective_get_type(), DataManagerPerspective)
#define DATA_MANAGER_PERSPECTIVE_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, data_manager_perspective_get_type (), DataManagerPerspectiveClass)
#define IS_DATA_MANAGER_PERSPECTIVE(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, data_manager_perspective_get_type ())

typedef struct _DataManagerPerspective DataManagerPerspective;
typedef struct _DataManagerPerspectiveClass DataManagerPerspectiveClass;
typedef struct _DataManagerPerspectivePriv DataManagerPerspectivePriv;

/* struct for the object's data */
struct _DataManagerPerspective
{
	GtkBox                     object;
	DataManagerPerspectivePriv *priv;
};

/* struct for the object's class */
struct _DataManagerPerspectiveClass
{
	GtkBoxClass                parent_class;
};

/**
 * SECTION:data-manager-perspective
 * @short_description: Perspective to manipulate data contained in the database
 * @title: Data Manager perspective
 * @stability: Stable
 * @see_also:
 */

GType                data_manager_perspective_get_type (void) G_GNUC_CONST;
BrowserPerspective  *data_manager_perspective_new      (BrowserWindow *bwin);
void                 data_manager_perspective_new_tab  (DataManagerPerspective *dmp, const gchar *xml_spec);

G_END_DECLS

#endif
