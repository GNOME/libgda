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
#include <gmodule.h>
#include <libgda/gda-client.h>
#include <libgda/gda-config.h>
#include <libgda/gda-intl.h>
#include <libgda/gda-log.h>
#include "gda-marshal.h"

#define PARENT_TYPE G_TYPE_OBJECT

typedef struct {
	GModule *handle;
	GdaServerProvider *provider;

	/* entry points to the plugin */
	const gchar * (* plugin_get_name) (void);
	const gchar * (* plugin_get_description) (void);
	GList * (* plugin_get_connection_params) (void);
	GdaServerProvider * (* plugin_create_provider) (void);
} LoadedProvider;

struct _GdaClientPrivate {
	GHashTable *providers;
	GList *connections;
};

static void gda_client_class_init (GdaClientClass *klass);
static void gda_client_init       (GdaClient *client, GdaClientClass *klass);
static void gda_client_finalize   (GObject *object);

enum {
	ERROR,
	LAST_SIGNAL
};

static gint gda_client_signals[LAST_SIGNAL] = { 0 };
static GObjectClass *parent_class = NULL;

/*
 * Callbacks
 */

static void
connection_error_cb (GdaConnection *cnc, GList *error_list, gpointer user_data)
{
	GdaClient *client = (GdaClient *) user_data;

	g_return_if_fail (GDA_IS_CLIENT (client));
	g_signal_emit (G_OBJECT (client), gda_client_signals[ERROR], 0, cnc, error_list);
}

static void
cnc_weak_cb (gpointer user_data, GObject *object)
{
	GdaConnection *cnc = (GdaConnection *) object;
	GdaClient *client = (GdaClient *) user_data;

	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	g_return_if_fail (GDA_IS_CLIENT (client));

	client->priv->connections = g_list_remove (client->priv->connections, cnc);
}

/*
 * GdaClient class implementation
 */

static void
gda_client_class_init (GdaClientClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	gda_client_signals[ERROR] =
		g_signal_new ("error",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaClientClass, error),
			      NULL, NULL,
			      gda_marshal_VOID__POINTER_POINTER,
			      G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_POINTER);

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
free_hash_provider (gpointer key, gpointer value, gpointer user_data)
{
	gchar *iid = (gchar *) key;
	LoadedProvider *prv = (LoadedProvider *) value;

	g_free (iid);
	if (prv) {
		g_module_close (prv->handle);
		g_object_unref (G_OBJECT (prv->provider));
		g_free (prv);
	}
}

static void
gda_client_finalize (GObject *object)
{
	GdaClient *client = (GdaClient *) object;

	g_return_if_fail (GDA_IS_CLIENT (client));

	/* free memory */
	gda_client_close_all_connections (client);

	g_hash_table_foreach (client->priv->providers, (GHFunc) free_hash_provider, NULL);
	g_hash_table_destroy (client->priv->providers);
	client->priv->providers = NULL;

	g_free (client->priv);
	client->priv = NULL;

	/* chain to parent class */
	parent_class->finalize (object);
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
 */
GdaClient *
gda_client_new (void)
{
	GdaClient *client;

	client = g_object_new (GDA_TYPE_CLIENT, NULL);
	return client;
}

/**
 * gda_client_open_connection
 */
GdaConnection *
gda_client_open_connection (GdaClient *client,
			    const gchar *dsn,
			    const gchar *username,
			    const gchar *password)
{
	GdaConnection *cnc;
	LoadedProvider *prv;
	GdaDataSourceInfo *dsn_info;

	g_return_val_if_fail (GDA_IS_CLIENT (client), NULL);

	/* get the data source info */
	dsn_info = gda_config_find_data_source (dsn);
	if (!dsn_info) {
		gda_log_error (_("Data source %s not found in configuration"), dsn);
		return NULL;
	}

	/* search for the connection in our private list */
	cnc = gda_client_find_connection (client, dsn, username, password);
	if (cnc) {
		g_object_ref (G_OBJECT (cnc));
		return cnc;
	}

	/* try to find provider in our hash table */
	prv = g_hash_table_lookup (client->priv->providers, dsn_info->provider);
	if (!prv) {
		GdaProviderInfo *prv_info;

		prv_info = gda_config_get_provider_by_name (dsn_info->provider);
		if (!prv_info) {
			gda_log_error (_("Provider %s is not installed"), dsn_info->provider);
			gda_config_free_data_source_info (dsn_info);
			return NULL;
		}

		/* load the new provider */
		prv = g_new0 (LoadedProvider, 1);
		prv->handle = g_module_open (prv_info->location, G_MODULE_BIND_LAZY);
		if (!prv->handle) {
			gda_log_error (_("Could not load %s provider"), dsn_info->provider);
			gda_config_free_data_source_info (dsn_info);
			g_free (prv);
			return NULL;
		}

		g_module_symbol (prv->handle, "plugin_get_name",
				 (gpointer) &prv->plugin_get_name);
		g_module_symbol (prv->handle, "plugin_get_description",
				 (gpointer) &prv->plugin_get_description);
		g_module_symbol (prv->handle, "plugin_get_connection_params",
				 (gpointer) &prv->plugin_get_connection_params);
		g_module_symbol (prv->handle, "plugin_create_provider",
				 (gpointer) &prv->plugin_create_provider);

		if (!prv->plugin_create_provider) {
			gda_log_error (_("Provider %s does not implement entry point"),
				       dsn_info->provider);
			gda_config_free_data_source_info (dsn_info);
			g_free (prv);
			return NULL;
		}

		prv->provider = prv->plugin_create_provider ();
		if (!prv->provider) {
			gda_log_error (_("Creation of GdaServerProvider failed"));
			gda_config_free_data_source_info (dsn_info);
			g_free (prv);
			return NULL;
		}

		g_object_ref (G_OBJECT (prv->provider));
		g_hash_table_insert (client->priv->providers,
				     g_strdup (dsn_info->provider),
				     prv);
	}

	cnc = gda_connection_new (client, prv->provider, dsn,
				  username ? username : "", password ? password : "");

	if (!GDA_IS_CONNECTION (cnc)) {
		gda_config_free_data_source_info (dsn_info);
		return NULL;
	}

	/* add list to our private list */
	client->priv->connections = g_list_append (client->priv->connections, cnc);
	g_object_weak_ref (G_OBJECT (cnc), (GWeakNotify) cnc_weak_cb, client);
	g_signal_connect (G_OBJECT (cnc), "error",
			  G_CALLBACK (connection_error_cb), client);

	/* free memory */
	gda_config_free_data_source_info (dsn_info);

	return cnc;
}

/**
 * gda_client_get_connection_list
 */
const GList *
gda_client_get_connection_list (GdaClient *client)
{
	g_return_val_if_fail (GDA_IS_CLIENT (client), NULL);
	return (const GList *) client->priv->connections;
}

/**
 * gda_client_find_connection
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
		tmp_dsn = gda_connection_get_dsn (cnc);
		tmp_usr = gda_connection_get_username (cnc);
		tmp_pwd = gda_connection_get_password (cnc);

		if (((!tmp_dsn && !dsn_info->cnc_string) || !strcmp (tmp_dsn, dsn_info->cnc_string)) &&
		    ((!tmp_usr && !username) || !strcmp (tmp_usr, username)) &&
		    ((!tmp_pwd && !password) || !strcmp (tmp_pwd, password))) {
			gda_config_free_data_source_info (dsn_info);
			return cnc;
		}
	}

	gda_config_free_data_source_info (dsn_info);

	return NULL;
}

/**
 * gda_client_close_all_connections
 */
void
gda_client_close_all_connections (GdaClient *client)
{
	g_return_if_fail (GDA_IS_CLIENT (client));

	g_list_foreach (client->priv->connections, (GFunc) g_object_unref, NULL);
	g_list_free (client->priv->connections);
	client->priv->connections = NULL;
}
