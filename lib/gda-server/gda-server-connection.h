/* GDA Server Library
 * Copyright (C) 2000 Rodrigo Moya
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

#include <bonobo/bonobo-xobject.h>
#include <GNOME_Database.h>
#include <gda-common-defs.h>
#include <gda-error.h>
#include <gda-listener.h>

typedef struct _GdaServerConnection GdaServerConnection;

#include <gda-server-recordset.h>

G_BEGIN_DECLS

#define GDA_TYPE_SERVER_CONNECTION            (gda_server_connection_get_type())
#define GDA_SERVER_CONNECTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_SERVER_CONNECTION, GdaServerConnection))
#define GDA_SERVER_CONNECTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_SERVER_CONNECTION, GdaServerConnectionClass))
#define GDA_IS_SERVER_CONNECTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_SERVER_CONNECTION))
#define GDA_IS_SERVER_CONNECTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_SERVER_CONNECTION))

typedef struct _GdaServerConnectionClass GdaServerConnectionClass;

struct _GdaServerConnection {
	BonoboXObject object;

	/* data */
	GdaServer *server_impl;
	gchar *dsn;
	gchar *username;
	gchar *password;
	GList *commands;
	GList *errors;
	GList *listeners;

	gpointer user_data;
};

struct _GdaServerConnectionClass {
	BonoboXObjectClass parent_class;

	POA_GNOME_Database_Connection__epv epv;
};

GType                gda_server_connection_get_type (void);
GdaServerConnection *gda_server_connection_new (GdaServer *server_impl);
gchar               *gda_server_connection_get_dsn (GdaServerConnection * cnc);
void                 gda_server_connection_set_dsn (GdaServerConnection * cnc,
						    const gchar * dsn);
gchar               *gda_server_connection_get_username (GdaServerConnection * cnc);
void                 gda_server_connection_set_username (GdaServerConnection * cnc,
							 const gchar * username);
gchar               *gda_server_connection_get_password (GdaServerConnection * cnc);
void                 gda_server_connection_set_password (GdaServerConnection * cnc,
							 const gchar * password);
void                 gda_server_connection_add_error (GdaServerConnection * cnc,
						      GdaError * error);
void                 gda_server_connection_add_error_string (GdaServerConnection *cnc,
							     const gchar * msg);
gpointer             gda_server_connection_get_user_data (GdaServerConnection *cnc);
void                 gda_server_connection_set_user_data (GdaServerConnection * cnc,
							  gpointer user_data);
void                 gda_server_connection_free (GdaServerConnection * cnc);

gint                 gda_server_connection_open (GdaServerConnection * cnc,
						 const gchar * dsn,
						 const gchar * user,
						 const gchar * password);
void                 gda_server_connection_close (GdaServerConnection * cnc);
gint                 gda_server_connection_begin_transaction (GdaServerConnection *cnc);
gint                 gda_server_connection_commit_transaction (GdaServerConnection *cnc);
gint                 gda_server_connection_rollback_transaction (GdaServerConnection *cnc);
GdaServerRecordset  *gda_server_connection_open_schema (GdaServerConnection * cnc,
							GdaError * error,
							GNOME_Database_Connection_QType t,
							GNOME_Database_Connection_Constraint *constraints,
							gint length);
glong                gda_server_connection_modify_schema (GdaServerConnection * cnc,
							  GNOME_Database_Connection_QType t,
							  GNOME_Database_Connection_Constraint *constraints,
							  gint length);
gint                 gda_server_connection_start_logging (GdaServerConnection * cnc,
							  const gchar * filename);
gint                 gda_server_connection_stop_logging (GdaServerConnection * cnc);
gchar               *gda_server_connection_create_table (GdaServerConnection * cnc,
							 GNOME_Database_RowAttributes *columns);
gboolean             gda_server_connection_supports (GdaServerConnection * cnc,
						     GNOME_Database_Connection_Feature feature);
GNOME_Database_ValueType gda_server_connection_get_gda_type (GdaServerConnection *cnc,
							     gulong sql_type);
gshort               gda_server_connection_get_c_type (GdaServerConnection * cnc,
						       GNOME_Database_ValueType type);
gchar               *gda_server_connection_sql2xml (GdaServerConnection * cnc,
						    const gchar * sql);
gchar               *gda_server_connection_xml2sql (GdaServerConnection * cnc,
						    const gchar * xml);
void                 gda_server_connection_make_error (GdaError *error,
						       GdaServerRecordset *recset,
						       GdaServerConnection *cnc,
						       gchar *where);

void                 gda_server_connection_add_listener (GdaServerConnection * cnc,
							 GNOME_Database_Listener listener);
void                 gda_server_connection_remove_listener (GdaServerConnection * cnc,
							    GNOME_Database_Listener listener);

G_END_DECLS

#endif
