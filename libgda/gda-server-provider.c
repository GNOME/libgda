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

#include <glib/gmessages.h>
#include <libgda/gda-server-provider.h>
#include <string.h>

#define PARENT_TYPE G_TYPE_OBJECT
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
 * GdaServerProvider class implementation
 */

static gboolean
_gda_server_provider_create_blob (GdaServerProvider *provider,
				 GdaConnection *cnc,
				 GdaBlob *blob)
{
	return FALSE;
}

static void
gda_server_provider_class_init (GdaServerProviderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_server_provider_finalize;
	klass->get_version = NULL;
	klass->open_connection = NULL;
	klass->reset_connection = NULL;
	klass->close_connection = NULL;
	klass->get_server_version = NULL;
	klass->get_database = NULL;
	klass->change_database = NULL;
	klass->create_database = NULL;
	klass->drop_database = NULL;
	klass->create_table = NULL;
	klass->drop_table = NULL;
	klass->execute_command = NULL;
	klass->get_last_insert_id = NULL;
	klass->begin_transaction = NULL;
	klass->commit_transaction = NULL;
	klass->rollback_transaction = NULL;
	klass->supports = NULL;
	klass->get_schema = NULL;
	klass->create_blob = _gda_server_provider_create_blob;
	klass->escape_string = NULL;
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

GType
gda_server_provider_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaServerProviderClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_server_provider_class_init,
			NULL,
			NULL,
			sizeof (GdaServerProvider),
			0,
			(GInstanceInitFunc) gda_server_provider_init
		};
		type = g_type_register_static (PARENT_TYPE, "GdaServerProvider", &info, G_TYPE_FLAG_ABSTRACT);
	}
	return type;
}

/**
 * gda_server_provider_get_version
 * @provider: a #GdaServerProvider object.
 *
 * Get the version of the given provider.
 *
 * Returns: a string containing the version identification.
 */
const gchar *
gda_server_provider_get_version (GdaServerProvider *provider)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);

	if (CLASS (provider)->get_version != NULL)
		return CLASS (provider)->get_version (provider);

	return PACKAGE_VERSION;
}

/**
 * gda_server_provider_open_connection
 * @provider: a #GdaServerProvider object.
 * @cnc: a #GdaConnection object.
 * @params:
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
				     GdaConnection *cnc,
				     GdaQuarkList *params,
				     const gchar *username,
				     const gchar *password)
{
	gboolean retcode;
	const gchar *pooling;

	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (CLASS (provider)->open_connection != NULL, FALSE);

	/* check if POOLING is specified */
	pooling = gda_quark_list_find (params, "POOLING");
	if (pooling) {
		if (!strcmp (pooling, "1")) {
		}

		gda_quark_list_remove (params, "POOLING");
	}

	retcode = CLASS (provider)->open_connection (provider, cnc, params, username, password);
	if (retcode) {
		provider->priv->connections = g_list_append (
			provider->priv->connections, cnc);
	}
	else {
		/* unref the object if we've got no connections */
		if (!provider->priv->connections)
			g_object_unref (G_OBJECT (provider));
	}

	return retcode;
}

/**
 * gda_server_provider_reset_connection:
 * @provider: A #GdaServerProvider object.
 * @cnc: The connection to be reset.
 *
 * Calls the reset_connection method implementation on the given #GdaServerProvider.
 *
 * Returns: %TRUE if successful, %FALSE otherwise.
 */
gboolean
gda_server_provider_reset_connection (GdaServerProvider *provider, GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (CLASS (provider)->reset_connection != NULL, FALSE);

	return CLASS (provider)->reset_connection (provider, cnc);
}

/**
 * gda_server_provider_close_connection
 * @provider:
 * @cnc:
 *
 * Returns:
 * 
 */
gboolean
gda_server_provider_close_connection (GdaServerProvider *provider, GdaConnection *cnc)
{
	gboolean retcode;

	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	if (CLASS (provider)->close_connection != NULL)
		retcode = CLASS (provider)->close_connection (provider, cnc);
	else
		retcode = TRUE;

	provider->priv->connections = g_list_remove (provider->priv->connections, cnc);
	if (!provider->priv->connections)
		g_object_unref (G_OBJECT (provider));

	return retcode;
}

/**
 * gda_server_provider_get_server_version
 * @provider:
 * @cnc:
 *
 * Returns:
 */
const gchar *
gda_server_provider_get_server_version (GdaServerProvider *provider, GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	g_return_val_if_fail (CLASS (provider)->get_server_version != NULL, NULL);

	return CLASS (provider)->get_server_version (provider, cnc);
}

/**
 * gda_server_provider_get_database
 * @provider: a #GdaServerProvider object.
 * @cnc: a #GdaConnection object.
 *
 * Proxy the call to the get_database method on the
 * #GdaServerProvider class to the corresponding provider.
 *
 * Returns: the name of the current database.
 */
const gchar *
gda_server_provider_get_database (GdaServerProvider *provider,
				  GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (CLASS (provider)->get_database != NULL, NULL);

	return CLASS (provider)->get_database (provider, cnc);
}

/**
 * gda_server_provider_change_database
 * @provider: a #GdaServerProvider object.
 * @cnc: a #GdaConnection object.
 * @name: database name.
 *
 * Proxy the call to the change_database method on the
 " #GdaServerProvider class to the corresponding provider.
 *
 * Returns: %TRUE if successful, %FALSE otherwise.
 */
gboolean
gda_server_provider_change_database (GdaServerProvider *provider,
				     GdaConnection *cnc,
				     const gchar *name)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);
	g_return_val_if_fail (CLASS (provider)->change_database != NULL, FALSE);

	return CLASS (provider)->change_database (provider, cnc, name);
}

/**
 * gda_server_provider_create_database
 * @provider: a #GdaServerProvider object.
 * @name: database name.
 * @cnc: a #GdaConnection object.
 *
 * Proxy the call to the create_database method on the
 * #GdaServerProvider class to the corresponding provider.
 *
 * Returns: %TRUE if successful, %FALSE otherwise.
 */
gboolean
gda_server_provider_create_database (GdaServerProvider *provider,
				     GdaConnection *cnc,
				     const gchar *name)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);
	g_return_val_if_fail (CLASS (provider)->create_database != NULL, FALSE);

	return CLASS (provider)->create_database (provider, cnc, name);
}

/**
 * gda_server_provider_drop_database
 * @provider: a #GdaServerProvider object.
 * @cnc: a #GdaConnection object.
 * @name: database name.
 *
 * Proxy the call to the drop_database method on the
 * #GdaServerProvider class to the corresponding provider.
 *
 * Returns: %TRUE if successful, %FALSE otherwise.
 */
gboolean
gda_server_provider_drop_database (GdaServerProvider *provider,
				   GdaConnection *cnc,
				   const gchar *name)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);
	g_return_val_if_fail (CLASS (provider)->drop_database != NULL, FALSE);

	return CLASS (provider)->drop_database (provider, cnc, name);
}

/**
 * gda_server_provider_create_table:
 * @provider: a #GdaServerProvider object.
 * @cnc: a #GdaConnection object.
 * @table_name: name of the table to create.
 * @attributes: list of attributes for all fields in the table.
 *
 * Proxy the call to the create_table method on the #GdaServerProvider class
 * to the corresponding provider.
 *
 * Returns: %TRUE if successful, %FALSE otherwise.
 */
gboolean
gda_server_provider_create_table (GdaServerProvider *provider,
				  GdaConnection *cnc,
				  const gchar *table_name,
				  const GdaDataModelColumnAttributes *attributes[])
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (table_name != NULL, FALSE);
	g_return_val_if_fail (attributes != NULL, FALSE);
	g_return_val_if_fail (CLASS (provider)->create_table != NULL, FALSE);

	return CLASS (provider)->create_table (provider, cnc, table_name, attributes);
}

/**
 * gda_server_provider_drop_table:
 * @provider: a #GdaServerProvider object.
 * @cnc: a #GdaConnection object.
 * @table_name: name of the table to remove.
 *
 * Proxy the call to the drop_table method on the #GdaServerProvider class
 * to the corresponding provider.
 *
 * Returns: %TRUE if successful, %FALSE otherwise.
 */
gboolean
gda_server_provider_drop_table (GdaServerProvider *provider,
				GdaConnection *cnc,
				const gchar *table_name)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (table_name != NULL, FALSE);
	g_return_val_if_fail (CLASS (provider)->drop_table != NULL, FALSE);

	return CLASS (provider)->drop_table (provider, cnc, table_name);
}

/**
 * gda_server_provider_execute_command
 * @provider:
 * @cnc:
 * @cmd:
 * @params:
 *
 * Returns:
 * 
 */
GList *
gda_server_provider_execute_command (GdaServerProvider *provider,
				     GdaConnection *cnc,
				     GdaCommand *cmd,
				     GdaParameterList *params)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cmd != NULL, NULL);
	g_return_val_if_fail (CLASS (provider)->execute_command != NULL, NULL);

	return CLASS (provider)->execute_command (provider, cnc, cmd, params);
}

/**
 * gda_server_provider_get_last_insert_id
 * @provider: a #GdaServerProvider object.
 * @cnc: connection to act upon.
 * @recset: resultset to get the last insert ID from.
 *
 * Retrieve from the given #GdaServerProvider the ID of the last inserted row.
 * A connection must be specified, and, optionally, a result set. If not NULL,
 * the provider should try to get the last insert ID for the given result set.
 *
 * Returns: a string representing the ID of the last inserted row, or NULL
 * if an error occurred or no row has been inserted. It is the caller's
 * reponsibility to free the returned string.
 */
gchar *
gda_server_provider_get_last_insert_id (GdaServerProvider *provider, GdaConnection *cnc, GdaDataModel *recset)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (CLASS (provider)->get_last_insert_id != NULL, NULL);

	return CLASS (provider)->get_last_insert_id (provider, cnc, recset);
}

/**
 * gda_server_provider_begin_transaction
 * @provider:
 * @cnc:
 * @xaction:
 *
 * Returns:
 * 
 */
gboolean
gda_server_provider_begin_transaction (GdaServerProvider *provider,
				       GdaConnection *cnc,
				       GdaTransaction *xaction)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (CLASS (provider)->begin_transaction != NULL, FALSE);

	return CLASS (provider)->begin_transaction (provider, cnc, xaction);
}

/**
 * gda_server_provider_commit_transaction
 * @provider:
 * @cnc:
 * @xaction:
 *
 * Returns:
 * 
 */
gboolean
gda_server_provider_commit_transaction (GdaServerProvider *provider,
					GdaConnection *cnc,
					GdaTransaction *xaction)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (CLASS (provider)->commit_transaction != NULL, FALSE);

	return CLASS (provider)->commit_transaction (provider, cnc, xaction);
}

/**
 * gda_server_provider_rollback_transaction
 * @provider:
 * @cnc:
 * @xaction:
 *
 * Returns:
 * 
 */
gboolean
gda_server_provider_rollback_transaction (GdaServerProvider *provider,
					  GdaConnection *cnc,
					  GdaTransaction *xaction)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (CLASS (provider)->rollback_transaction != NULL, FALSE);

	return CLASS (provider)->rollback_transaction (provider, cnc, xaction);
}

/**
 * gda_server_provider_supports
 * @provider:
 * @cnc:
 * @feature:
 *
 * Returns:
 */
gboolean
gda_server_provider_supports (GdaServerProvider *provider,
			      GdaConnection *cnc,
			      GdaConnectionFeature feature)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (CLASS (provider)->supports != NULL, FALSE);

	return CLASS (provider)->supports (provider, cnc, feature);
}

/**
 * gda_server_provider_get_schema
 * @provider:
 * @cnc:
 * @schema:
 * @params:
 *
 * Returns:
 * 
 */
GdaDataModel *
gda_server_provider_get_schema (GdaServerProvider *provider,
				GdaConnection *cnc,
				GdaConnectionSchema schema,
				GdaParameterList *params)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (CLASS (provider)->get_schema != NULL, NULL);

	return CLASS (provider)->get_schema (provider, cnc, schema, params);
}


/**
 * gda_server_provider_create_blob
 * @provider: a server provider.
 * @cnc: a #GdaConnection object.
 * @blob: a user-allocated #GdaBlob structure.
 *
 * Creates a BLOB (Binary Large OBject) with read/write access.
 *
 * Returns: %FALSE if the database does not support BLOBs. %TRUE otherwise
 * and the GdaBlob is created and ready to be used.
 */
gboolean
gda_server_provider_create_blob (GdaServerProvider *provider,
				 GdaConnection *cnc,
				 GdaBlob *blob)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (blob != NULL, FALSE);

	return CLASS (provider)->create_blob (provider, cnc, blob);
}

/**
 * gda_server_provider_escape_string
 * @provider: a server provider.
 * @cnc: a #GdaConnection object.
 * @from: String to be escaped.
 * @to: Buffer to place the resulting escaped string.
 *
 * Natively escapes string with \ slashes etc.
 *
 * Returns: %FALSE if the database does not support escaping.?
 */
gboolean
gda_server_provider_escape_string (GdaServerProvider *provider,
				   GdaConnection *cnc,
				   const gchar *from,
				   gchar *to)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (from != NULL, FALSE);
	g_return_val_if_fail (to != NULL, FALSE);

	return CLASS (provider)->escape_string (provider, cnc, from, to);
}
