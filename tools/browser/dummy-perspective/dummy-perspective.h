/*
 * Copyright (C) 2009 - 2015 Vivien Malerba <malerba@gnome-db.org>
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
typedef struct _DummyPerspectivePriv DummyPerspectivePriv;

/* struct for the object's data */
struct _DummyPerspective
{
	GtkBox                object;
	DummyPerspectivePriv *priv;
};

/* struct for the object's class */
struct _DummyPerspectiveClass
{
	GtkBoxClass           parent_class;
};

GType                dummy_perspective_get_type               (void) G_GNUC_CONST;
BrowserPerspective  *dummy_perspective_new                    (BrowserWindow *bwin);

G_END_DECLS

#endif
