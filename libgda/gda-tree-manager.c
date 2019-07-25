/*
 * Copyright (C) 2009 - 2013 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2010 Jonh Wendell <jwendell@gnome.org>
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
#define G_LOG_DOMAIN "GDA-tree-manager"

#include "gda-tree-manager.h"
#include "gda-tree-node.h"
#include "gda-value.h"
#include <string.h>

/**
 * GdaTreeManagerNodesFunc:
 * @manager: a #GdaTreeManager
 * @node: (nullable): a #GdaTreeNode object, or %NULL
 * @children_nodes: (element-type GdaTreeNode): a list of #GdaTreeNode nodes which have previously been created by a similar call and
 * need to be updated ore moved
 * @out_error: (out) (nullable): a boolean to store if there was an error (can be %NULL)
 * @error: a place to store errors, or %NULL
 *
 * Returns: (transfer container) (element-type GdaTreeNode): a new list of #GdaTreeNode objects.
 */

/**
 * GdaTreeManagerNodeFunc:
 * @manager: a #GdaTreeManager
 * @parent: (nullable): the parent the new node may have, or %NULL
 * @name: (nullable): name given to the new node, or %NULL
 *
 * Returns: (transfer full): a new #GdaTreeNode
 */

typedef struct {
	gchar *att_name;
	GValue *value;
} AddedAttribute;

typedef struct {
        GSList  *sub_managers; /* list of GdaTreeManager structures, no ref held */
	GSList  *ref_managers; /* list of GdaTreeManager structures, ref held */
	gboolean recursive;

	GdaTreeManagerNodeFunc node_create_func;
	GdaTreeManagerNodesFunc update_func;

	GSList *added_attributes; /* list of #AddedAttribute pointers, managed here */
} GdaTreeManagerPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GdaTreeManager, gda_tree_manager, G_TYPE_OBJECT)

static void gda_tree_manager_dispose    (GObject *object);
static void gda_tree_manager_set_property (GObject *object,
					   guint param_id,
					   const GValue *value,
					   GParamSpec *pspec);
static void gda_tree_manager_get_property (GObject *object,
					   guint param_id,
					   GValue *value,
					   GParamSpec *pspec);

/* properties */
enum {
	PROP_0,
	PROP_RECURS,
	PROP_FUNC,
	LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static void
gda_tree_manager_class_init (GdaTreeManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	/* virtual methods */
	klass->update_children = NULL;

	/* Properties */
	object_class->set_property = gda_tree_manager_set_property;
	object_class->get_property = gda_tree_manager_get_property;
	object_class->dispose = gda_tree_manager_dispose;

	/**
	 * GdaTreeManager:recursive:
	 *
	 * This property specifies if, when initially creating nodes or updating the list of nodes,
	 * the tree manager shoud also request that each node it has created or updated also
	 * initially create or update their children.
	 *
	 * This property can typically set to FALSE if the process of creating children nodes is lenghty
	 * and needs to be postponed while an event occurs.
	 */
	properties [PROP_RECURS] =
                g_param_spec_boolean ("recursive",
                                      NULL,
                                      "Recursive building/updating of children",
                                      TRUE,
                                      G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT);

	/**
	 * GdaTreeManager:func: (type GdaTreeManagerNodesFunc)
	 *
	 * This property specifies the function which needs to be called when the list of #GdaTreeNode nodes
	 * managed has to be updated
	 */
	properties [PROP_FUNC] =
                g_param_spec_pointer ("func",
                                      NULL,
                                      "Function called when building/updating of children",
                                      G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT);

	g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
gda_tree_manager_init (GdaTreeManager *manager)
{
	GdaTreeManagerPrivate *priv;

	g_return_if_fail (GDA_IS_TREE_MANAGER (manager));

	priv = gda_tree_manager_get_instance_private (manager);
	priv->sub_managers = NULL;
	priv->ref_managers = NULL;
	priv->added_attributes = NULL;
}

static void
gda_tree_manager_dispose (GObject *object)
{
	GdaTreeManager *manager = (GdaTreeManager *) object;
	GdaTreeManagerPrivate *priv;

	g_return_if_fail (GDA_IS_TREE_MANAGER (manager));

	priv = gda_tree_manager_get_instance_private (manager);
	g_clear_pointer (&priv->sub_managers, g_slist_free);
	g_clear_object (&priv->ref_managers);
	if (priv->added_attributes) {
		GSList *list;
		for (list = priv->added_attributes; list; list = list->next) {
			AddedAttribute *aa = (AddedAttribute*) list->data;
			g_free (aa->att_name);
			if (aa->value)
				gda_value_free (aa->value);
			g_free (aa);
		}
	}

	g_clear_pointer (&priv->added_attributes, g_slist_free);

	/* chain to parent class */
	G_OBJECT_CLASS (gda_tree_manager_parent_class)->dispose (object);
}


/* module error */
GQuark gda_tree_manager_error_quark (void)
{
        static GQuark quark;
        if (!quark)
                quark = g_quark_from_static_string ("gda_tree_manager_error");
        return quark;
}

static void
gda_tree_manager_set_property (GObject *object,
			       guint param_id,
			       const GValue *value,
			       GParamSpec *pspec)
{
	GdaTreeManager *manager = GDA_TREE_MANAGER (object);
	GdaTreeManagerPrivate *priv = gda_tree_manager_get_instance_private (manager);

        switch (param_id) {
	case PROP_RECURS:
		priv->recursive = g_value_get_boolean (value);
		break;
	case PROP_FUNC:
		priv->update_func = g_value_get_pointer (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
gda_tree_manager_get_property (GObject *object,
			       guint param_id,
			       GValue *value,
			       GParamSpec *pspec)
{
	GdaTreeManager *manager = GDA_TREE_MANAGER (object);
	GdaTreeManagerPrivate *priv = gda_tree_manager_get_instance_private (manager);

	switch (param_id) {
	case PROP_RECURS:
		g_value_set_boolean (value, priv->recursive);
		break;
	case PROP_FUNC:
		g_value_set_pointer (value, priv->update_func);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

/*
 * @manager: a #GdaTreeManager object
 * @node: (nullable): a #GdaTreeNode object, or %NULL
 * @children_nodes: (element-type GdaTreeNode): a list of #GdaTreeNode nodes which have previously been created by a similar call and
 * need to be updated ore moved
 * @out_error: (out) (nullable): a boolean to store if there was an error (can be %NULL)
 * @error: a place to store errors, or %NULL
 *
 * Creates (or updates) the list of #GdaTreeNode objects which are placed as children of @node. The returned
 * list will completely replace the existing list of nodes managed by @manager (as children of @node).
 *
 * If a node is already present in @children_nodes and needs to be kept in the new list, then it should be added
 * to the returned list and its reference count should be increased by one.
 *
 * Since: 4.2
 */
void
_gda_tree_manager_update_children (GdaTreeManager *manager, GdaTreeNode *node, const GSList *children_nodes,
				   gboolean *out_error, GError **error)
{
	GSList *nodes_list = NULL;
	GdaTreeManagerPrivate *priv;

	g_return_if_fail (GDA_IS_TREE_MANAGER (manager));
	g_return_if_fail (GDA_IS_TREE_NODE (node));

	priv = gda_tree_manager_get_instance_private (manager);

	if (out_error)
		*out_error = FALSE;
	if (priv->update_func)
		nodes_list = priv->update_func (manager, node, children_nodes, out_error, error);
	else {
		GdaTreeManagerClass *klass = GDA_TREE_MANAGER_GET_CLASS (manager);
		if (klass->update_children)
			nodes_list = klass->update_children (manager, node, children_nodes, out_error, error);
	}
	_gda_tree_node_add_children (node, manager, nodes_list);

	/* calling sub managers for each new node */
	GSList *list;
	for (list = nodes_list; list; list = list->next) {
		GSList *sl;
		for (sl = priv->sub_managers; sl; sl = sl->next) {
			if (priv->recursive) {
				gboolean lout_error = FALSE;
				GdaTreeNode *parent = GDA_TREE_NODE (list->data);
				GdaTreeManager *mgr = GDA_TREE_MANAGER (sl->data);
				_gda_tree_manager_update_children (mgr, parent,
								   _gda_tree_node_get_children_for_manager (parent, mgr),
								   &lout_error, error);
				if (lout_error) {
					if (out_error)
						*out_error = TRUE;
					return;
				}
			}
			else 
				_gda_tree_node_add_children (GDA_TREE_NODE (list->data), GDA_TREE_MANAGER (sl->data), NULL);
		}
	}
	if (nodes_list)
		g_slist_free (nodes_list);
}


/**
 * gda_tree_manager_new_with_func:
 * @update_func: (scope call): the function to call when the manager object is requested to create or update its list of
 * #GdaTreeNode nodes
 *
 * Use this method to create a new #GdaTreeManager if it's more convenient than subclassing; all is needed
 * is the @update_func function which is responsible for creating or updating the children nodes of a specified #GdaTreeNode.
 *
 * Returns: (transfer full): a new #GdaTreeManager
 *
 * Since: 4.2
 */
GdaTreeManager *
gda_tree_manager_new_with_func (GdaTreeManagerNodesFunc update_func)
{
	g_return_val_if_fail (update_func, NULL);

	return (GdaTreeManager*) g_object_new (GDA_TYPE_TREE_MANAGER,
	                                       "func", update_func,
	                                       NULL);
}

/**
 * gda_tree_manager_add_new_node_attribute:
 * @manager: a #GdaTreeManager
 * @attribute: an attribute name
 * @value: (nullable): the attribute's value, or %NULL
 *
 * Requests that for any new node managed (eg. created) by @manager, a new attribute will be set. This allows
 * one to customize the attributes of new nodes created by an existing #GdaTreeManager.
 *
 * As a side effect, if @value is %NULL, then the corresponding attribute, if it was set, is unset.
 *
 * Since: 4.2
 */
void
gda_tree_manager_add_new_node_attribute (GdaTreeManager *manager, const gchar *attribute, const GValue *value)
{
	AddedAttribute *aa = NULL;
	GSList *list;
	GdaTreeManagerPrivate *priv;

	g_return_if_fail (GDA_IS_TREE_MANAGER (manager));
	g_return_if_fail (attribute && *attribute);

	priv = gda_tree_manager_get_instance_private (manager);
	for (list = priv->added_attributes; list; list = list->next) {
		if (!strcmp (((AddedAttribute *) list->data)->att_name, attribute)) {
			aa = (AddedAttribute *) list->data;
			break;
		}
	}
	if (!aa) {
		aa = g_new0 (AddedAttribute, 1);
		aa->att_name = g_strdup (attribute);
		priv->added_attributes = g_slist_append (priv->added_attributes, aa);
	}
	else if (aa->value) {
		gda_value_free (aa->value);
		aa->value = NULL;
	}
	if (value)
		aa->value = gda_value_copy (value);

}

/**
 * gda_tree_manager_create_node:
 * @manager: a #GdaTreeManager
 * @parent: (nullable): the parent the new node may have, or %NULL
 * @name: (nullable): name given to the new node, or %NULL
 *
 * Requests that @manager creates a new #GdaTreeNode. The new node is not in any
 * way linked to @manager yet, consider this method as a #GdaTreeNode factory.
 *
 * This method is usually used when implementing a #GdaTreeManagerNodesFunc function (to create nodes),
 * or when subclassing the #GdaTreeManager.
 *
 * Returns: (transfer full): a new #GdaTreeNode
 *
 * Since: 4.2
 */
GdaTreeNode *
gda_tree_manager_create_node (GdaTreeManager *manager, GdaTreeNode *parent, const gchar *name)
{
	GdaTreeNode *node;
	GSList *list;
	GdaTreeManagerPrivate *priv;

	g_return_val_if_fail (GDA_IS_TREE_MANAGER (manager), NULL);

	priv = gda_tree_manager_get_instance_private (manager);
	if (priv->node_create_func)
		node = priv->node_create_func (manager, parent, name);
	else
		node = gda_tree_node_new (name);

	for (list = priv->added_attributes; list; list = list->next) {
		AddedAttribute *aa = (AddedAttribute*) list->data;
		gda_tree_node_set_node_attribute (node, aa->att_name, aa->value, NULL);
	}

	return node;
}

static gboolean
manager_is_sub_manager_of (GdaTreeManager *mgr, GdaTreeManager *sub)
{
	GdaTreeManagerPrivate *priv;

	priv = gda_tree_manager_get_instance_private (mgr);
	if (g_slist_find (priv->ref_managers, sub))
		return TRUE;

	GSList *list;
	for (list = priv->sub_managers; list; list = list->next) {
		if (manager_is_sub_manager_of (GDA_TREE_MANAGER (list->data), sub))
			return TRUE;
	}

	return FALSE;
}

/**
 * gda_tree_manager_add_manager:
 * @manager: a #GdaTreeManager object
 * @sub: a #GdaTreeManager object to add
 *
 * Adds a sub manager to @manager. Use this method to create the skeleton structure
 * of a #GdaTree. Note that a single #GdaTreeManager can be used by several #GdaTree objects
 * or several times in the same #GdaTree's structure.
 *
 * Please note that it's possible for @mgr and @sub to be the same object, but beware of the possible
 * infinite recursive behaviour in this case when creating children nodes 
 * (depending on the actual implementation of the #GdaTreeManager).
 *
 * Since: 4.2
 */
void
gda_tree_manager_add_manager (GdaTreeManager *manager, GdaTreeManager *sub)
{
	GdaTreeManagerPrivate *priv;
	g_return_if_fail (GDA_IS_TREE_MANAGER (manager));
	g_return_if_fail (GDA_IS_TREE_MANAGER (sub));

	priv = gda_tree_manager_get_instance_private (manager);
	priv->sub_managers = g_slist_append (priv->sub_managers, sub);

	/* determine if @sub should be ref'ed or not to avoid circular dependencies */
	if ((sub != manager) && !manager_is_sub_manager_of (sub, manager)) {
		priv->ref_managers = g_slist_prepend (priv->ref_managers, sub);
		g_object_ref (sub);
	}
}


/**
 * gda_tree_manager_get_managers:
 * @manager: a #GdaTreeManager object
 *
 * Get the list of sub managers which have already been added using gda_tree_manager_add_manager()
 *
 * Returns: (transfer none) (element-type Gda.TreeManager): a list of #GdaTreeMenager which should not be modified.
 *
 * Since: 4.2
 */
const GSList *
gda_tree_manager_get_managers (GdaTreeManager *manager)
{
	GdaTreeManagerPrivate *priv;

	g_return_val_if_fail (GDA_IS_TREE_MANAGER (manager), NULL);

	priv = gda_tree_manager_get_instance_private (manager);
	return priv->sub_managers;
}

/**
 * gda_tree_manager_set_node_create_func:
 * @manager: a #GdaTreeManager tree manager object
 * @func: (nullable) (scope call): a #GdaTreeManagerNodeFunc function pointer, or %NULL
 *
 * Sets the function to be called when a new node is being created by @manager. If @func is %NULL
 * then each created node will be a #GdaTreeNode object.
 *
 * Specifying a custom #GdaTreeManagerNodeFunc function for example allows one to use
 * specialized sub-classed #GdaTreeNode objects.
 *
 * Since: 4.2
 */
void
gda_tree_manager_set_node_create_func (GdaTreeManager *manager, GdaTreeManagerNodeFunc func)
{
	GdaTreeManagerPrivate *priv;

	g_return_if_fail (GDA_IS_TREE_MANAGER (manager));

	priv = gda_tree_manager_get_instance_private (manager);
	priv->node_create_func = func;
}

/**
 * gda_tree_manager_get_node_create_func: (skip)
 * @manager: a #GdaTreeManager tree manager object
 *
 * Get the function used by @manager when creating new #GdaTreeNode nodes
 *
 * Returns: the #GdaTreeManagerNodeFunc function, or %NULL if the default function is used
 *
 * Since: 4.2
 */
GdaTreeManagerNodeFunc
gda_tree_manager_get_node_create_func (GdaTreeManager *manager)
{
	GdaTreeManagerPrivate *priv;

	g_return_val_if_fail (GDA_IS_TREE_MANAGER (manager), NULL);

	priv = gda_tree_manager_get_instance_private (manager);
	return priv->node_create_func;
}
