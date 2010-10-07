/* GDA library
 * Copyright (C) 2009 - 2010 The GNOME Foundation.
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
#include <stdlib.h>
#include <glib/gprintf.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-attributes-manager.h>
#include "gda-tree.h"
#include "gda-tree-manager.h"
#include "gda-tree-node.h"

struct _GdaTreePrivate {
	GSList      *managers; /* list of GdaTreeManager */
	GdaTreeNode *root;

	gboolean     update_on_searching; /* set to FALSE if GdaTree's contents is supposed to be constant
					   * needs a PROPRERTY because it's now a constant, or even
					   * maybe move it to each GdaTreeManager */
};

static void gda_tree_class_init (GdaTreeClass *klass);
static void gda_tree_init       (GdaTree *tree, GdaTreeClass *klass);
static void gda_tree_dispose    (GObject *object);
static void gda_tree_set_property (GObject *object,
				   guint param_id,
				   const GValue *value,
				   GParamSpec *pspec);
static void gda_tree_get_property (GObject *object,
				   guint param_id,
				   GValue *value,
				   GParamSpec *pspec);

static gboolean create_or_update_children (GSList *mgrlist, GdaTreeNode *parent, gboolean disable_recurs, GError **error);
static void node_changed_cb (GdaTreeNode *reporting, GdaTreeNode *node, GdaTree *tree);
static void node_inserted_cb (GdaTreeNode *reporting, GdaTreeNode *node, GdaTree *tree);
static void node_has_child_toggled_cb (GdaTreeNode *reporting, GdaTreeNode *node, GdaTree *tree);
static void node_deleted_cb (GdaTreeNode *reporting, const gchar *relative_path, GdaTree *tree);

static void take_root_node (GdaTree *tree, GdaTreeNode *root);
static void unset_root_node (GdaTree *tree);

enum {
	NODE_CHANGED,
	NODE_INSERTED,
	NODE_HAS_CHILD_TOGGLED,
	NODE_DELETED,
	LAST_SIGNAL
};

static gint gda_tree_signals[LAST_SIGNAL] = { 0, 0, 0, 0 };
extern GdaAttributesManager *gda_tree_node_attributes_manager;

/* properties */
enum {
	PROP_0,
	PROP_IS_LIST
};

static GObjectClass *parent_class = NULL;

/*
 * GdaTree class implementation
 * @klass:
 */
static void
gda_tree_class_init (GdaTreeClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	/* signals */
	/**
	 * GdaTree::node-changed
	 * @tree: the #GdaTree
	 * @node: the #GdaTreeNode which has changed
	 *
	 * Gets emitted when a @node has changed in @tree
	 *
	 * Since: 4.2
	 */
	gda_tree_signals[NODE_CHANGED] =
		g_signal_new ("node_changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaTreeClass, node_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, GDA_TYPE_TREE_NODE);
	/**
	 * GdaTree::node-inserted
	 * @tree: the #GdaTree
	 * @node: the #GdaTreeNode which has inserted
	 *
	 * Gets emitted when a @node has been inserted in @tree
	 *
	 * Since: 4.2
	 */
	gda_tree_signals[NODE_INSERTED] =
		g_signal_new ("node_inserted",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaTreeClass, node_inserted),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, GDA_TYPE_TREE_NODE);
	/**
	 * GdaTree::node-has-child-toggled
	 * @tree: the #GdaTree
	 * @node: the #GdaTreeNode which changed from having children to being a
	 *        leaf or the other way around
	 *
	 * Gets emitted when a @node has has a child when it did not have any or when it
	 * does not have a ny children anymore when it had some
	 *
	 * Since: 4.2
	 */
	gda_tree_signals[NODE_HAS_CHILD_TOGGLED] =
		g_signal_new ("node_has-child-toggled",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaTreeClass, node_has_child_toggled),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, GDA_TYPE_TREE_NODE);
	/**
	 * GdaTree::node-deleted
	 * @tree: the #GdaTree
	 * @node_path: the position the node held in @tree as a tree path
	 *
	 * Gets emitted when a @node has been removed from @tree
	 *
	 * Since: 4.2
	 */
	gda_tree_signals[NODE_DELETED] =
		g_signal_new ("node_deleted",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaTreeClass, node_deleted),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);

	klass->node_changed = NULL;
	klass->node_inserted = NULL;
	klass->node_has_child_toggled = NULL;
	klass->node_deleted = NULL;

	/* Properties */
        object_class->set_property = gda_tree_set_property;
        object_class->get_property = gda_tree_get_property;

	g_object_class_install_property (object_class, PROP_IS_LIST,
                                         g_param_spec_boolean ("is-list", _("Tells if the GdaTree is a list or a tree"), NULL,
							       FALSE, G_PARAM_READABLE));

	object_class->dispose = gda_tree_dispose;
}

static void
gda_tree_init (GdaTree *tree, G_GNUC_UNUSED GdaTreeClass *klass)
{
	g_return_if_fail (GDA_IS_TREE (tree));

	tree->priv = g_new0 (GdaTreePrivate, 1);
	tree->priv->managers = NULL;

	take_root_node (tree, gda_tree_node_new (NULL));

	tree->priv->update_on_searching = FALSE;
}

static void
gda_tree_dispose (GObject *object)
{
	GdaTree *tree = (GdaTree *) object;

	g_return_if_fail (GDA_IS_TREE (tree));

	if (tree->priv) {
		if (tree->priv->root) 
			unset_root_node (tree);
		if (tree->priv->managers) {
			g_slist_foreach (tree->priv->managers, (GFunc) g_object_unref, NULL);
			g_slist_free (tree->priv->managers);
		}
		g_free (tree->priv);
		tree->priv = NULL;
	}

	/* chain to parent class */
	parent_class->dispose (object);
}


/* module error */
GQuark gda_tree_error_quark (void)
{
        static GQuark quark;
        if (!quark)
                quark = g_quark_from_static_string ("gda_tree_error");
        return quark;
}

/**
 * gda_tree_get_type:
 * 
 * Registers the #GdaTree class on the GLib type system.
 * 
 * Returns: the GType identifying the class.
 *
 * Since: 4.2
 */
GType
gda_tree_get_type (void)
{
        static GType type = 0;

        if (G_UNLIKELY (type == 0)) {
                static GStaticMutex registering = G_STATIC_MUTEX_INIT;
                static const GTypeInfo info = {
                        sizeof (GdaTreeClass),
                        (GBaseInitFunc) NULL,
                        (GBaseFinalizeFunc) NULL,
                        (GClassInitFunc) gda_tree_class_init,
                        NULL,
                        NULL,
                        sizeof (GdaTree),
                        0,
                        (GInstanceInitFunc) gda_tree_init,
			0
                };

                g_static_mutex_lock (&registering);
                if (type == 0)
                        type = g_type_register_static (G_TYPE_OBJECT, "GdaTree", &info, 0);
                g_static_mutex_unlock (&registering);
        }
        return type;
}

static void
gda_tree_set_property (GObject *object,
			 guint param_id,
			 G_GNUC_UNUSED const GValue *value,
			 G_GNUC_UNUSED GParamSpec *pspec)
{
	GdaTree *tree;

        tree = GDA_TREE (object);
        if (tree->priv) {
                switch (param_id) {
		}	
	}
}

static void
gda_tree_get_property (GObject *object,
			 guint param_id,
			 GValue *value,
			 G_GNUC_UNUSED GParamSpec *pspec)
{
	GdaTree *tree;
	
	tree = GDA_TREE (object);
	if (tree->priv) {
		switch (param_id) {
		case PROP_IS_LIST: {
			GSList *list;
			gboolean is_list = TRUE;
			for (list = tree->priv->managers; list; list = list->next) {
				if (gda_tree_manager_get_managers ((GdaTreeManager*) list->data)) {
					is_list = FALSE;
					break;
				}
			}
			g_value_set_boolean (value, is_list);
			break;
		}
		}
	}	
}

/**
 * gda_tree_new:
 * 
 * Creates a new #GdaTree object
 *
 * Returns: (transfer full): a new #GdaTree object
 *
 * Since: 4.2
 */
GdaTree*
gda_tree_new (void)
{
	return (GdaTree*) g_object_new (GDA_TYPE_TREE, NULL);
}


/**
 * gda_tree_add_manager:
 * @tree: a #GdaTree object
 * @manager: a #GdaTreeManager object
 * 
 * Sets @manager as a top #GdaTreeManager object, which will be responsible for creating top level nodes in @tree.
 *
 * Since: 4.2
 */
void
gda_tree_add_manager (GdaTree *tree, GdaTreeManager *manager)
{
	g_return_if_fail (GDA_IS_TREE (tree));
	g_return_if_fail (GDA_IS_TREE_MANAGER (manager));

	tree->priv->managers = g_slist_append (tree->priv->managers, manager);
	g_object_ref (manager);
}

#ifdef GDA_DEBUG_NO
static void
dump_attr_foreach_func (const gchar *att_name, const GValue *value, gpointer data)
{
	g_print ("%s ==> %p\n", att_name, value);
}
static void
dump_root_attributes (GdaTreeNode *root)
{
	g_print ("DUMPING attributes for %p\n", root);
	gda_attributes_manager_foreach (gda_tree_node_attributes_manager, root,
					(GdaAttributesManagerFunc) dump_attr_foreach_func, NULL);
}
#endif

/**
 * gda_tree_clean:
 * @tree: a #GdaTree object
 *
 * Removes any node in @tree
 *
 * Since: 4.2
 */
void
gda_tree_clean (GdaTree *tree)
{
	GdaTreeNode *new_root;

	g_return_if_fail (GDA_IS_TREE (tree));
	TO_IMPLEMENT; /* signal changes */

	new_root = gda_tree_node_new (NULL);

	gda_attributes_manager_copy (gda_tree_node_attributes_manager, (gpointer) tree->priv->root,
				     gda_tree_node_attributes_manager, (gpointer) new_root);

	take_root_node (tree, new_root);
}

/**
 * gda_tree_update_all:
 * @tree: a #GdaTree object
 * @error: (allow-none): a place to store errors, or %NULL
 *
 * Requests that @tree be populated with nodes. If an error occurs, then @tree's contents is left
 * unchanged, and otherwise @tree's previous contents is completely replaced by the new one.
 *
 * Returns: TRUE if no error occurred.
 *
 * Since: 4.2
 */
gboolean
gda_tree_update_all (GdaTree *tree, GError **error)
{
	g_return_val_if_fail (GDA_IS_TREE (tree), FALSE);
	return create_or_update_children (tree->priv->managers, tree->priv->root, FALSE, error);	
}

/**
 * gda_tree_update_part:
 * @tree: a #GdaTree object
 * @node: a #GdaTreeNode node in @tree
 * @error: (allow-none): a place to store errors, or %NULL
 *
 * Requests that @tree be populated with nodes, starting from @node
 *
 * Returns: TRUE if no error occurred.
 *
 * Since: 4.2
 */
gboolean
gda_tree_update_part (GdaTree *tree, GdaTreeNode *node, GError **error)
{
	GSList *mgrlist;

	g_return_val_if_fail (GDA_IS_TREE (tree), FALSE);
	g_return_val_if_fail (GDA_IS_TREE_NODE (node), FALSE);
	
	mgrlist = _gda_tree_node_get_managers_for_children (node);

	if (mgrlist) {
		gboolean res;
		res = create_or_update_children (mgrlist, node, FALSE, error);
		g_slist_free (mgrlist);
		return res;
	}
	return TRUE;
}

/**
 * gda_tree_dump:
 * @tree: a #GdaTree
 * @node: a #GdaTreeNode to start the dump from, or %NULL for a full dump
 * @stream: a stream to send the dump to, or %NULL for STDOUT
 *
 * Dumps the contents of @tree to @stream, using a hierarchical view.
 *
 * Since: 4.2
 */
void
gda_tree_dump (GdaTree *tree, GdaTreeNode *node, FILE *stream)
{
	GdaTreeNodeClass *klass;
	GString *string;

	g_return_if_fail (GDA_IS_TREE (tree));

	if (!node)
		node = tree->priv->root;

	string = g_string_new (".\n");
	klass = (GdaTreeNodeClass*) G_OBJECT_GET_CLASS (node);
	klass->dump_children (node, "", string);
	g_fprintf (stream ? stream : stdout, "%s", string->str);
	g_string_free (string, TRUE);
}

static GSList *real_gda_tree_get_nodes_in_path (GdaTree *tree, GSList *segments, gboolean use_names, 
						GdaTreeNode **out_last_node);
static GSList *decompose_path_as_segments (const gchar *path, gboolean use_names);

/**
 * gda_tree_get_nodes_in_path:
 * @tree: a #GdaTree object
 * @tree_path: full path to the required nodes (if @use_names is %TRUE, then it must start with '/')
 * @use_names: if %TRUE, then @tree_path will be interpreted as a unix style path, and if %FALSE,
 *             then @tree_path will be interpreted similarly to the #GtkTreePath's string representation.
 *
 * The returned list is a list of all the #GdaTreeNode nodes <emphasis>below</emphasis> the node
 * at the specified path.
 *
 * As a corner case if @tree_path is %NULL, then the returned list contains all the top level nodes.
 *
 * Returns: (transfer container) (element-type GdaTreeNode): a new list of #GdaTreeNode pointers, free it with g_slist_free()
 *
 * Since: 4.2
 */
GSList *
gda_tree_get_nodes_in_path (GdaTree *tree, const gchar *tree_path, gboolean use_names)
{
	GSList *segments, *nodes;
	
	g_return_val_if_fail (GDA_IS_TREE (tree), NULL);

	if (tree_path) {
		segments = decompose_path_as_segments (tree_path, use_names);
		nodes = real_gda_tree_get_nodes_in_path (tree, segments, use_names, NULL);
		if (segments) {
			g_slist_foreach (segments, (GFunc) g_free, NULL);
			g_slist_free (segments);
		}
	}
	else {
		nodes = gda_tree_node_get_children (tree->priv->root);
#ifdef GDA_DEBUG_NO
		GSList *list;
		g_print ("Top nodes:\n");
		for (list = nodes; list; list = list->next) {
			g_print ("Node %s(%p)\n", 
				 gda_value_stringify (gda_tree_node_fetch_attribute (GDA_TREE_NODE (list->data), GDA_ATTRIBUTE_NAME)),
				 list->data);
		}
#endif
	}
	return nodes;
}

/*
 * if @out_last_node is NULL, then it returns the children of the node pointed by @segments;
 * if @out_last_node is NOT NULL, then it returns NULL and sets @out_last_node to point to the last node encountered
 *    in @segments
 * 
 */
static GSList *
real_gda_tree_get_nodes_in_path (GdaTree *tree, GSList *segments, gboolean use_names, GdaTreeNode **out_last_node)
{
	/* update 1st level if necessary */
	if (tree->priv->update_on_searching)
		create_or_update_children (tree->priv->managers, tree->priv->root, TRUE, NULL);

	if (out_last_node)
		*out_last_node = NULL;

	/* handle the case where no segment (path) is specified */
	if (!segments) {
		if (out_last_node)
			return NULL;
		else
			return gda_tree_node_get_children (tree->priv->root);
	}

	/* get the GdatreeNode for @tree_path */
	GSList *seglist;
	GdaTreeNode *node;
	GdaTreeNode *parent;
	GSList *mgrlist;
	for (seglist = segments, parent = tree->priv->root;
	     seglist;
	     seglist = seglist->next, parent = node) {
		if (use_names)
			node = gda_tree_node_get_child_name (parent, (gchar *) seglist->data);
		else
			node = gda_tree_node_get_child_index (parent, atoi ((gchar *) seglist->data));
		if (!node && tree->priv->update_on_searching) {
			/* update level if necessary */
			mgrlist = _gda_tree_node_get_managers_for_children (parent);

			if (mgrlist) {
				create_or_update_children (mgrlist, parent, TRUE, NULL);
				g_slist_free (mgrlist);
			}

			/* try again now */
			if (use_names)
				node = gda_tree_node_get_child_name (parent, (gchar *) seglist->data);
			else
				node = gda_tree_node_get_child_index (parent, atoi ((gchar *) seglist->data));
		}
		if (!node) 
			return NULL;
	}

	if (tree->priv->update_on_searching) {
		mgrlist = _gda_tree_node_get_managers_for_children (node);
		if (mgrlist) {
			create_or_update_children (mgrlist, node, TRUE, NULL);
			g_slist_free (mgrlist);
		}
	}
	if (out_last_node) {
		*out_last_node = node;
		return NULL;
	}
	else
		return gda_tree_node_get_children (node);
}

static gboolean build_node_path (GdaTree *tree, GdaTreeNode *node, GArray *array);

/**
 * gda_tree_get_node_path:
 * @tree: a #GdaTree
 * @node: a #GdaTreeNode node in @tree
 *
 * Get the path associated to @node in @tree.
 *
 * Returns: (transfer full): a new string, or %NULL if @node is not in @tree
 *
 * Since: 4.2
 */
gchar *
gda_tree_get_node_path (GdaTree *tree, GdaTreeNode *node)
{
	GArray *array;
	gchar *str = NULL;
	g_return_val_if_fail (GDA_IS_TREE (tree), NULL);
	g_return_val_if_fail (GDA_IS_TREE_NODE (node), NULL);

	if (!tree->priv->root)
		return NULL;

	array = g_array_new (TRUE, FALSE, sizeof (gchar*));
	if (build_node_path (tree, node, array))
		str = g_strjoinv (":", (gchar **) array->data);
	
	gsize i;
	for (i = 0; i < array->len; i++)
		g_free (g_array_index (array, gchar *, i));
	g_array_free (array, TRUE);
	return str;
}

static gboolean
build_node_path (GdaTree *tree, GdaTreeNode *node, GArray *array)
{
	GdaTreeNode *parent;
	GSList *list;
	gint i;
	parent = gda_tree_node_get_parent (node);
	if (parent)
		list = gda_tree_node_get_children (parent);
	else
		list = gda_tree_node_get_children (tree->priv->root);

	i = g_slist_index (list, node);
	g_slist_free (list);

	if (i < 0)
		return FALSE;
	else {
		gchar *tmp;
		tmp = g_strdup_printf ("%d", i);
		g_array_prepend_val (array, tmp);
		if (parent)
			return build_node_path (tree, parent, array);
		else
			return TRUE;
	}
}

/**
 * gda_tree_get_node:
 * @tree: a #GdaTree object
 * @tree_path: full path to the required nodes (if @use_names is %TRUE, then it must start with '/')
 * @use_names: if %TRUE, then @tree_path will be interpreted as a unix style path, and if %FALSE,
 *             then @tree_path will be interpreted similarly to the #GtkTreePath's string representation.
 *
 * Locates a #GdaTreeNode using the @tree_path path.
 *
 * Returns: (transfer none) (allow-none): the requested #GdaTreeNode pointer, or %NULL if not found
 *
 * Since: 4.2
 */
GdaTreeNode *
gda_tree_get_node (GdaTree *tree, const gchar *tree_path, gboolean use_names)
{
	GSList *segments;
	GdaTreeNode *node = NULL;
	
	g_return_val_if_fail (GDA_IS_TREE (tree), NULL);

	segments = decompose_path_as_segments (tree_path, use_names);
	if (!segments)
		return NULL;

	g_assert (real_gda_tree_get_nodes_in_path (tree, segments, use_names, &node) == NULL);

	if (segments) {
		g_slist_foreach (segments, (GFunc) g_free, NULL);
		g_slist_free (segments);
	}

	return node;
}

/**
 * gda_tree_get_node_manager:
 * @tree: a #GdaTree
 * @node: a #GdaTreeNode present in @tree
 *
 * Get the #GdaTreeManager which created @node in @tree
 *
 * Returns: (transfer none): the #GdaTreeManager, or %NULL if @node is not present in @tree
 *
 * Since: 4.2
 */
GdaTreeManager *
gda_tree_get_node_manager (GdaTree *tree, GdaTreeNode *node)
{
	GdaTreeNode *parent;
	g_return_val_if_fail (GDA_IS_TREE (tree), NULL);
	g_return_val_if_fail (GDA_IS_TREE_NODE (node), NULL);

	parent = gda_tree_node_get_parent (node);
	return _gda_tree_node_get_manager_for_child (parent ? parent : tree->priv->root, node);
}

static gboolean
create_or_update_children (GSList *mgrlist, GdaTreeNode *parent, gboolean disable_recurs, GError **error)
{
	GSList *list;
	for (list = mgrlist; list; list = list->next) {
		GdaTreeManager *manager = GDA_TREE_MANAGER (list->data);
		gboolean recurs = FALSE;
		if (disable_recurs) {
			g_object_get (G_OBJECT (manager), "recursive", &recurs, NULL);
			if (recurs)
				g_object_set (G_OBJECT (manager), "recursive", FALSE, NULL);
		}
		
		gboolean has_error = FALSE;
		_gda_tree_manager_update_children (manager, parent, 
						   _gda_tree_node_get_children_for_manager (parent, 
											    manager), 
						   &has_error, error);
		if (has_error)
			return FALSE;
		if (disable_recurs && recurs)
			g_object_set (G_OBJECT (manager), "recursive", TRUE, NULL);
	}
	return TRUE;
}

static GSList *split_absolute_path (const gchar *path, gboolean *out_error);
static GSList *split_indexed_path (const gchar *path, gboolean *out_error);

/*
 * Returns: a new list of allocated strings (one for each segment of @path), or %NULL
 */
static GSList *
decompose_path_as_segments (const gchar *path, gboolean use_names)
{
	GSList *segments;
	gboolean path_error;
	if (!path)
		return NULL;

	if (use_names) {
		if (*path != '/') {
			g_warning (_("Path format error: %s"), path);
			return NULL;
		}
		segments = split_absolute_path (path, &path_error);
	}
	else 
		segments = split_indexed_path (path, &path_error);
	if (path_error) {
		g_warning (_("Path format error: %s"), path);
		return NULL;
	}
	return segments;
}


/*
 * Splits @path into a list of path segments, avoiding empty ("") segments
 * @path is expected to be a unix style path
 * FIXME: check for errors
 * Returns: a new list of allocated strings (one for each segment of @path)
 */
static GSList *
split_absolute_path (const gchar *path, gboolean *out_error)
{
	GSList *list = NULL;
	gchar *copy;
	gchar *start, *end;

	*out_error = FALSE;
	copy = g_strdup (path);
	start = copy;
	for (;;) {
		for (end = start; *end; end++) {
			if (*end == '/')
				break;
		}
		if (*end == '/') {
			*end = 0;
			if (start != end)
				list = g_slist_prepend (list, g_strdup (start));
			start = end + 1;
		}
		else {
			if (start != end)
				list = g_slist_prepend (list, g_strdup (start));
			break;
		}
	}
	g_free (copy);

	list = g_slist_reverse (list);

#ifdef GDA_DEBUG_NO
	GSList *l;
	for (l = list; l; l = l->next)
		g_print ("Part: #%s#\n", (gchar*) l->data);
#endif

	return list;
}

/*
 * Splits @path into a list of path segments, avoiding empty ("") segments
 * @path is expected to be a GtkTreePath's string representation path (ex: "3:2")
 * Returns: a new list of allocated strings (one for each segment of @path)
 */
static GSList *
split_indexed_path (const gchar *path, gboolean *out_error)
{
	GSList *list = NULL;
	gchar *copy;
	gchar *start, *end;

	*out_error = FALSE;
	copy = g_strdup (path);
	start = copy;
	for (;;) {
		for (end = start; *end; end++) {
			if (*end == ':')
				break;
			if ((*end < '0') || (*end > '9')) {
				/* error */
				*out_error = TRUE;
				g_slist_foreach (list, (GFunc) g_free, NULL);
				g_slist_free (list);
				g_free (copy);
				return NULL;
			}
		}
		if (*end == ':') {
			*end = 0;
			if (start != end)
				list = g_slist_prepend (list, g_strdup (start));
			start = end + 1;
		}
		else {
			if (start != end)
				list = g_slist_prepend (list, g_strdup (start));
			break;
		}
	}
	g_free (copy);

	list = g_slist_reverse (list);

#ifdef GDA_DEBUG_NO
	GSList *l;
	g_print ("Splitting path '%s' into: ", path);
	for (l = list; l; l = l->next)
		g_print ("%s ", (gchar*) l->data);
	g_print ("\n");
#endif

	return list;
}

/**
 * gda_tree_set_attribute:
 * @tree: a #GdaTree object
 * @attribute: attribute name
 * @value: a #GValue, or %NULL
 * @destroy: a function to be called when @attribute is not needed anymore, or %NULL
 *
 * Sets an attribute to @tree, which will be accessible to any node in it.
 *
 * Since: 4.2
 */
void
gda_tree_set_attribute (GdaTree *tree, const gchar *attribute, const GValue *value,
			GDestroyNotify destroy)
{
	g_return_if_fail (GDA_IS_TREE (tree));
	gda_tree_node_set_node_attribute (tree->priv->root, attribute, value, destroy);
}

static void
take_root_node (GdaTree *tree, GdaTreeNode *root)
{
	if (tree->priv->root)
		unset_root_node (tree);
	tree->priv->root = root;
	g_signal_connect (tree->priv->root, "node-changed",
			  G_CALLBACK (node_changed_cb), tree);
	g_signal_connect (tree->priv->root, "node-inserted",
			  G_CALLBACK (node_inserted_cb), tree);
	g_signal_connect (tree->priv->root, "node-has-child-toggled",
			  G_CALLBACK (node_has_child_toggled_cb), tree);
	g_signal_connect (tree->priv->root, "node-deleted",
			  G_CALLBACK (node_deleted_cb), tree);
}

static void
unset_root_node (GdaTree *tree)
{
	GSList *list;
	for (list = tree->priv->managers; list; list = list->next) {
		GdaTreeManager *manager = GDA_TREE_MANAGER (list->data);

		_gda_tree_node_add_children (tree->priv->root, manager, NULL);
	}

	g_signal_handlers_disconnect_by_func (tree->priv->root,
					      G_CALLBACK (node_changed_cb), tree);
	g_signal_handlers_disconnect_by_func (tree->priv->root,
					      G_CALLBACK (node_inserted_cb), tree);
	g_signal_handlers_disconnect_by_func (tree->priv->root,
					      G_CALLBACK (node_has_child_toggled_cb), tree);
	g_signal_handlers_disconnect_by_func (tree->priv->root,
					      G_CALLBACK (node_deleted_cb), tree);

	g_object_unref (tree->priv->root);
}

static void
node_changed_cb (GdaTreeNode *reporting, GdaTreeNode *node, GdaTree *tree)
{
	if ((reporting != node) || (reporting != tree->priv->root))
		g_signal_emit (tree, gda_tree_signals [NODE_CHANGED], 0, node);
}

static void
node_inserted_cb (GdaTreeNode *reporting, GdaTreeNode *node, GdaTree *tree)
{
	if ((reporting != node) || (reporting != tree->priv->root))
		g_signal_emit (tree, gda_tree_signals [NODE_INSERTED], 0, node);
}

static void
node_has_child_toggled_cb (GdaTreeNode *reporting, GdaTreeNode *node, GdaTree *tree)
{
	if ((reporting != node) || (reporting != tree->priv->root))
		g_signal_emit (tree, gda_tree_signals [NODE_HAS_CHILD_TOGGLED], 0, node);
}

static void
node_deleted_cb (G_GNUC_UNUSED GdaTreeNode *reporting, const gchar *relative_path, GdaTree *tree)
{
	g_signal_emit (tree, gda_tree_signals [NODE_DELETED], 0, relative_path);
}
