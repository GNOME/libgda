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
#define G_LOG_DOMAIN "GDA-tree-mgr-select"

#include <glib/gi18n-lib.h>
#include <libgda/libgda.h>
#include <sql-parser/gda-sql-parser.h>
#include "gda-tree-mgr-select.h"
#include "gda-tree-node.h"

typedef struct {
	GdaConnection *cnc;
	GdaStatement  *stmt;
	GdaSet        *params; /* set by the constructor, may not contain values for all @stmt's params */
	GdaSet        *priv_params;
	GSList        *non_bound_params;
} GdaTreeMgrSelectPrivate;
G_DEFINE_TYPE_WITH_PRIVATE (GdaTreeMgrSelect, gda_tree_mgr_select, GDA_TYPE_TREE_MANAGER)

static void gda_tree_mgr_select_dispose    (GObject *object);
static void gda_tree_mgr_select_set_property (GObject *object,
					      guint param_id,
					      const GValue *value,
					      GParamSpec *pspec);
static void gda_tree_mgr_select_get_property (GObject *object,
					      guint param_id,
					      GValue *value,
					      GParamSpec *pspec);

/* virtual methods */
static GSList *gda_tree_mgr_select_update_children (GdaTreeManager *manager, GdaTreeNode *node, const GSList *children_nodes,
						    gboolean *out_error, GError **error);

/* properties */
enum {
        PROP_0,
	PROP_CNC,
	PROP_STMT,
	PROP_PARAMS
};

/*
 * GdaTreeMgrSelect class implementation
 * @klass:
 */
static void
gda_tree_mgr_select_class_init (GdaTreeMgrSelectClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	/* virtual methods */
	((GdaTreeManagerClass*) klass)->update_children = gda_tree_mgr_select_update_children;

	/* Properties */
        object_class->set_property = gda_tree_mgr_select_set_property;
        object_class->get_property = gda_tree_mgr_select_get_property;

	g_object_class_install_property (object_class, PROP_CNC,
                                         g_param_spec_object ("connection", NULL, "Connection to use",
                                                              GDA_TYPE_CONNECTION,
                                                              G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class, PROP_STMT,
                                         g_param_spec_object ("statement", NULL, "SELECT statement",
                                                              GDA_TYPE_STATEMENT,
                                                              G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class, PROP_PARAMS,
                                         g_param_spec_object ("params", NULL, "Parameters for the SELECT statement",
                                                              GDA_TYPE_SET,
                                                              G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

	object_class->dispose = gda_tree_mgr_select_dispose;
}

static void
gda_tree_mgr_select_init (GdaTreeMgrSelect *mgr) {}

static void
gda_tree_mgr_select_dispose (GObject *object)
{
	GdaTreeMgrSelect *mgr = (GdaTreeMgrSelect *) object;

	g_return_if_fail (GDA_IS_TREE_MGR_SELECT (mgr));
	GdaTreeMgrSelectPrivate *priv = gda_tree_mgr_select_get_instance_private (mgr);

	if (priv->cnc)
		g_object_unref (priv->cnc);
	if (priv->stmt)
		g_object_unref (priv->stmt);
	if (priv->params)
		g_object_unref (priv->params);
	if (priv->priv_params)
		g_object_unref (priv->priv_params);
	if (priv->non_bound_params)
		g_slist_free (priv->non_bound_params);


	/* chain to parent class */
	G_OBJECT_CLASS (gda_tree_mgr_select_parent_class)->dispose (object);
}

static void
gda_tree_mgr_select_set_property (GObject *object,
				  guint param_id,
				  const GValue *value,
				  GParamSpec *pspec)
{
	GdaTreeMgrSelect *mgr;

	mgr = GDA_TREE_MGR_SELECT (object);
	GdaTreeMgrSelectPrivate *priv = gda_tree_mgr_select_get_instance_private (mgr);
	switch (param_id) {
		case PROP_CNC:
			priv->cnc = (GdaConnection*) g_value_get_object (value);
			if (priv->cnc)
				g_object_ref (priv->cnc);
			break;
		case PROP_STMT:
			priv->stmt = (GdaStatement*) g_value_get_object (value);
			if (priv->stmt) {
				GError *lerror = NULL;
				g_object_ref (priv->stmt);
				if (!gda_statement_get_parameters (priv->stmt, &(priv->priv_params), &lerror)) {
					g_warning (_("Could not get SELECT statement's parameters: %s"),
						   lerror && lerror->message ? lerror->message : _("No detail"));
					if (lerror)
						g_error_free (lerror);
				}
				if (priv->priv_params && gda_set_get_holders (priv->priv_params))
					priv->non_bound_params = g_slist_copy (gda_set_get_holders (priv->priv_params));
			}
			break;
		case PROP_PARAMS:
			priv->params = (GdaSet*) g_value_get_object (value);
			if (priv->params)
				g_object_ref (priv->params);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
	}

	if (priv->priv_params && priv->params) {
		/* bind holders in priv->priv_params to the ones in priv->params
		 * if they exist */
		GSList *params;
		GSList *non_bound_params = NULL;

		g_slist_free (priv->non_bound_params);
		for (params = gda_set_get_holders (priv->priv_params); params; params = params->next) {
			GdaHolder *frh = GDA_HOLDER (params->data);
			GdaHolder *toh = gda_set_get_holder (priv->params, gda_holder_get_id (frh));
			if (toh) {
				GError *lerror = NULL;
				if (!gda_holder_set_bind (frh, toh, &lerror)) {
					g_warning (_("Could not bind SELECT statement's parameter '%s' "
						     "to provided parameters: %s"),
						   gda_holder_get_id (frh),
						   lerror && lerror->message ? lerror->message : _("No detail"));
					if (lerror)
						g_error_free (lerror);
					non_bound_params = g_slist_prepend (non_bound_params, frh);
				}
			}
			else
				non_bound_params = g_slist_prepend (non_bound_params, frh);
		}
		priv->non_bound_params = non_bound_params;
	}
}

static void
gda_tree_mgr_select_get_property (GObject *object,
				   guint param_id,
				   GValue *value,
				   GParamSpec *pspec)
{
	GdaTreeMgrSelect *mgr;

	mgr = GDA_TREE_MGR_SELECT (object);
	GdaTreeMgrSelectPrivate *priv = gda_tree_mgr_select_get_instance_private (mgr);
	switch (param_id) {
		case PROP_CNC:
			g_value_set_object (value, priv->cnc);
			break;
		case PROP_STMT:
			g_value_set_object (value, priv->stmt);
			break;
		case PROP_PARAMS:
			g_value_set_object (value, priv->params);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
	}
}

/**
 * gda_tree_mgr_select_new:
 * @cnc: a #GdaConnection object
 * @stmt: a #GdaStatement object representing a SELECT statement
 * @params: a #GdaSet object representing fixed parameters which are to be used when executing @stmt
 *
 * Creates a new #GdaTreeMgrSelect object which will add one tree node for each row in
 * the #GdaDataModel resulting from the execution of @stmt.
 *
 * Returns: (transfer full): a new #GdaTreeManager object
 *
 * Since: 4.2
 */
GdaTreeManager*
gda_tree_mgr_select_new (GdaConnection *cnc, GdaStatement *stmt, GdaSet *params)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);
	g_return_val_if_fail (gda_statement_get_statement_type (stmt) == GDA_SQL_STATEMENT_SELECT, NULL);
	g_return_val_if_fail (!params || GDA_IS_SET (params), NULL);

	return (GdaTreeManager*) g_object_new (GDA_TYPE_TREE_MGR_SELECT,
					       "connection", cnc, 
					       "statement", stmt,
					       "params", params, NULL);
}

static GSList *
gda_tree_mgr_select_update_children (GdaTreeManager *manager, GdaTreeNode *node,
				     G_GNUC_UNUSED const GSList *children_nodes, gboolean *out_error,
				     GError **error)
{
	GdaTreeMgrSelect *mgr = GDA_TREE_MGR_SELECT (manager);
	GdaDataModel *model;
	GSList *list = NULL;
	GdaTreeMgrSelectPrivate *priv = gda_tree_mgr_select_get_instance_private (mgr);

	if (!priv->cnc) {
		g_set_error (error, GDA_TREE_MANAGER_ERROR, GDA_TREE_MANAGER_UNKNOWN_ERROR,
			     "%s", _("No connection specified"));
		if (out_error)
			*out_error = TRUE;
		return NULL;
	}

	if (!priv->stmt) {
		g_set_error (error, GDA_TREE_MANAGER_ERROR, GDA_TREE_MANAGER_UNKNOWN_ERROR,
			     "%s", _("No SELECT statement specified"));
		if (out_error)
			*out_error = TRUE;
		return NULL;
	}

	if (node && priv->non_bound_params) {
		/* looking for values in @node's attributes */
		GSList *nbplist;
		for (nbplist = priv->non_bound_params; nbplist; nbplist = nbplist->next) {
			const GValue *cvalue;
			GdaHolder *holder = (GdaHolder*) nbplist->data;
			cvalue = gda_tree_node_fetch_attribute (node, gda_holder_get_id (holder));
			if (cvalue) {
				if (!gda_holder_set_value (holder, cvalue, error)) {
					if (out_error)
						*out_error = TRUE;
					return NULL;
				}
			}
			else {
				g_set_error (error, GDA_TREE_MANAGER_ERROR, GDA_TREE_MANAGER_UNKNOWN_ERROR,
					     _("No value specified for parameter '%s'"), gda_holder_get_id (holder));
				if (out_error)
					*out_error = TRUE;
				return NULL;
			}
		}
	}

	model = gda_connection_statement_execute_select (priv->cnc, priv->stmt, priv->priv_params, error);
	if (!model) {
		if (out_error)
			*out_error = TRUE;
		return NULL;
	}

	GdaDataModelIter *iter;
	iter = gda_data_model_create_iter (model);
	for (; iter && gda_data_model_iter_move_next (iter);) {
		GdaTreeNode* snode = NULL;
		const GValue *cvalue;
		GSList *iholders;

		for (iholders = gda_set_get_holders (GDA_SET (iter)); iholders; iholders = iholders->next) {
			GdaHolder *holder = (GdaHolder*) iholders->data;

			if (!gda_holder_is_valid (holder) || !(cvalue = gda_holder_get_value (holder))) {
				if (list) {
					g_slist_free_full (list, (GDestroyNotify) g_object_unref);
				}

				if (out_error)
					*out_error = TRUE;
				g_set_error (error, GDA_TREE_MANAGER_ERROR, GDA_TREE_MANAGER_UNKNOWN_ERROR,
					     "%s", _("Unable to get iterator's value"));
				return NULL;
			}

			if (!snode) {
				gchar *str = gda_value_stringify (cvalue);
				snode = gda_tree_manager_create_node (manager, node, str);
				g_free (str);
				list = g_slist_prepend (list, snode);
			}
			
			gda_tree_node_set_node_attribute (snode, g_strdup (gda_holder_get_id (holder)), cvalue, g_free);
		}
	}
	if (iter)
		g_object_unref (iter);
	g_object_unref (model);

	return list;
}
