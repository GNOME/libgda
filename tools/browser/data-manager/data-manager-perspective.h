/* 
 * Copyright (C) 2009 - 2010 Vivien Malerba
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
	GtkVBox                     object;
	DataManagerPerspectivePriv *priv;
};

/* struct for the object's class */
struct _DataManagerPerspectiveClass
{
	GtkVBoxClass                parent_class;
};

GType                data_manager_perspective_get_type               (void) G_GNUC_CONST;
BrowserPerspective  *data_manager_perspective_new                    (BrowserWindow *bwin);

G_END_DECLS

#endif
