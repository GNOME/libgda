/* GDA library
 * Copyright (C) 1998-2001 The Free Software Foundation
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

#include <libgda/gda-client.h>
#include <libgda/gda-config.h>
#include <libgda/gda-log.h>
#include <bonobo/bonobo-i18n.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-moniker-util.h>

#define PARENT_TYPE BONOBO_X_OBJECT_TYPE

struct _GdaClientPrivate {
	GHashTable *providers;
	GList *connections;
};

static void gda_client_class_init (GdaClientClass *klass);
static void gda_client_init       (GdaClient *client, GdaClientClass *klass);
static void gda_client_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

/*
 * Callbacks
 */

static void
connection_finalized_cb (GObject *object, gpointer user_data)
{
	GdaConnection *cnc = (GdaConnection *) object;
	GdaClient *client = (GdaClient *) user_data;

	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	g_return_if_fail (GDA_IS_CLIENT (client));

	client->priv->connections = g_list_remove (client->priv->connections, cnc);
}

/*
 * CORBA methods implementation
 */

static void
impl_Client_notifyAction (PortableServer_Servant servant,
			  GNOME_Database_ActionId action,
			  GNOME_Database_ParameterList *corba_params,
			  CORBA_Environment *ev)
{
}

/*
 * GdaClient class implementation
 */

static void
gda_client_class_init (GdaClientClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	POA_GNOME_Database_Client__epv *epv;

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_client_finalize;

	/* set the epv */
	epv = &klass->epv;
	epv->notifyAction = (gpointer) impl_Client_notifyAction;
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
	CORBA_Environment ev;
	gchar *iid = (gchar *) key;
	GNOME_Database_Provider corba_provider = (GNOME_Database_Provider) value;

	g_free (iid);

	CORBA_exception_init (&ev);
	CORBA_Object_release (corba_provider, &ev);
	CORBA_exception_free (&ev);
}

static void
gda_client_finalize (GObject *object)
{
	GdaClient *client = (GdaClient *) object;

	g_return_if_fail (GDA_IS_CLIENT (client));

	/* free memory */
	g_list_foreach (client->priv->connections, (GFunc) gda_connection_close, NULL);
	g_list_free (client->priv->connections);
	client->priv->connections = NULL;

	g_hash_table_foreach (client->priv->providers, (GHFunc) free_hash_provider, NULL);
	g_hash_table_destroy (client->priv->providers);
	client->priv->providers = NULL;

	g_free (client->priv);
	client->priv = NULL;

	/* chain to parent class */
	parent_class->finalize (object);
}

BONOBO_X_TYPE_FUNC_FULL (GdaClient,
			 GNOME_Database_Client,
			 PARENT_TYPE,
			 gda_client)

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
	GList *l;
	GdaConnection *cnc;
	GNOME_Database_Provider corba_provider;
	GNOME_Database_Connection corba_cnc;
	CORBA_Environment ev;
	GdaDataSourceInfo *dsn_info;

	g_return_val_if_fail (GDA_IS_CLIENT (client), NULL);

	/* get the data source info */
	dsn_info = gda_config_find_data_source (dsn);
	if (!dsn_info) {
		gda_log_error (_("Data source %s not found in configuration"), dsn);
		return NULL;
	}

	/* search for the connection in our private list */
	for (l = client->priv->connections; l; l = l->next) {
		const gchar *tmp_str, *tmp_usr, *tmp_pwd;

		cnc = GDA_CONNECTION (l->data);
		tmp_str = gda_connection_get_string (cnc);
		tmp_usr = gda_connection_get_username (cnc);
		tmp_pwd = gda_connection_get_password (cnc);

		if (((!tmp_str && !dsn_info->cnc_string) || !strcmp (tmp_str, dsn_info->cnc_string)) &&
		    ((!tmp_usr && !username) || !strcmp (tmp_usr, username)) &&
		    ((!tmp_pwd && !password) || !strcmp (tmp_pwd, password))) {
			g_object_ref (G_OBJECT (cnc));
			gda_config_free_data_source_info (dsn_info);
			return cnc;
		}
	}

	/* try to find provider in our hash table */
	corba_provider = g_hash_table_lookup (client->priv->providers, dsn_info->provider);
	if (!corba_provider) {
		/* not found, so load the new provider */
		CORBA_exception_init (&ev);

		corba_provider = bonobo_get_object (dsn_info->provider,
						    "IDL:GNOME/Database/Provider:1.0",
						    &ev);
		if (BONOBO_EX (&ev)) {
			gda_log_error (_("Could not activate object %s"), dsn_info->provider);
			CORBA_exception_free (&ev);
			gda_config_free_data_source_info (dsn_info);
			return NULL;
		}

		g_hash_table_insert (client->priv->providers,
				     g_strdup (dsn_info->provider),
				     corba_provider);

		CORBA_exception_free (&ev);
	}

	CORBA_exception_init (&ev);

	corba_cnc = GNOME_Database_Provider_createConnection (corba_provider, &ev);
	if (BONOBO_EX (&ev)) {
		CORBA_exception_free (&ev);
		gda_log_error (_("Could not create connection component"));
		return NULL;
	}

	CORBA_exception_free (&ev);

	cnc = gda_connection_new (client, corba_cnc, dsn_info->cnc_string, username ? username : "", password ? password : "");
	gda_config_free_data_source_info (dsn_info);

	if (!GDA_IS_CONNECTION (cnc))
		return NULL;

	/* add list to our private list */
	client->priv->connections = g_list_append (client->priv->connections, cnc);
	g_signal_connect (G_OBJECT (cnc), "finalize",
			  G_CALLBACK (connection_finalized_cb), client);

	return cnc;
}

