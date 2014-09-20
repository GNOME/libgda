/*
 * Copyright (C) 2010 - 2014 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __DATA_CONSOLE_H__
#define __DATA_CONSOLE_H__

#include <gtk/gtk.h>
#include "common/t-connection.h"

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
	GtkBox               parent;
	DataConsolePrivate   *priv;
};

struct _DataConsoleClass {
	GtkBoxClass          parent_class;
};

GType                    data_console_get_type (void) G_GNUC_CONST;

GtkWidget               *data_console_new             (TConnection *tcnc);
GtkWidget               *data_console_new_with_fav_id (TConnection *tcnc, gint fav_id);
void                     data_console_set_text        (DataConsole *console, const gchar *text);
gchar                   *data_console_get_text        (DataConsole *console);
gboolean                 data_console_is_unused       (DataConsole *console);
void                     data_console_execute         (DataConsole *console);
void                     data_console_set_fav_id      (DataConsole *dconsole, gint fav_id,
						       GError **error);

G_END_DECLS

#endif
