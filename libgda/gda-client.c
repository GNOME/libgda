/* GDA library
 * Copyright (C) 1998 - 2006 The GNOME Foundation.
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

typedef struct {
	GModule              *handle;
	GdaServerProvider    *provider;

	/* entry points to the plugin */
	const gchar        *(*plugin_get_name) (void);
	const gchar        *(*plugin_get_description) (void);
	GdaServerProvider  *(*plugin_create_provider) (void);
	gchar              *(*get_dsn_spec) (void);
} LoadedProvider;

struct _GdaClientPrivate {
	GHashTable           *providers;
	GList                *connections;
};

static void gda_client_class_init (GdaClientClass *klass);
static void gda_client_init       (GdaClient *client, GdaClientClass *klass);
static void gda_client_finalize   (GObject *object);

static void cnc_error_cb (GdaConnection *cnc, GdaConnectionEvent *error, GdaClient *client);
static void cnc_destroyed_cb (GdaConnection *cnc, GdaClient *client);

enum {
	EVENT_NOTIFICATION,
	LAST_SIGNAL
};

static gint gda_client_signals[LAST_SIGNAL] = { 0 };
static GObjectClass *parent_class = NULL;

/*
 * Private functions
 */

static void
emit_client_error (GdaClient *client, GdaConnection *cnc, const gchar *format, ...)
{
	va_list args;
	gchar sz[2048];
	GdaConnectionEvent *error;

	/* build the message string */
	va_start (args, format);
	vsprintf (sz, format, args);
	va_end (args);

	g_print ("Error: %s\n", sz);

	/* create the error list */
	error = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);
	gda_connection_event_set_description (error, sz);
	gda_connection_event_set_source (error, "[GDA client]");
	gda_connection_add_event (cnc, error);
}

static void
free_hash_provider (gpointer key, gpointer value, gpointer user_data)
{
	gchar *iid = (gchar *) key;
	LoadedProvider *prv = (LoadedProvider *) value;

	g_free (iid);
	if (prv) {
		g_object_unref (G_OBJECT (prv->provider));
		g_module_close (prv->handle);
		g_free (prv);
	}
}

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

static void
cnc_destroyed_cb (GdaConnection *cnc, GdaClient *client)
{
	g_assert (g_list_find (client->priv->connections, cnc));
	g_signal_handlers_disconnect_by_func (G_OBJECT (cnc), 
					      G_CALLBACK (cnc_destroyed_cb), client);
	g_signal_handlers_disconnect_by_func (G_OBJECT (cnc), 
					      G_CALLBACK (cnc_error_cb), client);
	client->priv->connections = g_list_remove (client->priv->connections, cnc);
	g_object_unref (cnc);
}

typedef struct {
	GdaClient *client;
	GdaServerProvider *provider;
	gboolean already_removed;
} prv_weak_cb_data;

static gboolean
remove_provider_in_hash (gpointer key, gpointer value, gpointer user_data)
{
	LoadedProvider *prv = value;
	prv_weak_cb_data *cb_data = user_data;

	if (prv->provider == cb_data->provider && !cb_data->already_removed) {
		g_free (key);
		g_module_close (prv->handle);
		g_free (prv);
		cb_data->already_removed = TRUE;
	}
	return TRUE;
}

static void
provider_weak_cb (gpointer user_data, GObject *object)
{
	prv_weak_cb_data cb_data;
	GdaServerProvider *provider = (GdaServerProvider *) object;
	GdaClient *client = (GdaClient *) user_data;

	g_return_if_fail (GDA_IS_SERVER_PROVIDER (provider));
	g_return_if_fail (GDA_IS_CLIENT (client));

	cb_data.client = client;
	cb_data.provider = provider;
	cb_data.already_removed = FALSE;
	g_hash_table_foreach_remove (client->priv->providers, (GHRFunc) remove_provider_in_hash, &cb_data);
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
			      gda_marshal_VOID__POINTER_INT_POINTER,
			      G_TYPE_NONE, 3, G_TYPE_POINTER, G_TYPE_INT, G_TYPE_POINTER);

	object_class->finalize = gda_client_finalize;
}

static void
gda_client_init (GdaClient *client, GdaClientClass *klass)
{
	g_return_if_fail (GDA_IS_CLIENT (client));

	client->priv = g_new0 (GdaClientPrivate, 1);
	client->priv->providers = g_hash_table_new (g_str_hash, g_str_equal);
	client->priv->connections = NULL;
}

static void
remove_weak_ref (gpointer key, gpointer value, gpointer user_data)
{
	LoadedProvider *prv = (LoadedProvider *) value;

	g_object_weak_unref (G_OBJECT (prv->provider), (GWeakNotify) provider_weak_cb, G_OBJECT (user_data));
}

static void
gda_client_finalize (GObject *object)
{
	GdaClient *client = (GdaClient *) object;

	g_return_if_fail (GDA_IS_CLIENT (client));

	/* free memory */
	while (client->priv->connections)
		cnc_destroyed_cb (GDA_CONNECTION (client->priv->connections->data), client);

	g_hash_table_foreach (client->priv->providers, (GHFunc) remove_weak_ref, client);
	g_hash_table_foreach (client->priv->providers, (GHFunc) free_hash_provider, NULL);
	g_hash_table_destroy (client->priv->providers);
	client->priv->providers = NULL;

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

	if (!type) {
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

static LoadedProvider *
find_or_load_provider (GdaClient *client, const gchar *provider)
{
	LoadedProvider *prv = NULL;
	GdaProviderInfo *prv_info;

	prv_info = gda_config_get_provider_by_name (provider);
	if (!prv_info) {
		emit_client_error (client, NULL,
				   _("Could not find provider %s in the current setup"),
				   provider);
		return NULL;
	}
	
	/* load the new provider */
	prv = g_new0 (LoadedProvider, 1);
	prv->handle = g_module_open (prv_info->location, G_MODULE_BIND_LAZY);

	if (!prv->handle) {
		emit_client_error (client, NULL, g_module_error ());
		g_free (prv);
		return NULL;
	}
	
	g_module_make_resident (prv->handle);
	
	g_module_symbol (prv->handle, "plugin_get_name",
			 (gpointer) &prv->plugin_get_name);
	g_module_symbol (prv->handle, "plugin_get_description",
			 (gpointer) &prv->plugin_get_description);
	g_module_symbol (prv->handle, "plugin_create_provider",
			 (gpointer) &prv->plugin_create_provider);
	g_module_symbol (prv->handle, "plugin_get_dsn_spec",
			 (gpointer) &prv->get_dsn_spec);
	
	if (!prv->plugin_create_provider) {
		emit_client_error (client, NULL,
				   _("Provider %s does not implement entry function"),
				   provider);
		g_free (prv);
		return NULL;
	}
	
	prv->provider = prv->plugin_create_provider ();
	if (!prv->provider) {
		emit_client_error (client, NULL,
				   _("Could not create GdaServerProvider object from plugin"));
		g_free (prv);
		return NULL;
	}
	
	g_object_ref (G_OBJECT (prv->provider)); /* vivien: usefull? */
	g_object_weak_ref (G_OBJECT (prv->provider), (GWeakNotify) provider_weak_cb, client);
	g_hash_table_insert (client->priv->providers,
			     g_strdup (provider),
			     prv);

	return prv;
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
	g_signal_connect (G_OBJECT (cnc), "destroyed",
			  G_CALLBACK (cnc_destroyed_cb), client);

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
 *
 * Returns: the opened connection if successful, %NULL if there is
 * an error.
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
	LoadedProvider *prv;
	GdaDataSourceInfo *dsn_info;

	g_return_val_if_fail (GDA_IS_CLIENT (client), NULL);

	/* get the data source info */
	dsn_info = gda_config_find_data_source (dsn);
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
			g_object_ref (G_OBJECT (cnc));
			gda_client_notify_connection_opened_event (client, cnc);
			gda_data_source_info_free (dsn_info);
			return cnc;
		}
	}

	/* try to find provider in our hash table */
	if (dsn_info->provider != NULL) {
		prv = g_hash_table_lookup (client->priv->providers, dsn_info->provider);
		if (!prv)
			prv = find_or_load_provider (client, dsn_info->provider);
	
		if (prv) {
			cnc = gda_connection_new (client, prv->provider, dsn, username, password, 
						  options);
			if (!gda_connection_open (cnc, error)) {
				g_object_unref (cnc);
				cnc = NULL;
			}
		}
		else 
			g_set_error (error, GDA_CLIENT_ERROR, 0, 
				     _("Datasource configuration error: could not find provider '%s'"),
				     dsn_info->provider);
	}
	else {
		g_warning (_("Datasource configuration error: no provider specified"));
		g_set_error (error, GDA_CLIENT_ERROR, 0, 
			     _("Datasource configuration error: no provider specified"));
	}

	/* free memory */
	gda_data_source_info_free (dsn_info);

	return cnc;
}

/**
 * gda_client_open_connection_from_string
 * @client: a #GdaClient object.
 * @provider_id: provider ID to connect to.
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
 * separator. The possible keys depend on the provider, but
 * these keys should work with all providers:
 * USER, PASSWORD, HOST, DATABASE, PORT
 *
 * The username and password used to actually open the connection are the first
 * non-NULL string being chosen by order from
 * <itemizedlist>
 *  <listitem>the @username or @password</listitem>
 *  <listitem>the USERNAME= and PASSWORD= parts of the @cnc_string</listitem>
 * </itemizedlist>
 *

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
	LoadedProvider *prv;
	GdaConnection *cnc = NULL;
	GList *l;

	g_return_val_if_fail (GDA_IS_CLIENT (client), NULL);
	g_return_val_if_fail (provider_id != NULL, NULL);
	
	if (! (options & GDA_CONNECTION_OPTIONS_DONT_SHARE)) {
		for (l = client->priv->connections; l != NULL; l = l->next) {
			const gchar *tmp_prov, *tmp_cnc_string;
	
			cnc = GDA_CONNECTION (l->data);
			tmp_prov = gda_connection_get_provider (cnc);
			tmp_cnc_string = gda_connection_get_cnc_string (cnc);
	
			if (strcmp(provider_id, tmp_prov) == 0
			    && (cnc_string != NULL && strcmp (cnc_string, tmp_cnc_string) == 0))
				return cnc;
		}
	}

	/* try to find provider in our hash table */
	if (provider_id) {
		prv = g_hash_table_lookup (client->priv->providers, provider_id);
		if (!prv)
			prv = find_or_load_provider (client, provider_id);
	
		if (prv) {
			cnc = g_object_new (GDA_TYPE_CONNECTION, 
					    "client", client, 
					    "provider_obj", prv->provider, 
					    "cnc_string", cnc_string, 
					    "username", username, 
					    "password", password, 
					    "options", options, NULL);
			if (!gda_connection_open (cnc, error)) {
				g_object_unref (cnc);
				cnc = NULL;
			}
		}
		else 
			g_set_error (error, GDA_CLIENT_ERROR, 0, 
				     _("Datasource configuration error: could not find provider '%s'"),
				     provider_id);
	}
	else {
		g_warning (_("Datasource configuration error: no provider specified"));
		g_set_error (error, GDA_CLIENT_ERROR, 0, 
			     _("Datasource configuration error: no provider specified"));
	}

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
	dsn_info = gda_config_find_data_source (dsn);
	if (!dsn_info) {
		gda_log_error (_("Data source %s not found in configuration"), dsn);
		return NULL;
	}

	for (l = client->priv->connections; l; l = l->next) {
		const gchar *tmp_dsn, *tmp_usr, *tmp_pwd;

		cnc = GDA_CONNECTION (l->data);
		tmp_dsn = dsn ? dsn : gda_connection_get_dsn (cnc);
		tmp_usr = username ? username : gda_connection_get_username (cnc);
		tmp_pwd = password ? password : gda_connection_get_password (cnc);

		if (!strcmp (tmp_dsn ? tmp_dsn : "", dsn_info->name ? dsn_info->name : "")
		    && !strcmp (tmp_usr ? tmp_usr : "", username ? username : "")
		    && !strcmp (tmp_pwd ? tmp_pwd : "", password ? password : "")) {
			gda_data_source_info_free (dsn_info);
			return cnc;
		}
	}

	gda_data_source_info_free (dsn_info);

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
	g_return_if_fail (GDA_IS_CLIENT (client));

	if (!cnc || g_list_find (client->priv->connections, cnc)) {
		g_signal_emit (G_OBJECT (client), gda_client_signals[EVENT_NOTIFICATION], 0,
			       cnc, event, params);
	}
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
	g_return_if_fail (GDA_IS_CLIENT (client));
	g_return_if_fail (GDA_IS_CONNECTION (cnc));

	gda_client_notify_event (client, cnc, GDA_CLIENT_EVENT_CONNECTION_CLOSED, NULL);
}

/**
 * gda_client_notify_transaction_started_event
 * @client: a #GdaClient object.
 * @cnc: a #GdaConnection object.
 * @xaction: a #GdaTransaction object.
 *
 * Notifies the given #GdaClient of the #GDA_CLIENT_EVENT_TRANSACTION_STARTED
 * event.
 */
void
gda_client_notify_transaction_started_event (GdaClient *client, GdaConnection *cnc, GdaTransaction *xaction)
{
	GdaParameterList *params;
	GdaParameter *param;
	GValue *value;

	g_return_if_fail (GDA_IS_CLIENT (client));
	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	g_return_if_fail (GDA_IS_TRANSACTION (xaction));

	param = gda_parameter_new (G_TYPE_OBJECT);
	gda_object_set_name (GDA_OBJECT (param), "transaction");
	value = g_value_init (g_new0 (GValue, 1), G_TYPE_OBJECT);
	g_value_set_object (value, G_OBJECT (xaction));
	gda_parameter_set_value (param, value);
	gda_value_free (value);

	params = gda_parameter_list_new (NULL);
	gda_parameter_list_add_param (params, param);
	g_object_unref (param);
	gda_client_notify_event (client, cnc, GDA_CLIENT_EVENT_TRANSACTION_STARTED, params);
	g_object_unref (params);
}

/**
 * gda_client_notify_transaction_committed_event
 * @client: a #GdaClient object.
 * @cnc: a #GdaConnection object.
 * @xaction: a #GdaTransaction object.
 *
 * Notifies the given #GdaClient of the #GDA_CLIENT_EVENT_TRANSACTION_COMMITTED
 * event.
 */
void
gda_client_notify_transaction_committed_event (GdaClient *client, GdaConnection *cnc, GdaTransaction *xaction)
{
	GdaParameterList *params;
	GdaParameter *param;
	GValue *value;

	g_return_if_fail (GDA_IS_CLIENT (client));
	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	g_return_if_fail (GDA_IS_TRANSACTION (xaction));

	param = gda_parameter_new (G_TYPE_OBJECT);
	gda_object_set_name (GDA_OBJECT (param), "transaction");
	value = g_value_init (g_new0 (GValue, 1), G_TYPE_OBJECT);
	g_value_set_object (value, G_OBJECT (xaction));
	gda_parameter_set_value (param, value);
	gda_value_free (value);

	params = gda_parameter_list_new (NULL);
	gda_parameter_list_add_param (params, param);
	g_object_unref (param);
	gda_client_notify_event (client, cnc, GDA_CLIENT_EVENT_TRANSACTION_COMMITTED, params);
	g_object_unref (params);
}

/**
 * gda_client_notify_transaction_cancelled_event
 * @client: a #GdaClient object.
 * @cnc: a #GdaConnection object.
 * @xaction: a #GdaTransaction object.
 *
 * Notifies the given #GdaClient of the #GDA_CLIENT_EVENT_TRANSACTION_CANCELLED 
 * event.
 */
void
gda_client_notify_transaction_cancelled_event (GdaClient *client, GdaConnection *cnc, GdaTransaction *xaction)
{
	GdaParameterList *params;
	GdaParameter *param;
	GValue *value;

	g_return_if_fail (GDA_IS_CLIENT (client));
	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	g_return_if_fail (GDA_IS_TRANSACTION (xaction));

	param = gda_parameter_new (G_TYPE_OBJECT);
	gda_object_set_name (GDA_OBJECT (param), "transaction");
	value = g_value_init (g_new0 (GValue, 1), G_TYPE_OBJECT);
	g_value_set_object (value, G_OBJECT (xaction));
	gda_parameter_set_value (param, value);
	gda_value_free (value);

	params = gda_parameter_list_new (NULL);
	gda_parameter_list_add_param (params, param);
	g_object_unref (param);
	gda_client_notify_event (client, cnc, GDA_CLIENT_EVENT_TRANSACTION_CANCELLED, params);
	g_object_unref (params);
}

/**
 * gda_client_begin_transaction
 * @client: a #GdaClient object.
 * @xaction: a #GdaTransaction object.
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
gda_client_begin_transaction (GdaClient *client, GdaTransaction *xaction)
{
	GList *l;

	g_return_val_if_fail (GDA_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (GDA_IS_TRANSACTION (xaction), FALSE);

	for (l = client->priv->connections; l != NULL; l = l->next) {
		if (!gda_connection_begin_transaction (GDA_CONNECTION (l->data), xaction)) {
			gda_client_rollback_transaction (client, xaction);
			return FALSE;
		}
	}

	return TRUE;
}

/**
 * gda_client_commit_transaction
 * @client: a #GdaClient object.
 * @xaction: a #GdaTransaction object.
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
gda_client_commit_transaction (GdaClient *client, GdaTransaction *xaction)
{
	GList *l;

	g_return_val_if_fail (GDA_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (GDA_IS_TRANSACTION (xaction), FALSE);

	for (l = client->priv->connections; l != NULL; l = l->next) {
		if (!gda_connection_commit_transaction (GDA_CONNECTION (l->data), xaction)) {
			gda_client_rollback_transaction (client, xaction);
			return FALSE;
		}
	}

	return TRUE;
}

/**
 * gda_client_rollback_transaction
 * @client: a #GdaClient object.
 * @xaction: a #GdaTransaction object.
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
gda_client_rollback_transaction (GdaClient *client, GdaTransaction *xaction)
{
	GList *l;
	gint failures = 0;

	g_return_val_if_fail (GDA_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (GDA_IS_TRANSACTION (xaction), FALSE);

	for (l = client->priv->connections; l != NULL; l = l->next) {
		if (!gda_connection_rollback_transaction (GDA_CONNECTION (l->data), xaction))
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
	LoadedProvider *prv;
	
	g_return_val_if_fail (client && GDA_IS_CLIENT (client), NULL);
	
	if (!provider || !*provider)
		return NULL;

	prv = find_or_load_provider (client, provider);
	if (prv && prv->get_dsn_spec) 
		return (prv->get_dsn_spec)();
	else
		return NULL;
}

/**
 * gda_client_prepare_create_database
 * @client: a #GdaClient object.
 * @provider: a provider
 *
 * Creates a new #GdaServerOperation object which contains the specifications required
 * to create a database. Once these specifications provided, use 
 * gda_client_perform_create_database() to perform the database creation.
 *
 * Returns: new #GdaServerOperation object, or %NULL if the provider does not support database
 * creation
 */
GdaServerOperation *
gda_client_prepare_create_database (GdaClient *client, const gchar *provider)
{
	LoadedProvider *prv;

	g_return_val_if_fail (client && GDA_IS_CLIENT (client), NULL);

	if (!provider || !*provider)
		return NULL;

	prv = find_or_load_provider (client, provider);
	if (prv && prv->provider) 
		return gda_server_provider_create_operation (prv->provider, NULL, 
							     GDA_SERVER_OPERATION_CREATE_DB, 
							     NULL, NULL);
	else
		return NULL;
}

/**
 * gda_client_prepare_drop_database
 * @client: a #GdaClient object.
 * @provider: a provider
 *
 * Creates a new #GdaServerOperation object which contains the specifications required
 * to drop a database. Once these specifications provided, use 
 * gda_client_perform_drop_database() to perform the database creation.
 *
 * Returns: new #GdaServerOperation object, or %NULL if the provider does not support database
 * destruction
 */
GdaServerOperation *
gda_client_prepare_drop_database (GdaClient *client, const gchar *provider)
{
	LoadedProvider *prv;

	g_return_val_if_fail (client && GDA_IS_CLIENT (client), NULL);

	if (!provider || !*provider)
		return NULL;

	prv = find_or_load_provider (client, provider);
	if (prv && prv->provider) 
		return gda_server_provider_create_operation (prv->provider, NULL,
							     GDA_SERVER_OPERATION_DROP_DB, 
							     NULL, NULL);
	else
		return NULL;
}

/**
 * gda_client_perform_create_database
 * @client: a #GdaClient object.
 * @provider: a provider
 * @op: a #GdaServerOperation object obtained using gda_client_prepare_server_operation()
 * @error: a place to store en error, or %NULL
 *
 * Creates a new database using the specifications in @op. 
 *
 * Returns: TRUE if no error occurred and the database has been created
 */
gboolean
gda_client_perform_create_database (GdaClient *client, const gchar *provider, GdaServerOperation *op,
				    GError **error)
{
	LoadedProvider *prv;

	g_return_val_if_fail (GDA_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (GDA_IS_SERVER_OPERATION (op), FALSE);

	prv = find_or_load_provider (client, provider);
	if (prv && prv->provider) 
		return gda_server_provider_perform_operation (prv->provider, NULL, 
							      op, error);
	else {
		g_set_error (error, GDA_CLIENT_ERROR, 0, 
			     _("Could not find provider %s in the current setup"), provider); 
		return FALSE;
	}
}

/**
 * gda_client_perform_drop_database
 * @client: a #GdaClient object.
 * @provider: a provider
 * @op: a #GdaServerOperation object obtained using gda_client_prepare_server_operation()
 * @error: a place to store en error, or %NULL
 *
 * Destroys an existing database using the specifications in @op. 
 *
 * Returns: TRUE if no error occurred and the database has been destroyed
 */
gboolean
gda_client_perform_drop_database (GdaClient *client, const gchar *provider, 
				  GdaServerOperation *op, GError **error)
{
	LoadedProvider *prv;

	g_return_val_if_fail (GDA_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (GDA_IS_SERVER_OPERATION (op), FALSE);

	prv = find_or_load_provider (client, provider);
	if (prv && prv->provider) 
		return gda_server_provider_perform_operation (prv->provider, NULL, 
							      op, error);
	else {
		g_set_error (error, GDA_CLIENT_ERROR, 0, 
			     _("Could not find provider %s in the current setup"), provider); 
		return FALSE;
	}
}
