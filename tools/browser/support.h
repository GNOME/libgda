/* 
 * Copyright (C) 2009 The GNOME Foundation.
 *
 * AUTHORS:
 * 	Vivien Malerba <malerba@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __SUPPORT_H_
#define __SUPPORT_H_

#include <libgda/libgda.h>
#include <gtk/gtk.h>
#include "browser-connection.h"

G_BEGIN_DECLS

BrowserConnection *browser_connection_open (GError **error);
gboolean           browser_connection_close (GtkWindow *parent, BrowserConnection *bcnc);
void               browser_show_error (GtkWindow *parent, const gchar *format, ...);

GtkWidget*         browser_make_tab_label_with_stock (const gchar *label,
						      const gchar *stock_id, gboolean with_close,
						      GtkWidget **out_close_button);
GtkWidget*         browser_make_tab_label_with_pixbuf (const gchar *label,
						       GdkPixbuf *pixbuf, gboolean with_close,
						       GtkWidget **out_close_button);

/*
 * Widgets navigation
 */
GtkWidget        *browser_find_parent_widget (GtkWidget *current, GType requested_type);

/*
 * icons
 */
typedef enum {
	BROWSER_ICON_BOOKMARK,
	BROWSER_ICON_SCHEMA,
	BROWSER_ICON_TABLE,
	BROWSER_ICON_COLUMN,
	BROWSER_ICON_COLUMN_PK,
	BROWSER_ICON_COLUMN_FK,
	BROWSER_ICON_COLUMN_FK_NN,
	BROWSER_ICON_COLUMN_NN,
	BROWSER_ICON_REFERENCE,
	BROWSER_ICON_DIAGRAM,
	BROWSER_ICON_QUERY,

	BROWSER_ICON_LAST
} BrowserIconType;

GdkPixbuf         *browser_get_pixbuf_icon (BrowserIconType type);

G_END_DECLS

#endif
