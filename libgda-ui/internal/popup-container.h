/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef __POPUP_CONTAINER_H__
#define __POPUP_CONTAINER_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define POPUP_CONTAINER_TYPE            (popup_container_get_type())
#define POPUP_CONTAINER(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, POPUP_CONTAINER_TYPE, PopupContainer))
#define POPUP_CONTAINER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, POPUP_CONTAINER_TYPE, PopupContainerClass))
#define IS_POPUP_CONTAINER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, POPUP_CONTAINER_TYPE))
#define IS_POPUP_CONTAINER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), POPUP_CONTAINER_TYPE))

typedef struct _PopupContainer        PopupContainer;
typedef struct _PopupContainerClass   PopupContainerClass;
typedef struct _PopupContainerPrivate PopupContainerPrivate;

typedef void (*PopupContainerPositionFunc) (PopupContainer *cont, gint *out_x, gint *out_y);

struct _PopupContainer {
	GtkWindow               parent;
	PopupContainerPrivate  *priv;
};

struct _PopupContainerClass {
	GtkWindowClass          parent_class;
};

GType                  popup_container_get_type (void) G_GNUC_CONST;
GtkWidget             *popup_container_new (GtkWidget *position_widget);
GtkWidget             *popup_container_new_with_func (PopupContainerPositionFunc pos_func);

G_END_DECLS

#endif
