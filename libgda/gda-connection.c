/* GDA library
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *      Michael Lausch <michael@lausch.at>
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

#include <config.h>
#include <stdio.h>
#include <libgda/gda-client.h>
#include <libgda/gda-config.h>
#include <libgda/gda-connection.h>
#include <libgda/gda-intl.h>
#include <libgda/gda-log.h>
#include <libgda/gda-server-provider.h>

#define PARENT_TYPE G_TYPE_OBJECT

struct _GdaConnectionPrivate {
	GdaClient *client;
	GdaServerProvider *provider_obj;
	gchar *dsn;
	gchar *cnc_string;
	gchar *provider;
	gchar *username;
	gchar *password;
	gboolean is_open;
	GList *error_list;
	GList *recset_list;
};

static void gda_connection_class_init (GdaConnectionClass *klass);
static void gda_connection_init       (GdaConnection *cnc, GdaConnectionClass *klass);
static void gda_connection_finalize   (GObject *object);

enum {
	ERROR,
	LAST_SIGNAL
};

static gint gda_connection_signals[LAST_SIGNAL] = { 0 };
static GObjectClass *parent_class = NULL;

/*
 * Callbacks
 */

//static void
//recset_weak_cb (gpointer user_data, GObject *object)
//{
//	GdaRecordset *recset = (GdaRecordset *) object;
//	GdaConnection *cnc = (GdaConnection *) user_data;
//
//	g_return_if_fail (GDA_IS_RECORDSET (recset));
//	g_return_if_fail (GDA_IS_CONNECTION (cnc));
//
//	cnc->priv->recset_list = g_list_remove (cnc->priv->recset_list, recset);
//}

/*
 * GdaConnection class implementation
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
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);

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
	cnc->priv->error_list = NULL;
	cnc->priv->recset_list = NULL;
}

static void
gda_connection_finalize (GObject *object)
{
	GdaConnection *cnc = (GdaConnection *) object;

	g_return_if_fail (GDA_IS_CONNECTION (cnc));

	/* free memory */
	if (cnc->priv->is_open) {
		/* close the connection to the provider */
		gda_server_provider_close_connection (cnc->priv->provider_obj, cnc);
	}

	g_object_unref (G_OBJECT (cnc->priv->provider_obj));
	cnc->priv->provider_obj = NULL;

	g_free (cnc->priv->dsn);
	g_free (cnc->priv->cnc_string);
	g_free (cnc->priv->provider);
	g_free (cnc->priv->username);
	g_free (cnc->priv->password);

	g_list_foreach (cnc->priv->error_list, (GFunc) gda_error_free, NULL);
	g_list_free (cnc->priv->error_list);

	g_list_foreach (cnc->priv->recset_list, (GFunc) g_object_unref, NULL);

	g_free (cnc->priv);
	cnc->priv = NULL;

	/* chain to parent class */
	parent_class->finalize (object);
}

GType
gda_connection_get_type (void)
{
	static GType type = 0;

	if (!type) {
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
 *
 * This function creates a new #GdaConnection object. It is not
 * intended to be used directly by applications (use
 * #gda_client_open_connection instead).
 *
 * Returns: a newly allocated #GdaConnection object.
 */
GdaConnection *
gda_connection_new (GdaClient *client,
		    GdaServerProvider *provider,
		    const gchar *dsn,
		    const gchar *username,
		    const gchar *password)
{
	GdaConnection *cnc;
	GdaDataSourceInfo *dsn_info;
	GdaQuarkList *params;

	g_return_val_if_fail (GDA_IS_CLIENT (client), NULL);
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);

	/* get the data source info */
	dsn_info = gda_config_find_data_source (dsn);
	if (!dsn_info) {
		gda_log_error (_("Data source %s not found in configuration"), dsn);
		return NULL;
	}

	cnc = g_object_new (GDA_TYPE_CONNECTION, NULL);

	gda_connection_set_client (cnc, client);
	cnc->priv->provider_obj = provider;
	g_object_ref (G_OBJECT (cnc->priv->provider_obj));
	cnc->priv->dsn = g_strdup (dsn);
	cnc->priv->cnc_string = g_strdup (dsn_info->cnc_string);
	cnc->priv->provider = g_strdup (dsn_info->provider);
	cnc->priv->username = g_strdup (username);
	cnc->priv->password = g_strdup (password);

	gda_config_free_data_source_info (dsn_info);

	/* try to open the connection */
	params = gda_quark_list_new_from_string (cnc->priv->cnc_string);
	if (!gda_server_provider_open_connection (provider, cnc, params, username, password)) {
		gda_quark_list_free (params);
		g_object_unref (G_OBJECT (cnc));
		return NULL;
	}

	gda_quark_list_free (params);
	cnc->priv->is_open = TRUE;

	return cnc;
}

/**
 * gda_connection_close
 */
gboolean
gda_connection_close (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	g_object_unref (G_OBJECT (cnc));
	return TRUE;
}

gboolean
gda_connection_is_open (GdaConnection *cnc)
{
        g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	return cnc->priv->is_open;
}

/**
 * gda_connection_get_client
 */
GdaClient *
gda_connection_get_client (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	return cnc->priv->client;
}

/**
 * gda_connection_set_client
 */
void
gda_connection_set_client (GdaConnection *cnc, GdaClient *client)
{
	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	g_return_if_fail (GDA_IS_CLIENT (client));

	cnc->priv->client = client;
}

/**
 * gda_connection_get_dsn
 * @cnc: A #GdaConnection object
 *
 * Returns the data source name the connection object is connected
 * to.
 */
const gchar *
gda_connection_get_dsn (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	return (const gchar *) cnc->priv->dsn;
}

/**
 * gda_connection_get_cnc_string
 */
const gchar *
gda_connection_get_cnc_string (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	return (const gchar *) cnc->priv->cnc_string;
}

/**
 * gda_connection_get_provider
 */
const gchar *
gda_connection_get_provider (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	return (const gchar *) cnc->priv->provider;
}

/**
 * gda_connection_get_username
 */
const gchar *
gda_connection_get_username (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	return (const gchar *) cnc->priv->username;
}

/**
 * gda_connection_get_password
 */
const gchar *
gda_connection_get_password (GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	return (const gchar *) cnc->priv->password;
}

/**
 * gda_connection_add_error
 */
void
gda_connection_add_error (GdaConnection *cnc, GdaError *error)
{
	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	g_return_if_fail (GDA_IS_ERROR (error));

	cnc->priv->error_list = g_list_append (cnc->priv->error_list, error);

	g_signal_emit (G_OBJECT (cnc), gda_connection_signals[ERROR], 0, cnc->priv->error_list);
	cnc->priv->error_list = NULL;
}

/**
 * gda_connection_add_error_string
 * @cnc: A #GdaServerConnection object.
 * @str: Error message.
 *
 * Adds a new error to the given connection object. This is just a convenience
 * function that simply creates a @GdaError and then calls
 * @gda_server_connection_add_error.
 */
void
gda_connection_add_error_string (GdaConnection *cnc, const gchar *str, ...)
{
	GdaError *error;

	va_list args;
	gchar sz[2048];

	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	g_return_if_fail (str != NULL);

	/* build the message string */
	va_start (args, str);
	vsprintf (sz, str, args);
	va_end (args);
	
	error = gda_error_new ();
	gda_error_set_description (error, sz);
	gda_error_set_number (error, -1);
	gda_error_set_source (error, g_get_prgname ());
	gda_error_set_sqlstate (error, "-1");
	
	gda_connection_add_error (cnc, error);
}

/**
 * gda_connection_add_error_list
 */
void
gda_connection_add_error_list (GdaConnection *cnc, GList *error_list)
{
	GList *l;

	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	g_return_if_fail (error_list != NULL);

	for (l = error_list; l; l = l->next) {
		GdaError *error = GDA_ERROR (l->data);
		cnc->priv->error_list = g_list_append (cnc->priv->error_list, error);
	}

	/* notify errors */
	g_signal_emit (G_OBJECT (cnc), gda_connection_signals[ERROR], 0, cnc->priv->error_list);
	cnc->priv->error_list = NULL;

	g_list_free (error_list);
}

/**
 * gda_connection_execute_command
 */
GList *
gda_connection_execute_command (GdaConnection *cnc,
				GdaCommand *cmd,
				GdaParameterList *params)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cmd != NULL, NULL);

	/* execute the command on the provider */
	return gda_server_provider_execute_command (cnc->priv->provider_obj,
						    cnc, cmd, params);
}

/**
 * gda_connection_execute_single_command
 */
GdaDataModel *
gda_connection_execute_single_command (GdaConnection *cnc,
				       GdaCommand *cmd,
				       GdaParameterList *params)
{
	GList *reclist;
	GdaDataModel *model;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cmd != NULL, NULL);

	reclist = gda_connection_execute_command (cnc, cmd, params);
	if (!reclist)
		return NULL;

	model = GDA_DATA_MODEL (reclist->data);
	g_object_ref (G_OBJECT (model));

	g_list_foreach (reclist, (GFunc) g_object_unref, NULL);
	g_list_free (reclist);

	return model;
}

/**
 * gda_connection_begin_transaction
 */
gboolean
gda_connection_begin_transaction (GdaConnection *cnc, const gchar *id)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	return gda_server_provider_begin_transaction (cnc->priv->provider_obj, cnc, id);
}

/**
 * gda_connection_commit_transaction
 */
gboolean
gda_connection_commit_transaction (GdaConnection *cnc, const gchar *id)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	return gda_server_provider_commit_transaction (cnc->priv->provider_obj, cnc, id);
}

/**
 * gda_connection_rollback_transaction
 */
gboolean
gda_connection_rollback_transaction (GdaConnection *cnc, const gchar *id)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	return gda_server_provider_rollback_transaction (cnc->priv->provider_obj, cnc, id);
}

/**
 * gda_connection_supports
 */
gboolean
gda_connection_supports (GdaConnection *cnc, GdaConnectionFeature feature)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	return gda_server_provider_supports (cnc->priv->provider_obj, cnc, feature);
}

/**
 * gda_connection_get_schema
 */
GdaDataModel *
gda_connection_get_schema (GdaConnection *cnc,
			   GdaConnectionSchema schema,
			   GdaParameterList *params)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	return gda_server_provider_get_schema (cnc->priv->provider_obj, cnc, schema, params);
}
