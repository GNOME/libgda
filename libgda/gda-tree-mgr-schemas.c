/*
 * Copyright (C) 2009 - 2011 Vivien Malerba <malerba@gnome-db.org>
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

#include <glib/gi18n-lib.h>
#include <libgda/libgda.h>
#include <sql-parser/gda-sql-parser.h>
#include "gda-tree-mgr-schemas.h"
#include "gda-tree-node.h"

struct _GdaTreeMgrSchemasPriv {
	GdaConnection *cnc;
	GdaMetaStore  *mstore;
	GdaStatement  *stmt;
};

static void gda_tree_mgr_schemas_class_init (GdaTreeMgrSchemasClass *klass);
static void gda_tree_mgr_schemas_init       (GdaTreeMgrSchemas *tmgr1, GdaTreeMgrSchemasClass *klass);
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

static GObjectClass *parent_class = NULL;

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

	parent_class = g_type_class_peek_parent (klass);

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
gda_tree_mgr_schemas_init (GdaTreeMgrSchemas *mgr, G_GNUC_UNUSED GdaTreeMgrSchemasClass *klass)
{
	g_return_if_fail (GDA_IS_TREE_MGR_SCHEMAS (mgr));
	mgr->priv = g_new0 (GdaTreeMgrSchemasPriv, 1);
}

static void
gda_tree_mgr_schemas_dispose (GObject *object)
{
	GdaTreeMgrSchemas *mgr = (GdaTreeMgrSchemas *) object;

	g_return_if_fail (GDA_IS_TREE_MGR_SCHEMAS (mgr));

	if (mgr->priv) {
		if (mgr->priv->cnc)
			g_object_unref (mgr->priv->cnc);
		if (mgr->priv->mstore)
			g_object_unref (mgr->priv->mstore);
		if (mgr->priv->stmt)
			g_object_unref (mgr->priv->stmt);

		g_free (mgr->priv);
		mgr->priv = NULL;
	}

	/* chain to parent class */
	parent_class->dispose (object);
}

/**
 * gda_tree_mgr_schemas_get_type:
 *
 * Returns: the GType
 *
 * Since: 4.2
 */
GType
gda_tree_mgr_schemas_get_type (void)
{
        static GType type = 0;

        if (G_UNLIKELY (type == 0)) {
                static GStaticMutex registering = G_STATIC_MUTEX_INIT;
                static const GTypeInfo info = {
                        sizeof (GdaTreeMgrSchemasClass),
                        (GBaseInitFunc) NULL,
                        (GBaseFinalizeFunc) NULL,
                        (GClassInitFunc) gda_tree_mgr_schemas_class_init,
                        NULL,
                        NULL,
                        sizeof (GdaTreeMgrSchemas),
                        0,
                        (GInstanceInitFunc) gda_tree_mgr_schemas_init,
			0
                };

                g_static_mutex_lock (&registering);
                if (type == 0)
                        type = g_type_register_static (GDA_TYPE_TREE_MANAGER, "GdaTreeMgrSchemas", &info, 0);
                g_static_mutex_unlock (&registering);
        }
        return type;
}

static void
gda_tree_mgr_schemas_set_property (GObject *object,
				   guint param_id,
				   const GValue *value,
				   GParamSpec *pspec)
{
        GdaTreeMgrSchemas *mgr;

        mgr = GDA_TREE_MGR_SCHEMAS (object);
        if (mgr->priv) {
                switch (param_id) {
		case PROP_CNC:
			mgr->priv->cnc = (GdaConnection*) g_value_get_object (value);
			if (mgr->priv->cnc)
				g_object_ref (mgr->priv->cnc);
			break;
		case PROP_META_STORE:
			mgr->priv->mstore = (GdaMetaStore*) g_value_get_object (value);
			if (mgr->priv->mstore)
				g_object_ref (mgr->priv->mstore);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
                }
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
        if (mgr->priv) {
                switch (param_id) {
		case PROP_CNC:
			g_value_set_object (value, mgr->priv->cnc);
			break;
		case PROP_META_STORE:
			g_value_set_object (value, mgr->priv->mstore);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
                }
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

	if (!mgr->priv->cnc && !mgr->priv->mstore) {
		g_set_error (error, GDA_TREE_MANAGER_ERROR, GDA_TREE_MANAGER_UNKNOWN_ERROR,
			     _("No connection and no GdaMetaStore specified"));
		if (out_error)
			*out_error = TRUE;
		return NULL;
	}
	else if (mgr->priv->mstore)
		store = mgr->priv->mstore;
	else
		store = gda_connection_get_meta_store (mgr->priv->cnc);

	scnc = gda_meta_store_get_internal_connection (store);

	if (!mgr->priv->stmt) {
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
		mgr->priv->stmt = stmt;
	}

	model = gda_connection_statement_execute_select (scnc, mgr->priv->stmt, NULL, error);
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
				g_slist_foreach (list, (GFunc) g_object_unref, NULL);
				g_slist_free (list);
			}
			if (out_error)
				*out_error = TRUE;
			g_set_error (error, GDA_TREE_MANAGER_ERROR, GDA_TREE_MANAGER_UNKNOWN_ERROR,
				     _("Unable to get schema name"));
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
