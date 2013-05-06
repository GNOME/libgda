/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
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
#include "gda-tree-mgr-select.h"
#include "gda-tree-node.h"

struct _GdaTreeMgrSelectPriv {
	GdaConnection *cnc;
	GdaStatement  *stmt;
	GdaSet        *params; /* set by the constructor, may not contain values for all @stmt's params */
	GdaSet        *priv_params;
	GSList        *non_bound_params;
};

static void gda_tree_mgr_select_class_init (GdaTreeMgrSelectClass *klass);
static void gda_tree_mgr_select_init       (GdaTreeMgrSelect *tmgr1, GdaTreeMgrSelectClass *klass);
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

static GObjectClass *parent_class = NULL;

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

	parent_class = g_type_class_peek_parent (klass);

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
gda_tree_mgr_select_init (GdaTreeMgrSelect *mgr, G_GNUC_UNUSED GdaTreeMgrSelectClass *klass)
{
	g_return_if_fail (GDA_IS_TREE_MGR_SELECT (mgr));
	mgr->priv = g_new0 (GdaTreeMgrSelectPriv, 1);
}

static void
gda_tree_mgr_select_dispose (GObject *object)
{
	GdaTreeMgrSelect *mgr = (GdaTreeMgrSelect *) object;

	g_return_if_fail (GDA_IS_TREE_MGR_SELECT (mgr));

	if (mgr->priv) {
		if (mgr->priv->cnc)
			g_object_unref (mgr->priv->cnc);
		if (mgr->priv->stmt)
			g_object_unref (mgr->priv->stmt);
		if (mgr->priv->params)
			g_object_unref (mgr->priv->params);
		if (mgr->priv->priv_params)
			g_object_unref (mgr->priv->priv_params);
		if (mgr->priv->non_bound_params)
			g_slist_free (mgr->priv->non_bound_params);

		g_free (mgr->priv);
		mgr->priv = NULL;
	}

	/* chain to parent class */
	parent_class->dispose (object);
}

/**
 * gda_tree_mgr_select_get_type:
 *
 * Returns: the GType
 *
 * Since: 4.2
 */
GType
gda_tree_mgr_select_get_type (void)
{
        static GType type = 0;

        if (G_UNLIKELY (type == 0)) {
                static GMutex registering;
                static const GTypeInfo info = {
                        sizeof (GdaTreeMgrSelectClass),
                        (GBaseInitFunc) NULL,
                        (GBaseFinalizeFunc) NULL,
                        (GClassInitFunc) gda_tree_mgr_select_class_init,
                        NULL,
                        NULL,
                        sizeof (GdaTreeMgrSelect),
                        0,
                        (GInstanceInitFunc) gda_tree_mgr_select_init,
			0
                };

                g_mutex_lock (&registering);
                if (type == 0)
                        type = g_type_register_static (GDA_TYPE_TREE_MANAGER, "GdaTreeMgrSelect", &info, 0);
                g_mutex_unlock (&registering);
        }
        return type;
}

static void
gda_tree_mgr_select_set_property (GObject *object,
				  guint param_id,
				  const GValue *value,
				  GParamSpec *pspec)
{
        GdaTreeMgrSelect *mgr;

        mgr = GDA_TREE_MGR_SELECT (object);
        if (mgr->priv) {
                switch (param_id) {
		case PROP_CNC:
			mgr->priv->cnc = (GdaConnection*) g_value_get_object (value);
			if (mgr->priv->cnc)
				g_object_ref (mgr->priv->cnc);
			break;
		case PROP_STMT:
			mgr->priv->stmt = (GdaStatement*) g_value_get_object (value);
			if (mgr->priv->stmt) {
				GError *lerror = NULL;
				g_object_ref (mgr->priv->stmt);
				if (!gda_statement_get_parameters (mgr->priv->stmt, &(mgr->priv->priv_params), &lerror)) {
					g_warning (_("Could not get SELECT statement's parameters: %s"),
						   lerror && lerror->message ? lerror->message : _("No detail"));
					if (lerror)
						g_error_free (lerror);
				}
				if (mgr->priv->priv_params && mgr->priv->priv_params->holders)
					mgr->priv->non_bound_params = g_slist_copy (mgr->priv->priv_params->holders);
			}
			break;
		case PROP_PARAMS:
			mgr->priv->params = (GdaSet*) g_value_get_object (value);
			if (mgr->priv->params)
				g_object_ref (mgr->priv->params);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
                }
        }

	if (mgr->priv->priv_params && mgr->priv->params) {
		/* bind holders in mgr->priv->priv_params to the ones in mgr->priv->params
		 * if they exist */
		GSList *params;
		GSList *non_bound_params = NULL;

		g_slist_free (mgr->priv->non_bound_params);
		for (params = mgr->priv->priv_params->holders; params; params = params->next) {
			GdaHolder *frh = GDA_HOLDER (params->data);
			GdaHolder *toh = gda_set_get_holder (mgr->priv->params, gda_holder_get_id (frh));
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
		mgr->priv->non_bound_params = non_bound_params;
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
        if (mgr->priv) {
                switch (param_id) {
		case PROP_CNC:
			g_value_set_object (value, mgr->priv->cnc);
			break;
		case PROP_STMT:
			g_value_set_object (value, mgr->priv->stmt);
			break;
		case PROP_PARAMS:
			g_value_set_object (value, mgr->priv->params);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
                }
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

	if (!mgr->priv->cnc) {
		g_set_error (error, GDA_TREE_MANAGER_ERROR, GDA_TREE_MANAGER_UNKNOWN_ERROR,
			     "%s", _("No connection specified"));
		if (out_error)
			*out_error = TRUE;
		return NULL;
	}

	if (!mgr->priv->stmt) {
		g_set_error (error, GDA_TREE_MANAGER_ERROR, GDA_TREE_MANAGER_UNKNOWN_ERROR,
			     "%s", _("No SELECT statement specified"));
		if (out_error)
			*out_error = TRUE;
		return NULL;
	}

	if (node && mgr->priv->non_bound_params) {
		/* looking for values in @node's attributes */
		GSList *nbplist;
		for (nbplist = mgr->priv->non_bound_params; nbplist; nbplist = nbplist->next) {
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

	model = gda_connection_statement_execute_select (mgr->priv->cnc, mgr->priv->stmt, mgr->priv->priv_params, error);
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

		for (iholders = GDA_SET (iter)->holders; iholders; iholders = iholders->next) {
			GdaHolder *holder = (GdaHolder*) iholders->data;

			if (!gda_holder_is_valid (holder) || !(cvalue = gda_holder_get_value (holder))) {
				if (list) {
					g_slist_foreach (list, (GFunc) g_object_unref, NULL);
					g_slist_free (list);
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
