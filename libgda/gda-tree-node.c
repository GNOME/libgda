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

#include <string.h>
#include "gda-tree-node.h"
#include "gda-tree-manager.h"
#include <libgda/gda-attributes-manager.h>
#include <libgda/gda-value.h>

struct _GdaTreeNodePrivate {
	GdaTreeManager *manager;
        GSList         *children; /* list of GdaTreeNodesList */
	GdaTreeNode    *parent;
};

/*
 * The GdaTreeNodesList stores the list of children nodes created by a GdaTreeManager object
 */
typedef struct {
	GdaTreeManager *mgr; /* which created @nodes */
	GSList         *nodes; /* list of GdaTreeNode */
	gint            length; /* length of @nodes */
} GdaTreeNodesList;
#define GDA_TREE_NODES_LIST(x) ((GdaTreeNodesList*)(x))

GdaTreeNodesList *_gda_nodes_list_new (GdaTreeManager *mgr, GSList *nodes);
void              _gda_nodes_list_free (GdaTreeNodesList *nl);

/*
 * GObject functions
 */
static void gda_tree_node_class_init (GdaTreeNodeClass *klass);
static void gda_tree_node_init       (GdaTreeNode *tnode, GdaTreeNodeClass *klass);
static void gda_tree_node_dispose    (GObject *object);
static void gda_tree_node_set_property (GObject *object,
					guint param_id,
					const GValue *value,
					GParamSpec *pspec);
static void gda_tree_node_get_property (GObject *object,
					guint param_id,
					GValue *value,
					GParamSpec *pspec);

/* default virtual methods */
static gchar *gda_tree_node_dump_header (GdaTreeNode *node);
static void gda_tree_node_dump_children (GdaTreeNode *node, const gchar *prefix, GString *in_string);

enum {
	LAST_SIGNAL
};

static gint gda_tree_node_signals[LAST_SIGNAL] = { };

/* properties */
enum {
	PROP_0,
	PROP_NAME
};

static GObjectClass *parent_class = NULL;
GdaAttributesManager *gda_tree_node_attributes_manager;

/*
 * GdaTreeNode class implementation
 * @klass:
 */
static void
gda_tree_node_class_init (GdaTreeNodeClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	/* virtual methods */
	klass->dump_header = gda_tree_node_dump_header;
	klass->dump_children = gda_tree_node_dump_children;

	/* Properties */
        object_class->set_property = gda_tree_node_set_property;
        object_class->get_property = gda_tree_node_get_property;

	g_object_class_install_property (object_class, PROP_NAME,
                                         g_param_spec_string ("name", NULL,
                                                              "Node's name attribute",
                                                              NULL, G_PARAM_WRITABLE | G_PARAM_READABLE));

	object_class->dispose = gda_tree_node_dispose;

	/* extra */
	gda_tree_node_attributes_manager = gda_attributes_manager_new (TRUE, NULL, NULL);
}

static void
gda_tree_node_init (GdaTreeNode *tnode, GdaTreeNodeClass *klass)
{
	g_return_if_fail (GDA_IS_TREE_NODE (tnode));

	tnode->priv = g_new0 (GdaTreeNodePrivate, 1);
	tnode->priv->manager = NULL;
	tnode->priv->children = NULL;
}

static void
gda_tree_node_dispose (GObject *object)
{
	GdaTreeNode *tnode = (GdaTreeNode *) object;

	g_return_if_fail (GDA_IS_TREE_NODE (tnode));

	if (tnode->priv) {
		if (tnode->priv->manager)
			g_object_unref (tnode->priv->manager);
		if (tnode->priv->children) {
			g_slist_foreach (tnode->priv->children, (GFunc) _gda_nodes_list_free, NULL);
			g_slist_free (tnode->priv->children);
		}
		g_free (tnode->priv);
		tnode->priv = NULL;
	}

	/* chain to parent class */
	parent_class->dispose (object);
}


/* module error */
GQuark gda_tree_node_error_quark (void)
{
        static GQuark quark;
        if (!quark)
                quark = g_quark_from_static_string ("gda_tree_node_error");
        return quark;
}

/**
 * gda_tree_node_get_type
 * 
 * Registers the #GdaTreeNode class on the GLib type system.
 * 
 * Returns: the GType identifying the class.
 */
GType
gda_tree_node_get_type (void)
{
        static GType type = 0;

        if (G_UNLIKELY (type == 0)) {
                static GStaticMutex registering = G_STATIC_MUTEX_INIT;
                static const GTypeInfo info = {
                        sizeof (GdaTreeNodeClass),
                        (GBaseInitFunc) NULL,
                        (GBaseFinalizeFunc) NULL,
                        (GClassInitFunc) gda_tree_node_class_init,
                        NULL,
                        NULL,
                        sizeof (GdaTreeNode),
                        0,
                        (GInstanceInitFunc) gda_tree_node_init
                };

                g_static_mutex_lock (&registering);
                if (type == 0)
                        type = g_type_register_static (G_TYPE_OBJECT, "GdaTreeNode", &info, 0);
                g_static_mutex_unlock (&registering);
        }
        return type;
}

static void
gda_tree_node_set_property (GObject *object,
			    guint param_id,
			    const GValue *value,
			    GParamSpec *pspec)
{
	GdaTreeNode *tnode;

        tnode = GDA_TREE_NODE (object);
        if (tnode->priv) {
                switch (param_id) {
		case PROP_NAME:
			gda_attributes_manager_set (gda_tree_node_attributes_manager, tnode, GDA_ATTRIBUTE_NAME, value);
			break;
		}	
	}
}

static void
gda_tree_node_get_property (GObject *object,
			    guint param_id,
			    GValue *value,
			    GParamSpec *pspec)
{
	GdaTreeNode *tnode;
	
	tnode = GDA_TREE_NODE (object);
	if (tnode->priv) {
		switch (param_id) {
		case PROP_NAME: {
			const GValue *cvalue = gda_attributes_manager_get (gda_tree_node_attributes_manager, tnode,
									   GDA_ATTRIBUTE_NAME);
			g_value_set_string (value, cvalue ? g_value_get_string (cvalue): NULL);
			break;
		}
		}
	}	
}

static void
attributes_foreach_func (const gchar *att_name, const GValue *value, GString *string)
{
	gchar *str = gda_value_stringify (value);
	g_string_append_printf (string, " %s=%s", att_name, str);
	g_free (str);
}


static gchar *
gda_tree_node_dump_header (GdaTreeNode *node)
{
	const GValue *cvalue;

	cvalue = gda_attributes_manager_get (gda_tree_node_attributes_manager, node,
					     GDA_ATTRIBUTE_NAME);
#ifdef GDA_DEBUG_NO
	GString *string;
	string = g_string_new ("");
	if (cvalue)
		g_string_append (string, g_value_get_string (cvalue));
	else
		g_string_append (string, "Unnamed node");
	g_string_append_c (string, ':');
	gda_attributes_manager_foreach (gda_tree_node_attributes_manager, node,
					(GdaAttributesManagerFunc) attributes_foreach_func, string);
	return g_string_free (string, FALSE);
#else
	if (cvalue)
		return g_strdup (g_value_get_string (cvalue));
	else
		return g_strdup_printf ("Unnamed node");
#endif
}


static void
gda_tree_node_dump_children (GdaTreeNode *node, const gchar *prefix, GString *in_string)
{
	gchar *prefix2 = "|-- ";
	GdaTreeNodeClass *klass;
	GSList *parts;

	for (parts = node->priv->children; parts; parts = parts->next) { 
		GSList *list;
		GdaTreeNodesList *tn = GDA_TREE_NODES_LIST (parts->data);
		for (list = tn->nodes; list; list = list->next) {
			GdaTreeNode *child_node = GDA_TREE_NODE (list->data);
			klass = (GdaTreeNodeClass*) G_OBJECT_GET_CLASS (child_node);
			
			/* header */
			if ((!list->next) && (!parts->next))
				prefix2 = "`-- ";
			if (klass->dump_header) {
				gchar *tmp = klass->dump_header (child_node);
				g_string_append_printf (in_string, "%s%s%s\n", prefix, prefix2, tmp);
				g_free (tmp);
			}
			else
				g_string_append_printf (in_string, "%s%sGdaTreeNode (%s) %p\n", prefix, prefix2, 
							G_OBJECT_TYPE_NAME(child_node), child_node);
			
			/* contents */
			gchar *ch_prefix;
			if (list->next || parts->next)
				ch_prefix = g_strdup_printf ("%s|   ", prefix);
			else
				ch_prefix = g_strdup_printf ("%s    ", prefix);
			if (klass->dump_children)
				klass->dump_children (child_node, ch_prefix, in_string);
			else
				gda_tree_node_dump_children (child_node, ch_prefix, in_string);
			g_free (ch_prefix);
		}
	}
}

/**
 * gda_tree_node_new
 * @name: a name, or %NULL
 *
 * Creates a new #GdaTreeNode object
 *
 * Returns: a new #GdaTreeNode
 */
GdaTreeNode *
gda_tree_node_new (const gchar *name)
{
	return (GdaTreeNode*) g_object_new (GDA_TYPE_TREE_NODE, "name", name, NULL);
}

/**
 * _gda_tree_node_add_children
 * @node: a #GdaTreeNode
 * @mgr: a #GdaTreeManager object
 * @childen: a list of #GdaTreeNode objects, or %NULL
 *
 * Requests that @node take the list of nodes in @children as its children.
 *
 * @children is used as is and should not be used anymore afterwards by the caller. Moreover @node steals the
 * ownership of each #GdaTreeNode object listed in it.
 */
void
_gda_tree_node_add_children (GdaTreeNode *node, GdaTreeManager *mgr, GSList *children)
{
	GSList *list;
	GdaTreeNodesList *tn, *etn;
	gint pos;
	g_return_if_fail (GDA_IS_TREE_NODE (node));
	g_return_if_fail (GDA_IS_TREE_MANAGER (mgr));

	/* find existing GdaTreeNodesList */
	for (etn = NULL, pos = 0, list = node->priv->children; list; list = list->next, pos++) {
		if (GDA_TREE_NODES_LIST (list->data)->mgr == mgr) {
			etn = GDA_TREE_NODES_LIST (list->data);
			break;
		}
	}

	/* prepare new GdaTreeNodesList */
	for (list = children; list; list = list->next) {
		GdaTreeNode *child = GDA_TREE_NODE (list->data);
		child->priv->parent = node;
	}
	tn = _gda_nodes_list_new (mgr, children);
	
	/* insert into node->priv->children */
	if (etn) {
		g_slist_nth (node->priv->children, pos)->data = tn;
		_gda_nodes_list_free (etn);
	}
	else
		node->priv->children = g_slist_append (node->priv->children, tn);
}

/**
 * _gda_tree_node_get_children_for_manager
 * @node: a #GdaTreeNode
 * @manager: a #GdaTreeManager
 *
 * Get the list of #GdaTreeNode objects which have already bee created by @mgr.
 *
 * Returns: a list (which must not be modified), or %NULL
 */
const GSList *
_gda_tree_node_get_children_for_manager (GdaTreeNode *node, GdaTreeManager *mgr)
{
	GdaTreeNodesList *etn;
	GSList *list;
	g_return_val_if_fail (GDA_IS_TREE_NODE (node), NULL);
	g_return_val_if_fail (GDA_IS_TREE_MANAGER (mgr), NULL);
	
	for (etn = NULL, list = node->priv->children; list; list = list->next) {
		if (GDA_TREE_NODES_LIST (list->data)->mgr == mgr)
			return GDA_TREE_NODES_LIST (list->data)->nodes;
	}
	return NULL;
}

/**
 * _gda_tree_node_get_managers_for_children
 */
GSList *
_gda_tree_node_get_managers_for_children (GdaTreeNode *node)
{
	GSList *list, *mgrlist = NULL;
	g_return_val_if_fail (GDA_IS_TREE_NODE (node), NULL);
	
	for (list = node->priv->children; list; list = list->next)
		mgrlist = g_slist_prepend (mgrlist, GDA_TREE_NODES_LIST (list->data)->mgr);
	
	return g_slist_reverse (mgrlist);
}

/**
 * gda_tree_node_fetch_attribute
 * @node: a #GdaTreeNode
 * @attribute: attribute name as a string
 *
 * Get the value associated to the attribute named @attribute for @node. If the attribute is not set,
 * then @node's parents is queries (recursively up to the top level node).
 *
 * Attributes can have any name, but Libgda proposes some default names,
 * see <link linkend="libgda-40-Attributes-manager.synopsis">this section</link>.
 *
 * Returns: a read-only #GValue, or %NULL if not attribute named @attribute has been set for @node
 */
const GValue *
gda_tree_node_fetch_attribute (GdaTreeNode *node, const gchar *attribute)
{
	const GValue *cvalue;
	g_return_val_if_fail (GDA_IS_TREE_NODE (node), NULL);

	cvalue = gda_attributes_manager_get (gda_tree_node_attributes_manager, node, attribute);
	if (!cvalue && node->priv->parent)
		cvalue = gda_tree_node_fetch_attribute (node->priv->parent, attribute);
	return cvalue;
}

/**
 * gda_tree_node_get_node_attribute
 * @node: a #GdaTreeNode
 * @attribute: attribute name as a string
 *
 * Get the value associated to the attribute named @attribute for @node. The difference with gda_tree_node_fetch_attribute()
 * is that gda_tree_node_fetch_attribute() will also query @node's parents (recursively up to the top level node) if
 * the attribute is not set for @node.
 *
 * Attributes can have any name, but Libgda proposes some default names, 
 * see <link linkend="libgda-40-Attributes-manager.synopsis">this section</link>.
 *
 * Returns: a read-only #GValue, or %NULL if not attribute named @attribute has been set for @node
 */
const GValue *
gda_tree_node_get_node_attribute (GdaTreeNode *node, const gchar *attribute)
{
	g_return_val_if_fail (GDA_IS_TREE_NODE (node), NULL);

	return gda_attributes_manager_get (gda_tree_node_attributes_manager, node, attribute);
}

/**
 * gda_tree_node_set_node_attribute
 * @node: a #GdaTreeNode
 * @attribute: attribute name
 * @value: a #GValue, or %NULL
 * @destroy: a function to be called when @attribute is not needed anymore, or %NULL
 *
 * Set the value associated to a named attribute. The @attribute string is 'stolen' by this method, and
 * the memory it uses will be freed using the @destroy function when no longer needed (if @destroy is %NULL,
 * then the string will not be freed at all).
 *
 * Attributes can have any name, but Libgda proposes some default names, 
 * see <link linkend="libgda-40-Attributes-manager.synopsis">this section</link>.
 *
 * For example one would use it as:
 *
 * <code>
 * gda_tree_node_set_node_attribute (node, g_strdup (my_attribute), g_free, my_value);
 * gda_tree_node_set_node_attribute (node, GDA_ATTRIBUTE_NAME, NULL, my_value);
 * </code>
 *
 * If there is already an attribute named @attribute set, then its value is replaced with the new value (@value is
 * copied), except if @value is %NULL, in which case the attribute is removed.
 */
void
gda_tree_node_set_node_attribute (GdaTreeNode *node, const gchar *attribute, const GValue *value, GDestroyNotify destroy)
{
	const GValue *cvalue;
	g_return_if_fail (GDA_IS_TREE_NODE (node));

	cvalue = gda_attributes_manager_get (gda_tree_node_attributes_manager, node, attribute);
	if ((value && cvalue && !gda_value_differ (cvalue, value)) ||
	    (!value && !cvalue))
		return;

	gda_attributes_manager_set_full (gda_tree_node_attributes_manager, node, attribute, value, destroy);
}

/**
 * gda_tree_node_get_children
 * @node: a #GdaTreeNode object
 *
 * Get a list of all @node's children, free it with g_slist_free() after usage
 *
 * Returns: a new #GSList of #GdaTreeNode objects, or %NULL if @node does not have any child
 */
GSList *
gda_tree_node_get_children (GdaTreeNode *node)
{
	g_return_val_if_fail (GDA_IS_TREE_NODE (node), NULL);
	if (node->priv->children) {
		GSList *parts, *list = NULL;
		
		for (parts = node->priv->children; parts; parts = parts->next) {
			if (GDA_TREE_NODES_LIST (parts->data)->nodes)
				list = g_slist_concat (list, g_slist_copy (GDA_TREE_NODES_LIST (parts->data)->nodes));
		}
		return list;
	}
	else
		return NULL;
}

/**
 * gda_tree_node_get_child_index
 * @node: a #GdaTreeNode object
 * @index: a index
 *
 * Get the #GdaTreeNode child of @node at position @index (starting at 0).
 *
 * Returns: the #GdaTreeNode, or %NULL if not found
 */
GdaTreeNode *
gda_tree_node_get_child_index (GdaTreeNode *node, gint index)
{
	g_return_val_if_fail (GDA_IS_TREE_NODE (node), NULL);
	g_return_val_if_fail (index >= 0, NULL);

	if (node->priv->children) {
		gint i = index;
		GSList *parts;
		for (parts = node->priv->children; parts; parts = parts->next) {
			GdaTreeNodesList *tn = GDA_TREE_NODES_LIST (parts->data);
			if (index < tn->length)
				return g_slist_nth_data (tn->nodes, index);
			i -= tn->length;
		}
		return NULL;
	}
	else
		return NULL;
}

/**
 * gda_tree_node_get_child_name
 * @node: a #GdaTreeNode object
 * @name: requested node's name
 *
 * Get the #GdaTreeNode child of @node which has the #GDA_ATTRIBUTE_NAME set to @name
 *
 * Returns: the #GdaTreeNode, or %NULL if not found
 */
GdaTreeNode *
gda_tree_node_get_child_name (GdaTreeNode *node, const gchar *name)
{
	g_return_val_if_fail (GDA_IS_TREE_NODE (node), NULL);
	g_return_val_if_fail (name, NULL);

	if (node->priv->children) {
		GSList *parts;
		for (parts = node->priv->children; parts; parts = parts->next) {
			GdaTreeNodesList *tn = GDA_TREE_NODES_LIST (parts->data);
			GSList *list;
			for (list = tn->nodes; list; list = list->next) {
				const GValue *cvalue;
				cvalue = gda_attributes_manager_get (gda_tree_node_attributes_manager, list->data,
								     GDA_ATTRIBUTE_NAME);
				if (cvalue) {
					const gchar *cname = g_value_get_string (cvalue);
					if (cname && !strcmp (name, cname))
						return (GdaTreeNode*) list->data;
				}
			}
		}
	}
	return NULL;
}




/*
 * - increase @mgr's ref count
 * - steals @nodes and the ownenship of each #GdaTreeNode listed in it
 */
GdaTreeNodesList *
_gda_nodes_list_new (GdaTreeManager *mgr, GSList *nodes)
{
	GdaTreeNodesList *nl;

	g_assert (mgr);
	nl = g_new0 (GdaTreeNodesList, 1);
	nl->mgr = g_object_ref (mgr);
	if (nodes) {
		nl->nodes = nodes;
		nl->length = g_slist_length (nodes);
	}
	return nl;
}

void
_gda_nodes_list_free (GdaTreeNodesList *nl)
{
	if (nl->nodes) {
		g_slist_foreach (nl->nodes, (GFunc) g_object_unref, NULL);
		g_slist_free (nl->nodes);
		g_object_unref (nl->mgr);
	}
	g_free (nl);
}
