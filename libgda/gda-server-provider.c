/* GDA library
 * Copyright (C) 1998-2002 The GNOME Foundation.
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

#include <config.h>
#include <bonobo/bonobo-exception.h>
#include <libgda/gda-server-provider.h>

#define PARENT_TYPE BONOBO_OBJECT_TYPE
#define CLASS(provider) (GDA_SERVER_PROVIDER_CLASS (G_OBJECT_GET_CLASS (provider)))

struct _GdaServerProviderPrivate {
	GList *connections;
};

static void gda_server_provider_class_init (GdaServerProviderClass *klass);
static void gda_server_provider_init       (GdaServerProvider *provider,
					    GdaServerProviderClass *klass);
static void gda_server_provider_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

/*
 * CORBA methods implementation
 */

static CORBA_char *
impl_Provider_getVersion (PortableServer_Servant servant, CORBA_Environment *ev)
{
	return CORBA_string_dup (VERSION);
}

static GNOME_Database_Connection
impl_Provider_createConnection (PortableServer_Servant servant, CORBA_Environment *ev)
{
	GdaServerConnection *cnc;
	GdaServerProvider *provider = (GdaServerProvider *) bonobo_object (servant);

	bonobo_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), CORBA_OBJECT_NIL, ev);

	cnc = gda_server_connection_new (provider);
	if (GDA_IS_SERVER_CONNECTION (cnc))
		return bonobo_object_corba_objref (BONOBO_OBJECT (cnc));

	return CORBA_OBJECT_NIL; 
}

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
	klass->execute_command = NULL;
	klass->begin_transaction = NULL;
	klass->commit_transaction = NULL;
	klass->rollback_transaction = NULL;
	klass->supports = NULL;
	klass->get_schema = NULL;

	/* set the epv */
	epv = &klass->epv;
	epv->getVersion = impl_Provider_getVersion;
	epv->createConnection = impl_Provider_createConnection;
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
	g_list_free (provider->priv->connections);

	g_free (provider->priv);
	provider->priv = NULL;

	/* chain to parent class */
	parent_class->finalize (object);
}

BONOBO_TYPE_FUNC_FULL (GdaServerProvider,
			 GNOME_Database_Provider,
			 PARENT_TYPE,
			 gda_server_provider)

/**
 * gda_server_provider_open_connection
 * @provider: a #GdaServerProvider object.
 * @cnc: a #GdaServerConnection object.
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
gboolean
gda_server_provider_open_connection (GdaServerProvider *provider,
				     GdaServerConnection *cnc,
				     GdaQuarkList *params,
				     const gchar *username,
				     const gchar *password)
{
	gboolean retcode;
	const gchar *pooling;

	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (CLASS (provider)->open_connection != NULL, FALSE);

	/* check if POOLING is specified */
	pooling = gda_quark_list_find (params, "POOLING");
	if (pooling && !strcmp (pooling, "1")) {
	}

	retcode = CLASS (provider)->open_connection (provider, cnc, params, username, password);
	if (retcode) {
		provider->priv->connections = g_list_append (
			provider->priv->connections, cnc);
	}
	else {
		/* unref the object if we've got no connections */
		if (!provider->priv->connections)
			bonobo_object_idle_unref (BONOBO_OBJECT (provider));
	}

	return retcode;
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
	if (!provider->priv->connections)
		bonobo_object_idle_unref (BONOBO_OBJECT (provider));

	return retcode;
}

/**
 * gda_server_provider_execute_command
 */
GList *
gda_server_provider_execute_command (GdaServerProvider *provider,
				     GdaServerConnection *cnc,
				     GdaCommand *cmd,
				     GdaParameterList *params)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cmd != NULL, NULL);
	g_return_val_if_fail (CLASS (provider)->execute_command != NULL, NULL);

	return CLASS (provider)->execute_command (provider, cnc, cmd, params);
}

/**
 * gda_server_provider_begin_transaction
 */
gboolean
gda_server_provider_begin_transaction (GdaServerProvider *provider,
				       GdaServerConnection *cnc,
				       const gchar *trans_id)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (CLASS (provider)->begin_transaction != NULL, FALSE);

	return CLASS (provider)->begin_transaction (provider, cnc, trans_id);
}

/**
 * gda_server_provider_commit_transaction
 */
gboolean
gda_server_provider_commit_transaction (GdaServerProvider *provider,
				       GdaServerConnection *cnc,
				       const gchar *trans_id)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (CLASS (provider)->commit_transaction != NULL, FALSE);

	return CLASS (provider)->commit_transaction (provider, cnc, trans_id);
}

/**
 * gda_server_provider_rollback_transaction
 */
gboolean
gda_server_provider_rollback_transaction (GdaServerProvider *provider,
					  GdaServerConnection *cnc,
					  const gchar *trans_id)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (CLASS (provider)->rollback_transaction != NULL, FALSE);

	return CLASS (provider)->rollback_transaction (provider, cnc, trans_id);
}

/**
 * gda_server_provider_supports
 */
gboolean
gda_server_provider_supports (GdaServerProvider *provider,
			      GdaServerConnection *cnc,
			      GNOME_Database_Feature feature)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (CLASS (provider)->supports != NULL, FALSE);

	return CLASS (provider)->supports (provider, cnc, feature);
}

/**
 * gda_server_provider_get_schema
 */
GdaServerRecordset *
gda_server_provider_get_schema (GdaServerProvider *provider,
				GdaServerConnection *cnc,
				GNOME_Database_Connection_Schema schema,
				GdaParameterList *params)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	g_return_val_if_fail (CLASS (provider)->get_schema != NULL, NULL);

	return CLASS (provider)->get_schema (provider, cnc, schema, params);
}
