/* GDA library
 * Copyright (C) 1998 - 2008 The GNOME Foundation.
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

#undef GDA_DISABLE_DEPRECATED
#include <stdio.h>
#include <libgda/gda-client.h>
#include <libgda/gda-client-private.h>
#include <libgda/gda-config.h>
#include <libgda/gda-connection.h>
#include <libgda/gda-connection-private.h>
#include <libgda/gda-connection-event.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-log.h>
#include <libgda/gda-server-provider.h>
#include "gda-marshal.h"
#include <libgda/gda-transaction-status-private.h>
#include <string.h>
#include <libgda/gda-enum-types.h>
#include <libgda/gda-holder.h>
#include <libgda/gda-set.h>
#include <sql-parser/gda-sql-parser.h>
#include <sql-parser/gda-statement-struct-trans.h>

#define PROV_CLASS(provider) (GDA_SERVER_PROVIDER_CLASS (G_OBJECT_GET_CLASS (provider)))

struct _GdaConnectionPrivate {
	GdaClient            *client;
	GdaServerProvider    *provider_obj;
	GdaConnectionOptions  options; /* ORed flags */
	gchar                *dsn;
	gchar                *cnc_string;
	gchar                *auth_string;
	gboolean              is_open;

	GdaMetaStore         *meta_store;
	GList                *events_list;
	GList                *recset_list;

	GdaTransactionStatus *trans_status;
	GHashTable           *prepared_stmts;

	gpointer              provider_data;
	GDestroyNotify        provider_data_destroy_func;
};

static void gda_connection_class_init (GdaConnectionClass *klass);
static void gda_connection_init       (GdaConnection *cnc, GdaConnectionClass *klass);
static void gda_connection_dispose    (GObject *object);
static void gda_connection_finalize   (GObject *object);
static void gda_connection_set_property (GObject *object,
					 guint param_id,
					 const GValue *value,
					 GParamSpec *pspec);
static void gda_connection_get_property (GObject *object,
					 guint param_id,
					 GValue *value,
					 GParamSpec *pspec);

enum {
	ERROR,
	CONN_OPENED,
        CONN_TO_CLOSE,
        CONN_CLOSED,
	DSN_CHANGED,
	TRANSACTION_STATUS_CHANGED,
	LAST_SIGNAL
};

static gint gda_connection_signals[LAST_SIGNAL] = { 0, 0, 0, 0, 0, 0 };

/* properties */
enum
{
        PROP_0,
	PROP_CLIENT,
        PROP_DSN,
        PROP_CNC_STRING,
        PROP_PROVIDER_OBJ,
        PROP_AUTH_STRING,
        PROP_OPTIONS,
	PROP_META_STORE
};

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
                              G_SIGNAL_RUN_LAST,
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
	gda_connection_signals[TRANSACTION_STATUS_CHANGED] =
		g_signal_new ("transaction_status_changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaConnectionClass, transaction_status_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	/* Properties */
        object_class->set_property = gda_connection_set_property;
        object_class->get_property = gda_connection_get_property;

	g_object_class_install_property (object_class, PROP_CLIENT,
                                         g_param_spec_object ("client", _("GdaClient to use"), NULL,
                                                               GDA_TYPE_CLIENT,
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_DSN,
                                         g_param_spec_string ("dsn", _("DSN to use"), NULL, NULL,
							      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_CNC_STRING,
                                         g_param_spec_string ("cnc_string", _("Connection string to use"), NULL, NULL,
							      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_PROVIDER_OBJ,
                                         g_param_spec_object ("provider_obj", _("Provider to use"), NULL,
                                                               GDA_TYPE_SERVER_PROVIDER,
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));

        g_object_class_install_property (object_class, PROP_AUTH_STRING,
                                         g_param_spec_string ("auth_string", _("Authentification string to use"),
                                                              NULL, NULL,
                                                              (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property (object_class, PROP_OPTIONS,
                                         g_param_spec_flags ("options", _("Options (connection sharing)"),
							    NULL, GDA_TYPE_CONNECTION_OPTIONS, GDA_CONNECTION_OPTIONS_NONE,
							    (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property (object_class, PROP_META_STORE,
					 g_param_spec_object ("meta-store", _ ("GdaMetaStore used by the connection"),
							      NULL, GDA_TYPE_META_STORE,
							      (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	
	object_class->dispose = gda_connection_dispose;
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
	cnc->priv->auth_string = NULL;
	cnc->priv->is_open = FALSE;
	cnc->priv->events_list = NULL;
	cnc->priv->recset_list = NULL;
	cnc->priv->trans_status = NULL; /* no transaction yet */
}

static void prepared_stms_foreach_func (GdaStatement *gda_stmt, GdaStatement *prepared_stmt, GdaConnection *cnc);
static void
gda_connection_dispose (GObject *object)
{
	GdaConnection *cnc = (GdaConnection *) object;

	g_return_if_fail (GDA_IS_CONNECTION (cnc));

	/* free memory */
	gda_connection_close_no_warning (cnc);

	if (cnc->priv->prepared_stmts) {
		g_hash_table_foreach (cnc->priv->prepared_stmts, (GHFunc) prepared_stms_foreach_func, cnc);
		g_hash_table_destroy (cnc->priv->prepared_stmts);
		cnc->priv->prepared_stmts = NULL;
	}

	if (cnc->priv->provider_obj) {
		g_object_unref (G_OBJECT (cnc->priv->provider_obj));
		cnc->priv->provider_obj = NULL;
	}

	if (cnc->priv->events_list)
		gda_connection_event_list_free (cnc->priv->events_list);

	if (cnc->priv->recset_list)
		g_list_foreach (cnc->priv->recset_list, (GFunc) g_object_unref, NULL);
	
	if (cnc->priv->trans_status) {
		g_object_unref (cnc->priv->trans_status);
		cnc->priv->trans_status = NULL;
	}

	/* chain to parent class */
	parent_class->dispose (object);
}

static void
gda_connection_finalize (GObject *object)
{
	GdaConnection *cnc = (GdaConnection *) object;

	g_return_if_fail (GDA_IS_CONNECTION (cnc));

	/* free memory */
	g_free (cnc->priv->dsn);
	g_free (cnc->priv->cnc_string);
	g_free (cnc->priv->auth_string);

	g_free (cnc->priv);
	cnc->priv = NULL;

	/* chain to parent class */
	parent_class->finalize (object);
}

/* module error */
GQuark gda_connection_error_quark (void)
{
        static GQuark quark;
        if (!quark)
                quark = g_quark_from_static_string ("gda_connection_error");
        return quark;
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

	if (G_UNLIKELY (type == 0)) {
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
		type = g_type_register_static (G_TYPE_OBJECT, "GdaConnection", &info, 0);
	}

	return type;
}

static void
gda_connection_set_property (GObject *object,
			     guint param_id,
			     const GValue *value,
			     GParamSpec *pspec)
{
	GdaConnection *cnc;

        cnc = GDA_CONNECTION (object);
        if (cnc->priv) {
                switch (param_id) {
                case PROP_CLIENT:
                        if (cnc->priv->client)
				g_object_unref(cnc->priv->client);

			cnc->priv->client = g_value_get_object (value);
			if (cnc->priv->client)
				g_object_ref (cnc->priv->client);
			break;
                case PROP_DSN:
			gda_connection_set_dsn (cnc, g_value_get_string (value));
                        break;
                case PROP_CNC_STRING:
			g_free (cnc->priv->cnc_string);
			cnc->priv->cnc_string = NULL;
			if (g_value_get_string (value)) 
				cnc->priv->cnc_string = g_strdup (g_value_get_string (value));
                        break;
                case PROP_PROVIDER_OBJ:
                        if (cnc->priv->provider_obj)
				g_object_unref(cnc->priv->provider_obj);

			cnc->priv->provider_obj = g_value_get_object (value);
			g_object_ref (G_OBJECT (cnc->priv->provider_obj));
                        break;
                case PROP_AUTH_STRING:
			if (! cnc->priv->is_open) {
				const gchar *str = g_value_get_string (value);
				g_free (cnc->priv->auth_string);
				cnc->priv->auth_string = NULL;
				if (str)
					cnc->priv->auth_string = g_strdup (str);
			}
                        break;
                case PROP_OPTIONS:
			cnc->priv->options = g_value_get_flags (value);
			break;
		case PROP_META_STORE:
			if (cnc->priv->meta_store) {
				g_object_unref (cnc->priv->meta_store);
				cnc->priv->meta_store = NULL;
			}
			cnc->priv->meta_store = g_value_get_object (value);
			if (cnc->priv->meta_store)
				g_object_ref (cnc->priv->meta_store);
			break;
                }
        }	
}

static void
gda_connection_get_property (GObject *object,
			     guint param_id,
			     GValue *value,
			     GParamSpec *pspec)
{
	GdaConnection *cnc;

        cnc = GDA_CONNECTION (object);
        if (cnc->priv) {
                switch (param_id) {
                case PROP_CLIENT:
			g_value_set_object (value, G_OBJECT (cnc->priv->client));
			break;
                case PROP_DSN:
			g_value_set_string (value, cnc->priv->dsn);
                        break;
                case PROP_CNC_STRING:
			g_value_set_string (value, cnc->priv->cnc_string);
			break;
                case PROP_PROVIDER_OBJ:
			g_value_set_object (value, G_OBJECT (cnc->priv->provider_obj));
                        break;
                case PROP_AUTH_STRING:
			g_value_set_string (value, cnc->priv->auth_string);
                        break;
                case PROP_OPTIONS:
			g_value_set_flags (value, cnc->priv->options);
			break;
		case PROP_META_STORE:
			g_value_set_object (value, cnc->priv->meta_store);
			break;
                }
        }	
}

/**
 * gda_connection_open
 * @cnc: a #GdaConnection object
 * @error: a place to store errors, or %NULL
 *
 * Tries to open the connection.
 *
 * Returns: TRUE if the connection is opened, and FALSE otherwise.
 */
gboolean
gda_connection_open (GdaConnection *cnc, GError **error)
{
	GdaDataSourceInfo *dsn_info = NULL;
	GdaQuarkList *params, *auth;
	char *real_auth_string = NULL;

	g_return_val_if_fail (cnc && GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (cnc->priv, FALSE);

	/* don't do anything if connection is already opened */
	if (cnc->priv->is_open)
		return TRUE;

	/* connection string */
	if (cnc->priv->dsn) {
		/* get the data source info */
		dsn_info = gda_config_get_dsn (cnc->priv->dsn);
		if (!dsn_info) {
			gda_log_error (_("Data source %s not found in configuration"), cnc->priv->dsn);
			g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_NONEXIST_DSN_ERROR,
				     _("Data source %s not found in configuration"), cnc->priv->dsn);
			return FALSE;
		}

		g_free (cnc->priv->cnc_string);
		cnc->priv->cnc_string = g_strdup (dsn_info->cnc_string);
	}
	else {
		if (!cnc->priv->cnc_string) {
			gda_log_error (_("No DSN or connection string specified"));
			g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_NO_CNC_SPEC_ERROR,
				     _("No DSN or connection string specified"));
			return FALSE;
		}
		/* try to see if connection string has the <provider>://<rest of the string> format */
	}

	/* provider test */
	if (!cnc->priv->provider_obj) {
		gda_log_error (_("No provider specified"));
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_NO_PROVIDER_SPEC_ERROR,
			     _("No provider specified"));
		return FALSE;
	}

	params = gda_quark_list_new_from_string (cnc->priv->cnc_string);

	/* retrieve correct auth_string */
	if (cnc->priv->auth_string)
		real_auth_string = g_strdup (cnc->priv->auth_string);
	else {
		if (dsn_info && dsn_info->auth_string)
			real_auth_string = g_strdup (dsn_info->auth_string);
		else 
			/* look for authentification parameters in cnc string */
			real_auth_string = g_strdup (cnc->priv->cnc_string);
	}

	/* try to open the connection */
	auth = gda_quark_list_new_from_string (real_auth_string);
	if (gda_server_provider_open_connection (cnc->priv->provider_obj, cnc, params, auth)) {
		cnc->priv->is_open = TRUE;
		_gda_client_notify_connection_opened_event (cnc->priv->client, cnc);
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
						g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_OPEN_ERROR,
							     gda_connection_event_get_description (event));
					_gda_client_notify_error_event (cnc->priv->client, cnc, 
									GDA_CONNECTION_EVENT (l->data));
				}
			}
		}

		cnc->priv->is_open = FALSE;
	}

	/* free memory */
	gda_quark_list_free (params);
	gda_quark_list_free (auth);
	g_free (real_auth_string);

	if (cnc->priv->is_open) {
#ifdef GDA_DEBUG_signal
        g_print (">> 'CONN_OPENED' from %s\n", __FUNCTION__);
#endif
        g_signal_emit (G_OBJECT (cnc), gda_connection_signals[CONN_OPENED], 0);
#ifdef GDA_DEBUG_signal
        g_print ("<< 'CONN_OPENED' from %s\n", __FUNCTION__);
#endif
	}

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
	_gda_client_notify_connection_closed_event (cnc->priv->client, cnc);
	cnc->priv->is_open = FALSE;

	if (cnc->priv->provider_data) {
		if (cnc->priv->provider_data_destroy_func)
			cnc->priv->provider_data_destroy_func (cnc->priv->provider_data);
		else
			g_warning ("Provider did not clean its connection data");
		cnc->priv->provider_data = NULL;
	}

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

	return cnc->priv->provider_obj;
}

/**
 * gda_connection_get_provider_name
 * @cnc: a #GdaConnection object
 *
 * Get the name (identifier) of the database provider used by @cnc
 *
 * Returns: a non modifiable string
 */
const gchar *
gda_connection_get_provider_name (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cnc->priv, NULL);
	if (!cnc->priv->provider_obj)
		return NULL;

	return gda_server_provider_get_name (cnc->priv->provider_obj);
}

/**
 * gda_connection_set_dsn
 * @cnc: a #GdaConnection object
 * @datasource: a gda datasource
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

        dsn = gda_config_get_dsn (datasource);
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
 * gda_connection_get_authentification
 * @cnc: a #GdaConnection object.
 *
 * Gets the user name used to open this connection.
 *
 * Returns: the user name.
 */
const gchar *
gda_connection_get_authentification (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cnc->priv, NULL);

	return (const gchar *) cnc->priv->auth_string;
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
	static gint debug = -1;
	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	g_return_if_fail (cnc->priv);
	g_return_if_fail (GDA_IS_CONNECTION_EVENT (event));

	if (debug == -1) {
		const gchar *str;
		debug = 0;
		str = getenv ("GDA_CONNECTION_EVENTS_SHOW");
		if (str) {
			gchar **array;
			gint i;
			array = g_strsplit_set (str, " ,/;:", 0);
			for (i = 0; i < g_strv_length (array); i++) {
				if (!g_ascii_strcasecmp (array[i], "notice"))
					debug += 1;
				else if (!g_ascii_strcasecmp (array[i], "warning"))
					debug += 2;
				else if (!g_ascii_strcasecmp (array[i], "error"))
					debug += 4;
				else if (!g_ascii_strcasecmp (array[i], "command"))
					debug += 8;
			}
			g_strfreev (array);
		}
	}

	cnc->priv->events_list = g_list_append (cnc->priv->events_list, event);

	if (debug > 0) {
		const gchar *str = NULL;
		switch (gda_connection_event_get_event_type (event)) {
		case GDA_CONNECTION_EVENT_NOTICE:
			if (debug & 1) str = "NOTICE";
			break;
		case GDA_CONNECTION_EVENT_WARNING:
			if (debug & 2) str = "WARNING";
			break;
		case GDA_CONNECTION_EVENT_ERROR:
			if (debug & 4) str = "ERROR";
			break;
		case GDA_CONNECTION_EVENT_COMMAND:
			if (debug & 8) str = "COMMAND";
			break;
		default:
			break;
		}
		if (str)
			g_print ("EVENT> %s: %s (on cnx %p, %s)\n", str, 
				 gda_connection_event_get_description (event), cnc,
				 gda_connection_event_get_sqlstate (event));
	}

	if (gda_connection_event_get_event_type (event) == GDA_CONNECTION_EVENT_ERROR)
		g_signal_emit (G_OBJECT (cnc), gda_connection_signals[ERROR], 0, event);
}

/**
 * gda_connection_add_event_string
 * @cnc: a #GdaConnection object.
 * @str: a format string (see the printf(3) documentation).
 * @...: the arguments to insert in the error message.
 *
 * Adds a new error to the given connection object. This is just a convenience
 * function that simply creates a #GdaConnectionEvent and then calls
 * #gda_server_connection_add_error.
 *
 * Returns: a new #GdaConnectionEvent object, however the caller does not hold a reference to the returned
 * object, and if need be the caller must call g_object_ref() on it.
 */
GdaConnectionEvent *
gda_connection_add_event_string (GdaConnection *cnc, const gchar *str, ...)
{
	GdaConnectionEvent *error;

	va_list args;
	gchar sz[2048];

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cnc->priv, NULL);
	g_return_val_if_fail (str != NULL, NULL);

	/* build the message string */
	va_start (args, str);
	vsprintf (sz, str, args);
	va_end (args);
	
	error = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);
	gda_connection_event_set_description (error, sz);
	gda_connection_event_set_code (error, -1);
	gda_connection_event_set_source (error, gda_connection_get_provider_name (cnc));
	gda_connection_event_set_sqlstate (error, "-1");
	
	gda_connection_add_event (cnc, error);

	return error;
}

/** TODO: This actually frees the input GList. That's very very unusual. murrayc. */

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
 * gda_connection_create_parser
 * @cnc: a #GdaConnection object
 *
 * Creates a new parser object able to parse the SQL dialect understood by @cnc. 
 * If the #GdaServerProvider object internally used by @cnc does not have its own parser, 
 * then %NULL is returned, and a general SQL parser can be obtained
 * using gda_sql_parser_new().
 *
 * Returns: a new #GdaSqlParser object
 */
GdaSqlParser *
gda_connection_create_parser (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cnc->priv, NULL);
	g_return_val_if_fail (cnc->priv->provider_obj, NULL);

	return gda_server_provider_create_parser (cnc->priv->provider_obj, cnc);
}

/**
 * gda_connection_statement_to_sql
 * @cnc: a #GdaConnection object
 * @stmt: a #GdaStatement object
 * @params: a #GdaSet object (which can be obtained using gda_statement_get_parameters()), or %NULL
 * @flags: SQL rendering flags, as #GdaStatementSqlFlag OR'ed values
 * @params_used: a place to store the list of individual #GdaHolder objects within @params which have been used
 * @error: a place to store errors, or %NULL
 *
 * Renders @stmt as an SQL statement, adapted to the SQL dialect used by @cnc
 *
 * Returns: a new string, or %NULL if an error occurred
 */
gchar *
gda_connection_statement_to_sql (GdaConnection *cnc, GdaStatement *stmt, GdaSet *params, GdaStatementSqlFlag flags,
				 GSList **params_used, GError **error)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cnc->priv, NULL);
	g_return_val_if_fail (cnc->priv->provider_obj, NULL);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);

	if (PROV_CLASS (cnc->priv->provider_obj)->statement_to_sql)
		return (PROV_CLASS (cnc->priv->provider_obj)->statement_to_sql) (cnc->priv->provider_obj, 
										 cnc, stmt, params, flags, 
										 params_used, error);
	else
		return gda_statement_to_sql_extended (stmt, cnc, params, flags, params_used, error);
}

/**
 * gda_connection_statement_prepare
 * @cnc: a #GdaConnection
 * @stmt: a #GdaStatement object
 * @error: a place to store errors, or %NULL
 *
 * Ask the database accessed through the @cnc connection to prepare the usage of @stmt. This is only usefull
 * if @stmt will be used more than once (however some database providers may always prepare stamements 
 * before executing them).
 *
 * This function is also usefull to make sure @stmt is fully understood by the database before actually executing it.
 *
 * Note however that it is also possible that gda_connection_statement_prepare() fails when
 * gda_connection_statement_execute() does not fail (this will usually be the case with statements such as
 * <![CDATA["SELECT * FROM ##tablename::string"]]> because database usually don't allow variables to be used in place of a 
 * table name).
 *
 * Returns: TRUE if no error occurred.
 */
gboolean
gda_connection_statement_prepare (GdaConnection *cnc, GdaStatement *stmt, GError **error)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (cnc->priv, FALSE);
	g_return_val_if_fail (cnc->priv->provider_obj, FALSE);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), FALSE);

	if (PROV_CLASS (cnc->priv->provider_obj)->statement_prepare)
		return (PROV_CLASS (cnc->priv->provider_obj)->statement_prepare)(cnc->priv->provider_obj, 
										 cnc, stmt, error);
	else {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_METHOD_NON_IMPLEMENTED_ERROR,
			     _("Provider does not support statement preparation"));
		return FALSE;
	}
}

static GType *
make_col_types_array (gint init_size, va_list args)
{
	GType *types;
	gint max = 10;
	gint col;

	types = g_new (GType, max + 1);
	for (col = 0; col <= max; col ++)
		types[col] = G_TYPE_NONE;
	for (col = va_arg (args, gint); col >= 0; col = va_arg (args, gint)) {
		if (col > max) {
			gint i;
			types = g_renew (GType, types, col + 5 + 1);
			for (i = max; col <= col + 5; i ++)
				types[i] = G_TYPE_NONE;
			max = col + 5;
		}
		types [col] = va_arg (args, GType);
	}
	return types;
}

/*
 * Wrapper which adds @...
 */
static GObject *
gda_connection_statement_execute_v (GdaConnection *cnc, GdaStatement *stmt, GdaSet *params, 
				    GdaStatementModelUsage model_usage, GdaSet **last_inserted_row, GError **error, ...)
{
	va_list ap;
	GObject *obj;
	GType *types;
	va_start (ap, error);
	types = make_col_types_array (10, ap);
	va_end (ap);

	if (last_inserted_row) 
		*last_inserted_row = NULL;
	obj = PROV_CLASS (cnc->priv->provider_obj)->statement_execute (cnc->priv->provider_obj, cnc, stmt, params, 
								       model_usage, types, last_inserted_row, 
								       NULL, NULL, NULL, error);
	g_free (types);
	return obj;
}


/**
 * gda_connection_statement_execute
 * @cnc: a #GdaConnection
 * @stmt: a #GdaStatement object
 * @params: a #GdaSet object (which can be obtained using gda_statement_get_parameters()), or %NULL
 * @last_insert_row: a place to store a new #GdaSet object which contains the values of the last inserted row, or %NULL
 * @error: a place to store errors, or %NULL
 *
 * Executes @stmt. As @stmt can, by desing (and if not abused), contain only one SQL statement, the
 * return object will either be:
 * <itemizedlist>
 *   <listitem><para>a #GdaDataModel if @stmt is a SELECT statement (a GDA_SQL_STATEMENT_SELECT, see #GdaSqlStatementType)
 *             containing the results of the SELECT. The resulting data model is by default read only, but
 *             modifications can be made possible using gda_pmodel_set_modification_query() and/or
 *             gda_pmodel_compute_modification_queries().</para></listitem>
 *   <listitem><para>a #GdaSet for any other SQL statement which correctly executed. In this case
 *        (if the provider supports it), then the #GdaSet may contain value holders named:
 *        <itemizedlist>
 *          <listitem><para>a (gint) #GdaHolder named "IMPACTED_ROWS"</para></listitem>
 *          <listitem><para>a (GObject) #GdaHolder named "EVENT" which contains a #GdaConnectionEvent</para></listitem>
 *        </itemizedlist></para></listitem>
 * </itemizedlist>
 *
 * If @last_insert_row is not %NULL and @stmt is an INSERT statement, then it will contain (if the
 * provider used by @cnc supports it) a new #GdaSet object composed of value holders named "+&lt;column number&gt;"
 * starting at column 0 which contain the actual inserted values.
 *
 * Returns: a #GObject, or %NULL if an error occurred 
 */
GObject *
gda_connection_statement_execute (GdaConnection *cnc, GdaStatement *stmt, GdaSet *params, 
				  GdaStatementModelUsage model_usage, GdaSet **last_inserted_row, GError **error)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cnc->priv, NULL);
	g_return_val_if_fail (cnc->priv->provider_obj, NULL);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);
	g_return_val_if_fail (PROV_CLASS (cnc->priv->provider_obj)->statement_execute, NULL);

	if (last_inserted_row) 
		*last_inserted_row = NULL;

	return gda_connection_statement_execute_v (cnc, stmt, params, model_usage, last_inserted_row, error, -1);
}


/**
 * gda_connection_statement_execute_non_select
 * @cnc: a #GdaConnection object.
 * @stmt: a #GdaStatement object.
 * @params: a #GdaSet object (which can be obtained using gda_statement_get_parameters()), or %NULL
 * @last_insert_row: a place to store a new #GdaSet object which contains the values of the last inserted row, or %NULL
 * @error: a place to store an error, or %NULL
 *
 * Executes a non-selection statement on the given connection.
 *
 * This function returns the number of rows affected by the execution of @stmt, or -1
 * if an error occurred, or -2 if the connection's provider does not return the number of rows affected.
 *
 * This function is just a convenience function around the gda_connection_statement_execute()
 * function. 
 * See the documentation of the gda_connection_statement_execute() for information
 * about the @params list of parameters.
 *
 * If @last_insert_row is not %NULL and @stmt is an INSERT statement, then it will contain (if the
 * provider used by @cnc supports it) a new #GdaSet object composed of value holders named "+&lt;column number&gt;"
 * starting at column 0 which contain the actual inserted values.
 *
 * Returns: the number of rows affected (&gt;=0) or -1 or -2 
 */
gint
gda_connection_statement_execute_non_select (GdaConnection *cnc, GdaStatement *stmt,
					     GdaSet *params, GdaSet **last_insert_row, GError **error)
{
	GdaSet *set;
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), -1);
	g_return_val_if_fail (cnc->priv, -1);
	g_return_val_if_fail (cnc->priv->provider_obj, -1);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), -1);
	g_return_val_if_fail (PROV_CLASS (cnc->priv->provider_obj)->statement_execute, -1);

	if ((gda_statement_get_statement_type (stmt) == GDA_SQL_STATEMENT_SELECT) ||
	    (gda_statement_get_statement_type (stmt) == GDA_SQL_STATEMENT_COMPOUND)) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_STATEMENT_TYPE_ERROR,
			     _("Statement is a selection statement"));
		return -1;
	}
	
	if (last_insert_row)
		*last_insert_row = NULL;

	set = (GdaSet *) gda_connection_statement_execute_v (cnc, stmt, params, 
							     GDA_STATEMENT_MODEL_RANDOM_ACCESS, last_insert_row, 
							     error, -1);
	if (!set)
		return -1;
	
	if (!GDA_IS_SET (set)) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_STATEMENT_TYPE_ERROR,
			     _("Statement is a selection statement"));
		g_object_unref (set);
		return -1;
	}
	else {
		GdaHolder *h;
		gint retval = -2;

		h = gda_set_get_holder (set, "IMPACTED_ROWS");
		if (h) {
			const GValue *value;
			value = gda_holder_get_value (h);
			if (value && (G_VALUE_TYPE (value) == G_TYPE_INT))
				retval = g_value_get_int (value);
		}
		g_object_unref (set);
		return retval;
	}
}

/**
 * gda_connection_statement_execute_select
 * @cnc: a #GdaConnection object.
 * @stmt: a #GdaStatement object.
 * @params: a #GdaSet object (which can be obtained using gda_statement_get_parameters()), or %NULL
 * @error: a place to store an error, or %NULL
 *
 * Executes a selection command on the given connection.
 *
 * This function returns a #GdaDataModel resulting from the SELECT statement, or %NULL
 * if an error occurred.
 *
 * This function is just a convenience function around the gda_connection_statement_execute()
 * function.
 *
 * See the documentation of the gda_connection_statement_execute() for information
 * about the @params list of parameters.
 *
 * Returns: a #GdaDataModel containing the data returned by the
 * data source, or %NULL if an error occurred
 */
GdaDataModel *
gda_connection_statement_execute_select (GdaConnection *cnc, GdaStatement *stmt,
					 GdaSet *params, GError **error)
{
	GdaDataModel *model;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cnc->priv, NULL);
	g_return_val_if_fail (cnc->priv->provider_obj, NULL);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);
	g_return_val_if_fail (PROV_CLASS (cnc->priv->provider_obj)->statement_execute, NULL);

	model = (GdaDataModel *) gda_connection_statement_execute_v (cnc, stmt, params, 
								     GDA_STATEMENT_MODEL_RANDOM_ACCESS, NULL,
								     error, -1);
	if (model && !GDA_IS_DATA_MODEL (model)) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_STATEMENT_TYPE_ERROR,
			     _("Statement is not a selection statement"));
		g_object_unref (model);
		model = NULL;
	}
	return model;
}

/**
 * gda_connection_statement_execute_select_fullv
 * @cnc: a #GdaConnection object.
 * @stmt: a #GdaStatement object.
 * @params: a #GdaSet object (which can be obtained using gda_statement_get_parameters()), or %NULL
 * @model_usage: specifies how the returned data model will be used as a #GdaStatementModelUsage enum
 * @error: a place to store an error, or %NULL
 * @...: a (-1 terminated) list of (column number, GType) specifying for each column mentionned the GType
 * of the column in the returned #GdaDataModel.
 *
 * Executes a selection command on the given connection.
 *
 * This function returns a #GdaDataModel resulting from the SELECT statement, or %NULL
 * if an error occurred.
 *
 * This function is just a convenience function around the gda_connection_statement_execute()
 * function.
 *
 * See the documentation of the gda_connection_statement_execute() for information
 * about the @params list of parameters.
 *
 * Returns: a #GdaDataModel containing the data returned by the
 * data source, or %NULL if an error occurred
 */
GdaDataModel *
gda_connection_statement_execute_select_fullv (GdaConnection *cnc, GdaStatement *stmt,
					       GdaSet *params, GdaStatementModelUsage model_usage,
					       GError **error, ...)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cnc->priv, NULL);
	g_return_val_if_fail (cnc->priv->provider_obj, NULL);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);
	g_return_val_if_fail (PROV_CLASS (cnc->priv->provider_obj)->statement_execute, NULL);

	va_list ap;
	GdaDataModel *model;
	GType *types;
	
	va_start (ap, error);
	types = make_col_types_array (10, ap);
	va_end (ap);
	model = (GdaDataModel *) PROV_CLASS (cnc->priv->provider_obj)->statement_execute (cnc->priv->provider_obj, 
											  cnc, stmt, params, model_usage, 
											  types, NULL, NULL, 
											  NULL, NULL, error);
	g_free (types);
	if (model && !GDA_IS_DATA_MODEL (model)) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_STATEMENT_TYPE_ERROR,
			     _("Statement is not a selection statement"));
		g_object_unref (model);
		model = NULL;
	}
	return model;
}

/**
 * gda_connection_statement_execute_select_full
 * @cnc: a #GdaConnection object.
 * @stmt: a #GdaStatement object.
 * @params: a #GdaSet object (which can be obtained using gda_statement_get_parameters()), or %NULL
 * @model_usage: specifies how the returned data model will be used as a #GdaStatementModelUsage enum
 * @col_types: an array of GType to request each returned #GdaDataModel's column's GType, terminated with the G_TYPE_NONE
 * value. Any value left to 0 will make the database provider determine the real GType. @col_types can also be %NULL if no
 * column type is specified.
 * @error: a place to store an error, or %NULL
 *
 * Executes a selection command on the given connection.
 *
 * This function returns a #GdaDataModel resulting from the SELECT statement, or %NULL
 * if an error occurred.
 *
 * This function is just a convenience function around the gda_connection_statement_execute()
 * function.
 *
 * See the documentation of the gda_connection_statement_execute() for information
 * about the @params list of parameters.
 *
 * Returns: a #GdaDataModel containing the data returned by the
 * data source, or %NULL if an error occurred
 */
GdaDataModel *
gda_connection_statement_execute_select_full (GdaConnection *cnc, GdaStatement *stmt,
					      GdaSet *params, GdaStatementModelUsage model_usage,
					      GType *col_types, GError **error)
{
	GdaDataModel *model;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cnc->priv, NULL);
	g_return_val_if_fail (cnc->priv->provider_obj, NULL);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);
	g_return_val_if_fail (PROV_CLASS (cnc->priv->provider_obj)->statement_execute, NULL);

	model = (GdaDataModel *) PROV_CLASS (cnc->priv->provider_obj)->statement_execute (cnc->priv->provider_obj, 
											  cnc, stmt, params, 
											  model_usage, col_types, NULL, 
											  NULL, NULL, NULL, error);
	if (model && !GDA_IS_DATA_MODEL (model)) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_STATEMENT_TYPE_ERROR,
			     _("Statement is not a selection statement"));
		g_object_unref (model);
		model = NULL;
	}
	return model;
}

/**
 * gda_connection_begin_transaction
 * @cnc: a #GdaConnection object.
 * @name: the name of the transation to start
 * @level:
 * @error: a place to store errors, or %NULL
 *
 * Starts a transaction on the data source, identified by the
 * @xaction parameter.
 *
 * Before starting a transaction, you can check whether the underlying
 * provider does support transactions or not by using the
 * gda_connection_supports_feature() function.
 *
 * Returns: %TRUE if the transaction was started successfully, %FALSE
 * otherwise.
 */
gboolean
gda_connection_begin_transaction (GdaConnection *cnc, const gchar *name, GdaTransactionIsolation level,
				  GError **error)
{
	gboolean retval;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (cnc->priv, FALSE);
	g_return_val_if_fail (cnc->priv->provider_obj, FALSE);

	retval = gda_server_provider_begin_transaction (cnc->priv->provider_obj, cnc, name, level, error);
	if (retval)
		_gda_client_notify_event (cnc->priv->client, cnc, GDA_CLIENT_EVENT_TRANSACTION_STARTED, NULL);

	return retval;
}

/**
 * gda_connection_commit_transaction
 * @cnc: a #GdaConnection object.
 * @name: the name of the transation to commit
 * @error: a place to store errors, or %NULL
 *
 * Commits the given transaction to the backend database. You need to call
 * gda_connection_begin_transaction() first.
 *
 * Returns: %TRUE if the transaction was finished successfully,
 * %FALSE otherwise.
 */
gboolean
gda_connection_commit_transaction (GdaConnection *cnc, const gchar *name, GError **error)
{
	gboolean retval;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (cnc->priv, FALSE);
	g_return_val_if_fail (cnc->priv->provider_obj, FALSE);

	retval = gda_server_provider_commit_transaction (cnc->priv->provider_obj, cnc, name, error);
	if (retval)
		_gda_client_notify_event (cnc->priv->client, cnc, GDA_CLIENT_EVENT_TRANSACTION_COMMITTED, NULL);

	return retval;
}

/**
 * gda_connection_rollback_transaction
 * @cnc: a #GdaConnection object.
 * @name: the name of the transation to commit
 * @error: a place to store errors, or %NULL
 *
 * Rollbacks the given transaction. This means that all changes
 * made to the underlying data source since the last call to
 * #gda_connection_begin_transaction() or #gda_connection_commit_transaction()
 * will be discarded.
 *
 * Returns: %TRUE if the operation was successful, %FALSE otherwise.
 */
gboolean
gda_connection_rollback_transaction (GdaConnection *cnc, const gchar *name, GError **error)
{
	gboolean retval;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (cnc->priv, FALSE);
	g_return_val_if_fail (cnc->priv->provider_obj, FALSE);

	retval = gda_server_provider_rollback_transaction (cnc->priv->provider_obj, cnc, name, error);
	if (retval)
		_gda_client_notify_event (cnc->priv->client, cnc, GDA_CLIENT_EVENT_TRANSACTION_CANCELLED, NULL);

	return retval;
}

/**
 * gda_connection_add_savepoint
 * @cnc: a #GdaConnection object
 * @name: name of the savepoint to add
 * @error: a place to store errors or %NULL
 *
 * Adds a SAVEPOINT named @name.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_connection_add_savepoint (GdaConnection *cnc, const gchar *name, GError **error)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (cnc->priv, FALSE);
	g_return_val_if_fail (cnc->priv->provider_obj, FALSE);
	
	return gda_server_provider_add_savepoint (cnc->priv->provider_obj, cnc, name, error);
}

/**
 * gda_connection_rollback_savepoint
 * @cnc: a #GdaConnection object
 * @name: name of the savepoint to rollback to
 * @error: a place to store errors or %NULL
 *
 * Rollback all the modifications made after the SAVEPOINT named @name.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_connection_rollback_savepoint (GdaConnection *cnc, const gchar *name, GError **error)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (cnc->priv, FALSE);
	g_return_val_if_fail (cnc->priv->provider_obj, FALSE);	

	return gda_server_provider_rollback_savepoint (cnc->priv->provider_obj, cnc, name, error);
}

/**
 * gda_connection_delete_savepoint
 * @cnc: a #GdaConnection object
 * @name: name of the savepoint to delete
 * @error: a place to store errors or %NULL
 *
 * Delete the SAVEPOINT named @name when not used anymore.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_connection_delete_savepoint (GdaConnection *cnc, const gchar *name, GError **error)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (cnc->priv, FALSE);
	g_return_val_if_fail (cnc->priv->provider_obj, FALSE);

	return gda_server_provider_delete_savepoint (cnc->priv->provider_obj, cnc, name, error);
}

/**
 * gda_connection_get_transaction_status
 * @cnc: a #GdaConnection object
 *
 * Get the status of @cnc regarding transactions. The returned object should not be modified
 * or destroyed; however it may be modified or destroyed by the connection itself.
 *
 * If %NULL is returned, then no transaction has been associated with @cnc
 *
 * Returns: a #GdaTransactionStatus object, or %NULL
 */
GdaTransactionStatus *
gda_connection_get_transaction_status (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cnc->priv, NULL);

	return cnc->priv->trans_status;
}

/**
 * gda_connection_supports_feature
 * @cnc: a #GdaConnection object.
 * @feature: feature to ask for.
 *
 * Asks the underlying provider for if a specific feature is supported.
 *
 * Returns: %TRUE if the provider supports it, %FALSE if not.
 */
gboolean
gda_connection_supports_feature (GdaConnection *cnc, GdaConnectionFeature feature)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (cnc->priv, FALSE);
	g_return_val_if_fail (cnc->priv->provider_obj, FALSE);

	return gda_server_provider_supports_feature (cnc->priv->provider_obj, cnc, feature);
}

/*
 *
 */
static gint
check_parameters (GdaMetaContext *context, GError **error, gint nb, ...)
{
#define MAX_PARAMS 10
	gint i;
	va_list ap;
	gint retval = -1;
	GValue **pvalue;
	struct {
		GValue **pvalue;
		GType    type;
	} spec_array [MAX_PARAMS];
	gint nb_params = 0;

	va_start (ap, nb);
	/* make a list of all the GValue pointers */
	for (pvalue = va_arg (ap, GValue **); pvalue; pvalue = va_arg (ap, GValue **), nb_params++) {
		g_assert (nb_params < MAX_PARAMS); /* hard limit, recompile to change it (should never be needed) */
		spec_array[nb_params].pvalue = pvalue;
		spec_array[nb_params].type = va_arg (ap, GType);
	}

	/* test against each test case */
	for (i = 0; i < nb; i++) {
		gchar *pname;
		gboolean allfound = TRUE;
		gint j;
		for (j = 0; j < nb_params; j++)
			*(spec_array[j].pvalue) = NULL;

		for (pname = va_arg (ap, gchar*); pname; pname = va_arg (ap, gchar*)) {
			gint j;
			pvalue = va_arg (ap, GValue **);
			*pvalue = NULL;
			for (j = 0; allfound && (j < context->size); j++) {
				if (!strcmp (context->column_names[j], pname)) {
					*pvalue = context->column_values[j];
					break;
				}
			}
			if (j == context->size)
				allfound = FALSE;
		}
		if (allfound) {
			retval = i;
			break;
		}
	}
	va_end (ap);

	if (retval >= 0) {
		gint j;
		for (j = 0; j < nb_params; j++) {
			GValue *v = *(spec_array[j].pvalue);
			if (v && (gda_value_is_null (v) || (G_VALUE_TYPE (v) != spec_array[j].type))) {
				g_set_error (error, 0, 0,
					     _("Invalid argument"));
				retval = -1;
			}
		}
	}
	else 
		g_set_error (error, 0, 0,
			     _("Missing and/or wrong arguments"));

	g_print ("Check context => %d\n", retval);
	return retval;
}

static gboolean
local_meta_update (GdaServerProvider *provider, GdaConnection *cnc, GdaMetaContext *context, GError **error)
{
#ifdef GDA_DEBUG
#define ASSERT_TABLE_NAME(x,y) g_assert (!strcmp ((x), (y)));
#else
#define ASSERT_TABLE_NAME(x,y)
#endif
	const gchar *tname = context->table_name;
	GdaMetaStore *store;

	if (*tname != '_')
		return TRUE;
	tname ++;
	
	store = gda_connection_get_meta_store (cnc);
	switch (*tname) {
	case 'i':
		/* _information_schema_catalog_name, params: 
		 *  - none
		 */
		ASSERT_TABLE_NAME (tname, "information_schema_catalog_name")
		if (!PROV_CLASS (provider)->meta_funcs.info)
			break;
		return PROV_CLASS (provider)->meta_funcs.info (provider, cnc, store, context, error);

	case 's': {
		/* _schemata, params: 
		 *  - none
		 *  - @schema_name
		 */
		GValue *p_schema_name = NULL;
		if (check_parameters (context, error, 2,
				      &p_schema_name, G_TYPE_STRING, NULL,
				      "schema_name", &p_schema_name, NULL,
				      NULL) < 0)
			return FALSE;
		ASSERT_TABLE_NAME (tname, "schemata")
		if (!PROV_CLASS (provider)->meta_funcs.schemata)
			break;
		return PROV_CLASS (provider)->meta_funcs.schemata (provider, cnc, store, context, error, p_schema_name);
	}
	case 't': 
		if ((tname[1] == 'a') && (tname[2] == 'b') && (tname[3] == 'l') && (tname[4] == 'e') && (tname[5] == 's')) {
			/* _tables, params: 
			 *  - none
			 *  - @table_schema
			 *  - @table_schema AND @table_name
			 */
			const GValue *p_table_schema = NULL;
			const GValue *p_table_name = NULL;
			if (check_parameters (context, error, 3,
					      &p_table_schema, G_TYPE_STRING,
					      &p_table_name, G_TYPE_STRING, NULL,
					      "table_schema", &p_table_schema, "table_name", &p_table_name, NULL,
					      "table_schema", &p_table_schema, NULL,
					      NULL) < 0)
				return FALSE;
			
			ASSERT_TABLE_NAME (tname, "tables")
				if (p_table_schema) {
					if (!PROV_CLASS (provider)->meta_funcs.tables_views_s)
						break;
					return PROV_CLASS (provider)->meta_funcs.tables_views_s (provider, cnc, store, context, error, 
												 p_table_schema, p_table_name);
				}
				else {
					if (!PROV_CLASS (provider)->meta_funcs.tables_views)
						break;
					return PROV_CLASS (provider)->meta_funcs.tables_views (provider, cnc, store, context, error);
				}
		}
		break;

	case 'c': 
		if ((tname[1] == 'o') && (tname[2] == 'l') && (tname[2] == 'u')) {
			/* _columns,  params: 
			 *  - none
			 *  - @table_schema AND @table_name
			 *  - @table_schema AND @table_name AND @column_name
			 */
			const GValue *p_table_schema = NULL;
			const GValue *p_table_name = NULL;
			const GValue *p_column_name = NULL;
			
			if (check_parameters (context, error, 3,
					      &p_table_schema, G_TYPE_STRING,
					      &p_table_name, G_TYPE_STRING,
					      &p_column_name, G_TYPE_STRING, NULL,
					      "table_schema", &p_table_schema, "table_name", &p_table_name, "column_name", &p_column_name, NULL,
					      "table_schema", &p_table_schema, "table_name", &p_table_name, NULL,
					      NULL) < 0)
				return FALSE;
			
			ASSERT_TABLE_NAME (tname, "columns")
			if (p_table_schema) {
				if (p_column_name) {
					if (!PROV_CLASS (provider)->meta_funcs.columns_c)
						return FALSE;
					return PROV_CLASS (provider)->meta_funcs.columns_c (provider, cnc, store, context, error, 
											    p_table_schema, p_table_name, p_column_name);
				}
				else {
					if (!PROV_CLASS (provider)->meta_funcs.columns_t)
						return FALSE;
					return PROV_CLASS (provider)->meta_funcs.columns_t (provider, cnc, store, context, error, 
											    p_table_schema, p_table_name);
				}
			}
			else {
				if (!PROV_CLASS (provider)->meta_funcs.columns)
					return FALSE;
				return PROV_CLASS (provider)->meta_funcs.columns (provider, cnc, store, context, error);
			}
		}
		break;
	
	case 'b': {
		/* _builtin_data_types, params: 
		 *  - none
		 */
		ASSERT_TABLE_NAME (tname, "builtin_data_types")
		if (!PROV_CLASS (provider)->meta_funcs.btypes)
			break;
		return PROV_CLASS (provider)->meta_funcs.btypes (provider, cnc, store, context, error);
	}
	default:
		break;
	}
	return TRUE;
}

typedef struct {
	GdaServerProvider  *prov;
	GdaConnection      *cnc;
	GError            **error;
	gboolean            error_set;
} DetailledCallbackData;

static void
suggest_update_cb_detailled (GdaMetaStore *store, GdaMetaContext *suggest, DetailledCallbackData *data)
{
	if (data->error_set)
		return;
	if (!local_meta_update (data->prov, data->cnc, suggest, data->error))
		data->error_set = TRUE;
}

/**
 * gda_connection_update_meta_store
 * @cnc: a #GdaConnection object.
 * @context: description of which part of @cnc's associated #GdaMetaStore should be updated, or %NULL
 * @error: a place to store errors, or %NULL
 *
 * Updates @cnc's associated #GdaMetaStore. If @context is not %NULL, then only the parts described by
 * @context will be updated, and if it is %NULL, then the complete meta store will be updated.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_connection_update_meta_store (GdaConnection *cnc, GdaMetaContext *context, GError **error)
{
	GdaMetaStore *store;
	gboolean retval = TRUE;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (cnc->priv->provider_obj, FALSE);

	/* Get or create the GdaMetaStore object */
	store = gda_connection_get_meta_store (cnc);
	g_assert (store);

	/* prepare local context */
	if (!context) {
		GSList *tables, *list;
		tables = gda_meta_store_get_schema_tables (store);
		for (list = tables; list; list = list->next) {
			GdaMetaContext lcontext;
			memset (&lcontext, 0, sizeof (GdaMetaContext));
			lcontext.table_name = (gchar *) list->data;
			if (!local_meta_update (cnc->priv->provider_obj, cnc, &lcontext, error)) {
				retval = FALSE;
				break;
			}
		}
		g_slist_free (tables);
	}
	else {
		GdaMetaContext lcontext;
		lcontext = *context;
		/* alter local context because "_tables" and "_views" always go together so only
		   "_tables" should be updated and providers should always update "_tables" and "_views"
		*/
		if (!strcmp (lcontext.table_name, "_views"))
			lcontext.table_name = "_tables";

		/* actual update */
		gulong signal_id;
		DetailledCallbackData cbd;
		GError *lerror = NULL;
		
		cbd.prov = cnc->priv->provider_obj;
		cbd.cnc = cnc;
		cbd.error = &lerror;
		cbd.error_set = FALSE;
		signal_id = g_signal_connect (store, "suggest_update",
					      G_CALLBACK (suggest_update_cb_detailled), &cbd);
		
		retval = local_meta_update (cnc->priv->provider_obj, cnc, &lcontext, error);
		
		g_signal_handler_disconnect (store, signal_id);
		if (cbd.error_set)
			g_propagate_error (error, lerror);
	}

	return retval;
}

/*
 * predefined statements for meta store data retreival
 */
typedef struct {
	GdaConnectionMetaType  meta_type;
	gint                   nb_filters;
	gchar                **filters;
} MetaKey;

static guint
meta_key_hash (gconstpointer key)
{
	return ((((MetaKey*) key)->meta_type) << 2) + ((MetaKey*) key)->nb_filters;
}

static gboolean
meta_key_equal (gconstpointer a, gconstpointer b)
{
	MetaKey* ak = (MetaKey*) a;
	MetaKey* bk = (MetaKey*) b;
	gint i;

	if ((ak->meta_type != bk->meta_type) ||
	    (ak->nb_filters != bk->nb_filters))
		return FALSE;
	for (i = 0; i < ak->nb_filters; i++) 
		if (strcmp (ak->filters[i], bk->filters[i]))
			return FALSE;

	return TRUE;
}

static GHashTable *
prepare_meta_statements_hash (void)
{
	GHashTable *h;
	MetaKey *key;
	GdaStatement *stmt;
	GdaSqlParser *parser = gda_sql_parser_new ();
	const gchar *sql;

	gchar **name_array = g_new (gchar *, 1);
	name_array[0] = "name";

	h = g_hash_table_new (meta_key_hash, meta_key_equal);
	
	/* GDA_CONNECTION_META_NAMESPACES */
	key = g_new0 (MetaKey, 1);
	key->meta_type = GDA_CONNECTION_META_NAMESPACES;
	sql = "SELECT schema_name, schema_owner, schema_internal FROM _schemata";
	stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
	if (!stmt)
		g_error ("Could not parse internal statement: %s\n", sql);
	g_hash_table_insert (h, key, stmt);
	
	key = g_new0 (MetaKey, 1);
	key->meta_type = GDA_CONNECTION_META_NAMESPACES;
	key->nb_filters = 1;
	key->filters = name_array;
	sql = "SELECT schema_name, schema_owner, schema_internal FROM _schemata WHERE schema_name=##name::string";
	stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
	if (!stmt)
		g_error ("Could not parse internal statement: %s\n", sql);
	g_hash_table_insert (h, key, stmt);

	/* GDA_CONNECTION_META_TYPES */
	key = g_new0 (MetaKey, 1);
	key->meta_type = GDA_CONNECTION_META_TYPES;
	sql = "SELECT short_type_name, gtype, comments, synonyms FROM _all_types WHERE NOT internal";
	stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
	if (!stmt)
		g_error ("Could not parse internal statement: %s\n", sql);
	g_hash_table_insert (h, key, stmt);

	key = g_new0 (MetaKey, 1);
	key->meta_type = GDA_CONNECTION_META_TYPES;
	key->nb_filters = 1;
	key->filters = name_array;
	sql = "SELECT short_type_name, gtype, comments, synonyms FROM _all_types WHERE NOT internal AND short_type_name=##name::string";
	stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
	if (!stmt)
		g_error ("Could not parse internal statement: %s\n", sql);
	g_hash_table_insert (h, key, stmt);

	/* GDA_CONNECTION_META_TABLES */
	key = g_new0 (MetaKey, 1);
	key->meta_type = GDA_CONNECTION_META_TABLES;
	sql = "SELECT table_short_name, table_schema, table_full_name, table_owner, table_comments FROM _tables WHERE table_type='BASE TABLE'";
	stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
	if (!stmt)
		g_error ("Could not parse internal statement: %s\n", sql);
	g_hash_table_insert (h, key, stmt);

	key = g_new0 (MetaKey, 1);
	key->meta_type = GDA_CONNECTION_META_TABLES;
	key->nb_filters = 1;
	key->filters = name_array;
	sql = "SELECT table_short_name, table_schema, table_full_name, table_owner, table_comments FROM _tables WHERE table_type='BASE TABLE' AND table_short_name=##name::string";
	stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
	if (!stmt)
		g_error ("Could not parse internal statement: %s\n", sql);
	g_hash_table_insert (h, key, stmt);

	/* GDA_CONNECTION_META_VIEWS */
	key = g_new0 (MetaKey, 1);
	key->meta_type = GDA_CONNECTION_META_VIEWS;
	sql = "SELECT t.table_short_name, t.table_schema, t.table_full_name, t.table_owner, t.table_comments, v.view_definition FROM _views as v NATURAL JOIN _tables as t";
	stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
	if (!stmt)
		g_error ("Could not parse internal statement: %s\n", sql);
	g_hash_table_insert (h, key, stmt);

	key = g_new0 (MetaKey, 1);
	key->meta_type = GDA_CONNECTION_META_VIEWS;
	key->nb_filters = 1;
	key->filters = name_array;
	sql = "SELECT t.table_short_name, t.table_schema, t.table_full_name, t.table_owner, t.table_comments, v.view_definition FROM _views as v NATURAL JOIN _tables as t WHERE table_short_name=##name::string";
	stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
	if (!stmt)
		g_error ("Could not parse internal statement: %s\n", sql);
	g_hash_table_insert (h, key, stmt);

	/* GDA_CONNECTION_META_FIELDS */
	key = g_new0 (MetaKey, 1);
	key->meta_type = GDA_CONNECTION_META_FIELDS;
	key->nb_filters = 1;
	key->filters = name_array;
	sql = "SELECT c.column_name, c.data_type, c.gtype, c.numeric_precision, c.numeric_scale, c.is_nullable AS 'Nullable', c.column_default, c.extra FROM _columns as c NATURAL JOIN _tables as t WHERE t.table_short_name=##name::string";
	stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
	if (!stmt)
		g_error ("Could not parse internal statement: %s\n", sql);
	g_hash_table_insert (h, key, stmt);

	

	return h;
}

/**
 * gda_connection_get_meta_store_data
 * @cnc: a #GdaConnection object.
 * @meta_type: describes which data to get.
 * @error: a place to store errors, or %NULL
 * @nb_filters: the number of filters in the @... argument
 * @...: a list of (filter name (gchar *), filter value (GValue*)) pairs specifying
 * the filter to apply to the returned data model's contents (there must be @nb_filters pairs)
 *
 * Retreives data stored in @cnc's associated #GdaMetaStore object. This method is usefull
 * to easily get some information about the meta-data associated to @cnc, such as the list of
 * tables, views, and other database objects.
 *
 * Note: it's up to the caller to make sure the information contained within @cnc's associated #GdaMetaStore
 * is up to date using gda_connection_update_meta_store() (it can become outdated if the database's schema
 * is accessed from outside of Libgda).
 *
 * For more information about the returned data model's attributes, or about the @meta_type and @... filter arguments,
 * see <link linkend="GdaConnectionMetaTypeHead">this description</link>.
 * 
 * Returns: a #GdaDataModel containing the data required. The caller is responsible
 * of freeing the returned model using g_object_unref().
 */
GdaDataModel *
gda_connection_get_meta_store_data (GdaConnection *cnc,
				    GdaConnectionMetaType meta_type,
				    GError **error, gint nb_filters, ...)
{
	GdaMetaStore *store;
	GdaDataModel *model = NULL;
	static GHashTable *stmt_hash = NULL;
	GdaStatement *stmt;
	GdaSet *set = NULL;
	va_list ap;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cnc->priv->provider_obj, NULL);

	/* Get or create the GdaMetaStore object */
	store = gda_connection_get_meta_store (cnc);
	g_assert (store);
	
	/* fetch the statement */
	MetaKey key;
	gint i;
	gchar *fname;
	if (!stmt_hash)
		stmt_hash = prepare_meta_statements_hash ();
	key.meta_type = meta_type;
	key.nb_filters = nb_filters;
	key.filters = g_new (gchar *, nb_filters);
	va_start (ap, nb_filters);
	for (i = 0, fname = va_arg (ap, gchar*); fname && (i < nb_filters); fname = va_arg (ap, gchar*), i++) {
		GdaHolder *h;
		GValue *v;

		v = va_arg (ap, GValue*);
		if (!v || gda_value_is_null (v))
			continue;
		if (!set)
			set = gda_set_new (NULL);
		h = g_object_new (GDA_TYPE_HOLDER, "g-type", G_VALUE_TYPE (v), "id", fname, NULL);
		gda_holder_set_value (h, v);
		gda_set_add_holder (set, h);
		g_object_unref (h);
		key.filters[i] = fname;
	}
	va_end (ap);
	stmt = g_hash_table_lookup (stmt_hash, &key);
	g_free (key.filters);
	if (!stmt) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_MISSING_PARAM_ERROR,
			     _("Wrong filter arguments"));
		return NULL;
	}

	/* execute statement to fetch the requested result from the meta store's connection
	 * REM: at a latter time the data model should be specific and update itself whenever
	 * the meta store is updated
	 */
	model = gda_connection_statement_execute_select (gda_meta_store_get_internal_connection (store), 
							 stmt, set, error);
	if (set)
		g_object_unref (set);
	
	return model;
}

/**
 * gda_connection_get_events
 * @cnc: a #GdaConnection.
 *
 * Retrieves a list of the last errors occurred during the connection.
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
 * gda_connection_value_to_sql_string
 * @cnc: a #GdaConnection object.
 * @from: #GValue to convert from
 *
 * Produces a fully quoted and escaped string from a GValue
 *
 * Returns: escaped and quoted value or NULL if not supported.
 */
gchar *
gda_connection_value_to_sql_string (GdaConnection *cnc, GValue *from)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (cnc->priv, FALSE);
	g_return_val_if_fail (from != NULL, FALSE);
	g_return_val_if_fail (cnc->priv->provider_obj, FALSE);

	/* execute the command on the provider */
	return gda_server_provider_value_to_sql_string (cnc->priv->provider_obj, cnc, from);
}

/*
 * Internal functions to keep track
 * of the transactional status of the connection
 */
void
gda_connection_internal_transaction_started (GdaConnection *cnc, const gchar *parent_trans, const gchar *trans_name, 
					     GdaTransactionIsolation isol_level)
{
	GdaTransactionStatus *parent, *st;

	st = gda_transaction_status_new (trans_name);
	st->isolation_level = isol_level;
	parent = gda_transaction_status_find (cnc->priv->trans_status, parent_trans, NULL);
	if (!parent)
		cnc->priv->trans_status = st;
	else {
		gda_transaction_status_add_event_sub (parent, st);
		g_object_unref (st);
	}
#ifdef GDA_DEBUG_signal
        g_print (">> 'TRANSACTION_STATUS_CHANGED' from %s\n", __FUNCTION__);
#endif
        g_signal_emit (G_OBJECT (cnc), gda_connection_signals[TRANSACTION_STATUS_CHANGED], 0);
#ifdef GDA_DEBUG_signal
        g_print ("<< 'TRANSACTION_STATUS_CHANGED' from %s\n", __FUNCTION__);
#endif

#ifdef GDA_DEBUG_NO
	if (cnc->priv->trans_status)
		gda_transaction_status_dump (cnc->priv->trans_status, 5);
#endif
}

void 
gda_connection_internal_transaction_rolledback (GdaConnection *cnc, const gchar *trans_name)
{
	GdaTransactionStatus *st = NULL;
	GdaTransactionStatusEvent *ev = NULL;

	if (cnc->priv->trans_status)
		st = gda_transaction_status_find (cnc->priv->trans_status, trans_name, &ev);
	if (st) {
		if (ev) {
			/* there is a parent transaction */
			gda_transaction_status_free_events (ev->trans, ev, TRUE);
		}
		else {
			/* no parent transaction */
			g_object_unref (cnc->priv->trans_status);
			cnc->priv->trans_status = NULL;
		}
#ifdef GDA_DEBUG_signal
		g_print (">> 'TRANSACTION_STATUS_CHANGED' from %s\n", __FUNCTION__);
#endif
		g_signal_emit (G_OBJECT (cnc), gda_connection_signals[TRANSACTION_STATUS_CHANGED], 0);
#ifdef GDA_DEBUG_signal
		g_print ("<< 'TRANSACTION_STATUS_CHANGED' from %s\n", __FUNCTION__);
#endif
	}
	else
		g_warning (_("Connection transaction status tracking: no transaction exists for ROLLBACK"));
#ifdef GDA_DEBUG_NO
	if (cnc->priv->trans_status)
		gda_transaction_status_dump (cnc->priv->trans_status, 5);
#endif
}

void 
gda_connection_internal_transaction_committed (GdaConnection *cnc, const gchar *trans_name)
{
	GdaTransactionStatus *st = NULL;
	GdaTransactionStatusEvent *ev = NULL;

	if (cnc->priv->trans_status)
		st = gda_transaction_status_find (cnc->priv->trans_status, trans_name, &ev);
	if (st) {
		if (ev) {
			/* there is a parent transaction */
			gda_transaction_status_free_events (ev->trans, ev, TRUE);
		}
		else {
			/* no parent transaction */
			g_object_unref (cnc->priv->trans_status);
			cnc->priv->trans_status = NULL;
		}
#ifdef GDA_DEBUG_signal
		g_print (">> 'TRANSACTION_STATUS_CHANGED' from %s\n", __FUNCTION__);
#endif
		g_signal_emit (G_OBJECT (cnc), gda_connection_signals[TRANSACTION_STATUS_CHANGED], 0);
#ifdef GDA_DEBUG_signal
		g_print ("<< 'TRANSACTION_STATUS_CHANGED' from %s\n", __FUNCTION__);
#endif
	}
	else
		g_warning (_("Connection transaction status tracking: no transaction exists for COMMIT"));
#ifdef GDA_DEBUG_NO
	if (cnc->priv->trans_status)
		gda_transaction_status_dump (cnc->priv->trans_status, 5);
#endif
}

void
gda_connection_internal_sql_executed (GdaConnection *cnc, const gchar *sql, GdaConnectionEvent *error)
{
	GdaTransactionStatus *st = NULL;
	
	if (cnc->priv->trans_status)
		st = gda_transaction_status_find_current (cnc->priv->trans_status, NULL, FALSE);
	if (st)
		gda_transaction_status_add_event_sql (st, sql, error);
#ifdef GDA_DEBUG_signal
		g_print (">> 'TRANSACTION_STATUS_CHANGED' from %s\n", __FUNCTION__);
#endif
		g_signal_emit (G_OBJECT (cnc), gda_connection_signals[TRANSACTION_STATUS_CHANGED], 0);
#ifdef GDA_DEBUG_signal
		g_print ("<< 'TRANSACTION_STATUS_CHANGED' from %s\n", __FUNCTION__);
#endif
#ifdef GDA_DEBUG_NO
	if (cnc->priv->trans_status)
		gda_transaction_status_dump (cnc->priv->trans_status, 5);
#endif
}

void
gda_connection_internal_savepoint_added (GdaConnection *cnc, const gchar *parent_trans, const gchar *svp_name)
{
	GdaTransactionStatus *st;

	st = gda_transaction_status_find (cnc->priv->trans_status, parent_trans, NULL);
	if (st) {
		gda_transaction_status_add_event_svp (st, svp_name);
#ifdef GDA_DEBUG_signal
		g_print (">> 'TRANSACTION_STATUS_CHANGED' from %s\n", __FUNCTION__);
#endif
		g_signal_emit (G_OBJECT (cnc), gda_connection_signals[TRANSACTION_STATUS_CHANGED], 0);
#ifdef GDA_DEBUG_signal
		g_print ("<< 'TRANSACTION_STATUS_CHANGED' from %s\n", __FUNCTION__);
#endif
	}
	else
		g_warning (_("Connection transaction status tracking: no transaction exists for ADD SAVEPOINT"));
#ifdef GDA_DEBUG_NO
	if (cnc->priv->trans_status)
		gda_transaction_status_dump (cnc->priv->trans_status, 5);
#endif
}

void
gda_connection_internal_savepoint_rolledback (GdaConnection *cnc, const gchar *svp_name)
{
	GdaTransactionStatus *st;
	GdaTransactionStatusEvent *ev = NULL;

	st = gda_transaction_status_find (cnc->priv->trans_status, svp_name, &ev);
	if (st) {
		gda_transaction_status_free_events (st, ev, TRUE);
#ifdef GDA_DEBUG_signal
		g_print (">> 'TRANSACTION_STATUS_CHANGED' from %s\n", __FUNCTION__);
#endif
		g_signal_emit (G_OBJECT (cnc), gda_connection_signals[TRANSACTION_STATUS_CHANGED], 0);
#ifdef GDA_DEBUG_signal
		g_print ("<< 'TRANSACTION_STATUS_CHANGED' from %s\n", __FUNCTION__);
#endif
	}
	else
		g_warning (_("Connection transaction status tracking: no transaction exists for ROLLBACK SAVEPOINT"));
#ifdef GDA_DEBUG_NO
	if (cnc->priv->trans_status)
		gda_transaction_status_dump (cnc->priv->trans_status, 5);
#endif	
}

void
gda_connection_internal_savepoint_removed (GdaConnection *cnc, const gchar *svp_name)
{
	GdaTransactionStatus *st;
	GdaTransactionStatusEvent *ev = NULL;

	st = gda_transaction_status_find (cnc->priv->trans_status, svp_name, &ev);
	if (st) {
		gda_transaction_status_free_events (st, ev, FALSE);
#ifdef GDA_DEBUG_signal
		g_print (">> 'TRANSACTION_STATUS_CHANGED' from %s\n", __FUNCTION__);
#endif
		g_signal_emit (G_OBJECT (cnc), gda_connection_signals[TRANSACTION_STATUS_CHANGED], 0);
#ifdef GDA_DEBUG_signal
		g_print ("<< 'TRANSACTION_STATUS_CHANGED' from %s\n", __FUNCTION__);
#endif
	}
	else
		g_warning (_("Connection transaction status tracking: no transaction exists for REMOVE SAVEPOINT"));
#ifdef GDA_DEBUG_NO
	if (cnc->priv->trans_status)
		gda_transaction_status_dump (cnc->priv->trans_status, 5);
#endif		
}

void 
gda_connection_internal_statement_executed (GdaConnection *cnc, GdaStatement *stmt, GdaConnectionEvent *error)
{
	if (!error || (error && (gda_connection_event_get_event_type (error) != GDA_CONNECTION_EVENT_ERROR))) {
		GdaSqlStatement *sqlst;
		GdaSqlStatementTransaction *trans;
		g_object_get (G_OBJECT (stmt), "structure", &sqlst, NULL);
		trans = (GdaSqlStatementTransaction*) sqlst->contents; /* warning: this may be inaccurate if stmt_type is not
									  a transaction type, but the compiler does not care */

		switch (sqlst->stmt_type) {
		case GDA_SQL_STATEMENT_BEGIN:
			gda_connection_internal_transaction_started (cnc, NULL, trans->trans_name, 
								     trans->isolation_level);
			break;
		case GDA_SQL_STATEMENT_ROLLBACK:
			gda_connection_internal_transaction_rolledback (cnc, trans->trans_name);
			break;
		case GDA_SQL_STATEMENT_COMMIT:
			gda_connection_internal_transaction_committed (cnc, trans->trans_name);
			break;
		case GDA_SQL_STATEMENT_SAVEPOINT:
			gda_connection_internal_savepoint_added (cnc, NULL, trans->trans_name);
			break;
		case GDA_SQL_STATEMENT_ROLLBACK_SAVEPOINT:
			gda_connection_internal_savepoint_rolledback (cnc, trans->trans_name);
			break;
		case GDA_SQL_STATEMENT_DELETE_SAVEPOINT:
			gda_connection_internal_savepoint_removed (cnc, trans->trans_name);
			break;
		default:
			gda_connection_internal_sql_executed (cnc, sqlst->sql, error);
			break;
		}
		gda_sql_statement_free (sqlst);
	}
}

void
gda_connection_internal_change_transaction_state (GdaConnection *cnc,
						  GdaTransactionStatusState newstate)
{
	g_return_if_fail (cnc->priv->trans_status);

	if (cnc->priv->trans_status->state == newstate)
		return;

	cnc->priv->trans_status->state = newstate;
#ifdef GDA_DEBUG_signal
	g_print (">> 'TRANSACTION_STATUS_CHANGED' from %s\n", __FUNCTION__);
#endif
	g_signal_emit (G_OBJECT (cnc), gda_connection_signals[TRANSACTION_STATUS_CHANGED], 0);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'TRANSACTION_STATUS_CHANGED' from %s\n", __FUNCTION__);
#endif
}

/*
 * Prepared statements handling
 */

static void prepared_stms_stmt_destroyed_cb (GdaStatement *gda_stmt, GdaConnection *cnc);
static void statement_weak_notify_cb (GdaConnection *cnc, GdaStatement *stmt);

static void 
prepared_stms_stmt_destroyed_cb (GdaStatement *gda_stmt, GdaConnection *cnc)
{
	g_signal_handlers_disconnect_by_func (gda_stmt, G_CALLBACK (prepared_stms_stmt_destroyed_cb), cnc);
	g_object_weak_unref (G_OBJECT (gda_stmt), (GWeakNotify) statement_weak_notify_cb, cnc);
	g_assert (cnc->priv->prepared_stmts);
	g_hash_table_remove (cnc->priv->prepared_stmts, gda_stmt);
}

static void
statement_weak_notify_cb (GdaConnection *cnc, GdaStatement *stmt)
{
	g_assert (cnc->priv->prepared_stmts);
	g_hash_table_remove (cnc->priv->prepared_stmts, stmt);
}

void 
gda_connection_add_prepared_statement (GdaConnection *cnc, GdaStatement *gda_stmt, gpointer prepared_stmt)
{
	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	g_return_if_fail (cnc->priv);

	if (!cnc->priv->prepared_stmts)
		cnc->priv->prepared_stmts = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_object_unref);
	g_hash_table_remove (cnc->priv->prepared_stmts, gda_stmt);
	g_hash_table_insert (cnc->priv->prepared_stmts, gda_stmt, prepared_stmt);
	
	/* destroy the prepared statement if gda_stmt is destroyed, or changes */
	g_object_weak_ref (G_OBJECT (gda_stmt), (GWeakNotify) statement_weak_notify_cb, cnc);
	g_signal_connect (G_OBJECT (gda_stmt), "reset", G_CALLBACK (prepared_stms_stmt_destroyed_cb), cnc);
}

gpointer
gda_connection_get_prepared_statement (GdaConnection *cnc, GdaStatement *gda_stmt)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cnc->priv, NULL);

	if (!cnc->priv->prepared_stmts) 
		return NULL;

	return g_hash_table_lookup (cnc->priv->prepared_stmts, gda_stmt);
}

void
gda_connection_del_prepared_statement (GdaConnection *cnc, GdaStatement *gda_stmt)
{
	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	prepared_stms_stmt_destroyed_cb (gda_stmt, cnc);
}

static void
prepared_stms_foreach_func (GdaStatement *gda_stmt, GdaStatement *prepared_stmt, GdaConnection *cnc)
{
	g_signal_handlers_disconnect_by_func (gda_stmt, G_CALLBACK (prepared_stms_stmt_destroyed_cb), cnc);
}


/*
 * Provider's specific connection data management
 */

/**
 * gda_connection_internal_set_provider_data
 * @cnc: a #GdaConnection object
 * @data: an opaque structure, known only to the provider for which @cnc is opened
 * @destroy_func: function to call when the connection closes and @data needs to be destroyed
 *
 * Note: calling this function more than once will not make it call @destroy_func on any previously
 * set opaque @data, you'll have to do it yourself.
 */
void 
gda_connection_internal_set_provider_data (GdaConnection *cnc, gpointer data, GDestroyNotify destroy_func)
{
	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	cnc->priv->provider_data = data;
	cnc->priv->provider_data_destroy_func = destroy_func;
}

/**
 * gda_connection_internal_get_provider_data
 * @cnc: a #GdaConnection object
 *
 * Get the opaque pointer previously set using gda_connection_internal_set_provider_data().
 * If it's not set, then add a connection event and returns %NULL
 *
 * Returns: the pointer to the opaque structure set using gda_connection_internal_set_provider_data()
 */
gpointer
gda_connection_internal_get_provider_data (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	if (! cnc->priv->provider_data)
		gda_connection_add_event_string (cnc, _("Internal error: invalid provider handle"));
	return cnc->priv->provider_data;
}

/**
 * gda_connection_get_meta_store
 * @cnc: a #GdaConnection object
 *
 * Get or initializes the #GdaMetaStore associated to @cnc
 *
 * Returns: a #GdaMetaStore object
 */
GdaMetaStore *
gda_connection_get_meta_store (GdaConnection *cnc)
{
	if (cnc->priv->meta_store)
		return cnc->priv->meta_store;

	cnc->priv->meta_store = gda_meta_store_new (NULL);
	return cnc->priv->meta_store;
}
