/* GDA library
 * Copyright (C) 2009 The GNOME Foundation.
 *
 * AUTHORS:
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

#include <glib/gi18n-lib.h>
#include <libgda/libgda.h>
#include <sql-parser/gda-sql-parser.h>
#include "gda-tree-mgr-schemas.h"
#include "gda-tree-node.h"

struct _GdaTreeMgrSchemasPriv {
	GdaConnection *cnc;
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
	PROP_CNC
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

	g_object_class_install_property (object_class, PROP_CNC,
                                         g_param_spec_object ("connection", NULL, "Connection to use",
                                                              GDA_TYPE_CONNECTION,
                                                              G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
	object_class->dispose = gda_tree_mgr_schemas_dispose;
}

static void
gda_tree_mgr_schemas_init (GdaTreeMgrSchemas *mgr, GdaTreeMgrSchemasClass *klass)
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
		if (mgr->priv->stmt)
			g_object_unref (mgr->priv->stmt);

		g_free (mgr->priv);
		mgr->priv = NULL;
	}

	/* chain to parent class */
	parent_class->dispose (object);
}

/**
 * gda_tree_mgr_schemas_get_type
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
                        (GInstanceInitFunc) gda_tree_mgr_schemas_init
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
                }
        }
}

/**
 * gda_tree_mgr_schemas_new
 * @cnc: a #GdaConnection object
 *
 * Creates a new #GdaTreeManager object which will add one tree node for each database schema found
 * in @cnc.
 *
 * Returns: a new #GdaTreeManager object
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
gda_tree_mgr_schemas_update_children (GdaTreeManager *manager, GdaTreeNode *node, const GSList *children_nodes,
				      gboolean *out_error, GError **error)
{
	GdaTreeMgrSchemas *mgr = GDA_TREE_MGR_SCHEMAS (manager);
	GdaMetaStore *store;
	GdaDataModel *model;
	GSList *list = NULL;
	GdaConnection *scnc;

	if (!mgr->priv->cnc) {
		g_set_error (error, GDA_TREE_MANAGER_ERROR, GDA_TREE_MANAGER_UNKNOWN_ERROR,
			     _("No connection specified"));
		if (out_error)
			*out_error = TRUE;
		return NULL;
	}

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
		GdaTreeNode* node;
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

		node = gda_tree_node_new (g_value_get_string (cvalue));
		gda_tree_node_set_node_attribute (node, "schema", cvalue, NULL);
		list = g_slist_prepend (list, node);
	}
	if (iter)
		g_object_unref (iter);
	g_object_unref (model);

	return list;
}
