/* 
 * Copyright (C) 2009 Vivien Malerba
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


#ifndef __BROWSER_CONNECTIONS_LIST_H_
#define __BROWSER_CONNECTIONS_LIST_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BROWSER_TYPE_CONNECTIONS_LIST          (browser_connections_list_get_type())
#define BROWSER_CONNECTIONS_LIST(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, browser_connections_list_get_type(), BrowserConnectionsList)
#define BROWSER_CONNECTIONS_LIST_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, browser_connections_list_get_type (), BrowserConnectionsListClass)
#define BROWSER_IS_CONNECTIONS_LIST(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, browser_connections_list_get_type ())

typedef struct _BrowserConnectionsList BrowserConnectionsList;
typedef struct _BrowserConnectionsListPrivate BrowserConnectionsListPrivate;
typedef struct _BrowserConnectionsListClass BrowserConnectionsListClass;

/* struct for the object's data */
struct _BrowserConnectionsList
{
	GtkWindow                      object;
	BrowserConnectionsListPrivate *priv;
};

/* struct for the object's class */
struct _BrowserConnectionsListClass
{
	GtkWindowClass                 parent_class;
};

GType           browser_connections_list_get_type               (void) G_GNUC_CONST;
void            browser_connections_list_show                   (void);

G_END_DECLS

#endif
