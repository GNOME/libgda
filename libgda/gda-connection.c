/* GDA library
 * Copyright (C) 1998 - 2005 The GNOME Foundation.
 *
 * AUTHORS:
 *      Michael Lausch <michael@lausch.at>
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *	Bas Driessen <bas.driessen@xobas.com>
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

#include <stdio.h>
#include <libgda/gda-client.h>
#include <libgda/gda-config.h>
#include <libgda/gda-connection.h>
#include <libgda/gda-connection-event.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-dict.h>
#include <libgda/gda-log.h>
#include <libgda/gda-server-provider.h>
#include "gda-marshal.h"

#define PARENT_TYPE G_TYPE_OBJECT

struct _GdaConnectionPrivate {
	GdaClient            *client;
	GdaServerProvider    *provider_obj;
	GdaConnectionOptions  options;
	gchar                *dsn;
	gchar                *cnc_string;
	gchar                *provider;
	gchar                *username;
	gchar                *password;
	gboolean              is_open;
	GList                *events_list;
	GList                *recset_list;
};

static void gda_connection_class_init (GdaConnectionClass *klass);
static void gda_connection_init       (GdaConnection *cnc, GdaConnectionClass *klass);
static void gda_connection_finalize   (GObject *object);

enum {
	ERROR,
	CONN_OPENED,
        CONN_TO_CLOSE,
        CONN_CLOSED,
	DSN_CHANGED,
	LAST_SIGNAL
};

static gint gda_connection_signals[LAST_SIGNAL] = { 0, 0, 0, 0, 0 };
static GObjectClass *parent_class = NULL;

/*
 * GdaConnection class implementation
 * @klass:
 */
static void
gda_connection_class_init (GdaConnectionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	gda_connection_signals[ERROR] =
		g_signal_new ("error",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaConnectionClass, error),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, GDA_TYPE_CONNECTION_EVENT);
	gda_connection_signals[CONN_OPENED] =
                g_signal_new ("conn_opened",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaConnectionClass, conn_opened),
                              NULL, NULL,
                              gda_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
        gda_connection_signals[CONN_TO_CLOSE] =
                g_signal_new ("conn_to_close",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaConnectionClass, conn_to_close),
                              NULL, NULL,
                              gda_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
        gda_connection_signals[CONN_CLOSED] =    /* runs after user handlers */
                g_signal_new ("conn_closed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GdaConnectionClass, conn_closed),
                              NULL, NULL,
                              gda_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
	gda_connection_signals[DSN_CHANGED] =
		g_signal_new ("dsn_changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaConnectionClass, dsn_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	object_class->finalize = gda_connection_finalize;
}

static void
gda_connection_init (GdaConnection *cnc, GdaConnectionClass *klass)
{
	g_return_if_fail (GDA_IS_CONNECTION (cnc));

	cnc->priv = g_new0 (GdaConnectionPrivate, 1);
	cnc->priv->client = NULL;
	cnc->priv->provider_obj = NULL;
	cnc->priv->dsn = NULL;
	cnc->priv->cnc_string = NULL;
	cnc->priv->provider = NULL;
	cnc->priv->username = NULL;
	cnc->priv->password = NULL;
	cnc->priv->is_open = FALSE;
	cnc->priv->events_list = NULL;
	cnc->priv->recset_list = NULL;
}

static void
gda_connection_finalize (GObject *object)
{
	GdaConnection *cnc = (GdaConnection *) object;

	g_return_if_fail (GDA_IS_CONNECTION (cnc));

	/* free memory */
	gda_connection_close_no_warning (cnc);

	g_object_unref (G_OBJECT (cnc->priv->provider_obj));
	cnc->priv->provider_obj = NULL;

	g_free (cnc->priv->dsn);
	g_free (cnc->priv->cnc_string);
	g_free (cnc->priv->provider);
	g_free (cnc->priv->username);
	g_free (cnc->priv->password);

	gda_connection_event_list_free (cnc->priv->events_list);

	g_list_foreach (cnc->priv->recset_list, (GFunc) g_object_unref, NULL);

	g_free (cnc->priv);
	cnc->priv = NULL;

	/* chain to parent class */
	parent_class->finalize (object);
}

/**
 * gda_connection_get_type
 * 
 * Registers the #GdaConnection class on the GLib type system.
 * 
 * Returns: the GType identifying the class.
 */
GType
gda_connection_get_type (void)
{
   static GType type = 0;

	if (type == 0) {
		static GTypeInfo info = {
			sizeof (GdaConnectionClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_connection_class_init,
			NULL, NULL,
			sizeof (GdaConnection),
			0,
			(GInstanceInitFunc) gda_connection_init
		};
	type = g_type_register_static (PARENT_TYPE, "GdaConnection", &info, 0);
	}
	return type;
}

/**
 * gda_connection_new
 * @client: a #GdaClient object.
 * @provider: a #GdaServerProvider object.
 * @dsn: GDA data source to connect to.
 * @username: user name to use to connect.
 * @password: password for @username.
 * @options: options for the connection.
 *
 * This function creates a new #GdaConnection object. It is not
 * intended to be used directly by applications (use
 * #gda_client_open_connection instead).
 *
 * The connection is not opened at this stage; use 
 * gda_connection_open() to open the connection.
 *
 * Returns: a newly allocated #GdaConnection object.
 */
GdaConnection *
gda_connection_new (GdaClient *client,
		    GdaServerProvider *provider,
		    const gchar *dsn,
		    const gchar *username,
		    const gchar *password,
		    GdaConnectionOptions options)
{
	GdaConnection *cnc;

	g_return_val_if_fail (GDA_IS_CLIENT (client), NULL);
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);

	/* create the connection object */
	cnc = g_object_new (GDA_TYPE_CONNECTION, NULL);

	gda_connection_set_client (cnc, client);
	cnc->priv->provider_obj = provider;
	g_object_ref (G_OBJECT (cnc->priv->provider_obj));

	if (dsn)
		cnc->priv->dsn = g_strdup (dsn);
	if (username)
		cnc->priv->username = g_strdup (username);
	if (password)
		cnc->priv->password = g_strdup (password);
	cnc->priv->options = options;

	return cnc;
}

/**
 * gda_connection_open
 * @conn: a #GdaConnection object
 * @error: a place to store errors, or %NULL
 *
 * Tries to open the connection.
 *
 * Returns: TRUE if the connection is opened, and FALSE otherwise.
 */
gboolean
gda_connection_open (GdaConnection *cnc, GError **error)
{
	GdaDataSourceInfo *dsn_info;
	GdaQuarkList *params;
	char *real_username = NULL;
	char *real_password = NULL;

	g_return_val_if_fail (cnc && GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (cnc->priv, FALSE);

	/* don't do anything if connection is already opened */
	if (cnc->priv->is_open)
		return TRUE;

	/* get the data source info */
	dsn_info = gda_config_find_data_source (cnc->priv->dsn);
	if (!dsn_info) {
		gda_log_error (_("Data source %s not found in configuration"), cnc->priv->dsn);
		g_set_error (error, 0, 0,
			     _("Data source %s not found in configuration"), cnc->priv->dsn);
		return FALSE;
	}

	g_free (cnc->priv->cnc_string);
	cnc->priv->cnc_string = g_strdup (dsn_info->cnc_string);
	g_free (cnc->priv->provider);
	cnc->priv->provider = g_strdup (dsn_info->provider);

	params = gda_quark_list_new_from_string (dsn_info->cnc_string);

	/* retrieve correct username/password */
	if (cnc->priv->username)
		real_username = g_strdup (cnc->priv->username);
	else {
		if (dsn_info->username)
			real_username = g_strdup (dsn_info->username);
		else {
			const gchar *s;
			s = gda_quark_list_find (params, "USER");
			if (s) {
				real_username = g_strdup (s);
				gda_quark_list_remove (params, "USER");
			}
		}
	}

	if (cnc->priv->password)
		real_password = g_strdup (cnc->priv->password);
	else {
		if (dsn_info->password)
			real_password = g_strdup (dsn_info->password);
		else {
			const gchar *s;
			s = gda_quark_list_find (params, "PASSWORD");
			if (s) {
				real_password = g_strdup (s);
				gda_quark_list_remove (params, "PASSWORD");
			}
		}
	}

	/* try to open the connection */
	if (gda_server_provider_open_connection (cnc->priv->provider_obj, cnc, params,
						 real_username, real_password)) {
		cnc->priv->is_open = TRUE;
		gda_client_notify_connection_opened_event (cnc->priv->client, cnc);
	}
	else {
		const GList *events;
		
		events = gda_connection_get_events (cnc);
		if (events) {
			GList *l;

			for (l = (GList *) events; l != NULL; l = l->next) {
				GdaConnectionEvent *event;

				event = GDA_CONNECTION_EVENT (l->data);
				if (gda_connection_event_get_event_type (event) == GDA_CONNECTION_EVENT_ERROR) {
					if (error && !(*error))
						g_set_error (error, 0, 0,
							     gda_connection_event_get_description (event));
					gda_client_notify_error_event (cnc->priv->client, cnc, 
								       GDA_CONNECTION_EVENT (l->data));
				}
			}
		}

		cnc->priv->is_open = FALSE;
	}

	/* free memory */
	gda_data_source_info_free (dsn_info);
	gda_quark_list_free (params);

	return cnc->priv->is_open;
}


/**
 * gda_connection_close
 * @cnc: a #GdaConnection object.
 *
 * Closes the connection to the underlying data source, but first emits the 
 * "conn_to_close" signal.
 */
void
gda_connection_close (GdaConnection *cnc)
{
	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	g_return_if_fail (cnc->priv);

	if (! cnc->priv->is_open)
		return;

#ifdef GDA_DEBUG_signal
        g_print (">> 'CONN_TO_CLOSE' from %s\n", __FUNCTION__);
#endif
        g_signal_emit (G_OBJECT (cnc), gda_connection_signals[CONN_TO_CLOSE], 0);
#ifdef GDA_DEBUG_signal
        g_print ("<< 'CONN_TO_CLOSE' from %s\n", __FUNCTION__);
#endif

        gda_connection_close_no_warning (cnc);
}

/**
 * gda_connection_close_no_warning
 * @cnc: a #GdaConnection object.
 *
 * Closes the connection to the underlying data source, without emiting any warning signal.
 */
void
gda_connection_close_no_warning (GdaConnection *cnc)
{
	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	g_return_if_fail (cnc->priv);

	if (! cnc->priv->is_open)
		return;

	gda_server_provider_close_connection (cnc->priv->provider_obj, cnc);
	gda_client_notify_connection_closed_event (cnc->priv->client, cnc);
	cnc->priv->is_open = FALSE;

#ifdef GDA_DEBUG_signal
        g_print (">> 'CONN_CLOSED' from %s\n", __FUNCTION__);
#endif
        g_signal_emit (G_OBJECT (cnc), gda_connection_signals[CONN_CLOSED], 0);
#ifdef GDA_DEBUG_signal
        g_print ("<< 'CONN_CLOSED' from %s\n", __FUNCTION__);
#endif
}


/**
 * gda_connection_is_opened
 * @cnc: a #GdaConnection object.
 *
 * Checks whether a connection is open or not.
 *
 * Returns: %TRUE if the connection is open, %FALSE if it's not.
 */
gboolean
gda_connection_is_opened (GdaConnection *cnc)
{
        g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	return cnc->priv->is_open;
}

/**
 * gda_connection_get_client
 * @cnc: a #GdaConnection object.
 *
 * Gets the #GdaClient object associated with a connection. This
 * is always the client that created the connection, as returned
 * by #gda_client_open_connection.
 *
 * Returns: the client to which the connection belongs to.
 */
GdaClient *
gda_connection_get_client (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cnc->priv, NULL);

	return cnc->priv->client;
}

/**
 * gda_connection_set_client
 * @cnc: a #GdaConnection object.
 * @client: a #GdaClient object.
 *
 * Associates a #GdaClient with this connection. This function is
 * not intended to be called by applications.
 */
void
gda_connection_set_client (GdaConnection *cnc, GdaClient *client)
{
	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	g_return_if_fail (GDA_IS_CLIENT (client));

	cnc->priv->client = client;
}

/**
 * gda_connection_get_options
 * @cnc: a #GdaConnection object.
 *
 * Gets the #GdaConnectionOptions used to open this connection.
 *
 * Returns: the connection options.
 */
GdaConnectionOptions
gda_connection_get_options (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), -1);
	g_return_val_if_fail (cnc->priv, -1);

	return cnc->priv->options;
}

/**
 * gda_connection_get_provider_obj
 * @cnc: a #GdaConnection object
 *
 * Get a pointer to the #GdaServerProvider object used to access the database
 *
 * Returns: the #GdaServerProvider (NEVER NULL)
 */
GdaServerProvider *
gda_connection_get_provider_obj (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cnc->priv, NULL);

	g_assert (cnc->priv->provider_obj);
	return cnc->priv->provider_obj;
}

/**
 * gda_connection_get_infos
 * @cnc: a #GdaConnection object
 *
 * Get a pointer to a #GdaServerProviderInfo structure (which must not be modified)
 * to retreive specific information about the provider used by @cnc.
 */
GdaServerProviderInfo *
gda_connection_get_infos (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cnc->priv, NULL);

	return gda_server_provider_get_info (cnc->priv->provider_obj, cnc);
}

/**
 * gda_connection_get_server_version
 * @cnc: a #GdaConnection object.
 *
 * Gets the version string of the underlying database server.
 *
 * Returns: the server version string.
 */
const gchar *
gda_connection_get_server_version (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cnc->priv, NULL);

	return gda_server_provider_get_server_version (cnc->priv->provider_obj, cnc);
}

/**
 * gda_connection_get_database
 * @cnc: A #GdaConnection object.
 *
 * Gets the name of the currently active database in the given
 * @GdaConnection.
 *
 * Returns: the name of the current database.
 */
const gchar *
gda_connection_get_database (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cnc->priv, NULL);

	return gda_server_provider_get_database (cnc->priv->provider_obj, cnc);
}

/**
 * gda_connection_set_dsn
 * @srv: a #GdaConnection object
 * @dsn: a gda datasource
 *
 * Sets the data source of the connection. If the connection is already opened,
 * then no action is performed at all and FALSE is returned.
 *
 * If the requested datasource does not exist, then nothing is done and FALSE
 * is returned.
 *
 * Returns: TRUE on success
 */
gboolean
gda_connection_set_dsn (GdaConnection *cnc, const gchar *datasource)
{
	GdaDataSourceInfo *dsn;

        g_return_val_if_fail (cnc && GDA_IS_CONNECTION (cnc), FALSE);
        g_return_val_if_fail (cnc->priv, FALSE);
        g_return_val_if_fail (datasource && *datasource, FALSE);

        if (cnc->priv->is_open)
                return FALSE;

        dsn = gda_config_find_data_source (datasource);
        if (!dsn)
                return FALSE;

	g_free (cnc->priv->dsn);
	cnc->priv->dsn = g_strdup (datasource);
#ifdef GDA_DEBUG_signal
        g_print (">> 'DSN_CHANGED' from %s\n", __FUNCTION__);
#endif
        g_signal_emit (G_OBJECT (cnc), gda_connection_signals[DSN_CHANGED], 0);
#ifdef GDA_DEBUG_signal
        g_print ("<< 'DSN_CHANGED' from %s\n", __FUNCTION__);
#endif

	return TRUE;
}

/**
 * gda_connection_get_dsn
 * @cnc: a #GdaConnection object
 *
 * Returns the data source name the connection object is connected
 * to.
 */
const gchar *
gda_connection_get_dsn (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cnc->priv, NULL);

	return (const gchar *) cnc->priv->dsn;
}

/**
 * gda_connection_get_cnc_string
 * @cnc: a #GdaConnection object.
 *
 * Gets the connection string used to open this connection.
 *
 * The connection string is the string sent over to the underlying
 * database provider, which describes the parameters to be used
 * to open a connection on the underlying data source.
 *
 * Returns: the connection string used when opening the connection.
 */
const gchar *
gda_connection_get_cnc_string (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cnc->priv, NULL);

	return (const gchar *) cnc->priv->cnc_string;
}

/**
 * gda_connection_get_provider
 * @cnc: a #GdaConnection object.
 *
 * Gets the provider id that this connection is connected to.
 *
 * Returns: the provider ID used to open this connection.
 */
const gchar *
gda_connection_get_provider (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cnc->priv, NULL);

	return (const gchar *) cnc->priv->provider;
}

/**
 * gda_connection_set_username
 * @cnc: a #GdaConnection object
 * @username:
 *
 * Sets the user name for the connection. If the connection is already opened,
 * then no action is performed at all and FALSE is returned.
 *
 * Returns: TRUE on success
 */
gboolean
gda_connection_set_username (GdaConnection *cnc, const gchar *username)
{
	g_return_val_if_fail (cnc && GDA_IS_CONNECTION (cnc), FALSE);
        g_return_val_if_fail (cnc->priv, FALSE);
        g_return_val_if_fail (username, FALSE);

        if (cnc->priv->is_open)
                return FALSE;

        g_free (cnc->priv->username);
	cnc->priv->username = g_strdup (username);
        return TRUE;
}

/**
 * gda_connection_get_username
 * @cnc: a #GdaConnection object.
 *
 * Gets the user name used to open this connection.
 *
 * Returns: the user name.
 */
const gchar *
gda_connection_get_username (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cnc->priv, NULL);

	return (const gchar *) cnc->priv->username;
}

/**
 * gda_connection_set_password
 * @cnc: a #GdaConnection object
 * @password:
 *
 * Sets the user password for the connection to the server. If the connection is already opened,
 * then no action is performed at all and FALSE is returned.
 *
 * Returns: TRUE on success
 */
gboolean
gda_connection_set_password (GdaConnection *cnc, const gchar *password)
{
	g_return_val_if_fail (cnc && GDA_IS_CONNECTION (cnc), FALSE);
        g_return_val_if_fail (cnc->priv, FALSE);
        g_return_val_if_fail (password, FALSE);

        if (cnc->priv->is_open)
                return FALSE;

        g_free (cnc->priv->password);
	cnc->priv->password = g_strdup (password);

        return TRUE;
}

/**
 * gda_connection_get_password
 * @cnc: a #GdaConnection object.
 *
 * Gets the password used to open this connection.
 *
 * Returns: the password.
 */
const gchar *
gda_connection_get_password (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cnc->priv, NULL);

	return (const gchar *) cnc->priv->password;
}

/**
 * gda_connection_add_event
 * @cnc: a #GdaConnection object.
 * @event: is stored internally, so you don't need to unref it.
 *
 * Adds an event to the given connection. This function is usually
 * called by providers, to inform clients of events that happened
 * during some operation.
 *
 * As soon as a provider (or a client, it does not matter) calls this
 * function with an @event object which is an error,
 * the connection object (and the associated #GdaClient object)
 * emits the "error" signal, to which clients can connect to be
 * informed of events.
 *
 * WARNING: the reference to the @event object is stolen by this function!
 */
void
gda_connection_add_event (GdaConnection *cnc, GdaConnectionEvent *event)
{
	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	g_return_if_fail (cnc->priv);
	g_return_if_fail (GDA_IS_CONNECTION_EVENT (event));

	cnc->priv->events_list = g_list_append (cnc->priv->events_list, event);

	if (gda_connection_event_get_event_type (event) == GDA_CONNECTION_EVENT_ERROR)
		g_signal_emit (G_OBJECT (cnc), gda_connection_signals[ERROR], 0, event);
}

/**
 * gda_connection_add_event_string
 * @cnc: a #GdaServerConnection object.
 * @str: a format string (see the printf(3) documentation).
 * @...: the arguments to insert in the error message.
 *
 * Adds a new error to the given connection object. This is just a convenience
 * function that simply creates a #GdaConnectionEvent and then calls
 * #gda_server_connection_add_error.
 */
void
gda_connection_add_event_string (GdaConnection *cnc, const gchar *str, ...)
{
	GdaConnectionEvent *error;

	va_list args;
	gchar sz[2048];

	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	g_return_if_fail (cnc->priv);
	g_return_if_fail (str != NULL);

	/* build the message string */
	va_start (args, str);
	vsprintf (sz, str, args);
	va_end (args);
	
	error = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);
	gda_connection_event_set_description (error, sz);
	gda_connection_event_set_code (error, -1);
	gda_connection_event_set_source (error, gda_connection_get_provider (cnc));
	gda_connection_event_set_sqlstate (error, "-1");
	
	gda_connection_add_event (cnc, error);
}

/**
 * gda_connection_add_events_list
 * @cnc: a #GdaConnection object.
 * @events_list: a list of #GdaConnectionEvent.
 *
 * This is just another convenience function which lets you add
 * a list of #GdaConnectionEvent's to the given connection.*
 * As with
 * #gda_connection_add_event and #gda_connection_add_event_string,
 * this function makes the connection object emit the "error"
 * signal for each error event.
 *
 * @events_list is copied to an internal list and freed.
 */
void
gda_connection_add_events_list (GdaConnection *cnc, GList *events_list)
{
	GList *l;

	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	g_return_if_fail (cnc->priv);
	g_return_if_fail (events_list != NULL);

	cnc->priv->events_list = g_list_concat (cnc->priv->events_list, events_list);

	/* notify errors */
	for (l = events_list; l ; l = g_list_next (l))
		if (gda_connection_event_get_event_type (GDA_CONNECTION_EVENT (l->data)) ==
		    GDA_CONNECTION_EVENT_ERROR)
			g_signal_emit (G_OBJECT (cnc), gda_connection_signals[ERROR], 0, l->data);

	g_list_free (events_list);
}

/**
 * gda_connection_clear_events_list
 * @cnc: a #GdaConnection object.
 *
 * This function lets you clear the list of #GdaConnectionEvent's of the
 * given connection. 
 */
void
gda_connection_clear_events_list (GdaConnection *cnc)
{
	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	g_return_if_fail (cnc->priv);
	
	if (cnc->priv->events_list != NULL) {
		gda_connection_event_list_free (cnc->priv->events_list);
		cnc->priv->events_list =  NULL;
	}
}


/**
 * gda_connection_change_database
 * @cnc: a #GdaConnection object.
 * @name: name of database to switch to.
 *
 * Changes the current database for the given connection. This operation
 * is not available in all providers.
 *
 * Returns: %TRUE if successful, %FALSE otherwise.
 */
gboolean
gda_connection_change_database (GdaConnection *cnc, const gchar *name)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);

	return gda_server_provider_change_database (cnc->priv->provider_obj, cnc, name);
}

/**
 * gda_connection_create_table
 * @cnc: a #GdaConnection object.
 * @table_name: name of the table to be created.
 * @attributes_list: list of #GdaColumn for all fields in the table.
 * @index_list: list of #GdaDataModelIndex of all (multi column) index entries of a table.
 *
 * Creates a table on the given connection from the specified set of fields. In case of
 * none or single column index only, the various index fields can be set in #GdaColumn
 * and @index_list can be set to NULL. Index related values set in #GdaColumn
 * should not overlap settings in @index_list. A table can only have one Primary Key for instance.
 * Either set it in #GdaColumn that is part of @attributes_list or in 
 * #GdaColumnIndex that is part of @index_list.
 *
 * Returns: %TRUE if successful, %FALSE otherwise.
 */
gboolean
gda_connection_create_table (GdaConnection *cnc, const gchar *table_name, const GList *attributes_list, const GList *index_list)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (cnc->priv, FALSE);
	g_return_val_if_fail (table_name != NULL, FALSE);
	g_return_val_if_fail (attributes_list != NULL, FALSE);

	return gda_server_provider_create_table (cnc->priv->provider_obj, cnc, table_name, attributes_list, index_list);
}

/**
 * gda_connection_drop_table
 * @cnc: a #GdaConnection object.
 * @table_name: name of the table to be removed.
 *
 * Drops a table from the database.
 *
 * Returns: %TRUE if successful, %FALSE otherwise.
 */
gboolean
gda_connection_drop_table (GdaConnection *cnc, const gchar *table_name)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (cnc->priv, FALSE);
	g_return_val_if_fail (table_name != NULL, FALSE);

	return gda_server_provider_drop_table (cnc->priv->provider_obj, cnc, table_name);
}

/**
 * gda_connection_create_index
 * @cnc: a #GdaConnection object.
 * @index: a #GdaDataModelIndex object containing all index details.
 * @table_name: name of the table to create index for.
 *
 * Creates an index for a given table on the given connection from the specified set of fields.
 *
 * Returns: %TRUE if successful, %FALSE otherwise.
 */
gboolean
gda_connection_create_index (GdaConnection *cnc, const GdaDataModelIndex *index, const gchar *table_name)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (cnc->priv, FALSE);
	g_return_val_if_fail (index != NULL, FALSE);
	g_return_val_if_fail (table_name != NULL, FALSE);

	return gda_server_provider_create_index (cnc->priv->provider_obj, cnc, index, table_name);
}

/**
 * gda_connection_drop_index
 * @cnc: a #GdaConnection object.
 * @index_name: name of the index to be removed.
 * @primary_key: if the index is a primary key.
 * @table_name: name of the table of index to be removed from.
 *
 * Drops an index from a table from the database. Some data providers do not allow to assign a name
 * to a PRIMARY KEY. To be able to drop a PRIMARY KEY, the parameter @primary_key can be set to %TRUE.
 *
 * Returns: %TRUE if successful, %FALSE otherwise.
 */
gboolean
gda_connection_drop_index (GdaConnection *cnc, const gchar *index_name, gboolean primary_key, const gchar *table_name)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (cnc->priv, FALSE);
	g_return_val_if_fail (index_name != NULL, FALSE);
	g_return_val_if_fail (table_name != NULL, FALSE);

	return gda_server_provider_drop_index (cnc->priv->provider_obj, cnc, index_name, primary_key, table_name);
}

/**
 * gda_connection_execute_command_l
 * @cnc: a #GdaConnection object.
 * @cmd: a #GdaCommand.
 * @params: parameter list for the commands
 * @error: a place to store an error, or %NULL
 *
 * This function provides the way to send several commands
 * at once to the data source being accessed by the given
 * #GdaConnection object. The #GdaCommand specified can contain
 * a list of commands in its "text" property (usually a set
 * of SQL commands separated by ';').
 *
 * The return value is a GList of #GdaDataModel's, which you
 * are responsible to free when not needed anymore (and unref the
 * data models when they are not used anymore). Note that some nodes of the
 * returned list may actually not point to a GdaDataModel but may be NULL (which
 * corresponds to a command which did not return a data set, like UPDATE commands).
 *
 * The @params can contain the following parameters:
 * <itemizedlist>
 *   <listitem><para>a "ITER_MODEL_ONLY" parameter of type #GDA_VALUE_TYPE_BOOLEAN which, if set to TRUE
 *             will preferably return a data model which can be accessed only using an iterator.</para></listitem>
 * </itemizedlist>
 *
 * Returns: a list of #GdaDataModel's, as returned by the underlying
 * provider, or %NULL if an error occured.
 */
GList *
gda_connection_execute_command_l (GdaConnection *cnc, GdaCommand *cmd,
				  GdaParameterList *params, GError **error)
{
	GList *retval, *events;
	gboolean has_error = FALSE;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cnc->priv, NULL);
	g_return_val_if_fail (cmd != NULL, NULL);

	/* clean any previous connection events */
	gda_connection_clear_events_list (cnc);

	/* execute the command on the provider */
	retval = gda_server_provider_execute_command (cnc->priv->provider_obj,
						      cnc, cmd, params);

	/* make an error if necessary */
	events = cnc->priv->events_list;
	while (events && !has_error) {
		if (gda_connection_event_get_event_type (GDA_CONNECTION_EVENT (events->data)) == 
		    GDA_CONNECTION_EVENT_ERROR) {
			g_set_error (error, 0, 0,
				     gda_connection_event_get_description (GDA_CONNECTION_EVENT (events->data)));
			has_error = TRUE;
		}
		events = g_list_next (events);
	}
	if (has_error) {
		g_list_foreach (retval, (GFunc) g_object_unref, NULL);
		g_list_free (retval);
		retval = NULL;
	}

	return retval;
}

/**
 * gda_connection_get_last_insert_id
 * @cnc: a #GdaConnection object.
 * @recset: recordset.
 *
 * Retrieve from the given #GdaConnection the ID of the last inserted row.
 * A connection must be specified, and, optionally, a result set. If not NULL,
 * the underlying provider should try to get the last insert ID for the given result set.
 *
 * Returns: a string representing the ID of the last inserted row, or NULL
 * if an error occurred or no row has been inserted. It is the caller's
 * reponsibility to free the returned string.
 */
gchar *
gda_connection_get_last_insert_id (GdaConnection *cnc, GdaDataModel *recset)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cnc->priv, NULL);

	return gda_server_provider_get_last_insert_id (cnc->priv->provider_obj, cnc, recset);
}

/**
 * gda_connection_execute_command
 * @cnc: a #GdaConnection object.
 * @cmd: a #GdaCommand.
 * @params: parameter list for the command
 * @error: a place to store an error, or %NULL
 *
 * Executes a single command on the given connection.
 *
 * This function lets you retrieve a simple data model from
 * the underlying difference, instead of having to retrieve
 * a list of them, as is the case with #gda_connection_execute_command_l.
 *
 * Note that if the @cmd command is composed of several SQL statements, the data model
 * returned is the one corresponding to the last statement.
 *
 * See the documentation of the gda_connection_execute_command_l() for information
 * about the @params list of parameters.
 *
 * Returns: a #GdaDataModel containing the data returned by the
 * data source, %NULL if no data was expected, or GDA_CONNECTION_EXEC_FAILED if an error occured.
 */
GdaDataModel *
gda_connection_execute_command (GdaConnection *cnc, GdaCommand *cmd,
				GdaParameterList *params, GError **error)
{
	GList *reclist, *list;
	GdaDataModel *model;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cnc->priv, NULL);
	g_return_val_if_fail (cmd != NULL, NULL);

	reclist = gda_connection_execute_command_l (cnc, cmd, params, error);
	if (!reclist)
		return GDA_CONNECTION_EXEC_FAILED;

	model = GDA_DATA_MODEL (g_list_last (reclist)->data);
	if (model) {
		GdaConnectionEvent *event;
		gchar *str;
		gint nb = gda_data_model_get_n_rows (model);

		event = gda_connection_event_new (GDA_CONNECTION_EVENT_NOTICE);
		if (nb > 1)
			str = g_strdup_printf ("(%d rows)", nb);
		else
			str = g_strdup_printf ("(%d row)", nb);
		gda_connection_event_set_description (event, str);
		g_free (str);
		gda_connection_add_event (cnc, event);

		g_object_ref (G_OBJECT (model));
	}

	list = reclist;
	for (list = reclist; list; list = g_list_next (list))
		if (list->data)
			g_object_unref (list->data);
	g_list_free (reclist);

	return model;
}

/**
 * gda_connection_begin_transaction
 * @cnc: a #GdaConnection object.
 * @xaction: a #GdaTransaction object.
 *
 * Starts a transaction on the data source, identified by the
 * @xaction parameter.
 *
 * Before starting a transaction, you can check whether the underlying
 * provider does support transactions or not by using the
 * #gda_connection_supports function.
 *
 * Returns: %TRUE if the transaction was started successfully, %FALSE
 * otherwise.
 */
gboolean
gda_connection_begin_transaction (GdaConnection *cnc, GdaTransaction *xaction)
{
	gboolean retval;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (cnc->priv, FALSE);
	g_return_val_if_fail (GDA_IS_TRANSACTION (xaction), FALSE);

	retval = gda_server_provider_begin_transaction (cnc->priv->provider_obj, cnc, xaction);
	if (retval)
		gda_client_notify_transaction_started_event (cnc->priv->client, cnc, xaction);

	return retval;
}

/**
 * gda_connection_commit_transaction
 * @cnc: a #GdaConnection object.
 * @xaction: a #GdaTransaction object.
 *
 * Commits the given transaction to the backend database.  You need to do 
 * gda_connection_begin_transaction() first.
 *
 * Returns: %TRUE if the transaction was finished successfully,
 * %FALSE otherwise.
 */
gboolean
gda_connection_commit_transaction (GdaConnection *cnc, GdaTransaction *xaction)
{
	gboolean retval;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (cnc->priv, FALSE);
	g_return_val_if_fail (GDA_IS_TRANSACTION (xaction), FALSE);

	retval = gda_server_provider_commit_transaction (cnc->priv->provider_obj, cnc, xaction);
	if (retval)
		gda_client_notify_transaction_committed_event (cnc->priv->client, cnc, xaction);

	return retval;
}

/**
 * gda_connection_rollback_transaction
 * @cnc: a #GdaConnection object.
 * @xaction: a #GdaTransaction object.
 *
 * Rollbacks the given transaction. This means that all changes
 * made to the underlying data source since the last call to
 * #gda_connection_begin_transaction or #gda_connection_commit_transaction
 * will be discarded.
 *
 * Returns: %TRUE if the operation was successful, %FALSE otherwise.
 */
gboolean
gda_connection_rollback_transaction (GdaConnection *cnc, GdaTransaction *xaction)
{
	gboolean retval;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (cnc->priv, FALSE);
	g_return_val_if_fail (GDA_IS_TRANSACTION (xaction), FALSE);

	retval = gda_server_provider_rollback_transaction (cnc->priv->provider_obj, cnc, xaction);
	if (retval)
		gda_client_notify_transaction_cancelled_event (cnc->priv->client, cnc, xaction);

	return retval;
}

/**
 * gda_connection_supports
 * @cnc: a #GdaConnection object.
 * @feature: feature to ask for.
 *
 * Asks the underlying provider for if a specific feature is supported.
 *
 * Returns: %TRUE if the provider supports it, %FALSE if not.
 */
gboolean
gda_connection_supports (GdaConnection *cnc, GdaConnectionFeature feature)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (cnc->priv, FALSE);

	return gda_server_provider_supports (cnc->priv->provider_obj, cnc, feature);
}

/**
 * gda_connection_get_schema
 * @cnc: a #GdaConnection object.
 * @schema: database schema to get.
 * @params: parameter list.
 * @error: a place to store errors, or %NULL
 *
 * Asks the underlying data source for a list of database objects.
 *
 * This is the function that lets applications ask the different
 * providers about all their database objects (tables, views, procedures,
 * etc). The set of database objects that are retrieved are given by the
 * 2 parameters of this function: @schema, which specifies the specific
 * schema required, and @params, which is a list of parameters that can
 * be used to give more detail about the objects to be returned.
 *
 * The list of parameters is specific to each schema type.
 *
 * Returns: a #GdaDataModel containing the data required. The caller is responsible
 * of freeing the returned model.
 */
GdaDataModel *
gda_connection_get_schema (GdaConnection *cnc,
			   GdaConnectionSchema schema,
			   GdaParameterList *params,
			   GError **error)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	return gda_server_provider_get_schema (cnc->priv->provider_obj, cnc, schema, params, error);
}

/**
 * gda_connection_get_events
 * @cnc: a #GdaConnection.
 *
 * Retrieves a list of the last errors ocurred in the connection.
 * You can make a copy of the list using #gda_connection_event_list_copy.
 * 
 * Returns: a GList of #GdaConnectionEvent.
 *
 */
const GList *
gda_connection_get_events (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cnc->priv, FALSE);

	return cnc->priv->events_list;
}

/**
 * gda_connection_create_blob
 * @cnc: a #GdaConnection object.
 * @blob: a user-allocated #GdaBlob structure.
 *
 * Creates a BLOB (Binary Large OBject) with read/write access.
 *
 * Returns: %FALSE if the database does not support BLOBs. %TRUE otherwise
 * and the GdaBlob is created and ready to be used.
 */
GdaBlob *
gda_connection_create_blob (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (cnc->priv, FALSE);

	return gda_server_provider_create_blob (cnc->priv->provider_obj, cnc);
}

/**
 * gda_connection_create_blob
 * @cnc: a #GdaConnection object.
 * @blob: a user-allocated #GdaBlob structure.
 * @sql_id: the SQL ID of the blob to fetch
 *
 * Fetch an existing BLOB (Binary Large OBject) using its SQL ID.
 *
 * Returns: %FALSE if the database does not support BLOBs. %TRUE otherwise
 * and the GdaBlob is created and ready to be used.
 */
GdaBlob *
gda_connection_fetch_blob_by_id (GdaConnection *cnc, const gchar *sql_id)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (cnc->priv, FALSE);
	g_return_val_if_fail (sql_id, FALSE);

	return gda_server_provider_fetch_blob_by_id (cnc->priv->provider_obj, cnc, sql_id);
}

/**
 * gda_connection_value_to_sql_string
 * @cnc: a #GdaConnection object.
 * @from: #GdaValue to convert from
 *
 * Produces a fully quoted and escaped string from a GdaValue
 *
 * Returns: escaped and quoted value or NULL if not supported.
 */
gchar *
gda_connection_value_to_sql_string (GdaConnection *cnc, GdaValue *from)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (cnc->priv, FALSE);
	g_return_val_if_fail (from != NULL, FALSE);

	/* execute the command on the provider */
	return gda_server_provider_value_to_sql_string (cnc->priv->provider_obj, cnc, from);
}
