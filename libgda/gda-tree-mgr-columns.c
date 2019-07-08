/*
 * Copyright (C) 2009 - 2013 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2018 Daniel Espinosa <esodan@gmail.com>
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
#define G_LOG_DOMAIN "GDA-tree-mgr-columns"

#include <glib/gi18n-lib.h>
#include <libgda/libgda.h>
#include <sql-parser/gda-sql-parser.h>
#include "gda-tree-mgr-columns.h"
#include "gda-tree-node.h"

typedef struct {
	GdaConnection *cnc;
	GdaMetaStore  *mstore;
	gchar         *schema; /* imposed upon construction */
	gchar         *table_name; /* imposed upon construction */

	GdaStatement  *stmt;
	GdaSet        *params;
} GdaTreeMgrColumnsPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GdaTreeMgrColumns, gda_tree_mgr_columns, GDA_TYPE_TREE_MANAGER)

static void gda_tree_mgr_columns_dispose    (GObject *object);
static void gda_tree_mgr_columns_set_property (GObject *object,
					       guint param_id,
					       const GValue *value,
					       GParamSpec *pspec);
static void gda_tree_mgr_columns_get_property (GObject *object,
					       guint param_id,
					       GValue *value,
					       GParamSpec *pspec);

/* virtual methods */
static GSList *gda_tree_mgr_columns_update_children (GdaTreeManager *manager, GdaTreeNode *node, 
						     const GSList *children_nodes, gboolean *out_error, GError **error);

/* properties */
enum {
        PROP_0,
	PROP_CNC,
	PROP_SCHEMA,
	PROP_TABLE,
	PROP_META_STORE
};

/*
 * GdaTreeMgrColumns class implementation
 * @klass:
 */
static void
gda_tree_mgr_columns_class_init (GdaTreeMgrColumnsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	/* virtual methods */
	((GdaTreeManagerClass*) klass)->update_children = gda_tree_mgr_columns_update_children;

	/* Properties */
        object_class->set_property = gda_tree_mgr_columns_set_property;
        object_class->get_property = gda_tree_mgr_columns_get_property;

	/**
	 * GdaTreeMgrColumns:connection:
	 *
	 * Defines the #GdaConnection to display information for. Necessary upon construction unless
	 * the #GdaTreeMgrColumns:meta-store property is specified instead.
	 */
	g_object_class_install_property (object_class, PROP_CNC,
                                         g_param_spec_object ("connection", NULL, "Connection to use",
                                                              GDA_TYPE_CONNECTION,
                                                              G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

	/**
	 * GdaTreeMgrColumns:meta-store:
	 *
	 * Defines the #GdaMetaStore to extract information from. Necessary upon construction unless
	 * the #GdaTreeMgrColumns:connection property is specified instead. This property has
	 * priority over the GdaTreeMgrColumns:connection property.
	 *
	 * Since: 4.2.4
	 */
	g_object_class_install_property (object_class, PROP_META_STORE,
                                         g_param_spec_object ("meta-store", NULL, "GdaMetaStore to use",
                                                              GDA_TYPE_META_STORE,
                                                              G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

	/**
	 * GdaTreeMgrColumns:schema:
	 *
	 * If no set, then the table name will be fetched from the parent node using the "schema" attribute
	 */
        g_object_class_install_property (object_class, PROP_SCHEMA,
                                         g_param_spec_string ("schema", NULL,
                                                              "Database schema'name in which the table is",
                                                              NULL, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

	/**
	 * GdaTreeMgrColumns:table-name:
	 *
	 * If no set, then the table name will be fetched from the parent node using the "table_name" attribute
	 */
        g_object_class_install_property (object_class, PROP_TABLE,
                                         g_param_spec_string ("table-name", NULL,
                                                              "Database table's name",
                                                              NULL, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

	object_class->dispose = gda_tree_mgr_columns_dispose;
}

static void
gda_tree_mgr_columns_init (GdaTreeMgrColumns *mgr) {}

static void
gda_tree_mgr_columns_dispose (GObject *object)
{
	GdaTreeMgrColumns *mgr = (GdaTreeMgrColumns *) object;

	g_return_if_fail (GDA_IS_TREE_MGR_COLUMNS (mgr));
	GdaTreeMgrColumnsPrivate *priv = gda_tree_mgr_columns_get_instance_private (mgr);

	if (priv) {
		if (priv->cnc) {
			g_object_unref (priv->cnc);
			priv->cnc = NULL;
		}
		if (priv->mstore) {
			g_object_unref (priv->mstore);
			priv->mstore = NULL;
		}
		g_free (priv->schema);
		priv->schema = NULL;
		g_free (priv->table_name);
		priv->table_name = NULL;

		if (priv->stmt) {
			g_object_unref (priv->stmt);
			priv->stmt = NULL;
		}
		if (priv->params) {
			g_object_unref (priv->params);
			priv->params = NULL;
		}
	}

	/* chain to parent class */
	G_OBJECT_CLASS (gda_tree_mgr_columns_parent_class)->dispose (object);
}

static void
gda_tree_mgr_columns_set_property (GObject *object,
				   guint param_id,
				   const GValue *value,
				   GParamSpec *pspec)
{
	GdaTreeMgrColumns *mgr;

	mgr = GDA_TREE_MGR_COLUMNS (object);
	GdaTreeMgrColumnsPrivate *priv = gda_tree_mgr_columns_get_instance_private (mgr);
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
		case PROP_SCHEMA:
			priv->schema = g_value_dup_string (value);
			break;
		case PROP_TABLE:
			priv->table_name = g_value_dup_string (value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
	}
}

static void
gda_tree_mgr_columns_get_property (GObject *object,
				   guint param_id,
				   GValue *value,
				   GParamSpec *pspec)
{
	GdaTreeMgrColumns *mgr;

	mgr = GDA_TREE_MGR_COLUMNS (object);
	GdaTreeMgrColumnsPrivate *priv = gda_tree_mgr_columns_get_instance_private (mgr);
	switch (param_id) {
		case PROP_CNC:
			g_value_set_object (value, priv->cnc);
			break;
		case PROP_META_STORE:
			g_value_set_object (value, priv->mstore);
			break;
		case PROP_SCHEMA:
			g_value_set_string (value, priv->schema);
			break;
		case PROP_TABLE:
			g_value_set_string (value, priv->table_name);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
	}
}

/**
 * gda_tree_mgr_columns_new:
 * @cnc: a #GdaConnection object
 * @schema: a schema name
 * @table_name: the name of the table
 *
 * Creates a new #GdaTreeManager object which will add one tree node for each
 * column in the table named @table_name in the @schema schema.
 *
 * Returns: (transfer full): a new #GdaTreeManager object 
 *
 * Since: 4.2
 */
GdaTreeManager*
gda_tree_mgr_columns_new (GdaConnection *cnc, const gchar *schema, const gchar *table_name)
{
	GdaTreeMgrColumns *mgr;
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	mgr = (GdaTreeMgrColumns*) g_object_new (GDA_TYPE_TREE_MGR_COLUMNS,
						 "connection", cnc, 
						 "schema", schema, 
						 "table-name", table_name, NULL);
	return (GdaTreeManager*) mgr;
}

static GSList *
gda_tree_mgr_columns_update_children (GdaTreeManager *manager, GdaTreeNode *node, G_GNUC_UNUSED const GSList *children_nodes,
				      gboolean *out_error, GError **error)
{
	GdaTreeMgrColumns *mgr = GDA_TREE_MGR_COLUMNS (manager);
	GdaMetaStore *store;
	GdaDataModel *model;
	GSList *list = NULL;
	GdaConnection *scnc;
	GdaTreeMgrColumnsPrivate *priv = gda_tree_mgr_columns_get_instance_private (mgr);

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

	/* create statements if necessary */
	if (!priv->stmt) {
		GdaSqlParser *parser;
		GdaStatement *stmt;

		parser = gda_connection_create_parser (scnc);
		if (! parser)
			parser = gda_sql_parser_new ();

		stmt = gda_sql_parser_parse_string (parser,
						    "SELECT column_name FROM _columns WHERE "
						    "table_schema= ##schema::string AND "
						    "table_name= ##table_name::string "
						    "ORDER BY ordinal_position", NULL, error);
		g_object_unref (parser);
		if (!stmt) {
			if (out_error)
				*out_error = TRUE;
			return NULL;
		}

		if (!gda_statement_get_parameters (stmt, &(priv->params), error)) {
			if (out_error)
				*out_error = TRUE;
			g_object_unref (stmt);
			return NULL;
		}
		priv->stmt = stmt;
	}


	gboolean schema_specified = FALSE;
	gboolean table_specified = FALSE;
	if (priv->schema) {
		schema_specified = TRUE;
		g_assert (gda_set_set_holder_value (priv->params, NULL, "schema",
						    priv->schema));
	}
	if (priv->table_name) {
		table_specified = TRUE;
		g_assert (gda_set_set_holder_value (priv->params, NULL, "table_name",
						    priv->table_name));
	}
	if (!schema_specified && node) {
		/* looking for a schema in @node's attributes */
		const GValue *cvalue;
		cvalue = gda_tree_node_fetch_attribute (node, "schema");
		if (cvalue) {
			GdaHolder *h = gda_set_get_holder (priv->params, "schema");
			if (!gda_holder_set_value (h, cvalue, error)) {
				if (out_error)
					*out_error = TRUE;
				return NULL;
			}
			schema_specified = TRUE;
		}
	}
	if (!table_specified && node) {
		/* looking for a table in @node's attributes */
		const GValue *cvalue;
		cvalue = gda_tree_node_fetch_attribute (node, "table_name");
		if (cvalue) {
			GdaHolder *h = gda_set_get_holder (priv->params, "table_name");
			if (!gda_holder_set_value (h, cvalue, error)) {
				if (out_error)
					*out_error = TRUE;
				return NULL;
			}
			table_specified = TRUE;
		}
	}

	if (!schema_specified) {
		g_set_error (error, GDA_TREE_MANAGER_ERROR, GDA_TREE_MANAGER_UNKNOWN_ERROR,
			     "%s", _("No schema specified"));
		if (out_error)
			*out_error = TRUE;
		return NULL;
	}

	if (!table_specified) {
		g_set_error (error, GDA_TREE_MANAGER_ERROR, GDA_TREE_MANAGER_UNKNOWN_ERROR,
			     "%s", _("No table specified"));
		if (out_error)
			*out_error = TRUE;
		return NULL;
	}

	model = gda_connection_statement_execute_select (scnc, priv->stmt, priv->params, error);

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
				     "%s", _("Unable to get column name"));
			return NULL;
		}

		snode = gda_tree_manager_create_node (manager, node, g_value_get_string (cvalue));
		gda_tree_node_set_node_attribute (snode, "column_name", cvalue, NULL);
		list = g_slist_prepend (list, snode);
	}
	if (iter)
		g_object_unref (iter);
	g_object_unref (model);

	return list;
}
