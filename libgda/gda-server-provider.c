/* GDA library
 * Copyright (C) 1998 - 2006 The GNOME Foundation.
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
#include <libgda/gda-server-provider-extra.h>
#include <libgda/gda-server-provider-private.h>
#include <libgda/gda-data-handler.h>
#include <string.h>
#include <glib/gi18n-lib.h>

#define PARENT_TYPE G_TYPE_OBJECT
#define CLASS(provider) (GDA_SERVER_PROVIDER_CLASS (G_OBJECT_GET_CLASS (provider)))

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
	klass->get_server_version = NULL;
	klass->get_info = NULL;
	klass->supports = NULL;
	klass->get_schema = NULL;

	klass->get_data_handler = NULL;
	klass->string_to_value = NULL;

	klass->open_connection = NULL;
	klass->close_connection = NULL;
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
	klass->create_blob = _gda_server_provider_create_blob;
	klass->fetch_blob = _gda_server_provider_fetch_blob;
}

static void
gda_server_provider_init (GdaServerProvider *provider,
			  GdaServerProviderClass *klass)
{
	g_return_if_fail (GDA_IS_SERVER_PROVIDER (provider));

	provider->priv = g_new0 (GdaServerProviderPrivate, 1);
	provider->priv->connections = NULL;
	provider->priv->data_handlers = g_hash_table_new_full ((GHashFunc) gda_server_provider_handler_info_hash_func,
							       (GEqualFunc) gda_server_provider_handler_info_equal_func,
							       (GDestroyNotify) gda_server_provider_handler_info_free,
							       (GDestroyNotify) g_object_unref);
}

static void
gda_server_provider_finalize (GObject *object)
{
	GdaServerProvider *provider = (GdaServerProvider *) object;

	g_return_if_fail (GDA_IS_SERVER_PROVIDER (provider));

	/* free memory */
	if (provider->priv) {
		g_list_free (provider->priv->connections);
		g_hash_table_destroy (provider->priv->data_handlers);
		
		g_free (provider->priv);
		provider->priv = NULL;
	}

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
 * gda_server_provider_get_info
 * @provider: a #GdaServerProvider object.
 * @cnc: a #GdaConnection, or %NULL
 *
 * Retreive some information specific to the provider. The returned #GdaServerProviderInfo
 * structure must not be modified
 *
 * Returns: a #GdaServerProviderInfo pointer or %NULL if an error occurred
 */
GdaServerProviderInfo *
gda_server_provider_get_info (GdaServerProvider *provider, GdaConnection *cnc)
{
	GdaServerProviderInfo *retval = NULL;
	static GdaServerProviderInfo *default_info = NULL;

	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	if (cnc)
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	if (CLASS (provider)->get_info != NULL)
		retval = CLASS (provider)->get_info (provider, cnc);

	if (!retval) {
		if (!default_info) {
			default_info = g_new0 (GdaServerProviderInfo, 1);
			default_info->provider_name = NULL;
			default_info->is_case_insensitive = TRUE;
			default_info->implicit_data_types_casts = TRUE;
			default_info->alias_needs_as_keyword = TRUE;
		}

		retval = default_info;
	}

	return retval;
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
 * Returns: TRUE if no error occurred
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
 * gda_server_provider_supports_operation
 * @provider: a #GdaServerProvider object
 * @cnc: a #GdaConnection object which would be used to perform an action
 * @type: the type of operation requested
 * @options: a list of named parameters, or %NULL
 *
 * Tells if @provider supports the @type of operation on the @cnc connection, using the
 * (optional) @options parameters.
 *
 * Returns: TRUE if the operation is supported
 */
gboolean
gda_server_provider_supports_operation (GdaServerProvider *provider, GdaConnection *cnc, 
					GdaServerOperationType type, GdaParameterList *options)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
		g_return_val_if_fail (gda_connection_get_provider_obj (cnc) == provider, FALSE);
	}
	if (CLASS (provider)->supports_operation)
		return CLASS (provider)->supports_operation (provider, cnc, type, options);
	else
		return FALSE;
}

typedef struct {
	gchar                      *path;
	GdaServerOperationNodeType  node_type;
	GType                       data_type;
} OpReq;

static OpReq op_req_CREATE_TABLE [] = {
	{"/TABLE_DEF_P",               GDA_SERVER_OPERATION_NODE_PARAMLIST, 0},
	{"/TABLE_DEF_P/TABLE_NAME",    GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{"/FIELDS_A",                  GDA_SERVER_OPERATION_NODE_DATA_MODEL, 0},
	{"/FIELDS_A/@COLUMN_NAME",     GDA_SERVER_OPERATION_NODE_DATA_MODEL_COLUMN, G_TYPE_STRING},
	{"/FIELDS_A/@COLUMN_TYPE",     GDA_SERVER_OPERATION_NODE_DATA_MODEL_COLUMN, G_TYPE_STRING},
	{NULL}
};

static OpReq *op_req_table [GDA_SERVER_OPERATION_NB] = {
	op_req_CREATE_TABLE, /* GDA_SERVER_OPERATION_CREATE_TABLE */
	NULL, /* GDA_SERVER_OPERATION_DROP_TABLE */
	NULL, /* GDA_SERVER_OPERATION_CREATE_INDEX */
	NULL, /* GDA_SERVER_OPERATION_DROP_INDEX */
};

/**
 * gda_server_provider_create_operation
 * @provider: a #GdaServerProvider object
 * @cnc: a #GdaConnection object which will be used to perform an action
 * @type: the type of operation requested
 * @options: an optional list of parameters
 * @error: a place to store an error, or %NULL
 *
 * Creates a new #GdaServerOperation object which can be modified in order to perform the @type type of
 * action. The @options 
 *
 * Returns: a new #GdaServerOperation object, or %NULL in the provider does not support the @type type
 * of operation or if an error occured
 */
GdaServerOperation *
gda_server_provider_create_operation (GdaServerProvider *provider, GdaConnection *cnc, GdaServerOperationType type, 
				      GdaParameterList *options, GError **error)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
		g_return_val_if_fail (gda_connection_get_provider_obj (cnc) == provider, NULL);
	}
	if (CLASS (provider)->create_operation) {
		GdaServerOperation *op;
		GdaServerProviderInfo *pinfo;

		pinfo = gda_server_provider_get_info (provider, cnc);
		op = CLASS (provider)->create_operation (provider, cnc, type, options, error);
		if (op) {
			/* test op's conformance */
			gint i = 0;
			OpReq *opreq = op_req_table [type];
			while (opreq->path) {
				GdaServerOperationNodeType node_type;
				node_type = gda_server_operation_get_node_type (op, opreq->path, NULL);
				if (node_type == GDA_SERVER_OPERATION_NODE_UNKNOWN) 
					g_warning (_("Provider %s created a GdaServerOperation without node for '%s'"),
						   pinfo->provider_name, opreq->path);
				else 
					if (node_type != opreq->node_type)
						g_warning (_("Provider %s created a GdaServerOperation with wrong node type for '%s'"),
							   pinfo->provider_name, opreq->path);
				opreq += 1;
			}
		}
		return op;
	}
	else
		return NULL;
}

/**
 * gda_server_provider_render_operation
 * @provider: a #GdaServerProvider object
 * @cnc: a #GdaConnection object which will be used to perform an action
 * @op: a #GdaServerOperation object
 * @error: a place to store an error, or %NULL
 *
 * Creates an SQL statement (possibly using some specific extensions of the DBMS) corresponding to the
 * @op operation.
 *
 * Returns: a new string, or %NULL if an error occurred.
 */
gchar *
gda_server_provider_render_operation (GdaServerProvider *provider, GdaConnection *cnc, 
				      GdaServerOperation *op, GError **error)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
		g_return_val_if_fail (gda_connection_get_provider_obj (cnc) == provider, NULL);
	}
	if (CLASS (provider)->render_operation)
		return CLASS (provider)->render_operation (provider, cnc, op, error);
	else
		return NULL;
}

/**
 * gda_server_provider_perform_operation
 * @provider: a #GdaServerProvider object
 * @cnc: a #GdaConnection object which will be used to perform an action
 * @op: a #GdaServerOperation object
 * @error: a place to store an error, or %NULL
 *
 * Performs the operation described by @op.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_server_provider_perform_operation (GdaServerProvider *provider, GdaConnection *cnc, 
				       GdaServerOperation *op, GError **error)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
		g_return_val_if_fail (gda_connection_get_provider_obj (cnc) == provider, FALSE);
	}
	if (CLASS (provider)->perform_operation)
		return CLASS (provider)->perform_operation (provider, cnc, op, error);
	else {
		/* use the SQL from the provider to perform the action */
		gchar *sql;
		GdaCommand *cmd;
		GdaDataModel *model;

		sql = gda_server_provider_render_operation (provider, cnc, op, error);
		if (!sql)
			return FALSE;

		cmd = gda_command_new (sql, GDA_COMMAND_TYPE_SQL, GDA_COMMAND_OPTION_STOP_ON_ERRORS);
		g_free (sql);
		model = gda_connection_execute_command (cnc, cmd, NULL, error);		
		gda_command_free (cmd);
		if (model != GDA_CONNECTION_EXEC_FAILED) {
			if (model)
				g_object_unref (model);
			return TRUE;
		}
		else
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

	{
		GdaServerProviderInfo *info;

		info = gda_server_provider_get_info (provider, cnc);
		g_print ("==> %s (Provider %s on cnx %p)\n", cmd->text, info->provider_name, cnc);
	}

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
	if (cnc)
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
				GdaParameterList *params,
				GError **error)
{
	GdaDataModel *model;

	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	if (cnc)
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (CLASS (provider)->get_schema != NULL, NULL);

	model = CLASS (provider)->get_schema (provider, cnc, schema, params);
	if (model)
		/* test model validity */
		gda_server_provider_test_schema_model (model, schema, error);

	return model;
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
 * gda_server_provider_get_data_handler_gda
 * @provider: a server provider.
 * @cnc: a #GdaConnection object, or %NULL
 * @for_type: a #GType
 *
 * Find a #GdaDataHandler object to manipulate data of type @for_type.
 * 
 * Returns: a #GdaDataHandler, or %NULL if the provider does not support the requested @for_type data type 
 */
GdaDataHandler *
gda_server_provider_get_data_handler_gda (GdaServerProvider *provider,
					  GdaConnection *cnc,
					  GType for_type)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	if (cnc)
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	if (CLASS (provider)->get_data_handler)
		return CLASS (provider)->get_data_handler (provider, cnc, for_type, NULL);
	return NULL;
}

/**
 * gda_server_provider_get_data_handler_dbms
 * @provider: a server provider.
 * @cnc: a #GdaConnection object, or %NULL
 * @for_type: a DBMS type definition
 *
 * Find a #GdaDataHandler object to manipulate data of type @for_type.
 * 
 * Returns: a #GdaDataHandler, or %NULL if the provider does not know about the @for_type type
 */
GdaDataHandler *
gda_server_provider_get_data_handler_dbms (GdaServerProvider *provider,
					   GdaConnection *cnc,
					   const gchar *for_type)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	if (cnc)
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (for_type && *for_type, NULL);

	if (CLASS (provider)->get_data_handler)
		return CLASS (provider)->get_data_handler (provider, cnc, G_TYPE_INVALID, for_type);
	return NULL;
}

/**
 * gda_server_provider_string_to_value
 * @provider: a server provider.
 * @cnc: a #GdaConnection object.
 * @string: the SQL string to convert to a value
 * @prefered_type: a #GType
 *
 * Use @provider to create a new #GValue from a single string representation. 
 *
 * The @prefered_type can optionnaly ask @provider to return a #GValue of the requested type 
 * (but if such a value can't be created from @string, then %NULL is returned); 
 * pass G_TYPE_INVALID if any returned type is acceptable.
 *
 * The returned value is either a new #GValue or %NULL in the following cases:
 * - @string cannot be converted to @prefered_type type
 * - the provider does not handle @prefered_type
 * - the provider could not make a #GValue from @string
 *
 * Returns: a new #GValue, or %NULL
 */
GValue *
gda_server_provider_string_to_value (GdaServerProvider *provider,
				     GdaConnection *cnc,
				     const gchar *string, 
				     GType prefered_type, gchar **dbms_type)
{
	GValue *retval = NULL;
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	if (cnc)
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	if (CLASS (provider)->string_to_value)
		retval = CLASS (provider)->string_to_value (provider, cnc, string, prefered_type, dbms_type);
	
	if (!retval) {
		GdaDataHandler *dh;
		gint i;

		if (prefered_type != G_TYPE_INVALID) {
			dh = gda_server_provider_get_data_handler_gda (provider, cnc, prefered_type);
			if (dh) {
				retval = gda_data_handler_get_value_from_sql (dh, string, prefered_type);
				if (retval) {
					gchar *tmp;

					tmp = gda_data_handler_get_sql_from_value (dh, retval);
					if (strcmp (tmp, string)) {
						gda_value_free (retval);
						retval = NULL;
					}
					else {
						if (dbms_type)
							*dbms_type = gda_server_provider_get_default_dbms_type (provider, 
														cnc, prefered_type);
					}

					g_free (tmp);
				}
			}
		}
		else {
			/* test all the possible data types and see if we have a match */
			GType types[] = {G_TYPE_UCHAR,
					 GDA_TYPE_USHORT,
					 G_TYPE_UINT,
					 G_TYPE_UINT64,
					 
					 G_TYPE_CHAR,
					 GDA_TYPE_SHORT,
					 G_TYPE_INT,
					 G_TYPE_INT64,
					 
					 G_TYPE_FLOAT,
					 G_TYPE_DOUBLE,
					 GDA_TYPE_NUMERIC,
					 
					 G_TYPE_BOOLEAN,
					 GDA_TYPE_TIME,
					 G_TYPE_DATE,
					 GDA_TYPE_TIMESTAMP,
					 GDA_TYPE_GEOMETRIC_POINT,
					 G_TYPE_STRING,
					 GDA_TYPE_BINARY};

			for (i = 0; !retval && (i <= (sizeof(types)/sizeof (GType)) - 1); i++) {
				dh = gda_server_provider_get_data_handler_gda (provider, cnc, types [i]);
				if (dh) {
					retval = gda_data_handler_get_value_from_sql (dh, string, types [i]);
					if (retval) {
						gchar *tmp;
						
						tmp = gda_data_handler_get_sql_from_value (dh, retval);
						if (strcmp (tmp, string)) {
							gda_value_free (retval);
							retval = NULL;
						}
						else {
							if (dbms_type)
								*dbms_type = gda_server_provider_get_default_dbms_type (provider, 
															cnc, types[i]);
						}
						g_free (tmp);
					}
				}
			}
		}
	}

	return retval;
}

 
/**
 * gda_server_provider_value_to_sql_string
 * @provider: a server provider.
 * @cnc: a #GdaConnection object, or %NULL
 * @from: #GValue to convert from
 *
 * Produces a fully quoted and escaped string from a GValue
 *
 * Returns: escaped and quoted value or NULL if not supported.
 */
gchar *
gda_server_provider_value_to_sql_string (GdaServerProvider *provider,
					 GdaConnection *cnc,
					 GValue *from)
{
	GdaDataHandler *dh;

	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	if (cnc)
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (from != NULL, NULL);

	dh = gda_server_provider_get_data_handler_gda (provider, cnc, G_VALUE_TYPE (from));
	if (dh)
		return gda_data_handler_get_sql_from_value (dh, from);
	else
		return NULL;
}

/**
 * gda_server_provider_get_default_dbms_type
 * @provider: a server provider.
 * @cnc: a #GdaConnection object or %NULL
 * @gda_type: a #GType value type
 *
 * Get the name of the most common data type which has @gda_type GDA type
 *
 * Returns: the name of the DBMS type, or %NULL
 */
const gchar *
gda_server_provider_get_default_dbms_type (GdaServerProvider *provider,
					   GdaConnection *cnc,
					   GType gda_type)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	if (cnc)
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	
	if (CLASS (provider)->get_def_dbms_type)
		return (CLASS (provider)->get_def_dbms_type)(provider, cnc, gda_type);
	else
		return NULL;
}
