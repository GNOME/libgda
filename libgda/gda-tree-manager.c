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

#include "gda-tree-manager.h"
#include "gda-tree-node.h"

struct _GdaTreeManagerPrivate {
        GSList  *sub_managers; /* list of GdaTreeManager structures */
	gboolean recursive;

	GdaTreeManagerNodeFunc node_create_func;
};

static void gda_tree_manager_class_init (GdaTreeManagerClass *klass);
static void gda_tree_manager_init       (GdaTreeManager *manager, GdaTreeManagerClass *klass);
static void gda_tree_manager_dispose    (GObject *object);
static void gda_tree_manager_set_property (GObject *object,
					   guint param_id,
					   const GValue *value,
					   GParamSpec *pspec);
static void gda_tree_manager_get_property (GObject *object,
					   guint param_id,
					   GValue *value,
					   GParamSpec *pspec);

enum {
	DUMMY,
	LAST_SIGNAL
};

static gint gda_tree_manager_signals[LAST_SIGNAL] = { 0 };

/* properties */
enum {
	PROP_0,
	PROP_RECURS
};

static GObjectClass *parent_class = NULL;

/*
 * GdaTreeManager class implementation
 * @klass:
 */
static void
gda_tree_manager_class_init (GdaTreeManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	/* virtual methods */
	klass->update_children = NULL;

	/* Properties */
        object_class->set_property = gda_tree_manager_set_property;
        object_class->get_property = gda_tree_manager_get_property;

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
	g_object_class_install_property (object_class, PROP_RECURS,
                                         g_param_spec_boolean ("recursive", NULL, "Recursive building/updating of children",
                                                              TRUE,
                                                              G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

	object_class->dispose = gda_tree_manager_dispose;
}

static void
gda_tree_manager_init (GdaTreeManager *manager, GdaTreeManagerClass *klass)
{
	g_return_if_fail (GDA_IS_TREE_MANAGER (manager));

	manager->priv = g_new0 (GdaTreeManagerPrivate, 1);
	manager->priv->sub_managers = NULL;
}

static void
gda_tree_manager_dispose (GObject *object)
{
	GdaTreeManager *manager = (GdaTreeManager *) object;

	g_return_if_fail (GDA_IS_TREE_MANAGER (manager));

	if (manager->priv) {
		if (manager->priv->sub_managers) {
			g_slist_foreach (manager->priv->sub_managers, (GFunc) g_object_unref, NULL);
			g_slist_free (manager->priv->sub_managers);
		}
		g_free (manager->priv);
		manager->priv = NULL;
	}

	/* chain to parent class */
	parent_class->dispose (object);
}


/* module error */
GQuark gda_tree_manager_error_quark (void)
{
        static GQuark quark;
        if (!quark)
                quark = g_quark_from_static_string ("gda_tree_manager_error");
        return quark;
}

/**
 * gda_tree_manager_get_type
 * 
 * Registers the #GdaTreeManager class on the GLib type system.
 * 
 * Returns: the GType identifying the class.
 *
 * Since: 4.2
 */
GType
gda_tree_manager_get_type (void)
{
        static GType type = 0;

        if (G_UNLIKELY (type == 0)) {
                static GStaticMutex registering = G_STATIC_MUTEX_INIT;
                static const GTypeInfo info = {
                        sizeof (GdaTreeManagerClass),
                        (GBaseInitFunc) NULL,
                        (GBaseFinalizeFunc) NULL,
                        (GClassInitFunc) gda_tree_manager_class_init,
                        NULL,
                        NULL,
                        sizeof (GdaTreeManager),
                        0,
                        (GInstanceInitFunc) gda_tree_manager_init
                };

                g_static_mutex_lock (&registering);
                if (type == 0)
                        type = g_type_register_static (G_TYPE_OBJECT, "GdaTreeManager", &info, G_TYPE_FLAG_ABSTRACT);
                g_static_mutex_unlock (&registering);
        }
        return type;
}

static void
gda_tree_manager_set_property (GObject *object,
			       guint param_id,
			       const GValue *value,
			       GParamSpec *pspec)
{
	GdaTreeManager *manager;

        manager = GDA_TREE_MANAGER (object);
        if (manager->priv) {
                switch (param_id) {
		case PROP_RECURS:
			manager->priv->recursive = g_value_get_boolean (value);
			break;
		}	
	}
}

static void
gda_tree_manager_get_property (GObject *object,
			       guint param_id,
			       GValue *value,
			       GParamSpec *pspec)
{
	GdaTreeManager *manager;
	
	manager = GDA_TREE_MANAGER (object);
	if (manager->priv) {
		switch (param_id) {
		case PROP_RECURS:
			g_value_set_boolean (value, manager->priv->recursive);
			break;
		}
	}	
}

/**
 * gda_tree_manager_update_children
 * @manager: a #GdaTreeManager object
 * @node: a #GdaTreeNode object, or %NULL
 * @children_nodes: a list of #GdaTreeNode nodes which have previously been created by a similar call and
 * need to be updated ore moved
 * @out_error: a boolean to store if there was an error (can be %NULL)
 * @error: a place to store errors, or %NULL
 *
 * Creates (or updates) the list of #GdaTreeNode objects which are placed as children of @node. The returned
 * list will completely replace the existing list of nodes managed ny @manager (as children of @node).
 *
 * If a node is already present in @children_nodes and needs to be kept in the new list, then it should be added
 * to the returned list and its reference count should be increased by one.
 *
 * Returns: a new list of #GdaTreeNode objects.
 *
 * Since: 4.2
 */
GSList *
gda_tree_manager_update_children (GdaTreeManager *manager, GdaTreeNode *node, const GSList *children_nodes,
				  gboolean *out_error, GError **error)
{
	GSList *nodes_list = NULL;
	GdaTreeManagerClass *klass;

	g_return_val_if_fail (GDA_IS_TREE_MANAGER (manager), NULL);
	g_return_val_if_fail (!node || GDA_IS_TREE_NODE (node), NULL);

	if (out_error)
		*out_error = FALSE;
	klass = (GdaTreeManagerClass*) G_OBJECT_GET_CLASS (manager);
	if (klass->update_children)
		nodes_list = klass->update_children (manager, node, children_nodes, out_error, error);
	if (node && nodes_list)
		_gda_tree_node_add_children (node, manager, nodes_list);

	/* calling sub managers for each new node */
	GSList *list;
	for (list = nodes_list; list; list = list->next) {
		GSList *sl;
		for (sl = manager->priv->sub_managers; sl; sl = sl->next) {
			if (manager->priv->recursive) {
				GSList *snodes;
				gboolean lout_error = FALSE;
				GdaTreeNode *parent = GDA_TREE_NODE (list->data);
				GdaTreeManager *mgr = (GdaTreeManager *) sl->data;
				snodes = gda_tree_manager_update_children (mgr, parent,
									   _gda_tree_node_get_children_for_manager (parent, mgr),
									   &lout_error, error);
				if (lout_error) {
					if (out_error)
						*out_error = TRUE;
					return NULL;
				}
			}
			else 
				_gda_tree_node_add_children (GDA_TREE_NODE (list->data), (GdaTreeManager *) sl->data, NULL);	
		}
	}
	return nodes_list;
}

/**
 * gda_tree_manager_add_manager
 * @manager: a #GdaTreeManager object
 * @sub: a #GdaTreeManager object to add
 *
 * Adds a sub manager to @manager. Use this method to create the skeleton structure
 * of a #GdaTree. Note that a single #GdaTreeManager can be used by several #GdaTree objects
 * or several times in the same #GdaTree's structure.
 *
 * Since: 4.2
 */
void
gda_tree_manager_add_manager (GdaTreeManager *manager, GdaTreeManager *sub)
{
	g_return_if_fail (GDA_IS_TREE_MANAGER (manager));
	g_return_if_fail (GDA_IS_TREE_MANAGER (sub));
	g_return_if_fail (manager != sub);

	manager->priv->sub_managers = g_slist_append (manager->priv->sub_managers, sub);
	g_object_ref (sub);
}

/**
 * gda_tree_manager_set_node_create_func
 * @mgr: a #GdaTreeManager tree manager object
 * @func: a #GdaTreeManagerNodeFunc function pointer, or %NULL
 *
 * Sets the function to be called when a new node is being created by @mgr. If @func is %NULL
 * then each created node will be a #GdaTreeNode object.
 *
 * Specifying a custom #GdaTreeManagerNodeFunc function for example allows one to use
 * specialized sub-classed #GdaTreeNode objects.
 *
 * Since: 4.2
 */
void
gda_tree_manager_set_node_create_func (GdaTreeManager *mgr, GdaTreeManagerNodeFunc func)
{
	g_return_if_fail (GDA_IS_TREE_MANAGER (mgr));
	mgr->priv->node_create_func = func;
}

/**
 * gda_tree_manager_get_node_create_func
 * @mgr: a #GdaTreeManager tree manager object
 *
 * Get the function used by @mgr when creating new #GdaTreeNode nodes
 *
 * Returns: the #GdaTreeManagerNodeFunc function, or %NULL if the default function is used
 *
 * Since: 4.2
 */
GdaTreeManagerNodeFunc
gda_tree_manager_get_node_create_func (GdaTreeManager *mgr)
{
	g_return_val_if_fail (GDA_IS_TREE_MANAGER (mgr), NULL);
	return mgr->priv->node_create_func;
}
