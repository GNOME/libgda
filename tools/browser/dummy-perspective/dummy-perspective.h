/* 
 * Copyright (C) 2009 Vivien Malerba
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


#ifndef __DUMMY_PERSPECTIVE_H_
#define __DUMMY_PERSPECTIVE_H_

#include <glib-object.h>
#include "../browser-perspective.h"

G_BEGIN_DECLS

#define TYPE_DUMMY_PERSPECTIVE          (dummy_perspective_get_type())
#define DUMMY_PERSPECTIVE(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, dummy_perspective_get_type(), DummyPerspective)
#define DUMMY_PERSPECTIVE_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, dummy_perspective_get_type (), DummyPerspectiveClass)
#define IS_DUMMY_PERSPECTIVE(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, dummy_perspective_get_type ())

typedef struct _DummyPerspective DummyPerspective;
typedef struct _DummyPerspectiveClass DummyPerspectiveClass;

/* struct for the object's data */
struct _DummyPerspective
{
	GtkVBox              object;
};

/* struct for the object's class */
struct _DummyPerspectiveClass
{
	GtkVBoxClass         parent_class;
};

GType                dummy_perspective_get_type               (void) G_GNUC_CONST;
BrowserPerspective  *dummy_perspective_new                    (BrowserWindow *bwin);

G_END_DECLS

#endif
