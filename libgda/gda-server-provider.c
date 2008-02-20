/* GDA library
 * Copyright (C) 1998 - 2008 The GNOME Foundation.
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
#include <libgda/gda-util.h>
#include <libgda/gda-set.h>
#include <sql-parser/gda-sql-parser.h>
#include <string.h>
#include <glib/gi18n-lib.h>

#define PARENT_TYPE G_TYPE_OBJECT
#define CLASS(provider) (GDA_SERVER_PROVIDER_CLASS (G_OBJECT_GET_CLASS (provider)))

static void gda_server_provider_class_init (GdaServerProviderClass *klass);
static void gda_server_provider_init       (GdaServerProvider *provider,
					    GdaServerProviderClass *klass);
static void gda_server_provider_finalize   (GObject *object);

static void gda_server_provider_set_property (GObject *object,
					      guint param_id,
					      const GValue *value,
					      GParamSpec *pspec);
static void gda_server_provider_get_property (GObject *object,
					      guint param_id,
					      GValue *value,
					      GParamSpec *pspec);

static GObjectClass *parent_class = NULL;

/* properties */
enum {
        PROP_0,
};

/* module error */
GQuark gda_server_provider_error_quark (void)
{
        static GQuark quark;
        if (!quark)
                quark = g_quark_from_static_string ("gda_server_provider_error");
        return quark;
}

/*
 * GdaServerProvider class implementation
 */

static void
gda_server_provider_class_init (GdaServerProviderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_server_provider_finalize;

	klass->get_name = NULL;
	klass->get_version = NULL;
	klass->get_server_version = NULL;
	klass->supports_feature = NULL;

	klass->get_data_handler = NULL;
	klass->get_def_dbms_type = NULL;
	klass->escape_string = NULL;
	klass->unescape_string = NULL;

	klass->open_connection = NULL;
	klass->close_connection = NULL;
	klass->get_database = NULL;

	klass->supports_operation = NULL;
	klass->create_operation = NULL;
	klass->render_operation = NULL;
	klass->perform_operation = NULL;

	klass->begin_transaction = NULL;
	klass->commit_transaction = NULL;
	klass->rollback_transaction = NULL;
	klass->add_savepoint = NULL;
	klass->rollback_savepoint = NULL;
	klass->delete_savepoint = NULL;

	klass->create_parser = NULL;
	klass->statement_to_sql = NULL;
	klass->statement_prepare = NULL;
	klass->statement_execute = NULL;
	
	klass->is_busy = NULL;
	klass->cancel = NULL;
	klass->create_connection = NULL;
	memset (&(klass->meta_funcs), 0, sizeof (GdaServerProviderMeta));

	 /* Properties */
        object_class->set_property = gda_server_provider_set_property;
        object_class->get_property = gda_server_provider_get_property;
}

static guint
gda_server_provider_handler_info_hash_func  (GdaServerProviderHandlerInfo *key)
{
        guint hash;

        hash = g_int_hash (&(key->g_type));
        if (key->dbms_type)
                hash += g_str_hash (key->dbms_type);
        hash += GPOINTER_TO_UINT (key->cnc);

        return hash;
}

static gboolean
gda_server_provider_handler_info_equal_func (GdaServerProviderHandlerInfo *a, GdaServerProviderHandlerInfo *b)
{
        if ((a->g_type == b->g_type) &&
            (a->cnc == b->cnc) &&
            ((!a->dbms_type && !b->dbms_type) || !strcmp (a->dbms_type, b->dbms_type)))
                return TRUE;
        else
                return FALSE;
}

static void
gda_server_provider_handler_info_free (GdaServerProviderHandlerInfo *info)
{
        g_free (info->dbms_type);
        g_free (info);
}

static void
gda_server_provider_init (GdaServerProvider *provider,
			  GdaServerProviderClass *klass)
{
	g_return_if_fail (GDA_IS_SERVER_PROVIDER (provider));

	provider->priv = g_new0 (GdaServerProviderPrivate, 1);
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
		g_hash_table_destroy (provider->priv->data_handlers);
		if (provider->priv->parser)
			g_object_unref (provider->priv->parser);

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

	if (G_UNLIKELY (type == 0)) {
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

static void
gda_server_provider_set_property (GObject *object,
				  guint param_id,
				  const GValue *value,
				  GParamSpec *pspec) {
        GdaServerProvider *prov;

        prov = GDA_SERVER_PROVIDER (object);
        if (prov->priv) {
                switch (param_id) {
		default:
			g_assert_not_reached ();
		}
	}
}

static void
gda_server_provider_get_property (GObject *object,
				  guint param_id,
				  GValue *value,
				  GParamSpec *pspec) {
        GdaServerProvider *prov;

        prov = GDA_SERVER_PROVIDER (object);
        if (prov->priv) {
                switch (param_id) {
		default:
			g_assert_not_reached ();
		}
	}
}

/**
 * gda_server_provider_get_version
 * @provider: a #GdaServerProvider object.
 *
 * Get the version of the provider.
 *
 * Returns: a string containing the version identification.
 */
const gchar *
gda_server_provider_get_version (GdaServerProvider *provider)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	g_return_val_if_fail (CLASS (provider)->get_version, NULL);

	return CLASS (provider)->get_version (provider);
}

/**
 * gda_server_provider_get_info
 * @provider: a #GdaServerProvider object.
 *
 * Get the name (identifier) of the provider
 *
 * Returns: a string containing the provider's name
 */
const gchar *
gda_server_provider_get_name (GdaServerProvider *provider)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	g_return_val_if_fail (CLASS (provider)->get_name, NULL);

	return CLASS (provider)->get_name (provider);
}

/**
 * gda_server_provider_create_connection
 * @client: a #GdaClient object to which the returned connection will be attached, or %NULL
 * @provider: a #GdaServerProvider object.
 * @dsn: a DSN string.
 * @params:
 * @auth_string: authentification string
 * @options: options for the connection (see #GdaConnectionOptions).
 *
 * Requests that the @provider creates a new #GdaConnection connection object.
 * See gda_client_open_connection() for more information
 *
 * Returns: a newly-allocated #GdaConnection object, or NULL
 * if it fails.
 */
GdaConnection *
gda_server_provider_create_connection (GdaClient *client, GdaServerProvider *provider, 
				       const gchar *dsn, const gchar *auth_string,
				       GdaConnectionOptions options)
{
	GdaConnection *cnc;
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);

	if (CLASS (provider)->create_connection) {
		cnc = CLASS (provider)->create_connection (provider);
		if (cnc) {
			g_object_set (G_OBJECT (cnc), "client", client, "provider_obj", provider, NULL);
			if (dsn && *dsn)
				g_object_set (G_OBJECT (cnc), "dsn", dsn, NULL);
			g_object_set (G_OBJECT (cnc), "auth_string", auth_string, "options", options, NULL);
		}
	}
	else
		cnc = g_object_new (GDA_TYPE_CONNECTION, "client", client, "provider_obj", provider, 
				    "dsn", dsn, "auth_string", auth_string, 
				    "options", options, NULL);

	return cnc;
}

/**
 * gda_server_provider_create_connection_from_string
 * @client: a #GdaClient object to which the returned connection will be attached, or %NULL
 * @provider: a #GdaServerProvider object.
 * @cnc_string: a connection string.
 * @params:
 * @auth_string: authentification string
 * @options: options for the connection (see #GdaConnectionOptions).
 *
 * Requests that the @provider creates a new #GdaConnection connection object.
 * See gda_client_open_connection() for more information
 *
 * Returns: a newly-allocated #GdaConnection object, or NULL
 * if it fails.
 */
GdaConnection *
gda_server_provider_create_connection_from_string (GdaClient *client, GdaServerProvider *provider, 
						   const gchar *cnc_string, const gchar *auth_string,
						   GdaConnectionOptions options)
{
	GdaConnection *cnc;
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);

	if (CLASS (provider)->create_connection) {
		cnc = CLASS (provider)->create_connection (provider);
		if (cnc) {
			g_object_set (G_OBJECT (cnc), "client", client, "provider_obj", provider, NULL);
			if (cnc_string && *cnc_string)
				g_object_set (G_OBJECT (cnc), "cnc_string", cnc_string, NULL);
			g_object_set (G_OBJECT (cnc), "auth_string", auth_string, "options", options, NULL);
		}
	}
	else 
		cnc = (GdaConnection *) g_object_new (GDA_TYPE_CONNECTION, "client", client, "provider_obj", provider,
						      "cnc-string", cnc_string, "auth_string", auth_string,
						      "options", options, NULL);

	return cnc;
}

/**
 * gda_server_provider_open_connection
 * @provider: a #GdaServerProvider object.
 * @cnc: a #GdaConnection object.
 * @params:
 * @auth: authentification information
 *
 * Tries to open the @cnc connection using methods implemented by @provider. In case of failure,
 * or as a general notice, some #GdaConnectionEvent events (or errors) may be added to the connection
 * during that method, see gda_connection_get_events().
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_server_provider_open_connection (GdaServerProvider *provider, GdaConnection *cnc,
				     GdaQuarkList *params, GdaQuarkList *auth)
{
	gboolean retcode;

	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (CLASS (provider)->open_connection != NULL, FALSE);

	retcode = CLASS (provider)->open_connection (provider, cnc, params, auth, NULL, NULL, NULL);

	return retcode;
}



/**
 * gda_server_provider_close_connection
 * @provider: a #GdaServerProvider object.
 * @cnc: a #GdaConnection object.
 *
 * Close the connection.
 *
 * Returns: TRUE if no error occurred
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

	return retcode;
}

/**
 * gda_server_provider_get_server_version
 * @provider: a #GdaServerProvider object.
 * @cnc: a #GdaConnection object.
 *
 * Get the version of the database to which the connection is opened.
 * 
 * Returns: a (read only) string, or %NULL if an error occurred
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
 * Get the database name of the @cnc.
 *
 * Returns: the name of the current database, or %NULL if an error occurred or if the provider
 * does not implement that method.
 */
const gchar *
gda_server_provider_get_database (GdaServerProvider *provider,
				  GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	if (CLASS (provider)->get_database)
		return CLASS (provider)->get_database (provider, cnc);
	else
		return NULL;
}

/**
 * gda_server_provider_supports_operation
 * @provider: a #GdaServerProvider object
 * @cnc: a #GdaConnection object which would be used to perform an action, or %NULL
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
					GdaServerOperationType type, GdaSet *options)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
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

static OpReq op_req_CREATE_DB [] = {
	{"/DB_DEF_P",               GDA_SERVER_OPERATION_NODE_PARAMLIST, 0},
	{"/DB_DEF_P/DB_NAME",       GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{NULL}
};

static OpReq op_req_DROP_DB [] = {
	{"/DB_DESC_P",               GDA_SERVER_OPERATION_NODE_PARAMLIST, 0},
	{"/DB_DESC_P/DB_NAME",       GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{NULL}
};

static OpReq op_req_CREATE_TABLE [] = {
	{"/TABLE_DEF_P",               GDA_SERVER_OPERATION_NODE_PARAMLIST, 0},
	{"/TABLE_DEF_P/TABLE_NAME",    GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{"/FIELDS_A",                  GDA_SERVER_OPERATION_NODE_DATA_MODEL, 0},
	{"/FIELDS_A/@COLUMN_NAME",     GDA_SERVER_OPERATION_NODE_DATA_MODEL_COLUMN, G_TYPE_STRING},
	{"/FIELDS_A/@COLUMN_TYPE",     GDA_SERVER_OPERATION_NODE_DATA_MODEL_COLUMN, G_TYPE_STRING},
	{NULL}
};

static OpReq op_req_DROP_TABLE [] = {
	{"/TABLE_DESC_P/TABLE_NAME",   GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{NULL}
};

static OpReq op_req_RENAME_TABLE [] = {
	{"/TABLE_DESC_P",                  GDA_SERVER_OPERATION_NODE_PARAMLIST, 0},
	{"/TABLE_DESC_P/TABLE_NAME",       GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{"/TABLE_DESC_P/TABLE_NEW_NAME",   GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{NULL}
};

static OpReq op_req_ADD_COLUMN [] = {
	{"/COLUMN_DEF_P",               GDA_SERVER_OPERATION_NODE_PARAMLIST, 0},
	{"/COLUMN_DEF_P/TABLE_NAME",    GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{"/COLUMN_DEF_P/COLUMN_NAME",   GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{"/COLUMN_DEF_P/COLUMN_TYPE",   GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{NULL}
};

static OpReq op_req_DROP_COLUMN [] = {
	{"/COLUMN_DESC_P",               GDA_SERVER_OPERATION_NODE_PARAMLIST, 0},
	{"/COLUMN_DESC_P/TABLE_NAME",    GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{"/COLUMN_DESC_P/COLUMN_NAME",   GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{NULL}
};

static OpReq op_req_CREATE_INDEX [] = {
	{"/INDEX_DEF_P/INDEX_NAME",       GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{"/INDEX_DEF_P/INDEX_ON_TABLE",   GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{"/INDEX_FIELDS_S",               GDA_SERVER_OPERATION_NODE_SEQUENCE, 0},
	{NULL}
};

static OpReq op_req_DROP_INDEX [] = {
	{"/INDEX_DESC_P/INDEX_NAME",   GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{NULL}
};

static OpReq op_req_CREATE_VIEW [] = {
	{"/VIEW_DEF_P",               GDA_SERVER_OPERATION_NODE_PARAMLIST, 0},
	{"/VIEW_DEF_P/VIEW_NAME",     GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{"/VIEW_DEF_P/VIEW_DEF",      GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{NULL}
};

static OpReq op_req_DROP_VIEW [] = {
	{"/VIEW_DESC_P/VIEW_NAME",   GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{NULL}
};

/**
 * gda_server_provider_create_operation
 * @provider: a #GdaServerProvider object
 * @cnc: a #GdaConnection object which will be used to perform an action, or %NULL
 * @type: the type of operation requested
 * @options: an optional list of parameters
 * @error: a place to store an error, or %NULL
 *
 * Creates a new #GdaServerOperation object which can be modified in order to perform the @type type of
 * action. The @options can contain:
 * <itemizedlist>
 *  <listitem>parameters which ID is a path in the resulting GdaServerOperation object, to initialize some value</listitem>
 *  <listitem>parameters which may change the contents of the GdaServerOperation, see <link linkend="gda-server-op-information">this section</link> for more information</listitem>
 * </itemizedlist>
 *
 * Returns: a new #GdaServerOperation object, or %NULL in the provider does not support the @type type
 * of operation or if an error occurred
 */
GdaServerOperation *
gda_server_provider_create_operation (GdaServerProvider *provider, GdaConnection *cnc, 
				      GdaServerOperationType type, 
				      GdaSet *options, GError **error)
{
	OpReq **op_req_table = NULL;

	if (! op_req_table) {
		op_req_table = g_new0 (OpReq *, GDA_SERVER_OPERATION_NB);

		op_req_table [GDA_SERVER_OPERATION_CREATE_DB] = op_req_CREATE_DB;
		op_req_table [GDA_SERVER_OPERATION_DROP_DB] = op_req_DROP_DB;
		
		op_req_table [GDA_SERVER_OPERATION_CREATE_TABLE] = op_req_CREATE_TABLE;
		op_req_table [GDA_SERVER_OPERATION_DROP_TABLE] = op_req_DROP_TABLE;
		op_req_table [GDA_SERVER_OPERATION_RENAME_TABLE] = op_req_RENAME_TABLE;

		op_req_table [GDA_SERVER_OPERATION_ADD_COLUMN] = op_req_ADD_COLUMN;
		op_req_table [GDA_SERVER_OPERATION_DROP_COLUMN] = op_req_DROP_COLUMN;

		op_req_table [GDA_SERVER_OPERATION_CREATE_INDEX] = op_req_CREATE_INDEX;
		op_req_table [GDA_SERVER_OPERATION_DROP_INDEX] = op_req_DROP_INDEX;

		op_req_table [GDA_SERVER_OPERATION_CREATE_VIEW] = op_req_CREATE_VIEW;
		op_req_table [GDA_SERVER_OPERATION_DROP_VIEW] = op_req_DROP_VIEW;
	}

	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);

	if (CLASS (provider)->create_operation) {
		GdaServerOperation *op;

		op = CLASS (provider)->create_operation (provider, cnc, type, options, error);
		if (op) {
			/* test op's conformance */
			OpReq *opreq = op_req_table [type];
			while (opreq && opreq->path) {
				GdaServerOperationNodeType node_type;
				node_type = gda_server_operation_get_node_type (op, opreq->path, NULL);
				if (node_type == GDA_SERVER_OPERATION_NODE_UNKNOWN) 
					g_warning (_("Provider %s created a GdaServerOperation without node for '%s'"),
						   gda_server_provider_get_name (provider), opreq->path);
				else 
					if (node_type != opreq->node_type)
						g_warning (_("Provider %s created a GdaServerOperation with wrong node type for '%s'"),
							   gda_server_provider_get_name (provider), opreq->path);
				opreq += 1;
			}

			if (options) {
				/* pre-init parameters depending on the @options argument */
				GSList *list;
				xmlNodePtr top, node;

				top =  xmlNewNode (NULL, BAD_CAST "serv_op_data");
				for (list = options->holders; list; list = list->next) {
					const gchar *id;
					gchar *str = NULL;
					const GValue *value;

					id = gda_holder_get_id (GDA_HOLDER (list->data));
					value = gda_holder_get_value (GDA_HOLDER (list->data));
					if (value)
						str = gda_value_stringify (value);
					node = xmlNewChild (top, NULL, BAD_CAST "op_data", BAD_CAST str);
					g_free (str);
					xmlSetProp (node, BAD_CAST "path", BAD_CAST id);
				}

				if (! gda_server_operation_load_data_from_xml (op, top, error))
					g_warning ("Incorrect options");
				xmlFreeNode (top);
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
 * @cnc: a #GdaConnection object which will be used to render the action, or %NULL
 * @op: a #GdaServerOperation object
 * @error: a place to store an error, or %NULL
 *
 * Creates an SQL statement (possibly using some specific extensions of the DBMS) corresponding to the
 * @op operation. Note that the returned string may actually contain more than one SQL statement.
 *
 * This function's purpose is mainly informative to get the actual SQL code which would be executed to perform
 * the operation; to actually perform the operation, use gda_server_provider_perform_operation().
 *
 * Returns: a new string, or %NULL if an error occurred or operation cannot be rendered as SQL.
 */
gchar *
gda_server_provider_render_operation (GdaServerProvider *provider, GdaConnection *cnc, 
				      GdaServerOperation *op, GError **error)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	if (CLASS (provider)->render_operation)
		return CLASS (provider)->render_operation (provider, cnc, op, error);
	else
		return NULL;
}

/**
 * gda_server_provider_perform_operation
 * @provider: a #GdaServerProvider object
 * @cnc: a #GdaConnection object which will be used to perform the action, or %NULL
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
	if (CLASS (provider)->perform_operation)
		return CLASS (provider)->perform_operation (provider, cnc, op, NULL, NULL, NULL, error);
	else 
		return gda_server_provider_perform_operation_default (provider, cnc, op, error);
}

/**
 * gda_server_provider_begin_transaction
 * @provider: a #GdaServerProvider object
 * @cnc: a #GdaConnection object
 * @name: the name of the transation to start
 * @level:
 * @error: a place to store errors, or %NULL
 *
 * Starts a new transaction on @cnc.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_server_provider_begin_transaction (GdaServerProvider *provider, GdaConnection *cnc,
				       const gchar *name, GdaTransactionIsolation level,
				       GError **error)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);

	if (CLASS (provider)->begin_transaction)
		return CLASS (provider)->begin_transaction (provider, cnc, name, level, error);
	else
		return FALSE;
}

/**
 * gda_server_provider_commit_transaction
 * @provider: a #GdaServerProvider object
 * @cnc: a #GdaConnection object
 * @name: the name of the transation to commit
 * @error: a place to store errors, or %NULL
 *
 * Commit the current transaction
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_server_provider_commit_transaction (GdaServerProvider *provider, GdaConnection *cnc,
					const gchar *name, GError **error)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);

	if (CLASS (provider)->commit_transaction)
		return CLASS (provider)->commit_transaction (provider, cnc, name, error);
	else
		return FALSE;
}

/**
 * gda_server_provider_rollback_transaction
 * @provider: a #GdaServerProvider object
 * @cnc: a #GdaConnection object
 * @name: the name of the transation to commit
 * @error: a place to store errors, or %NULL
 *
 * Rolls back the current transaction
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_server_provider_rollback_transaction (GdaServerProvider *provider, GdaConnection *cnc,
					  const gchar *name, GError **error)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);

	if (CLASS (provider)->rollback_transaction)
		return CLASS (provider)->rollback_transaction (provider, cnc, name, error);
	else
		return FALSE;
}

/**
 * gda_server_provider_add_savepoint
 * @provider: a #GdaServerProvider object
 * @cnc: a #GdaConnection object
 * @name: name of the savepoint to add
 * @error: a place to store errors or %NULL
 *
 * Add a savepoint within the current transaction
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_server_provider_add_savepoint (GdaServerProvider *provider, GdaConnection *cnc, 
				   const gchar *name, GError **error)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	
	if (CLASS (provider)->add_savepoint)
		return CLASS (provider)->add_savepoint (provider, cnc, name, error);
	else
		return FALSE;
}

/**
 * gda_server_provider_rollback_savepoint
 * @provider: a #GdaServerProvider object
 * @cnc: a #GdaConnection object
 * @name: name of the savepoint to rollback to
 * @error: a place to store errors or %NULL
 *
 * Rolls back to a savepoint within the current transaction
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_server_provider_rollback_savepoint (GdaServerProvider *provider, GdaConnection *cnc, 
					const gchar *name, GError **error)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);

	if (CLASS (provider)->rollback_savepoint)
		return CLASS (provider)->rollback_savepoint (provider, cnc, name, error);
	else
		return FALSE;
}

/**
 * gda_server_provider_delete_savepoint
 * @provider: a #GdaServerProvider object
 * @cnc: a #GdaConnection object
 * @name: name of the savepoint to delete
 * @error: a place to store errors or %NULL
 *
 * Delete a savepoint within the current transaction
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_server_provider_delete_savepoint (GdaServerProvider *provider, GdaConnection *cnc, 
				      const gchar *name, GError **error)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);

	if (CLASS (provider)->delete_savepoint)
		return CLASS (provider)->delete_savepoint (provider, cnc, name, error);
	else
		return FALSE;
}

/**
 * gda_server_provider_supports_feature
 * @provider: a #GdaServerProvider object
 * @cnc: a #GdaConnection object, or %NULL
 * @feature: #GdaConnectionFeature feature to test
 *
 * Tests if a feature is supported
 *
 * Returns: TRUE if @feature is supported
 */
gboolean
gda_server_provider_supports_feature (GdaServerProvider *provider, GdaConnection *cnc,
				      GdaConnectionFeature feature)
{
	gboolean retval = FALSE;

	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);

	if (CLASS (provider)->supports_feature)
		retval = CLASS (provider)->supports_feature (provider, cnc, feature);

	if (retval) {
		switch (feature) {
		case GDA_CONNECTION_FEATURE_TRANSACTIONS:
			if (!CLASS (provider)->begin_transaction ||
			    !CLASS (provider)->commit_transaction ||
			    !CLASS (provider)->rollback_transaction)
				retval = FALSE;
			break;
		case GDA_CONNECTION_FEATURE_SAVEPOINTS:
			if (!CLASS (provider)->add_savepoint ||
			    !CLASS (provider)->rollback_savepoint)
				retval = FALSE;
			break;
		case GDA_CONNECTION_FEATURE_SAVEPOINTS_REMOVE:
			if (!CLASS (provider)->delete_savepoint)
				retval = FALSE;
			break;
		default:
			break;
		}
	}

	return retval;
}

/**
 * gda_server_provider_get_data_handler_gtype
 * @provider: a server provider.
 * @cnc: a #GdaConnection object, or %NULL
 * @for_type: a #GType
 *
 * Find a #GdaDataHandler object to manipulate data of type @for_type. The returned object must not be modified.
 * 
 * Returns: a #GdaDataHandler, or %NULL if the provider does not support the requested @for_type data type 
 */
GdaDataHandler *
gda_server_provider_get_data_handler_gtype (GdaServerProvider *provider, GdaConnection *cnc, GType for_type)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);

	if (CLASS (provider)->get_data_handler)
		return CLASS (provider)->get_data_handler (provider, cnc, for_type, NULL);
	else
		return gda_server_provider_get_data_handler_default (provider, cnc, for_type, NULL);
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
gda_server_provider_get_data_handler_dbms (GdaServerProvider *provider, GdaConnection *cnc, const gchar *for_type)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	g_return_val_if_fail (for_type && *for_type, NULL);

	if (CLASS (provider)->get_data_handler)
		return CLASS (provider)->get_data_handler (provider, cnc, G_TYPE_INVALID, for_type);
	else
		return gda_server_provider_get_data_handler_default (provider, cnc, G_TYPE_INVALID, for_type);
}

/**
 * gda_server_provider_get_default_dbms_type
 * @provider: a server provider.
 * @cnc: a #GdaConnection object or %NULL
 * @type: a #GType value type
 *
 * Get the name of the most common data type which has @type type.
 *
 * The returned value may be %NULL either if the provider does not implement that method, or if
 * there is no DBMS data type which could contain data of the @g_type type (for example %NULL may be
 * returned if a DBMS has integers only up to 4 bytes and a G_TYPE_INT64 is requested).
 *
 * Returns: the name of the DBMS type, or %NULL
 */
const gchar *
gda_server_provider_get_default_dbms_type (GdaServerProvider *provider, GdaConnection *cnc, GType type)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	
	if (CLASS (provider)->get_def_dbms_type)
		return (CLASS (provider)->get_def_dbms_type)(provider, cnc, type);
	else
		return NULL;
}


/**
 * gda_server_provider_string_to_value
 * @provider: a server provider.
 * @cnc: a #GdaConnection object.
 * @string: the SQL string to convert to a value
 * @prefered_type: a #GType, or G_TYPE_INVALID
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
gda_server_provider_string_to_value (GdaServerProvider *provider,  GdaConnection *cnc, const gchar *string, 
				     GType prefered_type, gchar **dbms_type)
{
	GValue *retval = NULL;
	GdaDataHandler *dh;
	gint i;

	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);

	if (prefered_type != G_TYPE_INVALID) {
		dh = gda_server_provider_get_data_handler_gtype (provider, cnc, prefered_type);
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
						*dbms_type = (gchar *) gda_server_provider_get_default_dbms_type (provider, 
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
			dh = gda_server_provider_get_data_handler_gtype (provider, cnc, types [i]);
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
							*dbms_type = (gchar *) gda_server_provider_get_default_dbms_type (provider, 
															  cnc, types[i]);
					}
					g_free (tmp);
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

	dh = gda_server_provider_get_data_handler_gtype (provider, cnc, G_VALUE_TYPE (from));
	if (dh)
		return gda_data_handler_get_sql_from_value (dh, from);
	else
		return NULL;
}

/**
 * gda_server_provider_escape_string
 * @provider: a server provider.
 * @cnc: a #GdaConnection object, or %NULL
 * @str: a string to escape
 *
 * Escapes @str for use within an SQL command (to avoid SQL injection attacks). Note that the returned value still needs
 * to be enclosed in single quotes before being used in an SQL statement.
 *
 * Returns: a new string suitable to use in SQL statements
 */
gchar *
gda_server_provider_escape_string (GdaServerProvider *provider, GdaConnection *cnc, const gchar *str)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	if (cnc)
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	if (CLASS (provider)->escape_string) {
		if (! CLASS (provider)->unescape_string)
			g_warning (_("GdaServerProvider object implements the escape_string() virtual method but "
				     "does not implement the unescape_string() one, please report this bug."));
		return (CLASS (provider)->escape_string)(provider, cnc, str);
	}
	else 
		return gda_default_escape_string (str);
}

/**
 * gda_server_provider_unescape_string
 * @provider: a server provider.
 * @cnc: a #GdaConnection object, or %NULL
 * @str: a string to escape
 *
 * Unescapes @str for use within an SQL command. This is the exact opposite of gda_server_provider_escape_string().
 *
 * Returns: a new string
 */
gchar *
gda_server_provider_unescape_string (GdaServerProvider *provider, GdaConnection *cnc, const gchar *str)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	if (cnc)
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	if (CLASS (provider)->unescape_string) {
		if (! CLASS (provider)->escape_string)
			g_warning (_("GdaServerProvider object implements the unescape_string() virtual method but "
				     "does not implement the escape_string() one, please report this bug."));
		return (CLASS (provider)->unescape_string)(provider, cnc, str);
	}
	else
		return gda_default_unescape_string (str);
}


/**
 * gda_server_provider_create_parser
 * @provider: a #GdaServerProvider provider object
 * @cnc: a #GdaConnection, or %NULL
 *
 * Creates a new #GdaSqlParser object which is adapted to @provider (and possibly depending on
 * @cnc for the actual database version).
 *
 * If @prov does not have its own parser, then %NULL is returned, and a general SQL parser can be obtained
 * using gda_sql_parser_new().
 *
 * Returns: a new #GdaSqlParser object, or %NULL.
 */
GdaSqlParser *
gda_server_provider_create_parser (GdaServerProvider *provider, GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);

	if (CLASS (provider)->create_parser)
		return (CLASS (provider)->create_parser)(provider, cnc);
	else
		return NULL;
}

