/*
 * Copyright (C) 2009 - 2013 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
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
#define G_LOG_DOMAIN "GDA-tree-mgr-schemas"

#include <glib/gi18n-lib.h>
#include <libgda/libgda.h>
#include <sql-parser/gda-sql-parser.h>
#include "gda-tree-mgr-schemas.h"
#include "gda-tree-node.h"

typedef struct {
	GdaConnection *cnc;
	GdaMetaStore  *mstore;
	GdaStatement  *stmt;
} GdaTreeMgrSchemasPrivate;
G_DEFINE_TYPE_WITH_PRIVATE (GdaTreeMgrSchemas, gda_tree_mgr_schemas, GDA_TYPE_TREE_MANAGER)

static void gda_tree_mgr_schemas_dispose    (GObject *object);
static void gda_tree_mgr_schemas_set_property (GObject *object,
					      guint param_id,
					      const GValue *value,
					      GParamSpec *pspec);
static void gda_tree_mgr_schemas_get_property (GObject *object,
					      guint param_id,
					      GValue *value,
					      GParamSpec *pspec);

/* virtual methods */
static GSList *gda_tree_mgr_schemas_update_children (GdaTreeManager *manager, GdaTreeNode *node, 
						     const GSList *children_nodes, gboolean *out_error, GError **error);

/* properties */
enum {
        PROP_0,
	PROP_CNC,
	PROP_META_STORE
};

/*
 * GdaTreeMgrSchemas class implementation
 * @klass:
 */
static void
gda_tree_mgr_schemas_class_init (GdaTreeMgrSchemasClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	/* virtual methods */
	((GdaTreeManagerClass*) klass)->update_children = gda_tree_mgr_schemas_update_children;

	/* Properties */
        object_class->set_property = gda_tree_mgr_schemas_set_property;
        object_class->get_property = gda_tree_mgr_schemas_get_property;

	/**
	 * GdaTreeMgrSchemas:connection:
	 *
	 * Defines the #GdaConnection to display information for. Necessary upon construction unless
	 * the #GdaTreeMgrSchema:meta-store property is specified instead.
	 */
	g_object_class_install_property (object_class, PROP_CNC,
                                         g_param_spec_object ("connection", NULL, "Connection to use",
                                                              GDA_TYPE_CONNECTION,
                                                              G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
	/**
	 * GdaTreeMgrSchemas:meta-store:
	 *
	 * Defines the #GdaMetaStore to extract information from. Necessary upon construction unless
	 * the #GdaTreeMgrSchema:connection property is specified instead. This property has
	 * priority over the GdaTreeMgrSchema:connection property.
	 *
	 * Since: 4.2.4
	 */
	g_object_class_install_property (object_class, PROP_META_STORE,
					 g_param_spec_object ("meta-store", NULL, "GdaMetaStore to use",
                                                              GDA_TYPE_META_STORE,
                                                              G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
					 
	object_class->dispose = gda_tree_mgr_schemas_dispose;
}

static void
gda_tree_mgr_schemas_init (GdaTreeMgrSchemas *mgr) {}

static void
gda_tree_mgr_schemas_dispose (GObject *object)
{
	GdaTreeMgrSchemas *mgr = (GdaTreeMgrSchemas *) object;

	g_return_if_fail (GDA_IS_TREE_MGR_SCHEMAS (mgr));
	GdaTreeMgrSchemasPrivate *priv = gda_tree_mgr_schemas_get_instance_private (mgr);

	if (priv->cnc) {
		g_object_unref (priv->cnc);
		priv->cnc = NULL;
	}
	if (priv->mstore) {
		g_object_unref (priv->mstore);
		priv->mstore = NULL;
	}
	if (priv->stmt) {
		g_object_unref (priv->stmt);
		priv->stmt = NULL;
	}


	/* chain to parent class */
	G_OBJECT_CLASS (gda_tree_mgr_schemas_parent_class)->dispose (object);
}

static void
gda_tree_mgr_schemas_set_property (GObject *object,
				   guint param_id,
				   const GValue *value,
				   GParamSpec *pspec)
{
	GdaTreeMgrSchemas *mgr;

	mgr = GDA_TREE_MGR_SCHEMAS (object);
	GdaTreeMgrSchemasPrivate *priv = gda_tree_mgr_schemas_get_instance_private (mgr);
	switch (param_id) {
		case PROP_CNC:
			priv->cnc = (GdaConnection*) g_value_get_object (value);
			if (priv->cnc)
				g_object_ref (priv->cnc);
			break;
		case PROP_META_STORE:
			priv->mstore = (GdaMetaStore*) g_value_get_object (value);
			if (priv->mstore)
				g_object_ref (priv->mstore);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
	}
}

static void
gda_tree_mgr_schemas_get_property (GObject *object,
				   guint param_id,
				   GValue *value,
				   GParamSpec *pspec)
{
	GdaTreeMgrSchemas *mgr;

	mgr = GDA_TREE_MGR_SCHEMAS (object);
	GdaTreeMgrSchemasPrivate *priv = gda_tree_mgr_schemas_get_instance_private (mgr);
	switch (param_id) {
		case PROP_CNC:
			g_value_set_object (value, priv->cnc);
			break;
		case PROP_META_STORE:
			g_value_set_object (value, priv->mstore);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
	}
}

/**
 * gda_tree_mgr_schemas_new:
 * @cnc: a #GdaConnection object
 *
 * Creates a new #GdaTreeManager object which will add one tree node for each database schema found
 * in @cnc.
 *
 * Returns: (transfer full): a new #GdaTreeManager object
 *
 * Since: 4.2
 */
GdaTreeManager*
gda_tree_mgr_schemas_new (GdaConnection *cnc)
{
	GdaTreeMgrSchemas *mgr;
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	mgr = (GdaTreeMgrSchemas*) g_object_new (GDA_TYPE_TREE_MGR_SCHEMAS,
						 "connection", cnc, NULL);
	return (GdaTreeManager*) mgr;
}

static GSList *
gda_tree_mgr_schemas_update_children (GdaTreeManager *manager, GdaTreeNode *node,
				      G_GNUC_UNUSED const GSList *children_nodes, gboolean *out_error,
				      GError **error)
{
	GdaTreeMgrSchemas *mgr = GDA_TREE_MGR_SCHEMAS (manager);
	GdaMetaStore *store;
	GdaDataModel *model;
	GSList *list = NULL;
	GdaConnection *scnc;
	GdaTreeMgrSchemasPrivate *priv = gda_tree_mgr_schemas_get_instance_private (mgr);

	if (!priv->cnc && !priv->mstore) {
		g_set_error (error, GDA_TREE_MANAGER_ERROR, GDA_TREE_MANAGER_UNKNOWN_ERROR,
			     "%s", _("No connection and no GdaMetaStore specified"));
		if (out_error)
			*out_error = TRUE;
		return NULL;
	}
	else if (priv->mstore)
		store = priv->mstore;
	else
		store = gda_connection_get_meta_store (priv->cnc);

	scnc = gda_meta_store_get_internal_connection (store);

	if (!priv->stmt) {
		GdaSqlParser *parser;
		GdaStatement *stmt;

		parser = gda_connection_create_parser (scnc);
		if (! parser)
			parser = gda_sql_parser_new ();

		stmt = gda_sql_parser_parse_string (parser,
						    "SELECT schema_name FROM _schemata "
						    "WHERE schema_internal = FALSE OR schema_name LIKE 'info%' "
						    "ORDER BY schema_name DESC", NULL, error);
		g_object_unref (parser);
		if (!stmt) {
			if (out_error)
				*out_error = TRUE;
			return NULL;
		}
		priv->stmt = stmt;
	}

	model = gda_connection_statement_execute_select (scnc, priv->stmt, NULL, error);
	if (!model) {
		if (out_error)
			*out_error = TRUE;
		return NULL;
	}

	GdaDataModelIter *iter;
	iter = gda_data_model_create_iter (model);
	for (; iter && gda_data_model_iter_move_next (iter);) {
		GdaTreeNode* snode;
		const GValue *cvalue;

		cvalue = gda_data_model_iter_get_value_at (iter, 0);
		if (!cvalue) {
			if (list) {
				g_slist_free_full (list, (GDestroyNotify) g_object_unref);
			}
			if (out_error)
				*out_error = TRUE;
			g_set_error (error, GDA_TREE_MANAGER_ERROR, GDA_TREE_MANAGER_UNKNOWN_ERROR,
				     "%s", _("Unable to get schema name"));
			return NULL;
		}

		snode = gda_tree_manager_create_node (manager, node, g_value_get_string (cvalue));
		gda_tree_node_set_node_attribute (snode, "schema", cvalue, NULL);
		list = g_slist_prepend (list, snode);
	}
	if (iter)
		g_object_unref (iter);
	g_object_unref (model);

	return list;
}
