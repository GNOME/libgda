/*
 * Copyright (C) 2011 The GNOME Foundation
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

#ifndef __CLASSES_VIEW_H__
#define __CLASSES_VIEW_H__

#include <gtk/gtk.h>
#include "../browser-connection.h"

G_BEGIN_DECLS

#define CLASSES_VIEW_TYPE            (classes_view_get_type())
#define CLASSES_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, CLASSES_VIEW_TYPE, ClassesView))
#define CLASSES_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, CLASSES_VIEW_TYPE, ClassesViewClass))
#define IS_CLASSES_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, CLASSES_VIEW_TYPE))
#define IS_CLASSES_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CLASSES_VIEW_TYPE))

typedef struct _ClassesView        ClassesView;
typedef struct _ClassesViewClass   ClassesViewClass;
typedef struct _ClassesViewPrivate ClassesViewPrivate;

struct _ClassesView {
	GtkTreeView           parent;
	ClassesViewPrivate *priv;
};

struct _ClassesViewClass {
	GtkTreeViewClass      parent_class;
};

GType        classes_view_get_type       (void) G_GNUC_CONST;

GtkWidget   *classes_view_new            (BrowserConnection *bcnc, const gchar *classname);
const gchar *classes_view_get_current_class (ClassesView *classes_view);
void         classes_view_set_current_class (ClassesView *classes_view, const gchar *classname);

G_END_DECLS

#endif
