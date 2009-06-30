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


#ifndef __BROWSER_WINDOW_H_
#define __BROWSER_WINDOW_H_

#include <gtk/gtk.h>
#include "decl.h"
#include "browser-connection.h"

G_BEGIN_DECLS

#define BROWSER_TYPE_WINDOW          (browser_window_get_type())
#define BROWSER_WINDOW(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, browser_window_get_type(), BrowserWindow)
#define BROWSER_WINDOW_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, browser_window_get_type (), BrowserWindowClass)
#define BROWSER_IS_WINDOW(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, browser_window_get_type ())

typedef struct _BrowserWindowClass BrowserWindowClass;
typedef struct _BrowserWindowPrivate BrowserWindowPrivate;

/* struct for the object's data */
struct _BrowserWindow
{
	GtkWindow             object;
	BrowserWindowPrivate *priv;
};

/* struct for the object's class */
struct _BrowserWindowClass
{
	GtkWindowClass        parent_class;
};

GType               browser_window_get_type               (void) G_GNUC_CONST;
BrowserWindow      *browser_window_new                    (BrowserConnection *bcnc, BrowserPerspectiveFactory *factory);
BrowserConnection  *browser_window_get_connection         (BrowserWindow *bwin);

guint               browser_window_push_status            (BrowserWindow *bwin, const gchar *context, const gchar *text);
void                browser_window_pop_status             (BrowserWindow *bwin, const gchar *context);

G_END_DECLS

#endif
