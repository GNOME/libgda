/*
 * Copyright (C) 2008 - 2014 Vivien Malerba <malerba@gnome-db.org>
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


#ifndef __WEB_SERVER_H_
#define __WEB_SERVER_H_

#include <glib-object.h>
#include <common/t-decl.h>

G_BEGIN_DECLS

#define WEB_TYPE_SERVER          (web_server_get_type())
#define WEB_SERVER(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, web_server_get_type(), WebServer)
#define WEB_SERVER_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, web_server_get_type (), WebServerClass)
#define WEB_IS_SERVER(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, web_server_get_type ())

typedef struct _WebServer WebServer;
typedef struct _WebServerClass WebServerClass;
typedef struct _WebServerPrivate WebServerPrivate;


/* error reporting */
extern GQuark web_server_error_quark (void);
#define WEB_SERVER_ERROR web_server_error_quark ()

typedef enum {
	WEB_SERVER__ERROR,
} WebServerError;

/* struct for the object's data */
struct _WebServer
{
	GObject                 object;
	WebServerPrivate       *priv;
};


/* struct for the object's class */
struct _WebServerClass
{
	GObjectClass               parent_class;
};

GType               web_server_get_type                (void) G_GNUC_CONST;
WebServer          *web_server_new                     (gint port, const gchar *auth_token);

G_END_DECLS

#endif
