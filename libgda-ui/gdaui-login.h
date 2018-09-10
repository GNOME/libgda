/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2018 Daniel Espinosa <esodan@gmail.com>
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

#ifndef __GDAUI_LOGIN_H__
#define __GDAUI_LOGIN_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_LOGIN            (gdaui_login_get_type())
G_DECLARE_DERIVABLE_TYPE(GdauiLogin, gdaui_login, GDAUI, LOGIN, GtkBox)

struct _GdauiLoginClass {
	GtkBoxClass       parent_class;

	/* signals */
	void               (*changed) (GdauiLogin *login, gboolean is_valid);
	gpointer            padding[12];
};

/**
 * GdauiLoginMode:
 * @GDA_UI_LOGIN_ENABLE_CONTROL_CENTRE_MODE: 
 * @GDA_UI_LOGIN_HIDE_DSN_SELECTION_MODE: 
 * @GDA_UI_LOGIN_HIDE_DIRECT_CONNECTION_MODE: 
 *
 * Defines the aspect of the #GdauiLogin widget
 */
typedef enum {
	GDA_UI_LOGIN_ENABLE_CONTROL_CENTRE_MODE = 1 << 0,
	GDA_UI_LOGIN_HIDE_DSN_SELECTION_MODE = 1 << 1,
	GDA_UI_LOGIN_HIDE_DIRECT_CONNECTION_MODE = 1 << 2
} GdauiLoginMode;

/**
 * SECTION:gdaui-login
 * @short_description: Connection opening widget
 * @title: GdauiLogin
 * @stability: Stable
 * @Image: vi-login.png
 * @see_also:
 *
 * The #GdauiLogin widget can be used when the user needs to enter
 * data to open a connection. It can be customized in several ways:
 * <itemizedlist>
 *   <listitem><para>data source (DSN) selection can be shown or hidden</para></listitem>
 *   <listitem><para>the button to launch the control center to declare new data sources can be
 *	shown or hidden</para></listitem>
 *   <listitem><para>the form to open a connection not using a DSN can be shown or hidden</para></listitem>
 * </itemizedlist>
 */

GtkWidget        *gdaui_login_new                        (const gchar *dsn);
void              gdaui_login_set_mode                   (GdauiLogin *login, GdauiLoginMode mode);
const GdaDsnInfo *gdaui_login_get_connection_information (GdauiLogin *login);

void              gdaui_login_set_dsn                    (GdauiLogin *login, const gchar *dsn);
void              gdaui_login_set_connection_information (GdauiLogin *login, const GdaDsnInfo *cinfo);

G_END_DECLS

#endif
