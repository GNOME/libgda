/*
 * Copyright (C) 2011 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __QUERY_CONSOLE_PAGE_H__
#define __QUERY_CONSOLE_PAGE_H__

#include <gtk/gtk.h>
#include "../browser-connection.h"

G_BEGIN_DECLS

#define QUERY_CONSOLE_PAGE_TYPE            (query_console_page_get_type())
#define QUERY_CONSOLE_PAGE(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, QUERY_CONSOLE_PAGE_TYPE, QueryConsolePage))
#define QUERY_CONSOLE_PAGE_PAGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, QUERY_CONSOLE_PAGE_TYPE, QueryConsolePageClass))
#define IS_QUERY_CONSOLE_PAGE_PAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, QUERY_CONSOLE_PAGE_TYPE))
#define IS_QUERY_CONSOLE_PAGE_PAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), QUERY_CONSOLE_PAGE_TYPE))

typedef struct _QueryConsolePage        QueryConsolePage;
typedef struct _QueryConsolePageClass   QueryConsolePageClass;
typedef struct _QueryConsolePagePrivate QueryConsolePagePrivate;

struct _QueryConsolePage {
	GtkVBox               parent;
	QueryConsolePagePrivate     *priv;
};

struct _QueryConsolePageClass {
	GtkVBoxClass          parent_class;
};

GType                    query_console_page_get_type (void) G_GNUC_CONST;

GtkWidget               *query_console_page_new      (BrowserConnection *bcnc);
void                     query_console_page_set_text (QueryConsolePage *console, const gchar *text, gint fav_id);

G_END_DECLS

#endif
