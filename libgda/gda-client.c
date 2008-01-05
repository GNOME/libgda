/* GDA library
 * Copyright (C) 1998 - 2007 The GNOME Foundation.
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

#include <gmodule.h>
#include <libgda/gda-client.h>
#include <libgda/gda-config.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-log.h>
#include <libgda/gda-server-provider.h>
#include <string.h>
#include "gda-marshal.h"
#include "gda-dict.h"
#include "gda-parameter-list.h"
#include "gda-value.h"
#include "gda-enum-types.h"

struct _GdaClientPrivate {
	GList                *connections;
};

static void gda_client_class_init (GdaClientClass *klass);
static void gda_client_init       (GdaClient *client, GdaClientClass *klass);
static void gda_client_finalize   (GObject *object);

static void cnc_error_cb (GdaConnection *cnc, GdaConnectionEvent *error, GdaClient *client);

enum {
	EVENT_NOTIFICATION,
	LAST_SIGNAL
};

static gint gda_client_signals[LAST_SIGNAL] = { 0 };
static GObjectClass *parent_class = NULL;

/*
 * Callbacks
 */

static void
cnc_error_cb (GdaConnection *cnc, GdaConnectionEvent *error, GdaClient *client)
{
	g_return_if_fail (GDA_IS_CLIENT (client));

	/* notify error */
	gda_client_notify_error_event (client, cnc, error);
}

/*
 * GdaClient class implementation
 */

static void
gda_client_class_init (GdaClientClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	gda_client_signals[EVENT_NOTIFICATION] =
		g_signal_new ("event_notification",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaClientClass, event_notification),
			      NULL, NULL,
			      gda_marshal_VOID__OBJECT_ENUM_OBJECT,
			      G_TYPE_NONE, 3, GDA_TYPE_CONNECTION, GDA_TYPE_CLIENT_EVENT, GDA_TYPE_PARAMETER_LIST);

	object_class->finalize = gda_client_finalize;
}

static void
gda_client_init (GdaClient *client, GdaClientClass *klass)
{
	g_return_if_fail (GDA_IS_CLIENT (client));

	client->priv = g_new0 (GdaClientPrivate, 1);
	client->priv->connections = NULL;
}

static void
gda_client_finalize (GObject *object)
{
	GdaClient *client = (GdaClient *) object;
	GList *list;

	g_return_if_fail (GDA_IS_CLIENT (client));

	/* free memory */
	for (list = client->priv->connections; list; list = list->next)
		g_object_unref (GDA_CONNECTION (list->data));

	g_free (client->priv);
	client->priv = NULL;

	/* chain to parent class */
	parent_class->finalize (object);
}

/* module error */
GQuark gda_client_error_quark (void)
{
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("gda_client_error");
	return quark;
}

GType
gda_client_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdaClientClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_client_class_init,
			NULL,
			NULL,
			sizeof (GdaClient),
			0,
			(GInstanceInitFunc) gda_client_init
		};
		type = g_type_register_static (G_TYPE_OBJECT, "GdaClient", &info, 0);
	}

	return type;
}

/**
 * gda_client_new
 *
 * Creates a new #GdaClient object, which is the entry point for libgda
 * client applications. This object, once created, can be used for
 * opening new database connections and activating other services
 * available to GDA clients.
 *
 * Returns: the newly created object.
 */
GdaClient *
gda_client_new (void)
{
	GdaClient *client;

	client = g_object_new (GDA_TYPE_CLIENT, NULL);
	return client;
}

/**
 * gda_client_declare_connection
 * @client: a #GdaClient object
 * @cnc: a #GdaConnection object
 *
 * Declares the @cnc to @client. This function should not be used directly
 */
void
gda_client_declare_connection (GdaClient *client, GdaConnection *cnc)
{
	g_return_if_fail (client && GDA_IS_CLIENT (client));
	g_return_if_fail (client->priv);
	g_return_if_fail (cnc && GDA_IS_CONNECTION (cnc));
	g_return_if_fail (cnc->priv);
	
	if (g_list_find (client->priv->connections, cnc))
		return;

	client->priv->connections = g_list_append (client->priv->connections, cnc);
	g_object_ref (cnc);

	/* signals */
	g_signal_connect (G_OBJECT (cnc), "error",
			  G_CALLBACK (cnc_error_cb), client);
}

/**
 * gda_client_open_connection
 * @client: a #GdaClient object.
 * @dsn: data source name.
 * @username: user name or %NULL
 * @password: password for @username, or %NULL
 * @options: options for the connection (see #GdaConnectionOptions).
 * @error: a place to store an error, or %NULL
 *
 * This function is the way of opening database connections with libgda.
 *
 * Establishes a connection to a data source. The connection will be opened
 * if no identical connection is available in the #GdaClient connection pool,
 * and re-used if available. If you dont want to share the connection,
 * specify #GDA_CONNECTION_OPTIONS_DONT_SHARE as one of the flags in
 * the @options parameter.
 *
 * The username and password used to actually open the connection are the first
 * non-NULL string being chosen by order from
 * <itemizedlist>
 *  <listitem>the @username or @password</listitem>
 *  <listitem>the username or password sprcified in the DSN definition</listitem>
 *  <listitem>the USERNAME= and PASSWORD= parts of the connection string in the DSN definition</listitem>
 * </itemizedlist>
 *
 * If a new #GdaConnection is created, then the caller will hold a reference on it, and if a #GdaConnection
 * already existing is used, then the reference count of that object will be increased by one.
 *
 * Returns: the opened connection if successful, %NULL if there was an error.
 */
GdaConnection *
gda_client_open_connection (GdaClient *client,
			    const gchar *dsn,
			    const gchar *username,
			    const gchar *password,
			    GdaConnectionOptions options,
			    GError **error)
{
	GdaConnection *cnc = NULL;
	GdaDataSourceInfo *dsn_info;

	g_return_val_if_fail (GDA_IS_CLIENT (client), NULL);

	/* get the data source info */
	dsn_info = gda_config_get_dsn (dsn);
	if (!dsn_info) {
		gda_log_error (_("Data source %s not found in configuration"), dsn);
		g_set_error (error, GDA_CLIENT_ERROR, 0, 
			     _("Data source %s not found in configuration"), dsn);
		return NULL;
	}

	/* search for the connection in our private list */
	if (! (options & GDA_CONNECTION_OPTIONS_DONT_SHARE)) {
		cnc = gda_client_find_connection (client, dsn, username, password);
		if (cnc &&
		    ! (gda_connection_get_options (cnc) & GDA_CONNECTION_OPTIONS_DONT_SHARE)) {
			if (!gda_connection_open (cnc, error)) 
				cnc = NULL;
			else
				g_object_ref (G_OBJECT (cnc));

			return cnc;
		}
	}

	/* try to find provider in our hash table */
	if (dsn_info->provider != NULL) {
		GdaServerProvider *prov;

		prov = gda_config_get_provider_object (dsn_info->provider, error);
		if (prov) {
			cnc = gda_server_provider_create_connection (client, prov, dsn, username, password,
								     options);
			if (!gda_connection_open (cnc, error)) {
				g_object_unref (cnc);
				cnc = NULL;
			}
		}
	}
	else {
		g_warning (_("Datasource configuration error: no provider specified"));
		g_set_error (error, GDA_CLIENT_ERROR, 0, 
			     _("Datasource configuration error: no provider specified"));
	}

	if (cnc) 
		gda_client_declare_connection (client, cnc);

	return cnc;
}

/**
 * gda_client_open_connection_from_string
 * @client: a #GdaClient object.
 * @provider_id: provider ID to connect to, or %NULL
 * @cnc_string: connection string.
 * @username: user name.
 * @password: password for @username.
 * @options: options for the connection (see #GdaConnectionOptions).
 * @error: a place to store an error, or %NULL
 *
 * Opens a connection given a provider ID and a connection string. This
 * allows applications to open connections without having to create
 * a data source in the configuration. The format of @cnc_string is
 * similar to PostgreSQL and MySQL connection strings. It is a semicolumn-separated
 * series of key=value pairs. Do not add extra whitespace after the semicolumn
 * separator. The possible keys depend on the provider, the "gda-sql-3.0 -L" command
 * can be used to list the actual keys for each installed database provider.
 *
 * For example the connection string to open an SQLite connection to a database
 * file named "my_data.db" in the current directory would be "DB_DIR=.;DB_NAME=my_data".
 *
 * The username and password used to actually open the connection are the first
 * non-NULL string being chosen by order from
 * <itemizedlist>
 *  <listitem>the @username or @password</listitem>
 *  <listitem>the USERNAME= and PASSWORD= parts of the @cnc_string</listitem>
 * </itemizedlist>
 *
 * Additionnally, it is possible to have the connection string
 * respect the "&lt;provider_name&gt;://&lt;real cnc string&gt;" format, in which case the provider name
 * and the real connection string will be extracted from that string (note that if @provider_id
 * is not %NULL then it will still be used as the provider ID).
 *
 * Returns: the opened connection if successful, %NULL if there is
 * an error.
 */
GdaConnection *
gda_client_open_connection_from_string (GdaClient *client,
					const gchar *provider_id,
					const gchar *cnc_string,
					const gchar *username,
					const gchar *password,
					GdaConnectionOptions options,
					GError **error)
{
	GdaConnection *cnc = NULL;
	GList *l;
	gchar *ptr, *dup;

	g_return_val_if_fail (GDA_IS_CLIENT (client), NULL);
	g_return_val_if_fail (cnc_string, NULL);

	/* try to see if connection string has the "<provider>://<real cnc string>" format */
	dup = g_strdup (cnc_string);
	for (ptr = dup; *ptr; ptr++) {
		if ((*ptr == ':') && (*(ptr+1) == '/') && (*(ptr+2) == '/')) {
			if (!provider_id)
				provider_id = dup;
			*ptr = 0;
			cnc_string = ptr + 3;
		}
	}
	
	if (!provider_id) {
		g_set_error (error, GDA_CLIENT_ERROR, 0, _("No provider specified"));
		g_free (dup);
		return NULL;
	}

	if (! (options & GDA_CONNECTION_OPTIONS_DONT_SHARE)) {
		for (l = client->priv->connections; l != NULL; l = l->next) {
			const gchar *tmp_prov, *tmp_cnc_string;
	
			cnc = GDA_CONNECTION (l->data);
			tmp_prov = gda_connection_get_provider (cnc);
			tmp_cnc_string = gda_connection_get_cnc_string (cnc);
	
			if (strcmp (provider_id, tmp_prov) == 0
			    && (! strcmp (cnc_string, tmp_cnc_string))) {
				g_free (dup);
				return cnc;
			}
		}
	}

	/* try to find provider in our hash table */
	if (provider_id) {
		GdaServerProvider *prov;

		prov = gda_config_get_provider_object (provider_id, error);
		if (prov) {
			cnc = gda_server_provider_create_connection_from_string (client, prov,
										 cnc_string, username, password, options);
			if (!gda_connection_open (cnc, error)) {
				g_object_unref (cnc);
				cnc = NULL;
			}
		}
	}
	else {
		g_warning (_("Datasource configuration error: no provider specified"));
		g_set_error (error, GDA_CLIENT_ERROR, 0, 
			     _("Datasource configuration error: no provider specified"));
	}

	g_free (dup);

	return cnc;
}

/**
 * gda_client_get_connections
 * @client: a #GdaClient object.
 *
 * Gets the list of all open connections in the given #GdaClient object.
 * The GList returned is an internal pointer, so DON'T TRY TO
 * FREE IT.
 *
 * Returns: a GList of #GdaConnection objects; dont't modify that list
 */
const GList *
gda_client_get_connections (GdaClient *client)
{
	g_return_val_if_fail (GDA_IS_CLIENT (client), NULL);
	return (const GList *) client->priv->connections;
}

/**
 * gda_client_find_connection
 * @client: a #GdaClient object.
 * @dsn: data source name.
 * @username: user name.
 * @password: password for @username.
 *
 * Looks for an open connection given a data source name (per libgda
 * configuration), a username and a password.
 *
 * This function iterates over the list of open connections in the
 * given #GdaClient and looks for one that matches the given data source
 * name, username and password.
 *
 * Returns: a pointer to the found connection, or %NULL if it could not
 * be found.
 */
GdaConnection *
gda_client_find_connection (GdaClient *client,
			    const gchar *dsn,
			    const gchar *username,
			    const gchar *password)
{
	GList *l;
	GdaDataSourceInfo *dsn_info;
	GdaConnection *cnc;

	g_return_val_if_fail (GDA_IS_CLIENT (client), NULL);

	/* get the data source info */
	dsn_info = gda_config_get_dsn (dsn);
	if (!dsn_info) {
		gda_log_error (_("Data source %s not found in configuration"), dsn);
		return NULL;
	}

	for (l = client->priv->connections; l; l = l->next) {
		const gchar *tmp_dsn, *tmp_usr, *tmp_pwd;

		cnc = GDA_CONNECTION (l->data);
		tmp_dsn = gda_connection_get_dsn (cnc);
		tmp_usr = gda_connection_get_username (cnc);
		tmp_pwd = gda_connection_get_password (cnc);

		if (!strcmp (tmp_dsn ? tmp_dsn : "", dsn_info->name ? dsn_info->name : "")
		    && !strcmp (tmp_usr ? tmp_usr : "", username ? username : "")
		    && !strcmp (tmp_pwd ? tmp_pwd : "", password ? password : "")) {
			return cnc;
		}
	}

	return NULL;
}

/**
 * gda_client_close_all_connections
 * @client: a #GdaClient object.
 *
 * Closes all connections opened by the given #GdaClient object.
 */
void
gda_client_close_all_connections (GdaClient *client)
{
	g_return_if_fail (GDA_IS_CLIENT (client));
	g_return_if_fail (client->priv);

	if (client->priv->connections) 
		g_list_foreach (client->priv->connections, (GFunc) gda_connection_close, NULL);
}

/**
 * gda_client_notify_event
 * @client: a #GdaClient object.
 * @cnc: a #GdaConnection object where the event has occurred.
 * @event: event ID.
 * @params: parameters associated with the event.
 *
 * Notifies an event to the given #GdaClient's listeners. The event can be
 * anything (see #GdaClientEvent) ranging from a connection opening
 * operation, to changes made to a table in an underlying database.
 */
void
gda_client_notify_event (GdaClient *client,
			 GdaConnection *cnc,
			 GdaClientEvent event,
			 GdaParameterList *params)
{
	if (!client)
		return;

	g_return_if_fail (GDA_IS_CLIENT (client));
	g_signal_emit (G_OBJECT (client), gda_client_signals[EVENT_NOTIFICATION], 0,
		       cnc, event, params);
}

/**
 * gda_client_notify_error_event
 * @client: a #GdaClient object.
 * @cnc: a #GdaConnection object.
 * @error: the error to be notified.
 *
 * Notifies the given #GdaClient of the #GDA_CLIENT_EVENT_ERROR event.
 */
void
gda_client_notify_error_event (GdaClient *client, GdaConnection *cnc, GdaConnectionEvent *error)
{
	GdaParameterList *params;
	GdaParameter *param;
	GValue *value;

	if (!client)
		return;

	g_return_if_fail (GDA_IS_CLIENT (client));
	g_return_if_fail (error != NULL);

	param = gda_parameter_new (G_TYPE_OBJECT);
	gda_object_set_name (GDA_OBJECT (param), "error");
	value = g_value_init (g_new0 (GValue, 1), G_TYPE_OBJECT);
	g_value_set_object (value, G_OBJECT (error));
	gda_parameter_set_value (param, value);
	gda_value_free (value);

	params = gda_parameter_list_new (NULL);
	gda_parameter_list_add_param (params, param);
	g_object_unref (param);
	gda_client_notify_event (client, cnc, GDA_CLIENT_EVENT_ERROR, params);
	g_object_unref (params);
}

/**
 * gda_client_notify_connection_opened_event
 * @client: a #GdaClient object.
 * @cnc: a #GdaConnection object.
 *
 * Notifies the given #GdaClient of the #GDA_CLIENT_EVENT_CONNECTION_OPENED 
 * event.
 */
void
gda_client_notify_connection_opened_event (GdaClient *client, GdaConnection *cnc)
{
	if (!client)
		return;

	g_return_if_fail (GDA_IS_CLIENT (client));
	g_return_if_fail (GDA_IS_CONNECTION (cnc));

	gda_client_notify_event (client, cnc, GDA_CLIENT_EVENT_CONNECTION_OPENED, NULL);
}

/**
 * gda_client_notify_connection_closed_event
 * @client: a #GdaClient object.
 * @cnc: a #GdaConnection object.
 *
 * Notifies the given #GdaClient of the #GDA_CLIENT_EVENT_CONNECTION_CLOSED 
 * event.
 */
void
gda_client_notify_connection_closed_event (GdaClient *client, GdaConnection *cnc)
{
	if (!client)
		return;

	g_return_if_fail (GDA_IS_CLIENT (client));
	g_return_if_fail (GDA_IS_CONNECTION (cnc));

	gda_client_notify_event (client, cnc, GDA_CLIENT_EVENT_CONNECTION_CLOSED, NULL);
}

/**
 * gda_client_begin_transaction
 * @client: a #GdaClient object.
 * @name: the name of the transation to start
 * @level:
 * @error: a place to store errors, or %NULL
 *
 * Starts a transaction on all connections being managed by the given
 * #GdaClient. It is important to note that this operates on all
 * connections opened within a #GdaClient, which could not be what
 * you're looking for.
 *
 * To execute a transaction on a unique connection, use
 * #gda_connection_begin_transaction, #gda_connection_commit_transaction
 * and #gda_connection_rollback_transaction.
 *
 * Returns: %TRUE if all transactions could be started successfully,
 * or %FALSE if one of them fails.
 */
gboolean
gda_client_begin_transaction (GdaClient *client, const gchar *name, GdaTransactionIsolation level,
			      GError **error)
{
	GList *l;

	g_return_val_if_fail (GDA_IS_CLIENT (client), FALSE);

	for (l = client->priv->connections; l != NULL; l = l->next) {
		if (!gda_connection_begin_transaction (GDA_CONNECTION (l->data), name, level, error)) {
			gda_client_rollback_transaction (client, name, NULL);
			return FALSE;
		}
	}

	return TRUE;
}

/**
 * gda_client_commit_transaction
 * @client: a #GdaClient object.
 * @name: the name of the transation to commit
 * @error: a place to store errors, or %NULL
 *
 * Commits a running transaction on all connections being managed by the given
 * #GdaClient. It is important to note that this operates on all
 * connections opened within a #GdaClient, which could not be what
 * you're looking for.
 *
 * To execute a transaction on a unique connection, use
 * #gda_connection_begin_transaction, #gda_connection_commit_transaction
 * and #gda_connection_rollback_transaction.
 *
 * Returns: %TRUE if all transactions could be committed successfully,
 * or %FALSE if one of them fails.
 */
gboolean
gda_client_commit_transaction (GdaClient *client, const gchar *name, GError **error)
{
	GList *l;

	g_return_val_if_fail (GDA_IS_CLIENT (client), FALSE);

	for (l = client->priv->connections; l != NULL; l = l->next) {
		if (!gda_connection_commit_transaction (GDA_CONNECTION (l->data), name, error)) {
			gda_client_rollback_transaction (client, name, NULL);
			return FALSE;
		}
	}

	return TRUE;
}

/**
 * gda_client_rollback_transaction
 * @client: a #GdaClient object.
 * @name: the name of the transation to rollback
 * @error: a place to store errors, or %NULL
 *
 * Cancels a running transaction on all connections being managed by the given
 * #GdaClient. It is important to note that this operates on all
 * connections opened within a #GdaClient, which could not be what
 * you're looking for.
 *
 * To execute a transaction on a unique connection, use
 * #gda_connection_begin_transaction, #gda_connection_commit_transaction
 * and #gda_connection_rollback_transaction.
 *
 * Returns: %TRUE if all transactions could be cancelled successfully,
 * or %FALSE if one of them fails.
 */
gboolean
gda_client_rollback_transaction (GdaClient *client, const gchar *name, GError **error)
{
	GList *l;
	gint failures = 0;

	g_return_val_if_fail (GDA_IS_CLIENT (client), FALSE);

	for (l = client->priv->connections; l != NULL; l = l->next) {
		if (!gda_connection_rollback_transaction (GDA_CONNECTION (l->data), name, error))
			failures++;
	}

	return failures == 0 ? TRUE : FALSE;
}

/**
 * gda_client_get_dsn_specs
 * @client: a #GdaClient object.
 * @provider: a provider
 *
 * Get an XML string representing the parameters which can be present in the
 * DSN string used to open a connection.
 *
 * Returns: a string (free it after usage), or %NULL if an error occurred
 *
 */
gchar *
gda_client_get_dsn_specs (GdaClient *client, const gchar *provider)
{
	GdaProviderInfo *pinfo;
	g_return_val_if_fail (client && GDA_IS_CLIENT (client), NULL);
	
	if (!provider || !*provider)
		return NULL;

	pinfo = gda_config_get_provider_info (provider);
	if (pinfo) 
		return g_strdup (pinfo->dsn_spec);
	else
		return NULL;
}

/**
 * gda_client_prepare_create_database
 * @client: a #GdaClient object.
 * @db_name: the name of the database to create, or %NULL
 * @provider: a provider
 *
 * Creates a new #GdaServerOperation object which contains the specifications required
 * to create a database. Once these specifications provided, use 
 * gda_client_perform_create_database() to perform the database creation.
 *
 * If @db_name is left %NULL, then the name of the database to create will have to be set in the
 * returned #GdaServerOperation using gda_server_operation_set_value_at().
 *
 * Returns: new #GdaServerOperation object, or %NULL if the provider does not support database
 * creation
 */
GdaServerOperation *
gda_client_prepare_create_database (GdaClient *client, const gchar *db_name, const gchar *provider)
{
	GdaServerProvider *prov;

	g_return_val_if_fail (client && GDA_IS_CLIENT (client), NULL);

	if (!provider || !*provider)
		return NULL;

	prov = gda_config_get_provider_object (provider, NULL);
	if (prov) {
		GdaServerOperation *op;
		op = gda_server_provider_create_operation (prov, NULL, 
							   GDA_SERVER_OPERATION_CREATE_DB, 
							   NULL, NULL);
		if (op) {
			g_object_set_data (G_OBJECT (op), "_gda_provider_name", prov);
			if (db_name)
				gda_server_operation_set_value_at (op, db_name, 
								   NULL, "/DB_DEF_P/DB_NAME");
		}
		return op;
	}
	else
		return NULL;
}

/**
 * gda_client_prepare_drop_database
 * @client: a #GdaClient object.
 * @db_name: the name of the database to drop, or %NULL
 * @provider: a provider
 *
 * Creates a new #GdaServerOperation object which contains the specifications required
 * to drop a database. Once these specifications provided, use 
 * gda_client_perform_drop_database() to perform the database creation.
 *
 * If @db_name is left %NULL, then the name of the database to drop will have to be set in the
 * returned #GdaServerOperation using gda_server_operation_set_value_at().
 *
 * Returns: new #GdaServerOperation object, or %NULL if the provider does not support database
 * destruction
 */
GdaServerOperation *
gda_client_prepare_drop_database (GdaClient *client, const gchar *db_name, const gchar *provider)
{
	GdaServerProvider *prov;

	g_return_val_if_fail (client && GDA_IS_CLIENT (client), NULL);

	if (!provider || !*provider)
		return NULL;

	prov = gda_config_get_provider_object (provider, NULL);
	if (prov) {
		GdaServerOperation *op;
		op = gda_server_provider_create_operation (prov, NULL,
							   GDA_SERVER_OPERATION_DROP_DB, 
							   NULL, NULL);
		if (op) {
			g_object_set_data (G_OBJECT (op), "_gda_provider_name", prov);
			if (db_name)
				gda_server_operation_set_value_at (op, db_name, 
								   NULL, "/DB_DESC_P/DB_NAME");
		}
		return op;
	}
	else
		return NULL;
}

/**
 * gda_client_perform_create_database
 * @client: a #GdaClient object.
 * @op: a #GdaServerOperation object obtained using gda_client_prepare_create_database()
 * @error: a place to store en error, or %NULL
 *
 * Creates a new database using the specifications in @op, which must have been obtained using
 * gda_client_prepare_create_database ()
 *
 * Returns: TRUE if no error occurred and the database has been created
 */
gboolean
gda_client_perform_create_database (GdaClient *client, GdaServerOperation *op, GError **error)
{
	GdaServerProvider *provider;

	g_return_val_if_fail (GDA_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (GDA_IS_SERVER_OPERATION (op), FALSE);

	provider = g_object_get_data (G_OBJECT (op), "_gda_provider_name");
	if (provider) 
		return gda_server_provider_perform_operation (provider, NULL, op, error);
	else {
		g_set_error (error, GDA_CLIENT_ERROR, 0, 
			     _("Could not find operation's associated provider, "
			       "did you use gda_client_prepare_create_database() ?")); 
		return FALSE;
	}
}

/**
 * gda_client_perform_drop_database
 * @client: a #GdaClient object.
 * @op: a #GdaServerOperation object obtained using gda_client_prepare_drop_database()
 * @error: a place to store en error, or %NULL
 *
 * Destroys an existing database using the specifications in @op,  which must have been obtained using
 * gda_client_prepare_drop_database ()
 *
 * Returns: TRUE if no error occurred and the database has been destroyed
 */
gboolean
gda_client_perform_drop_database (GdaClient *client, GdaServerOperation *op, GError **error)
{
	GdaServerProvider *provider;

	g_return_val_if_fail (GDA_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (GDA_IS_SERVER_OPERATION (op), FALSE);

	provider = g_object_get_data (G_OBJECT (op), "_gda_provider_name");
	if (provider) 
		return gda_server_provider_perform_operation (provider, NULL, op, error);
	else {
		g_set_error (error, GDA_CLIENT_ERROR, GDA_CLIENT_GENERAL_ERROR, 
			     _("Could not find operation's associated provider, "
			       "did you use gda_client_prepare_drop_database() ?")); 
		return FALSE;
	}
}
