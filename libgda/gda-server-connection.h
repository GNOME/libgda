/* GDA server library
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
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

#if !defined(__gda_server_connection_h__)
#  define __gda_server_connection_h__

#include <libgda/gda-error.h>
#include <libgda/gda-parameter.h>
#include <libgda/GNOME_Database.h>
#include <bonobo/bonobo-xobject.h>

G_BEGIN_DECLS

#define GDA_TYPE_SERVER_CONNECTION            (gda_server_connection_get_type())
#define GDA_SERVER_CONNECTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_SERVER_CONNECTION, GdaServerConnection))
#define GDA_SERVER_CONNECTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_SERVER_CONNECTION, GdaServerConnectionClass))
#define GDA_IS_SERVER_CONNECTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_SERVER_CONNECTION))
#define GDA_IS_SERVER_CONNECTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_SERVER_CONNECTION))

typedef struct _GdaServerConnection        GdaServerConnection;
typedef struct _GdaServerConnectionClass   GdaServerConnectionClass;
typedef struct _GdaServerConnectionPrivate GdaServerConnectionPrivate;

typedef struct _GdaServerProvider          GdaServerProvider;

struct _GdaServerConnection {
	BonoboObject object;
	GdaServerConnectionPrivate *priv;
};

struct _GdaServerConnectionClass {
	BonoboObjectClass parent_class;
	POA_GNOME_Database_Connection__epv epv;
};

GType                gda_server_connection_get_type (void);
GdaServerConnection *gda_server_connection_new (GdaServerProvider *provider);

const gchar         *gda_server_connection_get_string (GdaServerConnection *cnc);
const gchar         *gda_server_connection_get_username (GdaServerConnection *cnc);
const gchar         *gda_server_connection_get_password (GdaServerConnection *cnc);

void                 gda_server_connection_notify_action (GdaServerConnection *cnc,
							  GNOME_Database_ActionId action,
							  GdaParameterList *params);

void                 gda_server_connection_add_error (GdaServerConnection *cnc,
						      GdaError *error);
void                 gda_server_connection_add_error_string (GdaServerConnection *cnc,
							     const gchar *msg);
void                 gda_server_connection_free_error_list (GdaServerConnection *cnc);

G_END_DECLS

#endif
