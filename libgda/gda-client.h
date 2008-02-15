/* GDA library
 * Copyright (C) 1998 - 2008 The GNOME Foundation.
 *
 * AUTHORS:
 *      Michael Lausch <michael@lausch.at>
 *	Rodrigo Moya <rodrigo@gnome-db.org>
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

#ifndef __GDA_CLIENT_H__
#define __GDA_CLIENT_H__

#include <libgda/gda-connection.h>
#include <libgda/gda-server-operation.h>

G_BEGIN_DECLS

#define GDA_TYPE_CLIENT            (gda_client_get_type())
#define GDA_CLIENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_CLIENT, GdaClient))
#define GDA_CLIENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_CLIENT, GdaClientClass))
#define GDA_IS_CLIENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_CLIENT))
#define GDA_IS_CLIENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_CLIENT))

typedef enum {
	GDA_CLIENT_EVENT_INVALID,

	/* events usually notified by the library itself, and which the providers
	   should notify on very special cases (like a transaction being started
	   or committed via a BEGIN/COMMIT command directly sent to the
	   execute_command method on the provider */
	GDA_CLIENT_EVENT_ERROR,                 /* params: "error" */
	GDA_CLIENT_EVENT_CONNECTION_OPENED,     /* params: */
	GDA_CLIENT_EVENT_CONNECTION_CLOSED,     /* params: */
	GDA_CLIENT_EVENT_TRANSACTION_STARTED,   /* params: "transaction" */
	GDA_CLIENT_EVENT_TRANSACTION_COMMITTED, /* params: "transaction" */
	GDA_CLIENT_EVENT_TRANSACTION_CANCELLED  /* params: "transaction" */
} GdaClientEvent;

struct _GdaClient {
	GObject           object;
	GdaClientPrivate *priv;
};

struct _GdaClientClass {
	GObjectClass      parent_class;

	/* signals */
	void (* event_notification) (GdaClient *client,
				     GdaConnection *cnc,
				     GdaClientEvent event,
				     GdaSet *params);
};

/* error reporting */
extern GQuark gda_client_error_quark (void);
#define GDA_CLIENT_ERROR gda_client_error_quark ()

typedef enum
{
  GDA_CLIENT_GENERAL_ERROR
} GdaClientError;

GType          gda_client_get_type                           (void) G_GNUC_CONST;
GdaClient     *gda_client_new                                (void);

GdaConnection *gda_client_open_connection                    (GdaClient *client,
							      const gchar *dsn,
							      const gchar *auth_string,
							      GdaConnectionOptions options,
							      GError **error);
GdaConnection *gda_client_open_connection_from_string        (GdaClient *client,
							      const gchar *provider_id,
							      const gchar *cnc_string,
							      const gchar *auth_string,
							      GdaConnectionOptions options,
							      GError **error);
const GList   *gda_client_get_connections                    (GdaClient *client);
GdaConnection *gda_client_find_connection                    (GdaClient *client,
							      const gchar *dsn,
							      const gchar *auth_string);
void           gda_client_close_all_connections              (GdaClient *client);

void           gda_client_notify_event                       (GdaClient *client, GdaConnection *cnc,
							      GdaClientEvent event, GdaSet *params);
void           gda_client_notify_error_event                 (GdaClient *client, GdaConnection *cnc, GdaConnectionEvent *error);
void           gda_client_notify_connection_opened_event     (GdaClient *client, GdaConnection *cnc);
void           gda_client_notify_connection_closed_event     (GdaClient *client, GdaConnection *cnc);

/*
 * Database creation and destruction functions
 */
GdaServerOperation *gda_client_prepare_create_database       (GdaClient *client, const gchar *db_name, 
							      const gchar *provider);
gboolean       gda_client_perform_create_database            (GdaClient *client, GdaServerOperation *op, GError **error);
GdaServerOperation *gda_client_prepare_drop_database         (GdaClient *client, const gchar *db_name, 
							      const gchar *provider);
gboolean       gda_client_perform_drop_database              (GdaClient *client, GdaServerOperation *op, GError **error);

/*
 * Connection stack functions
 */

gboolean       gda_client_begin_transaction                  (GdaClient *client, const gchar *name, GdaTransactionIsolation level,
							      GError **error);
gboolean       gda_client_commit_transaction                 (GdaClient *client, const gchar *name, GError **error);
gboolean       gda_client_rollback_transaction               (GdaClient *client, const gchar *name, GError **error);



G_END_DECLS

#endif
