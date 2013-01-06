/*
 * Copyright (C) 2001 - 2004 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2002 - 2003 Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 * Copyright (C) 2002 Holger Thon <holger.thon@gnome-db.org>
 * Copyright (C) 2003 Laurent Sansonetti <laurent@datarescue.be>
 * Copyright (C) 2004 - 2005 Alan Knowles <alank@src.gnome.org>
 * Copyright (C) 2004 Dani Baeyens <daniel.baeyens@hispalinux.es>
 * Copyright (C) 2004 Julio M. Merino Vidal <jmmv@menta.net>
 * Copyright (C) 2005 - 2006 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2005 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2005 Álvaro Peña <alvaropg@telefonica.net>
 * Copyright (C) 2007 Armin Burgmeier <armin@openismus.com>
 * Copyright (C) 2008 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2008 PrzemysÅ‚aw Grzegorczyk <pgrzegorczyk@gmail.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2010 Jonh Wendell <jwendell@gnome.org>
 * Copyright (C) 2012 Daniel Espinosa <despinosa@src.gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <glib.h>
#include <libgda/gda-server-provider.h>
#include <libgda/gda-server-provider-extra.h>
#include <libgda/gda-server-provider-private.h>
#include <libgda/gda-data-handler.h>
#include <libgda/gda-util.h>
#include <libgda/gda-set.h>
#include <sql-parser/gda-sql-parser.h>
#include <string.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-lockable.h>

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
	klass->handle_async = NULL;

	klass->create_connection = NULL;
	memset (&(klass->meta_funcs), 0, sizeof (GdaServerProviderMeta));
	klass->xa_funcs = NULL;

	klass->limiting_thread = GDA_SERVER_PROVIDER_UNDEFINED_LIMITING_THREAD;

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
			  G_GNUC_UNUSED GdaServerProviderClass *klass)
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
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static const GTypeInfo info = {
			sizeof (GdaServerProviderClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_server_provider_class_init,
			NULL,
			NULL,
			sizeof (GdaServerProvider),
			0,
			(GInstanceInitFunc) gda_server_provider_init,
			0
		};
		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (G_TYPE_OBJECT, "GdaServerProvider", &info, G_TYPE_FLAG_ABSTRACT);
		g_static_mutex_unlock (&registering);
	}

	return type;
}

static void
gda_server_provider_set_property (GObject *object,
				  guint param_id,
				  G_GNUC_UNUSED const GValue *value,
				  GParamSpec *pspec) {
        GdaServerProvider *prov;

        prov = GDA_SERVER_PROVIDER (object);
        if (prov->priv) {
                switch (param_id) {
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
	}
}

static void
gda_server_provider_get_property (GObject *object,
				  guint param_id,
				  G_GNUC_UNUSED GValue *value,
				  GParamSpec *pspec) {
        GdaServerProvider *prov;

        prov = GDA_SERVER_PROVIDER (object);
        if (prov->priv) {
                switch (param_id) {
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
	}
}

/**
 * gda_server_provider_get_version:
 * @provider: a #GdaServerProvider object.
 *
 * Get the version of the provider.
 *
 * Returns: (transfer none): a string containing the version identification.
 */
const gchar *
gda_server_provider_get_version (GdaServerProvider *provider)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	g_return_val_if_fail (CLASS (provider)->get_version, NULL);

	return CLASS (provider)->get_version (provider);
}

/**
 * gda_server_provider_get_name:
 * @provider: a #GdaServerProvider object.
 *
 * Get the name (identifier) of the provider
 *
 * Returns: (transfer none): a string containing the provider's name
 */
const gchar *
gda_server_provider_get_name (GdaServerProvider *provider)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	g_return_val_if_fail (CLASS (provider)->get_name, NULL);

	return CLASS (provider)->get_name (provider);
}

/**
 * gda_server_provider_get_server_version:
 * @provider: a #GdaServerProvider object.
 * @cnc: a #GdaConnection object
 *
 * Get the version of the database to which the connection is opened.
 * 
 * Returns: (transfer none): a (read only) string, or %NULL if an error occurred
 */
const gchar *
gda_server_provider_get_server_version (GdaServerProvider *provider, GdaConnection *cnc)
{
	const gchar *retval;
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (CLASS (provider)->get_server_version != NULL, NULL);

	gda_lockable_lock ((GdaLockable*) cnc);
	retval = CLASS (provider)->get_server_version (provider, cnc);
	gda_lockable_unlock ((GdaLockable*) cnc);

	return retval;
}

/**
 * gda_server_provider_supports_operation:
 * @provider: a #GdaServerProvider object
 * @cnc: (allow-none): a #GdaConnection object which would be used to perform an action, or %NULL
 * @type: the type of operation requested
 * @options: (allow-none): a list of named parameters, or %NULL
 *
 * Tells if @provider supports the @type of operation on the @cnc connection, using the
 * (optional) @options parameters.
 *
 * Returns: %TRUE if the operation is supported
 */
gboolean
gda_server_provider_supports_operation (GdaServerProvider *provider, GdaConnection *cnc, 
					GdaServerOperationType type, GdaSet *options)
{
	gboolean retval = FALSE;
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (!cnc || GDA_IS_CONNECTION (cnc), FALSE);

	if (cnc)
		gda_lockable_lock ((GdaLockable*) cnc);
	if (CLASS (provider)->supports_operation)
		retval = CLASS (provider)->supports_operation (provider, cnc, type, options);
	if (cnc)
		gda_lockable_unlock ((GdaLockable*) cnc);
	return retval;
}

typedef struct {
	gchar                      *path;
	GdaServerOperationNodeType  node_type;
	GType                       data_type;
} OpReq;

static OpReq op_req_CREATE_DB [] = {
	{"/DB_DEF_P",               GDA_SERVER_OPERATION_NODE_PARAMLIST, 0},
	{"/DB_DEF_P/DB_NAME",       GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{NULL, 0, 0}
};

static OpReq op_req_DROP_DB [] = {
	{"/DB_DESC_P",               GDA_SERVER_OPERATION_NODE_PARAMLIST, 0},
	{"/DB_DESC_P/DB_NAME",       GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{NULL, 0, 0}
};

static OpReq op_req_CREATE_TABLE [] = {
	{"/TABLE_DEF_P",               GDA_SERVER_OPERATION_NODE_PARAMLIST, 0},
	{"/TABLE_DEF_P/TABLE_NAME",    GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{"/FIELDS_A",                  GDA_SERVER_OPERATION_NODE_DATA_MODEL, 0},
	{"/FIELDS_A/@COLUMN_NAME",     GDA_SERVER_OPERATION_NODE_DATA_MODEL_COLUMN, G_TYPE_STRING},
	{"/FIELDS_A/@COLUMN_TYPE",     GDA_SERVER_OPERATION_NODE_DATA_MODEL_COLUMN, G_TYPE_STRING},
	{NULL, 0, 0}
};

static OpReq op_req_DROP_TABLE [] = {
	{"/TABLE_DESC_P/TABLE_NAME",   GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{NULL, 0, 0}
};

static OpReq op_req_RENAME_TABLE [] = {
	{"/TABLE_DESC_P",                  GDA_SERVER_OPERATION_NODE_PARAMLIST, 0},
	{"/TABLE_DESC_P/TABLE_NAME",       GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{"/TABLE_DESC_P/TABLE_NEW_NAME",   GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{NULL, 0, 0}
};

static OpReq op_req_COMMENT_TABLE [] = {
	{"/TABLE_DESC_P",               GDA_SERVER_OPERATION_NODE_PARAMLIST, 0},
	{"/TABLE_DESC_P/TABLE_NAME",    GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{"/TABLE_DESC_P/TABLE_COMMENT", GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{NULL, 0, 0}
};

static OpReq op_req_ADD_COLUMN [] = {
	{"/COLUMN_DEF_P",               GDA_SERVER_OPERATION_NODE_PARAMLIST, 0},
	{"/COLUMN_DEF_P/TABLE_NAME",    GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{"/COLUMN_DEF_P/COLUMN_NAME",   GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{"/COLUMN_DEF_P/COLUMN_TYPE",   GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{NULL, 0, 0}
};

static OpReq op_req_DROP_COLUMN [] = {
	{"/COLUMN_DESC_P",               GDA_SERVER_OPERATION_NODE_PARAMLIST, 0},
	{"/COLUMN_DESC_P/TABLE_NAME",    GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{"/COLUMN_DESC_P/COLUMN_NAME",   GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{NULL, 0, 0}
};

static OpReq op_req_COMMENT_COLUMN [] = {
	{"/COLUMN_DESC_P",                GDA_SERVER_OPERATION_NODE_PARAMLIST, 0},
	{"/COLUMN_DESC_P/TABLE_NAME",     GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{"/COLUMN_DESC_P/COLUMN_NAME",    GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{"/COLUMN_DESC_P/COLUMN_COMMENT", GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{NULL, 0, 0}
};

static OpReq op_req_CREATE_INDEX [] = {
	{"/INDEX_DEF_P/INDEX_NAME",       GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{"/INDEX_DEF_P/INDEX_ON_TABLE",   GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{"/INDEX_FIELDS_S",               GDA_SERVER_OPERATION_NODE_SEQUENCE, 0},
	{NULL, 0, 0}
};

static OpReq op_req_DROP_INDEX [] = {
	{"/INDEX_DESC_P/INDEX_NAME",   GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{NULL, 0, 0}
};

static OpReq op_req_CREATE_VIEW [] = {
	{"/VIEW_DEF_P",               GDA_SERVER_OPERATION_NODE_PARAMLIST, 0},
	{"/VIEW_DEF_P/VIEW_NAME",     GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{"/VIEW_DEF_P/VIEW_DEF",      GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{NULL, 0, 0}
};

static OpReq op_req_DROP_VIEW [] = {
	{"/VIEW_DESC_P/VIEW_NAME",   GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{NULL, 0, 0}
};

static OpReq op_req_CREATE_USER [] = {
	{"/USER_DEF_P",               GDA_SERVER_OPERATION_NODE_PARAMLIST, 0},
	{"/USER_DEF_P/USER_NAME",     GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{NULL, 0, 0}
};


/**
 * gda_server_provider_create_operation:
 * @provider: a #GdaServerProvider object
 * @cnc: (allow-none): a #GdaConnection object which will be used to perform an action, or %NULL
 * @type: the type of operation requested
 * @options: (allow-none): a list of parameters or %NULL
 * @error: (allow-none): a place to store an error, or %NULL
 *
 * Creates a new #GdaServerOperation object which can be modified in order to perform the @type type of
 * action. The @options can contain:
 * <itemizedlist>
 *  <listitem>named values which ID is a path in the resulting GdaServerOperation object, to initialize some value</listitem>
 *  <listitem>named values which may change the contents of the GdaServerOperation, see <link linkend="gda-server-op-information-std">this section</link> for more information</listitem>
 * </itemizedlist>
 *
 * Returns: (transfer full) (allow-none): a new #GdaServerOperation object, or %NULL in the provider does not support the @type type of operation or if an error occurred
 */
GdaServerOperation *
gda_server_provider_create_operation (GdaServerProvider *provider, GdaConnection *cnc, 
				      GdaServerOperationType type, 
				      GdaSet *options, GError **error)
{
	static GStaticMutex init_mutex = G_STATIC_MUTEX_INIT;
	static OpReq **op_req_table = NULL;

	g_static_mutex_lock (&init_mutex);
	if (! op_req_table) {
		op_req_table = g_new0 (OpReq *, GDA_SERVER_OPERATION_LAST);

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

		op_req_table [GDA_SERVER_OPERATION_COMMENT_TABLE] = op_req_COMMENT_TABLE;
		op_req_table [GDA_SERVER_OPERATION_COMMENT_COLUMN] = op_req_COMMENT_COLUMN;

		op_req_table [GDA_SERVER_OPERATION_CREATE_USER] = op_req_CREATE_USER;
	}
	g_static_mutex_unlock (&init_mutex);

	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	g_return_val_if_fail (!cnc || GDA_IS_CONNECTION (cnc), FALSE);

	if (CLASS (provider)->create_operation) {
		GdaServerOperation *op;

		if (cnc)
			gda_lockable_lock ((GdaLockable*) cnc);
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
					node = xmlNewTextChild (top, NULL, BAD_CAST "op_data", BAD_CAST str);
					g_free (str);
					xmlSetProp (node, BAD_CAST "path", BAD_CAST id);
				}

				if (! gda_server_operation_load_data_from_xml (op, top, error))
					g_warning ("Incorrect options");
				xmlFreeNode (top);
			}
		}
		if (cnc)
			gda_lockable_unlock ((GdaLockable*) cnc);
		return op;
	}
	else
		return NULL;
}

/**
 * gda_server_provider_render_operation:
 * @provider: a #GdaServerProvider object
 * @cnc: (allow-none): a #GdaConnection object which will be used to render the action, or %NULL
 * @op: a #GdaServerOperation object
 * @error: (allow-none): a place to store an error, or %NULL
 *
 * Creates an SQL statement (possibly using some specific extensions of the DBMS) corresponding to the
 * @op operation. Note that the returned string may actually contain more than one SQL statement.
 *
 * This function's purpose is mainly informative to get the actual SQL code which would be executed to perform
 * the operation; to actually perform the operation, use gda_server_provider_perform_operation().
 *
 * Returns: (transfer full) (allow-none): a new string, or %NULL if an error occurred or operation cannot be rendered as SQL.
 */
gchar *
gda_server_provider_render_operation (GdaServerProvider *provider, GdaConnection *cnc, 
				      GdaServerOperation *op, GError **error)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	g_return_val_if_fail (!cnc || GDA_IS_CONNECTION (cnc), NULL);

	if (CLASS (provider)->render_operation) {
		gchar *retval;
		if (cnc)
			gda_lockable_lock ((GdaLockable*) cnc);
		retval = CLASS (provider)->render_operation (provider, cnc, op, error);
		if (cnc)
			gda_lockable_unlock ((GdaLockable*) cnc);
		return retval;
	}
	else
		return NULL;
}

/**
 * gda_server_provider_perform_operation:
 * @provider: a #GdaServerProvider object
 * @cnc: (allow-none): a #GdaConnection object which will be used to perform the action, or %NULL
 * @op: a #GdaServerOperation object
 * @error: (allow-none): a place to store an error, or %NULL
 *
 * Performs the operation described by @op. Note that @op is not destroyed by this method
 * and can be reused.
 *
 * Returns: %TRUE if no error occurred
 */
gboolean
gda_server_provider_perform_operation (GdaServerProvider *provider, GdaConnection *cnc, 
				       GdaServerOperation *op, GError **error)
{
	gboolean retval;
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (!cnc || GDA_IS_CONNECTION (cnc), FALSE);

#ifdef GDA_DEBUG_NO
	{
		g_print ("Perform GdaServerOperation:\n");
		xmlNodePtr node;
		node = gda_server_operation_save_data_to_xml (op, NULL);
		xmlDocPtr doc;
		doc = xmlNewDoc ("1.0");
		xmlDocSetRootElement (doc, node);
		xmlDocDump (stdout, doc);
		xmlFreeDoc (doc);
	}
#endif
	if (cnc)
		gda_lockable_lock ((GdaLockable*) cnc);
	if (CLASS (provider)->perform_operation)
		retval = CLASS (provider)->perform_operation (provider, cnc, op, NULL, NULL, NULL, error);
	else 
		retval = gda_server_provider_perform_operation_default (provider, cnc, op, error);
	if (cnc)
		gda_lockable_unlock ((GdaLockable*) cnc);
	return retval;
}

/**
 * gda_server_provider_supports_feature:
 * @provider: a #GdaServerProvider object
 * @cnc: (allow-none): a #GdaConnection object, or %NULL
 * @feature: #GdaConnectionFeature feature to test
 *
 * Tests if a feature is supported
 *
 * Returns: %TRUE if @feature is supported
 */
gboolean
gda_server_provider_supports_feature (GdaServerProvider *provider, GdaConnection *cnc,
				      GdaConnectionFeature feature)
{
	gboolean retval = FALSE;

	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (!cnc || GDA_IS_CONNECTION (cnc), FALSE);

	if (cnc)
		gda_lockable_lock ((GdaLockable*) cnc);
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
	if (cnc)
		gda_lockable_unlock ((GdaLockable*) cnc);
	return retval;
}

/**
 * gda_server_provider_get_data_handler_g_type:
 * @provider: a server provider.
 * @cnc: (allow-none): a #GdaConnection object, or %NULL
 * @for_type: a #GType
 *
 * Find a #GdaDataHandler object to manipulate data of type @for_type. The returned object must not be modified.
 * 
 * Returns: (transfer none): a #GdaDataHandler, or %NULL if the provider does not support the requested @for_type data type 
 */
GdaDataHandler *
gda_server_provider_get_data_handler_g_type (GdaServerProvider *provider, GdaConnection *cnc, GType for_type)
{
	GdaDataHandler *retval;
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	g_return_val_if_fail (!cnc || GDA_IS_CONNECTION (cnc), NULL);

	if (cnc)
		gda_lockable_lock ((GdaLockable*) cnc);
	if (CLASS (provider)->get_data_handler)
		retval = CLASS (provider)->get_data_handler (provider, cnc, for_type, NULL);
	else
		retval = gda_server_provider_handler_use_default (provider, for_type);
	if (cnc)
		gda_lockable_unlock ((GdaLockable*) cnc);
	return retval;
}

/**
 * gda_server_provider_get_data_handler_dbms:
 * @provider: a server provider.
 * @cnc: (allow-none): a #GdaConnection object, or %NULL
 * @for_type: a DBMS type definition
 *
 * Find a #GdaDataHandler object to manipulate data of type @for_type.
 *
 * Note: this function is currently very poorly implemented by database providers.
 * 
 * Returns: (transfer none): a #GdaDataHandler, or %NULL if the provider does not know about the @for_type type
 */
GdaDataHandler *
gda_server_provider_get_data_handler_dbms (GdaServerProvider *provider, GdaConnection *cnc, const gchar *for_type)
{
	GdaDataHandler *retval;
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	g_return_val_if_fail (for_type && *for_type, NULL);
	g_return_val_if_fail (!cnc || GDA_IS_CONNECTION (cnc), NULL);

	if (cnc)
		gda_lockable_lock ((GdaLockable*) cnc);
	if (CLASS (provider)->get_data_handler)
		retval = CLASS (provider)->get_data_handler (provider, cnc, G_TYPE_INVALID, for_type);
	if (cnc)
		gda_lockable_unlock ((GdaLockable*) cnc);
	return retval;
}

/**
 * gda_server_provider_get_default_dbms_type:
 * @provider: a server provider.
 * @cnc: (allow-none):  a #GdaConnection object or %NULL
 * @type: a #GType value type
 *
 * Get the name of the most common data type which has @type type.
 *
 * The returned value may be %NULL either if the provider does not implement that method, or if
 * there is no DBMS data type which could contain data of the @g_type type (for example %NULL may be
 * returned if a DBMS has integers only up to 4 bytes and a #G_TYPE_INT64 is requested).
 *
 * Returns: (transfer none) (allow-none): the name of the DBMS type, or %NULL
 */
const gchar *
gda_server_provider_get_default_dbms_type (GdaServerProvider *provider, GdaConnection *cnc, GType type)
{
	const gchar *retval = NULL;
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	g_return_val_if_fail (!cnc || GDA_IS_CONNECTION (cnc), NULL);

	if (CLASS (provider)->get_def_dbms_type) {
		if (cnc)
			gda_lockable_lock ((GdaLockable*) cnc);
		retval = (CLASS (provider)->get_def_dbms_type) (provider, cnc, type);
		if (cnc)
			gda_lockable_unlock ((GdaLockable*) cnc);
	}

	return retval;
}


/**
 * gda_server_provider_string_to_value:
 * @provider: a server provider.
 * @cnc: (allow-none): a #GdaConnection object, or %NULL
 * @string: the SQL string to convert to a value
 * @preferred_type: a #GType, or #G_TYPE_INVALID
 * @dbms_type: (allow-none): place to get the actual database type used if the conversion succeeded, or %NULL
 *
 * Use @provider to create a new #GValue from a single string representation. 
 *
 * The @preferred_type can optionally ask @provider to return a #GValue of the requested type 
 * (but if such a value can't be created from @string, then %NULL is returned); 
 * pass #G_TYPE_INVALID if any returned type is acceptable.
 *
 * The returned value is either a new #GValue or %NULL in the following cases:
 * - @string cannot be converted to @preferred_type type
 * - the provider does not handle @preferred_type
 * - the provider could not make a #GValue from @string
 *
 * If @dbms_type is not %NULL, then if will contain a constant string representing
 * the database type used for the conversion if the conversion was successfull, or %NULL
 * otherwise.
 *
 * Returns: (transfer full): a new #GValue, or %NULL
 */
GValue *
gda_server_provider_string_to_value (GdaServerProvider *provider,  GdaConnection *cnc, const gchar *string, 
				     GType preferred_type, gchar **dbms_type)
{
	GValue *retval = NULL;
	GdaDataHandler *dh;
	gsize i;

	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	g_return_val_if_fail (!cnc || GDA_IS_CONNECTION (cnc), NULL);

	if (dbms_type)
		*dbms_type = NULL;

	if (cnc)
		gda_lockable_lock ((GdaLockable*) cnc);
	if (preferred_type != G_TYPE_INVALID) {
		dh = gda_server_provider_get_data_handler_g_type (provider, cnc, preferred_type);
		if (dh) {
			retval = gda_data_handler_get_value_from_sql (dh, string, preferred_type);
			if (retval) {
				gchar *tmp;
				
				tmp = gda_data_handler_get_sql_from_value (dh, retval);
				if (!tmp || strcmp (tmp, string)) {
					gda_value_free (retval);
					retval = NULL;
				}
				else {
					if (dbms_type)
						*dbms_type = (gchar *) gda_server_provider_get_default_dbms_type (provider, 
														  cnc, preferred_type);
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
			dh = gda_server_provider_get_data_handler_g_type (provider, cnc, types [i]);
			if (dh) {
				retval = gda_data_handler_get_value_from_sql (dh, string, types [i]);
				if (retval) {
					gchar *tmp;
					
					tmp = gda_data_handler_get_sql_from_value (dh, retval);
					if (!tmp || strcmp (tmp, string)) {
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
	if (cnc)
		gda_lockable_unlock ((GdaLockable*) cnc);

	return retval;
}

 
/**
 * gda_server_provider_value_to_sql_string:
 * @provider: a server provider.
 * @cnc: (allow-none): a #GdaConnection object, or %NULL
 * @from: #GValue to convert from
 *
 * Produces a fully quoted and escaped string from a GValue
 *
 * Returns: (transfer full): escaped and quoted value or NULL if not supported.
 */
gchar *
gda_server_provider_value_to_sql_string (GdaServerProvider *provider,
					 GdaConnection *cnc,
					 GValue *from)
{
	gchar *retval = NULL;
	GdaDataHandler *dh;

	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	g_return_val_if_fail (!cnc || GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (from != NULL, NULL);

	if (cnc)
		gda_lockable_lock ((GdaLockable*) cnc);
	dh = gda_server_provider_get_data_handler_g_type (provider, cnc, G_VALUE_TYPE (from));
	if (dh)
		retval = gda_data_handler_get_sql_from_value (dh, from);
	if (cnc)
		gda_lockable_unlock ((GdaLockable*) cnc);
	return retval;
}

/**
 * gda_server_provider_escape_string:
 * @provider: a server provider.
 * @cnc: (allow-none): a #GdaConnection object, or %NULL
 * @str: a string to escape
 *
 * Escapes @str for use within an SQL command (to avoid SQL injection attacks). Note that the returned value still needs
 * to be enclosed in single quotes before being used in an SQL statement.
 *
 * Returns: (transfer full): a new string suitable to use in SQL statements
 */
gchar *
gda_server_provider_escape_string (GdaServerProvider *provider, GdaConnection *cnc, const gchar *str)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	g_return_val_if_fail (!cnc || GDA_IS_CONNECTION (cnc), NULL);

	if (CLASS (provider)->escape_string) {
		gchar *retval;
		if (! CLASS (provider)->unescape_string)
			g_warning (_("GdaServerProvider object implements the %s virtual method but "
				     "does not implement the %s one, please report this bug to "
				     "http://bugzilla.gnome.org/ for the \"libgda\" product."), 
				   "escape_string()", "unescape_string()");
		if (cnc)
			gda_lockable_lock ((GdaLockable*) cnc);
		retval = (CLASS (provider)->escape_string) (provider, cnc, str);
		if (cnc)
			gda_lockable_unlock ((GdaLockable*) cnc);
		return retval;
	}
	else 
		return gda_default_escape_string (str);
}

/**
 * gda_server_provider_unescape_string:
 * @provider: a server provider.
 * @cnc: (allow-none): a #GdaConnection object, or %NULL
 * @str: a string to escape
 *
 * Unescapes @str for use within an SQL command. This is the exact opposite of gda_server_provider_escape_string().
 *
 * Returns: (transfer full): a new string
 */
gchar *
gda_server_provider_unescape_string (GdaServerProvider *provider, GdaConnection *cnc, const gchar *str)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	g_return_val_if_fail (!cnc || GDA_IS_CONNECTION (cnc), NULL);

	if (CLASS (provider)->unescape_string) {
		gchar *retval;
		if (! CLASS (provider)->escape_string)
			g_warning (_("GdaServerProvider object implements the %s virtual method but "
				     "does not implement the %s one, please report this bug to "
				     "http://bugzilla.gnome.org/ for the \"libgda\" product."),
				   "unescape_string()", "escape_string()");
		if (cnc)
			gda_lockable_lock ((GdaLockable*) cnc);
		retval = (CLASS (provider)->unescape_string)(provider, cnc, str);
		if (cnc)
			gda_lockable_unlock ((GdaLockable*) cnc);
		return retval;
	}
	else
		return gda_default_unescape_string (str);
}


/**
 * gda_server_provider_create_parser:
 * @provider: a #GdaServerProvider provider object
 * @cnc: (allow-none): a #GdaConnection, or %NULL
 *
 * Creates a new #GdaSqlParser object which is adapted to @provider (and possibly depending on
 * @cnc for the actual database version).
 *
 * If @prov does not have its own parser, then %NULL is returned, and a general SQL parser can be obtained
 * using gda_sql_parser_new().
 *
 * Returns: (transfer full): a new #GdaSqlParser object, or %NULL.
 */
GdaSqlParser *
gda_server_provider_create_parser (GdaServerProvider *provider, GdaConnection *cnc)
{
	GdaSqlParser *parser = NULL;

	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	g_return_val_if_fail (!cnc || GDA_IS_CONNECTION (cnc), NULL);

	if (CLASS (provider)->create_parser)
		parser = (CLASS (provider)->create_parser) (provider, cnc);
	return parser;
}

