/*
 * Copyright (C) 2009 - 2010 The GNOME Foundation
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

#ifndef __DATA_CONSOLE_H__
#define __DATA_CONSOLE_H__

#include <gtk/gtk.h>
#include "../browser-connection.h"

G_BEGIN_DECLS

#define DATA_CONSOLE_TYPE            (data_console_get_type())
#define DATA_CONSOLE(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, DATA_CONSOLE_TYPE, DataConsole))
#define DATA_CONSOLE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, DATA_CONSOLE_TYPE, DataConsoleClass))
#define IS_DATA_CONSOLE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, DATA_CONSOLE_TYPE))
#define IS_DATA_CONSOLE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), DATA_CONSOLE_TYPE))

typedef struct _DataConsole        DataConsole;
typedef struct _DataConsoleClass   DataConsoleClass;
typedef struct _DataConsolePrivate DataConsolePrivate;

struct _DataConsole {
	GtkVBox               parent;
	DataConsolePrivate   *priv;
};

struct _DataConsoleClass {
	GtkVBoxClass          parent_class;
};

GType                    data_console_get_type (void) G_GNUC_CONST;

GtkWidget               *data_console_new             (BrowserConnection *bcnc);
GtkWidget               *data_console_new_with_fav_id (BrowserConnection *bcnc, gint fav_id);
void                     data_console_set_text        (DataConsole *console, const gchar *text);
gchar                   *data_console_get_text        (DataConsole *console);
void                     data_console_execute         (DataConsole *console);
void                     data_console_set_fav_id      (DataConsole *dconsole, gint fav_id,
						       GError **error);

G_END_DECLS

#endif
