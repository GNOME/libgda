/*
 * Copyright (C) 2005 - 2009 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2006 - 2007 Murray Cumming <murrayc@murrayc-desktop>
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

#ifndef __CC_UTIL_H__
#define __CC_UTIL_H__

#include <libgda/gda-data-model.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

GtkWidget   *cc_new_menu_item (const gchar *label,
				     gboolean pixmap,
				     GCallback cb_func,
				     gpointer user_data);
GtkWidget   *cc_new_vbox_widget (gboolean homogenous, gint spacing);
GtkWidget   *cc_new_hbox_widget (gboolean homogenous, gint spacing);
GtkWidget   *cc_new_table_widget (gint rows, gint cols, gboolean homogenous);
GtkWidget   *cc_new_label_widget (const gchar *text);
GtkWidget   *cc_new_notebook_widget (void);

G_END_DECLS

#endif
