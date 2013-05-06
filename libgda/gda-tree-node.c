/*
 * Copyright (C) 2009 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
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

#include <string.h>
#include "gda-tree-node.h"
#include "gda-tree-manager.h"
#include <libgda/gda-attributes-manager.h>
#include <libgda/gda-value.h>

struct _GdaTreeNodePrivate {
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
	NODE_CHANGED,
	NODE_INSERTED,
	NODE_HAS_CHILD_TOGGLED,
	NODE_DELETED,
	LAST_SIGNAL
};

static gint gda_tree_node_signals[LAST_SIGNAL] = { 0, 0, 0, 0 };

/* properties */
enum {
	PROP_0,
	PROP_NAME
};

static GObjectClass *parent_class = NULL;
GdaAttributesManager *_gda_tree_node_attributes_manager;

static void m_node_changed (GdaTreeNode *reporting, GdaTreeNode *node);
static void m_node_inserted (GdaTreeNode *reporting, GdaTreeNode *node);
static void m_node_has_child_toggled (GdaTreeNode *reporting, GdaTreeNode *node);
static void m_node_deleted (GdaTreeNode *reporting, const gchar *relative_path);

/*
 * GdaTreeNode class implementation
 * @klass:
 */
static void
gda_tree_node_class_init (GdaTreeNodeClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	/* signals */
	/**
	 * GdaTreeNode::node-changed:
	 * @reporting: the #GdaTreeNode which emits the signal (may be a parent of @node, or @node itself)
	 * @node: the #GdaTreeNode which has changed
	 *
	 * Gets emitted when a @node has changed
	 *
	 * Since: 4.2
	 */
	gda_tree_node_signals[NODE_CHANGED] =
		g_signal_new ("node_changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaTreeNodeClass, node_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, GDA_TYPE_TREE_NODE);
	/**
	 * GdaTreeNode::node-inserted:
	 * @reporting: the #GdaTreeNode which emits the signal (may be a parent of @node, or @node itself)
	 * @node: the #GdaTreeNode which has been inserted
	 *
	 * Gets emitted when a @node has been inserted
	 *
	 * Since: 4.2
	 */
	gda_tree_node_signals[NODE_INSERTED] =
		g_signal_new ("node_inserted",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaTreeNodeClass, node_inserted),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, GDA_TYPE_TREE_NODE);
	/**
	 * GdaTreeNode::node-has-child-toggled:
	 * @reporting: the #GdaTreeNode which emits the signal (may be a parent of @node, or @node itself)
	 * @node: the #GdaTreeNode which changed from having children to being a
	 *        leaf or the other way around
	 *
	 * Gets emitted when a @node has has a child when it did not have any or when it
	 * does not have a ny children anymore when it had some
	 *
	 * Since: 4.2
	 */
	gda_tree_node_signals[NODE_HAS_CHILD_TOGGLED] =
		g_signal_new ("node-has-child-toggled",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaTreeNodeClass, node_has_child_toggled),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, GDA_TYPE_TREE_NODE);
	/**
	 * GdaTreeNode::node-deleted:
	 * @reporting: the #GdaTreeNode which emits the signal (a parent of the removed node)
	 * @relative_path: the path the node held, relative to @reporting
	 *
	 * Gets emitted when a @node has been removed
	 *
	 * Since: 4.2
	 */
	gda_tree_node_signals[NODE_DELETED] =
		g_signal_new ("node_deleted",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaTreeNodeClass, node_deleted),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);

	klass->node_changed = m_node_changed;
	klass->node_inserted = m_node_inserted;
	klass->node_has_child_toggled = m_node_has_child_toggled;
	klass->node_deleted = m_node_deleted;

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
	_gda_tree_node_attributes_manager = gda_attributes_manager_new (TRUE, NULL, NULL);
}

static void
m_node_changed (GdaTreeNode *reporting, GdaTreeNode *node)
{
	if (reporting->priv->parent)
		g_signal_emit (reporting->priv->parent, gda_tree_node_signals[NODE_CHANGED],
			       0, node);
}

static void
m_node_inserted (GdaTreeNode *reporting, GdaTreeNode *node)
{
	if (reporting->priv->parent)
		g_signal_emit (reporting->priv->parent, gda_tree_node_signals[NODE_INSERTED],
			       0, node);
}

static void
m_node_has_child_toggled (GdaTreeNode *reporting, GdaTreeNode *node)
{
	if (reporting->priv->parent)
		g_signal_emit (reporting->priv->parent, gda_tree_node_signals[NODE_HAS_CHILD_TOGGLED],
			       0, node);
}

/*
 * Get child's position
 *
 * Returns: the requested position, or -1 if child not found 
 */
static gint
_get_child_pos (GdaTreeNode *node, GdaTreeNode *child)
{
	gint pos = 0;
	g_return_val_if_fail (GDA_IS_TREE_NODE (node), -1);

	if (node->priv->children) {
		GSList *parts;
		for (parts = node->priv->children; parts; parts = parts->next) {
			GdaTreeNodesList *tn = GDA_TREE_NODES_LIST (parts->data);
			GSList *sl;
			for (sl = tn->nodes; sl; sl = sl->next) {
				if (sl->data == child)
					return pos;
				else
					pos++;
			}
		}
	}

	return -1;
}

static void
m_node_deleted (GdaTreeNode *reporting, const gchar *relative_path)
{
	if (reporting->priv->parent) {
		gint pos;
		gchar *path;
		pos = _get_child_pos (reporting->priv->parent, reporting);
		g_assert (pos >= 0);
		path = g_strdup_printf ("%d:%s", pos, relative_path);
		g_signal_emit (reporting->priv->parent, gda_tree_node_signals[NODE_DELETED],
			       0, path);
		g_free (path);
	}
}

static void
gda_tree_node_init (GdaTreeNode *tnode, G_GNUC_UNUSED GdaTreeNodeClass *klass)
{
	g_return_if_fail (GDA_IS_TREE_NODE (tnode));

	tnode->priv = g_new0 (GdaTreeNodePrivate, 1);
	tnode->priv->children = NULL;
}

static void
gda_tree_node_dispose (GObject *object)
{
	GdaTreeNode *tnode = (GdaTreeNode *) object;

	g_return_if_fail (GDA_IS_TREE_NODE (tnode));

	if (tnode->priv) {
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
 * gda_tree_node_get_type:
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
                static GMutex registering;
                static const GTypeInfo info = {
                        sizeof (GdaTreeNodeClass),
                        (GBaseInitFunc) NULL,
                        (GBaseFinalizeFunc) NULL,
                        (GClassInitFunc) gda_tree_node_class_init,
                        NULL,
                        NULL,
                        sizeof (GdaTreeNode),
                        0,
                        (GInstanceInitFunc) gda_tree_node_init,
			0
                };

                g_mutex_lock (&registering);
                if (type == 0)
                        type = g_type_register_static (G_TYPE_OBJECT, "GdaTreeNode", &info, 0);
                g_mutex_unlock (&registering);
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
			gda_attributes_manager_set (_gda_tree_node_attributes_manager, tnode, GDA_ATTRIBUTE_NAME, value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
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
			const GValue *cvalue = gda_attributes_manager_get (_gda_tree_node_attributes_manager, tnode,
									   GDA_ATTRIBUTE_NAME);
			g_value_set_string (value, cvalue ? g_value_get_string (cvalue): NULL);
			break;
		}
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
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

	cvalue = gda_attributes_manager_get (_gda_tree_node_attributes_manager, node,
					     GDA_ATTRIBUTE_NAME);
	if (g_getenv ("GDA_TREE_DUMP_ALL_ATTRIBUTES")) {
		GString *string;
		string = g_string_new ("");
		if (cvalue && (G_VALUE_TYPE (cvalue) == G_TYPE_STRING))
			g_string_append (string, g_value_get_string (cvalue));
		else
			g_string_append (string, "Unnamed node");
		g_string_append_c (string, ':');
		gda_attributes_manager_foreach (_gda_tree_node_attributes_manager, node,
						(GdaAttributesManagerFunc) attributes_foreach_func, string);
		return g_string_free (string, FALSE);
	}
	else {
		if (cvalue)
			return g_strdup (g_value_get_string (cvalue));
		else
			return g_strdup_printf ("Unnamed node");
	}
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
 * gda_tree_node_new:
 * @name: (allow-none): a name, or %NULL
 *
 * Creates a new #GdaTreeNode object
 *
 * Returns: (transfer full): a new #GdaTreeNode
 *
 * Since: 4.2
 */
GdaTreeNode *
gda_tree_node_new (const gchar *name)
{
	return (GdaTreeNode*) g_object_new (GDA_TYPE_TREE_NODE, "name", name, NULL);
}

/*
 * _gda_tree_node_add_children:
 * @node: a #GdaTreeNode
 * @mgr: a #GdaTreeManager object
 * @childen: (allow-none): a list of #GdaTreeNode objects, or %NULL
 *
 * Requests that @node take the list of nodes in @children as its children. The children list
 * may be a new list of children, or may be an update of a list of children which previously existed.
 *
 * @node steals the ownership of each #GdaTreeNode object listed in @children.
 */
void
_gda_tree_node_add_children (GdaTreeNode *node, GdaTreeManager *mgr, const GSList *children)
{
	GSList *list;
	const GSList *clist;
	GdaTreeNodesList *etn;
	gint pos, nb_before_etn = 0;
	gint n_children = 0;

	g_return_if_fail (GDA_IS_TREE_NODE (node));
	g_return_if_fail (GDA_IS_TREE_MANAGER (mgr));

	/* find existing GdaTreeNodesList */
	for (etn = NULL, pos = 0, list = node->priv->children; list; list = list->next, pos++) {
		if (GDA_TREE_NODES_LIST (list->data)->mgr == mgr) {
			etn = GDA_TREE_NODES_LIST (list->data);
			nb_before_etn = n_children;
			n_children += GDA_TREE_NODES_LIST (list->data)->length;
		}
		else
			n_children += GDA_TREE_NODES_LIST (list->data)->length;
	}

	/* prepare new GdaTreeNodesList */
	for (clist = children; clist; clist = clist->next) {
		GdaTreeNode *child = GDA_TREE_NODE (clist->data);
		child->priv->parent = node;
	}
	if (!etn) {
		/* create new GdaTreeNodesList */
		etn = g_new0 (GdaTreeNodesList, 1);
		etn->mgr = g_object_ref (mgr);
		node->priv->children = g_slist_append (node->priv->children, etn);

		/* fill it */
		for (clist = children; clist; clist = clist->next) {
			etn->nodes = g_slist_append (etn->nodes, clist->data);
			etn->length ++;
			n_children ++;
			g_signal_emit (node, gda_tree_node_signals [NODE_INSERTED], 0, clist->data);
			if (n_children == 1)
				g_signal_emit (node, gda_tree_node_signals [NODE_HAS_CHILD_TOGGLED], 0, node);
		}
	}
	else {
		GSList *enodes;

		for (clist = children, enodes = etn->nodes, pos = nb_before_etn;
		     clist;
		     clist = clist->next, pos ++) {
			if (enodes) {
				if (clist->data == enodes->data) {
					/* signal a change */
					g_signal_emit (node, gda_tree_node_signals [NODE_CHANGED], 0, (gpointer) clist->data);
					g_object_unref (G_OBJECT (clist->data)); /* loose 1 reference */
					enodes = enodes->next;
				}
				else {
					gchar *rpath = g_strdup_printf ("%d", pos);
					for (; enodes; ) {
						if (clist->data != enodes->data) {
							/* signal deletion */
							GSList *next = enodes->next;
							g_object_unref (G_OBJECT (enodes->data));
							etn->nodes = g_slist_delete_link (etn->nodes, enodes);
							etn->length --;
							n_children --;
							enodes = next;
							g_signal_emit (node, gda_tree_node_signals [NODE_DELETED], 0, rpath);
							
							if (n_children == 0)
								g_signal_emit (node, gda_tree_node_signals [NODE_HAS_CHILD_TOGGLED], 0, node);
						}
						else
							break;
					}
					g_free (rpath);

					if (enodes) {
						/* signal a change */
						g_signal_emit (node, gda_tree_node_signals [NODE_CHANGED], 0,
							       (gpointer) clist->data);
						g_object_unref (G_OBJECT (clist->data)); /* loose 1 reference */
						enodes = enodes->next;
					}
					else {
						/* signal an insertion */
						etn->nodes = g_slist_append (etn->nodes, clist->data);
						etn->length ++;
						n_children ++;
						g_signal_emit (node, gda_tree_node_signals [NODE_INSERTED], 0,
							       (gpointer) clist->data);
						if (n_children == 1)
							g_signal_emit (node, gda_tree_node_signals [NODE_HAS_CHILD_TOGGLED], 0, node);
					}
				}
			}
			else {
				/* signal an insertion */
				etn->nodes = g_slist_append (etn->nodes, clist->data);
				etn->length ++;
				n_children ++;
				g_signal_emit (node, gda_tree_node_signals [NODE_INSERTED], 0, (gpointer) clist->data);
				if (n_children == 1)
					g_signal_emit (node, gda_tree_node_signals [NODE_HAS_CHILD_TOGGLED], 0, node);
			}
		}
		if (enodes) {
			gchar *rpath;
			rpath = g_strdup_printf ("%d", pos);
			for (; enodes; ) {
				/* signal deletion */
				GSList *next = enodes->next;
				g_object_unref (G_OBJECT (enodes->data));
				etn->nodes = g_slist_delete_link (etn->nodes, enodes);
				etn->length --;
				n_children --;
				enodes = next;
				g_signal_emit (node, gda_tree_node_signals [NODE_DELETED], 0, rpath);
				if (n_children == 0)
					g_signal_emit (node, gda_tree_node_signals [NODE_HAS_CHILD_TOGGLED], 0, node);
			}
			g_free (rpath);
		}
	}
}

/*
 * _gda_tree_node_get_children_for_manager:
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
	GSList *list;
	g_return_val_if_fail (GDA_IS_TREE_NODE (node), NULL);
	g_return_val_if_fail (GDA_IS_TREE_MANAGER (mgr), NULL);
	
	for (list = node->priv->children; list; list = list->next) {
		if (GDA_TREE_NODES_LIST (list->data)->mgr == mgr)
			return GDA_TREE_NODES_LIST (list->data)->nodes;
	}
	return NULL;
}

/*
 * _gda_tree_node_get_managers_for_children:
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

/*
 * _gda_tree_node_get_manager_for_child:
 */
GdaTreeManager *
_gda_tree_node_get_manager_for_child (GdaTreeNode *node, GdaTreeNode *child)
{
	GSList *list;
	g_return_val_if_fail (GDA_IS_TREE_NODE (node), NULL);
	g_return_val_if_fail (GDA_IS_TREE_NODE (child), NULL);
	g_return_val_if_fail (child->priv->parent == node, NULL);
	
	for (list = node->priv->children; list; list = list->next)
		if (g_slist_find (GDA_TREE_NODES_LIST (list->data)->nodes, child))
			return GDA_TREE_NODES_LIST (list->data)->mgr;
	
	return NULL;
}

/**
 * gda_tree_node_fetch_attribute:
 * @node: a #GdaTreeNode
 * @attribute: attribute name as a string
 *
 * Get the value associated to the attribute named @attribute for @node. If the attribute is not set,
 * then @node's parents is queries (recursively up to the top level node).
 *
 * Attributes can have any name, but Libgda proposes some default names,
 * see <link linkend="libgda-40-Attributes-manager.synopsis">this section</link>.
 *
 * Returns: (transfer none): a read-only #GValue, or %NULL if not attribute named @attribute has been set for @node
 *
 * Since: 4.2
 */
const GValue *
gda_tree_node_fetch_attribute (GdaTreeNode *node, const gchar *attribute)
{
	const GValue *cvalue;
	g_return_val_if_fail (GDA_IS_TREE_NODE (node), NULL);

	cvalue = gda_attributes_manager_get (_gda_tree_node_attributes_manager, node, attribute);
	if (!cvalue && node->priv->parent)
		cvalue = gda_tree_node_fetch_attribute (node->priv->parent, attribute);
	return cvalue;
}

/**
 * gda_tree_node_get_node_attribute:
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
 * Returns: (transfer none): a read-only #GValue, or %NULL if not attribute named @attribute has been set for @node
 *
 * Since: 4.2
 */
const GValue *
gda_tree_node_get_node_attribute (GdaTreeNode *node, const gchar *attribute)
{
	g_return_val_if_fail (GDA_IS_TREE_NODE (node), NULL);

	return gda_attributes_manager_get (_gda_tree_node_attributes_manager, node, attribute);
}

/**
 * gda_tree_node_set_node_attribute:
 * @node: a #GdaTreeNode
 * @attribute: attribute name
 * @value: (transfer none) (allow-none): a #GValue, or %NULL
 * @destroy: a function to be called when @attribute is not needed anymore, or %NULL
 *
 * Set the value associated to a named attribute. The @attribute string is used AS IT IS by this method (eg.
 * no copy of it is made), and
 * the memory it uses will be freed using the @destroy function when no longer needed (if @destroy is %NULL,
 * then the string will not be freed at all).
 *
 * Attributes can have any name, but Libgda proposes some default names, 
 * see <link linkend="libgda-40-Attributes-manager.synopsis">this section</link>.
 *
 * For example one would use it as:
 *
 * <code>
 * gda_tree_node_set_node_attribute (node, g_strdup (my_attribute), my_value, g_free);
 * gda_tree_node_set_node_attribute (node, GDA_ATTRIBUTE_NAME, my_value, NULL);
 * </code>
 *
 * If there is already an attribute named @attribute set, then its value is replaced with the new value (@value is
 * copied), except if @value is %NULL, in which case the attribute is removed.
 *
 * Since: 4.2
 */
void
gda_tree_node_set_node_attribute (GdaTreeNode *node, const gchar *attribute, const GValue *value, GDestroyNotify destroy)
{
	const GValue *cvalue;
	g_return_if_fail (GDA_IS_TREE_NODE (node));
	g_return_if_fail (attribute);

	cvalue = gda_attributes_manager_get (_gda_tree_node_attributes_manager, node, attribute);
	if ((value && cvalue && !gda_value_differ (cvalue, value)) ||
	    (!value && !cvalue))
		return;

	if (!strcmp (attribute, GDA_ATTRIBUTE_TREE_NODE_UNKNOWN_CHILDREN) &&
	    (!value || (G_VALUE_TYPE (value) == G_TYPE_BOOLEAN))) {
		gboolean ouc = FALSE;
		gboolean nuc = FALSE;
		if (cvalue && (G_VALUE_TYPE (cvalue) == G_TYPE_BOOLEAN) &&
		    g_value_get_boolean (cvalue))
			ouc = TRUE;

		if (value && g_value_get_boolean (value))
			nuc = TRUE;

		if (ouc != nuc) {
			gda_attributes_manager_set_full (_gda_tree_node_attributes_manager, node,
							 attribute, value, destroy);
			g_signal_emit (node, gda_tree_node_signals[NODE_HAS_CHILD_TOGGLED], 0, node);
			g_signal_emit (node, gda_tree_node_signals[NODE_CHANGED], 0, node);
			return;
		}
	}

	gda_attributes_manager_set_full (_gda_tree_node_attributes_manager, node, attribute,
					 value, destroy);
	g_signal_emit (node, gda_tree_node_signals[NODE_CHANGED], 0, node);
}

/**
 * gda_tree_node_get_parent:
 * @node: a #GdaTreeNode object
 *
 * Get the #GdaTreeNode parent of @node in the #GdaTree node belongs to. If @node is at the top level,
 * then this method return %NULL.
 *
 * Returns: (transfer none): the parent #GdaTreeNode
 *
 * Since: 4.2
 */
GdaTreeNode *
gda_tree_node_get_parent (GdaTreeNode *node)
{
	GdaTreeNode *parent;
	g_return_val_if_fail (GDA_IS_TREE_NODE (node), NULL);
	parent = node->priv->parent;

	if (parent && !parent->priv->parent)
		return NULL; /* avoid returning the private GdaTree's ROOT node */
	else
		return parent;
}

/**
 * gda_tree_node_get_children:
 * @node: a #GdaTreeNode object
 *
 * Get a list of all @node's children, free it with g_slist_free() after usage
 *
 * Returns: (transfer container) (element-type GdaTreeNode): a new #GSList of #GdaTreeNode objects, or %NULL if @node does not have any child
 *
 * Since: 4.2
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
 * gda_tree_node_get_child_index:
 * @node: a #GdaTreeNode object
 * @index: a index
 *
 * Get the #GdaTreeNode child of @node at position @index (starting at 0).
 *
 * Returns: (transfer none): the #GdaTreeNode, or %NULL if not found
 *
 * Since: 4.2
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
			if (i < tn->length)
				return g_slist_nth_data (tn->nodes, i);
			i -= tn->length;
		}
		return NULL;
	}
	else
		return NULL;
}

/**
 * gda_tree_node_get_child_name:
 * @node: a #GdaTreeNode object
 * @name: requested node's name
 *
 * Get the #GdaTreeNode child of @node which has the #GDA_ATTRIBUTE_NAME set to @name
 *
 * Returns: (transfer none): the #GdaTreeNode, or %NULL if not found
 *
 * Since: 4.2
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
				cvalue = gda_attributes_manager_get (_gda_tree_node_attributes_manager, list->data,
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

void
_gda_nodes_list_free (GdaTreeNodesList *nl)
{
	if (nl->nodes) {
		g_slist_foreach (nl->nodes, (GFunc) g_object_unref, NULL);
		g_slist_free (nl->nodes);
	}
	g_object_unref (nl->mgr);
	g_free (nl);
}
