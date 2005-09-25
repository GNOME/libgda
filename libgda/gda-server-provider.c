/* GDA library
 * Copyright (C) 1998 - 2005 The GNOME Foundation.
 *
 * AUTHORS:
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

#include <glib/gmessages.h>
#include <libgda/gda-server-provider.h>
#include <string.h>
#include <glib/gi18n-lib.h>

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

static GdaBlob *
_gda_server_provider_create_blob (GdaServerProvider *provider,
				  GdaConnection *cnc)
{
	return NULL;
}

static GdaBlob *
_gda_server_provider_fetch_blob (GdaServerProvider *provider,
				 GdaConnection *cnc, const gchar *sql_id)
{
	return NULL;
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

	klass->get_specs = NULL;
	klass->perform_action_params = NULL;

	klass->create_database_cnc = NULL;
	klass->drop_database_cnc = NULL;

	klass->create_table = NULL;
	klass->drop_table = NULL;
	klass->create_index = NULL;
	klass->drop_index = NULL;
	klass->execute_command = NULL;
	klass->get_last_insert_id = NULL;
	klass->begin_transaction = NULL;
	klass->commit_transaction = NULL;
	klass->rollback_transaction = NULL;
	klass->supports = NULL;
	klass->get_schema = NULL;
	klass->create_blob = _gda_server_provider_create_blob;
	klass->fetch_blob = _gda_server_provider_fetch_blob;
	klass->value_to_sql_string = NULL;
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
 * gda_server_provider_get_specs
 * @provider: a #GdaServerProvider object
 * @action_type: what action the specs are for
 *
 * Fetch a list of parameters required to create a database for the specific
 * @provider provider.
 *
 * The list of parameters is returned as an XML string listing each paraleter, its type,
 * its name, etc.
 *
 * Returns: a new XML string, or %NULL if @provider does not implement that method.
 */
gchar *
gda_server_provider_get_specs  (GdaServerProvider *provider,
				GdaClientSpecsType action_type)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);

	if (CLASS (provider)->get_specs)
		return CLASS (provider)->get_specs (provider, action_type);
	else
		return NULL;
}

/**
 * gda_server_provider_perform_action_params
 * @provider: a #GdaServerProvider object
 * @params: a list of parameters required to create a database
 * @action_type: action to perform
 * @error: a place to store an error, or %NULL
 *
 * Performs a specific action specified by the @action_type argument
 * using the parameters listed in @params (the list of parameters may have
 * been obtained using the gda_server_provider_get_specs() method).
 *
 * Returns: TRUE if no error occured
 */
gboolean
gda_server_provider_perform_action_params (GdaServerProvider *provider, 
					   GdaParameterList *params, 
					   GdaClientSpecsType action_type,
					   GError **error)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);

	if (CLASS (provider)->perform_action_params)
		return CLASS (provider)->perform_action_params (provider, params, action_type,
								error);
	else {
		gchar *str;

		str = g_strdup_printf (_("Provider does not support the '%s()' method"), 
				       "perform_action_params");
		g_set_error (error, 0, 0, str);
		g_free (str);
		return FALSE;
	}
}


/**
 * gda_server_provider_create_database_cnc
 * @provider: a #GdaServerProvider object.
 * @name: database name.
 * @cnc: a #GdaConnection object.
 *
 * Creates a database named @name using the @cnc connection.
 *
 * Returns: %TRUE if successful, %FALSE otherwise.
 */
gboolean
gda_server_provider_create_database_cnc (GdaServerProvider *provider,
					 GdaConnection *cnc,
					 const gchar *name)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);
	g_return_val_if_fail (CLASS (provider)->create_database_cnc != NULL, FALSE);

	return CLASS (provider)->create_database_cnc (provider, cnc, name);
}

/**
 * gda_server_provider_drop_database_cnc
 * @provider: a #GdaServerProvider object.
 * @cnc: a #GdaConnection object.
 * @name: database name.
 *
 * Destroy the database named @name using the @cnc connection.
 *
 * Returns: %TRUE if successful, %FALSE otherwise.
 */
gboolean
gda_server_provider_drop_database_cnc (GdaServerProvider *provider,
				       GdaConnection *cnc,
				       const gchar *name)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);
	g_return_val_if_fail (CLASS (provider)->drop_database_cnc != NULL, FALSE);

	return CLASS (provider)->drop_database_cnc (provider, cnc, name);
}


/**
 * gda_server_provider_create_table:
 * @provider: a #GdaServerProvider object.
 * @cnc: a #GdaConnection object.
 * @table_name: name of the table to create.
 * @attributes_list: list of #GdaColumn for all fields in the table.
 * @index_list: list of #GdaDataModelIndex for all (additional) indexes in the table.
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
				  const GList *attributes_list,
				  const GList *index_list)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (table_name != NULL, FALSE);
	g_return_val_if_fail (attributes_list != NULL, FALSE);
	g_return_val_if_fail (CLASS (provider)->create_table != NULL, FALSE);

	return CLASS (provider)->create_table (provider, cnc, table_name, attributes_list, index_list);
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
 * gda_server_provider_create_index:
 * @provider: a #GdaServerProvider object.
 * @cnc: a #GdaConnection object.
 * @index: a #GdaDataModelIndex object containing all index information.
 * @table_name: name of the table to create index for.
 *
 * Proxy the call to the create_index method on the #GdaServerProvider class
 * to the corresponding provider.
 *
 * Returns: %TRUE if successful, %FALSE otherwise.
 */
gboolean
gda_server_provider_create_index (GdaServerProvider *provider,
				  GdaConnection *cnc,
				  const GdaDataModelIndex *index,
				  const gchar *table_name)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (table_name != NULL, FALSE);
	g_return_val_if_fail (index != NULL, FALSE);
	g_return_val_if_fail (CLASS (provider)->create_index != NULL, FALSE);

	return CLASS (provider)->create_index (provider, cnc, index, table_name);
}

/**
 * gda_server_provider_drop_index:
 * @provider: a #GdaServerProvider object.
 * @cnc: a #GdaConnection object.
 * @index_name: name of the index to remove.
 * @primary_key: if index is a PRIMARY KEY.
 * @table_name: name of the table index to remove from.
 *
 * Proxy the call to the drop_index method on the #GdaServerProvider class
 * to the corresponding provider.
 *
 * Returns: %TRUE if successful, %FALSE otherwise.
 */
gboolean
gda_server_provider_drop_index (GdaServerProvider *provider,
				GdaConnection *cnc,
				const gchar *index_name,
				gboolean primary_key,
				const gchar *table_name)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (index_name != NULL, FALSE);
	g_return_val_if_fail (table_name != NULL, FALSE);
	g_return_val_if_fail (CLASS (provider)->drop_index != NULL, FALSE);

	return CLASS (provider)->drop_index (provider, cnc, index_name, primary_key, table_name);
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
 *
 * Creates a BLOB (Binary Large OBject) with read/write access.
 *
 * Returns: a new #GdaBlob object, or %NULL if the database (or the libgda's provider)
 * does not support BLOBS
 */
GdaBlob *
gda_server_provider_create_blob (GdaServerProvider *provider,
				 GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	return CLASS (provider)->create_blob (provider, cnc);
}

/**
 * gda_server_provider_fetch_blob_by_id
 * @provider: a server provider.
 * @cnc: a #GdaConnection object.
 * @sql_id: the SQL ID of the blob to fetch
 *
 * Fetch an existing BLOB (Binary Large OBject) using its SQL ID.
 *
 * Returns: a new #GdaBlob object, or %NULL if the database (or the libgda's provider)
 * does not support BLOBS
 */
GdaBlob *
gda_server_provider_fetch_blob_by_id (GdaServerProvider *provider,
				      GdaConnection *cnc, const gchar *sql_id)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (sql_id, FALSE);

	return CLASS (provider)->fetch_blob (provider, cnc, sql_id);
}

 
/**
 * gda_server_provider_value_to_sql_string
 * @provider: a server provider.
 * @cnc: a #GdaConnection object.
 * @from: #GdaValue to convert from
 *
 * Produces a fully quoted and escaped string from a GdaValue
 *
 * Returns: escaped and quoted value or NULL if not supported.
 */
gchar *
gda_server_provider_value_to_sql_string (GdaServerProvider *provider,
				   GdaConnection *cnc,
				   GdaValue *from)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (from != NULL, FALSE);

	return CLASS (provider)->value_to_sql_string (provider, cnc, from);
}
