/*
 * Copyright (C) 2009 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
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

#ifndef __GDAUI_SERVER_OPERATION__
#define __GDAUI_SERVER_OPERATION__

#include <gtk/gtk.h>
#include <libgda/libgda.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_SERVER_OPERATION          (gdaui_server_operation_get_type())
G_DECLARE_DERIVABLE_TYPE(GdauiServerOperation, gdaui_server_operation, GDAUI, SERVER_OPERATION, GtkBox)
/* struct for the object's class */
struct _GdauiServerOperationClass
{
	GtkBoxClass         parent_class;
	gpointer            padding[12];
};

/**
 * SECTION:gdaui-server-operation
 * @short_description: Enter information to perform a DDL query
 * @title: GdauiServerOperation
 * @stability: Stable
 * @Image:
 * @see_also: See the #GdaServerOperation which actually holds the information to perform the action
 *
 * The #GdauiServerOperation widget allows the user to enter information to perform
 * Data Definition queries (all queries which are not SELECT, INSERT, UPDATE or DELETE).
 * For example the figure shows a #GdauiServerOperation widget set to create an index in an
 * SQLite database.
 */

GtkWidget        *gdaui_server_operation_new              (GdaServerOperation *op);
GtkWidget        *gdaui_server_operation_new_in_dialog    (GdaServerOperation *op, GtkWindow *parent,
							   const gchar *title, const gchar *header);
void              gdaui_server_operation_update_parameters (GdauiServerOperation *op, GError **error);

G_END_DECLS

#endif



