/*
 * Copyright (C) 2009 The GNOME Foundation
 *
 * AUTHORS:
 *      Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __QUERY_CONSOLE_H__
#define __QUERY_CONSOLE_H__

#include <gtk/gtk.h>
#include "../browser-connection.h"

G_BEGIN_DECLS

#define QUERY_CONSOLE_TYPE            (query_console_get_type())
#define QUERY_CONSOLE(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, QUERY_CONSOLE_TYPE, QueryConsole))
#define QUERY_CONSOLE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, QUERY_CONSOLE_TYPE, QueryConsoleClass))
#define IS_QUERY_CONSOLE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, QUERY_CONSOLE_TYPE))
#define IS_QUERY_CONSOLE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), QUERY_CONSOLE_TYPE))

typedef struct _QueryConsole        QueryConsole;
typedef struct _QueryConsoleClass   QueryConsoleClass;
typedef struct _QueryConsolePrivate QueryConsolePrivate;

struct _QueryConsole {
	GtkVBox               parent;
	QueryConsolePrivate     *priv;
};

struct _QueryConsoleClass {
	GtkVBoxClass          parent_class;
};

GType                    query_console_get_type (void) G_GNUC_CONST;

GtkWidget               *query_console_new      (BrowserConnection *bcnc);
void                     query_console_set_text (QueryConsole *console, const gchar *text);

G_END_DECLS

#endif
