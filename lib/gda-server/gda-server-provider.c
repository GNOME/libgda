/* GDA server library
 * Copyright (C) 1998-2001 The Free Software Foundation
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

#include "gda-server-provider.h"

#define PARENT_TYPE BONOBO_X_OBJECT_TYPE
#define CLASS(provider) (GDA_SERVER_PROVIDER_CLASS (G_OBJECT (provider)->klass))

struct _GdaServerProviderPrivate {
	GList *connections;
};

static void gda_server_provider_class_init (GdaServerProviderClass *klass);
static void gda_server_provider_init       (GdaServerProvider *provider,
					    GdaServerProviderClass *klass);
static void gda_server_provider_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

/*
 * GdaServerProvider class implementation
 */

static void
gda_server_provider_class_init (GdaServerProviderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	POA_GNOME_Database_Provider__epv *epv;

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_server_provider_finalize;
	klass->open_connection = NULL;
	klass->close_connection = NULL;

	/* sets the epv */
	epv = &klass->epv;
	epv->openConnection = impl_Provider_openConnection;
	epv->closeConnection = impl_Provider_closeConnection;
}

static void
gda_server_provider_init (GdaServerProvider *provider,
			  GdaServerProviderClass *klass)
{
	g_return_if_fail (GDA_IS_SERVER_PROVIDER (provider));

	provider->priv = g_new0 (GdaServerProviderPrivate, 1);
	provider->priv->connections = NULL;
}

static void
gda_server_provider_finalize (GObject *object)
{
	GdaServerProvider *provider = (GdaServerProvider *) object;

	g_return_if_fail (GDA_IS_SERVER_PROVIDER (provider));

	/* free memory */
	g_hash_table_foreach (provider->priv->connections, free_hash_connection, NULL);
	g_hash_table_destroy (provider->priv->connections);

	g_free (provider->priv);
	provider->priv = NULL;

	/* chain to parent class */
	parent_class->finalize (object);
}

BONOBO_X_TYPE_FUNC_FULL (GdaServerProvider,
			 GNOME_Database_Provider,
			 PARENT_TYPE,
			 gda_server_provider)

/**
 * gda_server_provider_open_connection
 * @provider: a #GdaServerProvider object.
 * @cnc_string: connection string.
 * @username: user name for logging in.
 * @password: password for authentication.
 *
 * Tries to open a new connection on the given #GdaServerProvider
 * object.
 *
 * Returns: a newly-allocated #GdaServerConnection object, or NULL
 * if it fails.
 */
GdaServerConnection *
gda_server_provider_open_connection (GdaServerProvider *provider,
				     const gchar *cnc_string,
				     const gchar *username,
				     const gchar *password)
{
	GdaServerConnection *cnc;

	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	g_return_val_if_fail (CLASS (provider)->open_connection != NULL, NULL);

	cnc = CLASS (provider)->open_connection (provider, cnc_string, username, password);
	if (cnc) {
		provider->priv->connections = g_list_append (
			provider->priv->connections, cnc);
	}

	return cnc;
}

/**
 * gda_server_provider_close_connection
 */
gboolean
gda_server_provider_close_connection (GdaServerProvider *provider, GdaServerConnection *cnc)
{
	gboolean retcode;

	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), FALSE);

	if (CLASS (provider)->close_connection != NULL)
		retcode = CLASS (provider)->close_connection (provider, cnc);
	else
		retcode = TRUE;

	provider->priv->connections = g_list_remove (provider->priv->connections, cnc);
	g_object_unref (G_OBJECT (cnc));

	return retcode;
}
