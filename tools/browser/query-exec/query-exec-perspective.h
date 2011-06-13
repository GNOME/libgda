/*
 * Copyright (C) 2009 Vivien Malerba <malerba@gnome-db.org>
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


#ifndef __QUERY_EXEC_PERSPECTIVE_H_
#define __QUERY_EXEC_PERSPECTIVE_H_

#include <glib-object.h>
#include "../browser-perspective.h"

G_BEGIN_DECLS

#define TYPE_QUERY_EXEC_PERSPECTIVE          (query_exec_perspective_get_type())
#define QUERY_EXEC_PERSPECTIVE(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, query_exec_perspective_get_type(), QueryExecPerspective)
#define QUERY_EXEC_PERSPECTIVE_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, query_exec_perspective_get_type (), QueryExecPerspectiveClass)
#define IS_QUERY_EXEC_PERSPECTIVE(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, query_exec_perspective_get_type ())

typedef struct _QueryExecPerspective QueryExecPerspective;
typedef struct _QueryExecPerspectiveClass QueryExecPerspectiveClass;
typedef struct _QueryExecPerspectivePrivate QueryExecPerspectivePrivate;

/* struct for the object's data */
struct _QueryExecPerspective
{
	GtkVBox              object;
	QueryExecPerspectivePrivate *priv;
};

/* struct for the object's class */
struct _QueryExecPerspectiveClass
{
	GtkVBoxClass         parent_class;
};

GType                query_exec_perspective_get_type               (void) G_GNUC_CONST;
BrowserPerspective  *query_exec_perspective_new                    (BrowserWindow *bwin);

G_END_DECLS

#endif
