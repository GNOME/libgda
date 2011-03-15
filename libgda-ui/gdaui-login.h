/*
 * Copyright (C) 1999 - 2011 The GNOME Foundation.
 *
 * AUTHORS:
 *      Rodrigo Moya <rodrigo@gnome-db.org>
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

#ifndef __GDAUI_LOGIN_H__
#define __GDAUI_LOGIN_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_LOGIN            (gdaui_login_get_type())
#define GDAUI_LOGIN(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDAUI_TYPE_LOGIN, GdauiLogin))
#define GDAUI_LOGIN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDAUI_TYPE_LOGIN, GdauiLoginClass))
#define GDAUI_IS_LOGIN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDAUI_TYPE_LOGIN))
#define GDAUI_IS_LOGIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDAUI_TYPE_LOGIN))

typedef struct _GdauiLogin        GdauiLogin;
typedef struct _GdauiLoginClass   GdauiLoginClass;
typedef struct _GdauiLoginPrivate GdauiLoginPrivate;

struct _GdauiLogin {
	GtkVBox            parent;
	GdauiLoginPrivate *priv;
};

struct _GdauiLoginClass {
	GtkVBoxClass       parent_class;

	/* signals */
	void               (*changed) (GdauiLogin *login, gboolean is_valid);
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

GType             gdaui_login_get_type                   (void) G_GNUC_CONST;
GtkWidget        *gdaui_login_new                        (const gchar *dsn);
void              gdaui_login_set_mode                   (GdauiLogin *login, GdauiLoginMode mode);
const GdaDsnInfo *gdaui_login_get_connection_information (GdauiLogin *login);

void              gdaui_login_set_dsn                    (GdauiLogin *login, const gchar *dsn);
void              gdaui_login_set_connection_information (GdauiLogin *login, const GdaDsnInfo *cinfo);

G_END_DECLS

#endif
