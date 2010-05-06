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
#include "gda-tree-mgr-tables.h"
#include "gda-tree-node.h"

struct _GdaTreeMgrTablesPriv {
	GdaConnection *cnc;
	gchar         *schema; /* imposed upon construction */

	GdaStatement  *stmt_with_schema;
	GdaStatement  *stmt_all_visible;
	GdaSet        *params;
};

static void gda_tree_mgr_tables_class_init (GdaTreeMgrTablesClass *klass);
static void gda_tree_mgr_tables_init       (GdaTreeMgrTables *tmgr1, GdaTreeMgrTablesClass *klass);
static void gda_tree_mgr_tables_dispose    (GObject *object);
static void gda_tree_mgr_tables_set_property (GObject *object,
					      guint param_id,
					      const GValue *value,
					      GParamSpec *pspec);
static void gda_tree_mgr_tables_get_property (GObject *object,
					      guint param_id,
					      GValue *value,
					      GParamSpec *pspec);

/* virtual methods */
static GSList *gda_tree_mgr_tables_update_children (GdaTreeManager *manager, GdaTreeNode *node, const GSList *children_nodes,
						    gboolean *out_error, GError **error);

static GObjectClass *parent_class = NULL;

/* properties */
enum {
        PROP_0,
	PROP_CNC,
	PROP_SCHEMA
};

/*
 * GdaTreeMgrTables class implementation
 * @klass:
 */
static void
gda_tree_mgr_tables_class_init (GdaTreeMgrTablesClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	/* virtual methods */
	((GdaTreeManagerClass*) klass)->update_children = gda_tree_mgr_tables_update_children;

	/* Properties */
        object_class->set_property = gda_tree_mgr_tables_set_property;
        object_class->get_property = gda_tree_mgr_tables_get_property;

	g_object_class_install_property (object_class, PROP_CNC,
                                         g_param_spec_object ("connection", NULL, "Connection to use",
                                                              GDA_TYPE_CONNECTION,
                                                              G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
	/**
	 * GdaTreeMgrTables:schema:
	 *
	 * If no set, then the table name will be fetched from the parent node using the "schema" attribute. If not
	 * found that way, then the list of visible tables (tables which can be identified without having to specify
	 * a schema) will be used
	 */
        g_object_class_install_property (object_class, PROP_SCHEMA,
                                         g_param_spec_string ("schema", NULL,
                                                              "Database schema to get the tables list from",
                                                              NULL, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

	object_class->dispose = gda_tree_mgr_tables_dispose;
}

static void
gda_tree_mgr_tables_init (GdaTreeMgrTables *mgr, GdaTreeMgrTablesClass *klass)
{
	g_return_if_fail (GDA_IS_TREE_MGR_TABLES (mgr));
	mgr->priv = g_new0 (GdaTreeMgrTablesPriv, 1);
}

static void
gda_tree_mgr_tables_dispose (GObject *object)
{
	GdaTreeMgrTables *mgr = (GdaTreeMgrTables *) object;

	g_return_if_fail (GDA_IS_TREE_MGR_TABLES (mgr));

	if (mgr->priv) {
		if (mgr->priv->cnc)
			g_object_unref (mgr->priv->cnc);
		g_free (mgr->priv->schema);

		if (mgr->priv->stmt_with_schema)
			g_object_unref (mgr->priv->stmt_with_schema);
		if (mgr->priv->stmt_all_visible)
			g_object_unref (mgr->priv->stmt_all_visible);
		if (mgr->priv->params)
			g_object_unref (mgr->priv->params);

		g_free (mgr->priv);
		mgr->priv = NULL;
	}

	/* chain to parent class */
	parent_class->dispose (object);
}

/**
 * gda_tree_mgr_select_get_type
 *
 * Returns: the GType
 *
 * Since: 4.2
 */
GType
gda_tree_mgr_tables_get_type (void)
{
        static GType type = 0;

        if (G_UNLIKELY (type == 0)) {
                static GStaticMutex registering = G_STATIC_MUTEX_INIT;
                static const GTypeInfo info = {
                        sizeof (GdaTreeMgrTablesClass),
                        (GBaseInitFunc) NULL,
                        (GBaseFinalizeFunc) NULL,
                        (GClassInitFunc) gda_tree_mgr_tables_class_init,
                        NULL,
                        NULL,
                        sizeof (GdaTreeMgrTables),
                        0,
                        (GInstanceInitFunc) gda_tree_mgr_tables_init
                };

                g_static_mutex_lock (&registering);
                if (type == 0)
                        type = g_type_register_static (GDA_TYPE_TREE_MANAGER, "GdaTreeMgrTables", &info, 0);
                g_static_mutex_unlock (&registering);
        }
        return type;
}

static void
gda_tree_mgr_tables_set_property (GObject *object,
				   guint param_id,
				   const GValue *value,
				   GParamSpec *pspec)
{
        GdaTreeMgrTables *mgr;

        mgr = GDA_TREE_MGR_TABLES (object);
        if (mgr->priv) {
                switch (param_id) {
		case PROP_CNC:
			mgr->priv->cnc = (GdaConnection*) g_value_get_object (value);
			if (mgr->priv->cnc)
				g_object_ref (mgr->priv->cnc);
			break;
		case PROP_SCHEMA:
			mgr->priv->schema = g_value_dup_string (value);
			break;
                }
        }
}

static void
gda_tree_mgr_tables_get_property (GObject *object,
				   guint param_id,
				   GValue *value,
				   GParamSpec *pspec)
{
        GdaTreeMgrTables *mgr;

        mgr = GDA_TREE_MGR_TABLES (object);
        if (mgr->priv) {
                switch (param_id) {
		case PROP_CNC:
			g_value_set_object (value, mgr->priv->cnc);
			break;
		case PROP_SCHEMA:
			g_value_set_string (value, mgr->priv->schema);
			break;
                }
        }
}

/**
 * gda_tree_mgr_tables_new
 * @cnc: a #GdaConnection object
 * @schema: a schema name or %NULL
 *
 * Creates a new #GdaTreeManager object which will add one tree node for each table found in the
 * @schema if it is not %NULL, or for each table visible by default in @cnc.
 *
 * Returns: a new #GdaTreeManager object
 *
 * Since: 4.2
 */
GdaTreeManager*
gda_tree_mgr_tables_new (GdaConnection *cnc, const gchar *schema)
{
	GdaTreeMgrTables *mgr;
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	mgr = (GdaTreeMgrTables*) g_object_new (GDA_TYPE_TREE_MGR_TABLES,
						"connection", cnc, 
						"schema", schema, NULL);
	return (GdaTreeManager*) mgr;
}

static GSList *
gda_tree_mgr_tables_update_children (GdaTreeManager *manager, GdaTreeNode *node, const GSList *children_nodes,
				     gboolean *out_error, GError **error)
{
	GdaTreeMgrTables *mgr = GDA_TREE_MGR_TABLES (manager);
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

	/* create statements if necessary */
	if (!mgr->priv->stmt_with_schema) {
		GdaSqlParser *parser;
		GdaStatement *stmt1, *stmt2;

		parser = gda_connection_create_parser (scnc);
		if (! parser)
			parser = gda_sql_parser_new ();

		stmt1 = gda_sql_parser_parse_string (parser,
						     "SELECT table_name, table_schema FROM "
						     "_tables WHERE "
						     "table_type LIKE '%TABLE%' AND "
						     "table_schema = ##schema::string "
						     "ORDER BY table_name DESC", NULL, error);

		stmt2 = gda_sql_parser_parse_string (parser,
						     "SELECT table_short_name, table_schema "
						     "FROM _tables WHERE "
						     "table_type LIKE '%TABLE%' AND "
						     "table_short_name != table_full_name "
						     "ORDER BY table_short_name DESC", NULL, error);
		g_object_unref (parser);
		if (!stmt1 || !stmt2) {
			if (out_error)
				*out_error = TRUE;
			if (stmt1) g_object_unref (stmt1);
			if (stmt2) g_object_unref (stmt2);
			return NULL;
		}

		if (!gda_statement_get_parameters (stmt1, &(mgr->priv->params), error)) {
			if (out_error)
				*out_error = TRUE;
			g_object_unref (stmt1);
			g_object_unref (stmt2);
			return NULL;
		}
		mgr->priv->stmt_with_schema = stmt1;
		mgr->priv->stmt_all_visible = stmt2;
	}

	gboolean schema_specified = FALSE;
	gboolean schema_from_parent = FALSE;
	if (mgr->priv->schema) {
		schema_specified = TRUE;
		g_assert (gda_set_set_holder_value (mgr->priv->params, NULL, "schema", mgr->priv->schema));
	}
	else if (node) {
		/* looking for a schema in @node's attributes */
		const GValue *cvalue;
		cvalue = gda_tree_node_fetch_attribute (node, "schema");
		if (cvalue) {
			GdaHolder *h = gda_set_get_holder (mgr->priv->params, "schema");
			if (!gda_holder_set_value (h, cvalue, error)) {
				if (out_error)
					*out_error = TRUE;
				return NULL;
			}
			schema_specified = TRUE;
			schema_from_parent = TRUE;
		}
	}
	if (schema_specified)
		model = gda_connection_statement_execute_select (scnc, mgr->priv->stmt_with_schema, 
								 mgr->priv->params, error);
	else
		model = gda_connection_statement_execute_select (scnc, mgr->priv->stmt_all_visible, 
								 NULL, error);

	if (!model) {
		if (out_error)
			*out_error = TRUE;
		return NULL;
	}

	GdaDataModelIter *iter;
	iter = gda_data_model_create_iter (model);
	for (; iter && gda_data_model_iter_move_next (iter);) {
		GdaTreeNode* snode;
		const GValue *cvalue, *cvalue2;

		cvalue = gda_data_model_iter_get_value_at (iter, 0);
		cvalue2 = gda_data_model_iter_get_value_at (iter, 1);
		if (!cvalue || !cvalue2) {
			if (list) {
				g_slist_foreach (list, (GFunc) g_object_unref, NULL);
				g_slist_free (list);
			}
			if (out_error)
				*out_error = TRUE;
			g_set_error (error, GDA_TREE_MANAGER_ERROR, GDA_TREE_MANAGER_UNKNOWN_ERROR,
				     _("Unable to get table name"));
			return NULL;
		}

		snode = gda_tree_manager_create_node (manager, node, g_value_get_string (cvalue));
		gda_tree_node_set_node_attribute (snode, "table_name", cvalue, NULL);
		if (!schema_from_parent)
			gda_tree_node_set_node_attribute (snode, "schema", cvalue2, NULL);
		list = g_slist_prepend (list, snode);
	}
	if (iter)
		g_object_unref (iter);
	g_object_unref (model);

	return list;
}
