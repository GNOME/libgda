/*
 * Copyright (C) 2009 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2012,2018 Daniel Espinosa <esodan@gmail.com>
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

#include "gdaui-tree-store.h"
#include <string.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-tree-node.h>
#include <libgda/gda-tree-manager.h>
#include <libgda/gda-value.h>
#include <libgda/gda-attributes-manager.h>
#include <gtk/gtk.h>
#include "marshallers/gdaui-marshal.h"

static void gdaui_tree_store_dispose (GObject *object);

static void gdaui_tree_store_set_property (GObject *object,
					   guint param_id,
					   const GValue *value,
					   GParamSpec *pspec);
static void gdaui_tree_store_get_property (GObject *object,
					   guint param_id,
					   GValue *value,
					   GParamSpec *pspec);
/*
 * GtkTreeDragSource interface
 */
static void              tree_store_drag_source_init   (GtkTreeDragSourceIface *iface);
static gboolean          tree_store_row_draggable      (GtkTreeDragSource *drag_dest,
							GtkTreePath *dest);
static gboolean          tree_store_drag_data_get      (GtkTreeDragSource *drag_dest,
							GtkTreePath *dest_path,
							GtkSelectionData *selection_data);
static gboolean          tree_store_drag_data_delete   (GtkTreeDragSource *drag_dest,
							GtkTreePath *dest_path);

/*
 * GtkTreeDragDest interface
 */
static void              tree_store_drag_dest_init     (GtkTreeDragDestIface *iface);
static gboolean          tree_store_drag_data_received (GtkTreeDragDest *drag_dest,
							GtkTreePath *dest,
							GtkSelectionData *selection_data);
static gboolean          tree_store_row_drop_possible  (GtkTreeDragDest *drag_dest,
							GtkTreePath *dest_path,
							GtkSelectionData *selection_data);

/*
 * GtkTreeModel interface
 */
static void              tree_store_tree_model_init (GtkTreeModelIface *iface);
static GtkTreeModelFlags tree_store_get_flags       (GtkTreeModel *tree_model);
static gint              tree_store_get_n_columns   (GtkTreeModel *tree_model);
static GType             tree_store_get_column_type (GtkTreeModel *tree_model, gint index);
static gboolean          tree_store_get_iter        (GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreePath *path);
static GtkTreePath      *tree_store_get_path        (GtkTreeModel *tree_model, GtkTreeIter *iter);
static void              tree_store_get_value       (GtkTreeModel *tree_model, GtkTreeIter *iter, gint column, GValue *value);
static gboolean          tree_store_iter_next       (GtkTreeModel *tree_model, GtkTreeIter *iter);
static gboolean          tree_store_iter_children   (GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreeIter *parent);
static gboolean          tree_store_iter_has_child  (GtkTreeModel *tree_model, GtkTreeIter *iter);
static gint              tree_store_iter_n_children (GtkTreeModel *tree_model, GtkTreeIter *iter);
static gboolean          tree_store_iter_nth_child  (GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreeIter *parent, gint n);
static gboolean          tree_store_iter_parent     (GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreeIter *child);


#define NOT_A_NODE ((GdaTreeNode*) 0x01)

/* signals */
enum
{
	DRAG_CAN_DRAG,
	DRAG_GET,
	DRAG_CAN_DROP,
        DRAG_DROP,
        DRAG_DELETE,
        LAST_SIGNAL
};
static guint gdaui_tree_store_signals [LAST_SIGNAL] = { 0, 0, 0, 0, 0 };

/* properties */
enum
{
	PROP_0,
	PROP_TREE
};

typedef struct {
	GType  type;
	gchar *attribute_name;
} ColumnSpec;
static void
column_spec_free (ColumnSpec *cs)
{
	g_free (cs->attribute_name);
	g_free (cs);
}

typedef struct {
	GdaTree *tree;
	GArray  *column_specs; /* array of ColumnSpec pointers, owned there */
	gint     stamp; /* Random integer to check whether an iter belongs to our model */
} GdauiTreeStorePrivate;
G_DEFINE_TYPE_WITH_CODE (GdauiTreeStore, gdaui_tree_store, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GdauiTreeStore)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL, tree_store_tree_model_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_SOURCE, tree_store_drag_source_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_DEST, tree_store_drag_dest_init))


static void tree_node_changed_cb (GdaTree *tree, GdaTreeNode *node, GdauiTreeStore *store);
static void tree_node_inserted_cb (GdaTree *tree, GdaTreeNode *node, GdauiTreeStore *store);
static void tree_node_has_child_toggled_cb (GdaTree *tree, GdaTreeNode *node, GdauiTreeStore *store);
static void tree_node_deleted_cb (GdaTree *tree, const gchar *node_path, GdauiTreeStore *store);

static void
tree_store_tree_model_init (GtkTreeModelIface *iface)
{
        iface->get_flags       = tree_store_get_flags;
        iface->get_n_columns   = tree_store_get_n_columns;
        iface->get_column_type = tree_store_get_column_type;
        iface->get_iter        = tree_store_get_iter;
        iface->get_path        = tree_store_get_path;
        iface->get_value       = tree_store_get_value;
        iface->iter_next       = tree_store_iter_next;
        iface->iter_children   = tree_store_iter_children;
        iface->iter_has_child  = tree_store_iter_has_child;
        iface->iter_n_children = tree_store_iter_n_children;
        iface->iter_nth_child  = tree_store_iter_nth_child;
        iface->iter_parent     = tree_store_iter_parent;
}

static void
tree_store_drag_source_init (GtkTreeDragSourceIface *iface)
{
	iface->row_draggable = tree_store_row_draggable;
	iface->drag_data_get = tree_store_drag_data_get;
	iface->drag_data_delete = tree_store_drag_data_delete;
}

static void
tree_store_drag_dest_init (GtkTreeDragDestIface *iface)
{
	iface->drag_data_received = tree_store_drag_data_received;
	iface->row_drop_possible = tree_store_row_drop_possible;
}

static void
gdaui_tree_store_class_init (GdauiTreeStoreClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = gdaui_tree_store_dispose;

	/* signals */
	gdaui_tree_store_signals [DRAG_CAN_DRAG] = 
		g_signal_new ("drag-can-drag",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GdauiTreeStoreClass, drag_can_drag),
                              NULL, NULL,
                              _gdaui_marshal_BOOLEAN__STRING, G_TYPE_BOOLEAN, 1,
                              G_TYPE_STRING);
	gdaui_tree_store_signals [DRAG_GET] = 
		g_signal_new ("drag-get",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GdauiTreeStoreClass, drag_get),
                              NULL, NULL,
                              _gdaui_marshal_BOOLEAN__STRING_POINTER, G_TYPE_BOOLEAN, 2,
                              G_TYPE_STRING, G_TYPE_POINTER);
	gdaui_tree_store_signals [DRAG_CAN_DROP] = 
		g_signal_new ("drag-can-drop",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GdauiTreeStoreClass, drag_can_drop),
                              NULL, NULL,
			      _gdaui_marshal_BOOLEAN__STRING_POINTER, G_TYPE_BOOLEAN, 2,
                              G_TYPE_STRING, G_TYPE_POINTER);
	gdaui_tree_store_signals [DRAG_DROP] = 
		g_signal_new ("drag-drop",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GdauiTreeStoreClass, drag_drop),
                              NULL, NULL,
			      _gdaui_marshal_BOOLEAN__STRING_POINTER, G_TYPE_BOOLEAN, 2,
                              G_TYPE_STRING, G_TYPE_POINTER);
	gdaui_tree_store_signals [DRAG_DELETE] = 
		g_signal_new ("drag-delete",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GdauiTreeStoreClass, drag_delete),
                              NULL, NULL,
			      _gdaui_marshal_BOOLEAN__STRING, G_TYPE_BOOLEAN, 1,
                              G_TYPE_STRING);
	klass->drag_can_drag = NULL;
	klass->drag_get = NULL;
	klass->drag_can_drop = NULL;
	klass->drag_drop = NULL;
	klass->drag_delete = NULL;

	/* Properties */
        object_class->set_property = gdaui_tree_store_set_property;
        object_class->get_property = gdaui_tree_store_get_property;

        g_object_class_install_property (object_class, PROP_TREE,
                                         g_param_spec_object ("tree", _("GdaTree to use"), NULL,
							      GDA_TYPE_TREE,
							      (G_PARAM_READABLE | G_PARAM_WRITABLE |
							       G_PARAM_CONSTRUCT_ONLY)));
}

static void
gdaui_tree_store_init (GdauiTreeStore *store)
{
	/* Private structure */
	GdauiTreeStorePrivate *priv = gdaui_tree_store_get_instance_private (store);
	priv->tree = NULL;
	priv->stamp = g_random_int_range (1, G_MAXINT);
	priv->column_specs = g_array_new (FALSE, FALSE, sizeof (ColumnSpec*));
}


static void
gdaui_tree_store_dispose (GObject *object)
{
	GdauiTreeStore *store;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_TREE_STORE (object));

	store = GDAUI_TREE_STORE (object);
	GdauiTreeStorePrivate *priv = gdaui_tree_store_get_instance_private (store);

		if (priv->column_specs) {
			gsize i;
			for (i = 0; i < priv->column_specs->len; i++) {
				ColumnSpec *cs;
				cs = g_array_index (priv->column_specs, ColumnSpec*, i);
				column_spec_free (cs);
			}
			g_array_free (priv->column_specs, TRUE);
		}
		if (priv->tree) {
			priv->stamp = g_random_int_range (1, G_MAXINT);

			g_signal_handlers_disconnect_by_func (priv->tree,
							      G_CALLBACK (tree_node_changed_cb), store);
			g_signal_handlers_disconnect_by_func (priv->tree,
							      G_CALLBACK (tree_node_inserted_cb), store);
			g_signal_handlers_disconnect_by_func (priv->tree,
							      G_CALLBACK (tree_node_has_child_toggled_cb), store);
			g_signal_handlers_disconnect_by_func (priv->tree,
							      G_CALLBACK (tree_node_deleted_cb), store);

			g_object_unref (priv->tree);
			priv->tree = NULL;
		}

	/* for the parent class */
	G_OBJECT_CLASS (gdaui_tree_store_parent_class)->dispose (object);
}

static void
gdaui_tree_store_set_property (GObject *object,
			       guint param_id,
			       const GValue *value,
			       GParamSpec *pspec)
{
	GdauiTreeStore *store;

	store = GDAUI_TREE_STORE (object);
	GdauiTreeStorePrivate *priv = gdaui_tree_store_get_instance_private (store);
		GdaTree *tree;

	switch (param_id) {
	case PROP_TREE:
		g_assert (!priv->tree);
		tree = (GdaTree*) g_value_get_object (value);
		g_return_if_fail (GDA_IS_TREE (tree));
		priv->tree = g_object_ref (tree);

		g_signal_connect (priv->tree, "node-changed",
				  G_CALLBACK (tree_node_changed_cb), store);
		g_signal_connect (priv->tree, "node-inserted",
				  G_CALLBACK (tree_node_inserted_cb), store);
		g_signal_connect (priv->tree, "node-has-child-toggled",
				  G_CALLBACK (tree_node_has_child_toggled_cb), store);
		g_signal_connect (priv->tree, "node-deleted",
				  G_CALLBACK (tree_node_deleted_cb), store);

		/* connect to row changes */
		priv->stamp = g_random_int_range (1, G_MAXINT);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
gdaui_tree_store_get_property (GObject *object,
			       guint param_id,
			       GValue *value,
			       GParamSpec *pspec)
{
	GdauiTreeStore *store;

	store = GDAUI_TREE_STORE (object);
	GdauiTreeStorePrivate *priv = gdaui_tree_store_get_instance_private (store);
	switch (param_id) {
	case PROP_TREE:
		g_value_set_object (value, priv->tree);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

/**
 * gdaui_tree_store_new:
 * @tree: a #GdaTree object
 * @n_columns: number of columns in the tree store
 * @...: couples of (GType, attribute name) for each column, from first to last
 *
 * Creates a #GtkTreeModel interface with a #GdaTree, mapping columns to attributes' values.
 *
 * As an example, <literal>gdaui_tree_store_new (tree, 2, G_TYPE_STRING, "name", G_TYPE_STRING, "schema");</literal> creates
 * a #GtkTreeStore with two columns (of type G_TYPE_STRING), one with the values of the "name" attribute, and one with
 * the values of the "schema" attribute.
 *
 * Note that the GType has to correspond to the type of value associated with the attribute name (no type
 * conversion is done), and a warning will be displayed in case of type mismatch.
 *
 * Returns: (transfer full): the new object, or %NULL if an attribute's name was NULL or an empty string
 *
 * Since: 4.2
 */
GtkTreeModel *
gdaui_tree_store_new (GdaTree *tree, guint n_columns, ...)
{
	GObject *obj;
	va_list args;
	guint i;
	GdauiTreeStore *store;

	g_return_val_if_fail (GDA_IS_TREE (tree), NULL);
	obj = g_object_new (GDAUI_TYPE_TREE_STORE, "tree", tree, NULL);
	store = GDAUI_TREE_STORE (obj);
	GdauiTreeStorePrivate *priv = gdaui_tree_store_get_instance_private (store);

	va_start (args, n_columns);
	for (i = 0; i < n_columns; i++) {
		ColumnSpec *cs;
		GType type = va_arg (args, GType);
		gchar *attname = va_arg (args, gchar*);
		if (!attname || !*attname) {
			g_warning ("Invalid attribute name");
			g_object_unref (obj);
			va_end (args);
			return NULL;
		}
		cs = g_new (ColumnSpec, 1);
		cs->type = type;
		cs->attribute_name = g_strdup (attname);
		g_array_append_val (priv->column_specs, cs);
	}
	va_end (args);

	return (GtkTreeModel *) obj;
}

/**
 * gdaui_tree_store_newv:
 * @tree: a #GdaTree object
 * @n_columns: number of columns in the tree store
 * @types: an array of @n_columns GType to specify the type of each column
 * @attribute_names: an array of @n_columns strings to specify the attribute name
 *                   to map each column on
 *
 * Creates a #GtkTreeModel interface with a #GdaTree, mapping columns to attributes' values.
 * For more information and limitations, see gdaui_tree_store_new().
 *
 * Returns: (transfer full): the new object, or %NULL if an inconsistency exists in the parameters
 *
 * Since: 4.2
 */
GtkTreeModel *
gdaui_tree_store_newv (GdaTree *tree, guint n_columns, GType *types, const gchar **attribute_names)
{
	GObject *obj;
	guint i;
	GdauiTreeStore *store;

	g_return_val_if_fail (GDA_IS_TREE (tree), NULL);
	obj = g_object_new (GDAUI_TYPE_TREE_STORE, "tree", tree, NULL);
	store = GDAUI_TREE_STORE (obj);
	GdauiTreeStorePrivate *priv = gdaui_tree_store_get_instance_private (store);
	
	for (i = 0; i < n_columns; i++) {
		ColumnSpec *cs;
		GType type = types [i];
		const gchar *attname = attribute_names [i];
		if (!attname || !*attname) {
			g_warning ("Invalid attribute name");
			g_object_unref (obj);
			return NULL;
		}
		cs = g_new (ColumnSpec, 1);
		cs->type = type;
		cs->attribute_name = g_strdup (attname);
		g_array_append_val (priv->column_specs, cs);
	}

	return (GtkTreeModel *) obj;
}

/* 
 * GtkTreeModel Interface implementation
 *
 * REM about the GtkTreeIter:
 *     iter->user_data <==> GdaTreeNode for the row
 *     iter->user_data2 <==> parent GdaTreeNode if @user_data == NOT_A_NODE
 *     iter->stamp is reset any time the model changes, 0 means invalid iter
 */

/**
 * gdaui_tree_store_get_node:
 * @store: a #GdauiTreeStore object
 * @iter: a valid #GtkTreeIter
 *
 * Get the  #GdaTreeNode represented by @iter.
 *
 * Returns: (transfer none): the #GdaTreeNode represented by @iter, or %NULL if an error occurred
 *
 * Since: 4.2.8
 */
GdaTreeNode *
gdaui_tree_store_get_node (GdauiTreeStore *store, GtkTreeIter *iter)
{
	g_return_val_if_fail (GDAUI_IS_TREE_STORE (store), NULL);
	GdauiTreeStorePrivate *priv = gdaui_tree_store_get_instance_private (store);
	g_return_val_if_fail (iter, NULL);
	g_return_val_if_fail (iter->stamp == priv->stamp, NULL);

	GdaTreeNode *node;
	node = (GdaTreeNode*) iter->user_data;
	if (node == NOT_A_NODE)
		return NULL;
	return node;
}

/**
 * gdaui_tree_store_get_iter_from_node:
 * @store: a #GdauiTreeStore object
 * @iter: a #GtkTreeIter
 * @node: a #GdaTreeNode in @store
 *
 * Sets @iter to represent @node in the tree.
 *
 * Returns: %TRUE if no error occurred and @iter is valid
 *
 * Since: 5.2
 */
gboolean
gdaui_tree_store_get_iter_from_node (GdauiTreeStore *store, GtkTreeIter *iter, GdaTreeNode *node)
{
	g_return_val_if_fail (GDAUI_IS_TREE_STORE (store), FALSE);
	g_return_val_if_fail (GDA_IS_TREE_NODE (node), FALSE);
	GdauiTreeStorePrivate *priv = gdaui_tree_store_get_instance_private (store);

	GdaTreeNode *parent = NULL;
	GSList *rootnodes;

	rootnodes = gda_tree_get_nodes_in_path (priv->tree, NULL, FALSE);
	if (rootnodes) {
		for (parent = node;
		     parent;
		     parent = gda_tree_node_get_parent (parent)) {
			if (g_slist_find (rootnodes, parent))
				break;
		}
		g_slist_free (rootnodes);
	}

	iter->user_data2 = NULL;
	if (parent) {
		iter->stamp = priv->stamp;
		iter->user_data = (gpointer) node;
		return TRUE;
	}
	else {
		iter->stamp = 0;
		iter->user_data = NULL;
		return FALSE;
	}
}

static GtkTreeModelFlags
tree_store_get_flags (GtkTreeModel *tree_model)
{
	gboolean is_list;
	GdauiTreeStore *store;
	g_return_val_if_fail (GDAUI_IS_TREE_STORE (tree_model), 0);
	store = GDAUI_TREE_STORE (tree_model);
	GdauiTreeStorePrivate *priv = gdaui_tree_store_get_instance_private (store);

	g_object_get (G_OBJECT (priv->tree), "is-list", &is_list, NULL);
	if (is_list)
		return GTK_TREE_MODEL_LIST_ONLY;
	else
		return 0;
}

static gint
tree_store_get_n_columns (GtkTreeModel *tree_model)
{
        GdauiTreeStore *store;

        g_return_val_if_fail (GDAUI_IS_TREE_STORE (tree_model), 0);
        store = GDAUI_TREE_STORE (tree_model);
        GdauiTreeStorePrivate *priv = gdaui_tree_store_get_instance_private (store);

        return priv->column_specs->len;
}

static GType
tree_store_get_column_type (GtkTreeModel *tree_model, gint index)
{
	GdauiTreeStore *store;
	ColumnSpec *cs;

        g_return_val_if_fail (GDAUI_IS_TREE_STORE (tree_model), 0);
        store = GDAUI_TREE_STORE (tree_model);
        GdauiTreeStorePrivate *priv = gdaui_tree_store_get_instance_private (store);

        cs = g_array_index (priv->column_specs, ColumnSpec*, index);
	g_return_val_if_fail (cs, G_TYPE_STRING);
	return cs->type;
}

static gboolean
tree_store_get_iter (GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreePath *path)
{
        GdauiTreeStore *store;
	GdaTreeNode *node;
	gchar *path_str;

        g_return_val_if_fail (GDAUI_IS_TREE_STORE (tree_model), FALSE);
        store = GDAUI_TREE_STORE (tree_model);
        GdauiTreeStorePrivate *priv = gdaui_tree_store_get_instance_private (store);
        g_return_val_if_fail (priv, FALSE);
        g_return_val_if_fail (priv->tree, FALSE);
        g_return_val_if_fail (path, FALSE);
        g_return_val_if_fail (iter, FALSE);

	path_str = gtk_tree_path_to_string (path);
	node = gda_tree_get_node (priv->tree, path_str, FALSE);
	/*g_print ("Path %s => node %p\n", path_str, node);*/
	g_free (path_str);

	if (node) {
		iter->stamp = priv->stamp;
                iter->user_data = (gpointer) node;
                iter->user_data2 = NULL;
		return TRUE;
	}
	else {
		GtkTreePath *path2;
		path2 = gtk_tree_path_copy (path);
		if (gtk_tree_path_up (path2)) {
			path_str = gtk_tree_path_to_string (path2);
			node = gda_tree_get_node (priv->tree, path_str, FALSE);
			/*g_print ("Path2 %s => node %p\n", path_str, node);*/
			g_free (path_str);
		}
		gtk_tree_path_free (path2);

		if (node) {
			const GValue *cv;
			cv = gda_tree_node_get_node_attribute (node,
							       GDA_ATTRIBUTE_TREE_NODE_UNKNOWN_CHILDREN);
			if (cv && (G_VALUE_TYPE (cv) == G_TYPE_BOOLEAN) &&
			    g_value_get_boolean (cv)) {
				iter->stamp = priv->stamp;
				iter->user_data = (gpointer) NOT_A_NODE;
				iter->user_data2 = (gpointer) node;
				return TRUE;
			}
		}

		iter->stamp = 0;
		iter->user_data = NULL;
                iter->user_data2 = NULL;
		return FALSE;
	}
}

static GtkTreePath *
tree_store_get_path (GtkTreeModel *tree_model, GtkTreeIter *iter)
{
        GdauiTreeStore *store;
        GtkTreePath *path = NULL;
	gchar *path_str = NULL;
	GdaTreeNode *node;

        g_return_val_if_fail (GDAUI_IS_TREE_STORE (tree_model), NULL);
        store = GDAUI_TREE_STORE (tree_model);
        GdauiTreeStorePrivate *priv = gdaui_tree_store_get_instance_private (store);
        g_return_val_if_fail (iter, NULL);
        g_return_val_if_fail (iter->stamp == priv->stamp, NULL);

	node = (GdaTreeNode*) iter->user_data;
	if (node == NOT_A_NODE) {
		GtkTreeIter iter2;
		gchar *tmp;

		iter2 = *iter;
		g_assert (gtk_tree_model_iter_parent (tree_model, &iter2, iter));
		path_str = gda_tree_get_node_path (priv->tree,
						   (GdaTreeNode*) iter2.user_data);
		tmp = g_strdup_printf ("%s:0", path_str);
		g_free (path_str);
		path_str = tmp;
	}
	else
		path_str = gda_tree_get_node_path (priv->tree, node);

	/*g_print ("Node %p => path %s\n", iter->user_data, path_str);*/
	path = gtk_tree_path_new_from_string (path_str);
	g_free (path_str);

        return path;
}

static void
tree_store_get_value (GtkTreeModel *tree_model, GtkTreeIter *iter, gint column, GValue *value)
{
	GdauiTreeStore *store;
	const GValue *tmp = NULL;
	ColumnSpec *cs;
	GdaTreeNode *node;
	
	g_return_if_fail (GDAUI_IS_TREE_STORE (tree_model));
        store = GDAUI_TREE_STORE (tree_model);
        GdauiTreeStorePrivate *priv = gdaui_tree_store_get_instance_private (store);
        g_return_if_fail (priv->tree);
        g_return_if_fail (iter);
        g_return_if_fail (iter->stamp == priv->stamp);
        g_return_if_fail (value);

	cs = g_array_index (priv->column_specs, ColumnSpec*, column);
	g_return_if_fail (cs);

	if (!cs) {
		g_warning (_("Unknown column number %d"), column);
		gda_value_set_null (value);
		return;
	}

	node = (GdaTreeNode*) iter->user_data;
	if (node != NOT_A_NODE)
		tmp = gda_tree_node_fetch_attribute (node, cs->attribute_name);

	if (!tmp) {
		g_value_init (value, cs->type);
		return;
	}
	if (G_VALUE_TYPE (tmp) == cs->type) {
		g_value_init (value, cs->type);
		g_value_copy (tmp, value);
	}
	else if (gda_value_is_null (tmp))
		gda_value_set_null (value);
	else {
		g_warning (_("Type mismatch: expected a value of type %s and got of type %s"),
			   g_type_name (cs->type), g_type_name (G_VALUE_TYPE (tmp)));
		g_value_init (value, cs->type);
	}
}

static gboolean
tree_store_iter_next (GtkTreeModel *tree_model, GtkTreeIter *iter)
{
        GdauiTreeStore *store;
	GdaTreeNode *parent;
	GdaTreeNode *node;

        g_return_val_if_fail (GDAUI_IS_TREE_STORE (tree_model), FALSE);
        store = GDAUI_TREE_STORE (tree_model);
        GdauiTreeStorePrivate *priv = gdaui_tree_store_get_instance_private (store);
        g_return_val_if_fail (priv->tree, FALSE);
        g_return_val_if_fail (iter, FALSE);
        g_return_val_if_fail (iter->stamp == priv->stamp, FALSE);

	node = (GdaTreeNode*) iter->user_data;
	if (node == NOT_A_NODE) {
		iter->stamp = 0;
		iter->user_data = NULL;
                iter->user_data2 = NULL;
		return FALSE;
	}
	else {
		GSList *list, *current;
		parent = gda_tree_node_get_parent (node);
		if (parent) 
			list = gda_tree_node_get_children (parent);
		else
			list = gda_tree_get_nodes_in_path (priv->tree, NULL, FALSE);
		
		current = g_slist_find (list, iter->user_data);
		g_assert (current);
		if (current->next) {
#ifdef GDA_DEBUG_NO
#define GDA_ATTRIBUTE_NAME "__gda_attr_name"
			g_print ("Next %s(%p) => %s(%p)\n",
				 gda_value_stringify (gda_tree_node_fetch_attribute (GDA_TREE_NODE (iter->user_data), GDA_ATTRIBUTE_NAME)),
				 iter->user_data,
				 gda_value_stringify (gda_tree_node_fetch_attribute (GDA_TREE_NODE (current->next->data), GDA_ATTRIBUTE_NAME)),
				 current->next->data);
#endif
			iter->user_data = (gpointer) current->next->data;
			iter->user_data2 = NULL;
			g_slist_free (list);
			return TRUE;
		}
		else {
			iter->stamp = 0;
			iter->user_data = NULL;
			iter->user_data2 = NULL;
			g_slist_free (list);
			return FALSE;
		}
	}
}

static gboolean
tree_store_iter_children (GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreeIter *parent)
{
        GdauiTreeStore *store;
	GSList *list = NULL;
	GdaTreeNode *node = NULL;

        g_return_val_if_fail (GDAUI_IS_TREE_STORE (tree_model), FALSE);
        store = GDAUI_TREE_STORE (tree_model);
        GdauiTreeStorePrivate *priv = gdaui_tree_store_get_instance_private (store);
        g_return_val_if_fail (priv->tree, FALSE);
        g_return_val_if_fail (iter, FALSE);

	if (parent) {
		g_return_val_if_fail (parent->stamp == priv->stamp, FALSE);
		node = (GdaTreeNode*) parent->user_data;
		if (node != NOT_A_NODE)
			list = gda_tree_node_get_children (node);
	}
	else
		list = gda_tree_get_nodes_in_path (priv->tree, NULL, FALSE);

	if (list) {
		iter->stamp = priv->stamp;
		iter->user_data = (gpointer) list->data;
                iter->user_data2 = NULL;
		g_slist_free (list);
		return TRUE;
	}
	else if (node != NOT_A_NODE) {
		const GValue *cv;
		cv = gda_tree_node_get_node_attribute (node,
						       GDA_ATTRIBUTE_TREE_NODE_UNKNOWN_CHILDREN);
		if (cv && (G_VALUE_TYPE (cv) == G_TYPE_BOOLEAN) &&
		    g_value_get_boolean (cv)) {
			iter->stamp = priv->stamp;
			iter->user_data = (gpointer) NOT_A_NODE;
			iter->user_data2 = (gpointer) node;
			return TRUE;
		}
	}

	iter->stamp = 0;
	iter->user_data = NULL;
	iter->user_data2 = NULL;
	return FALSE;
}

static gboolean
tree_store_iter_has_child (GtkTreeModel *tree_model, GtkTreeIter *iter)
{
	GdauiTreeStore *store;
	GdaTreeNode *node;

        g_return_val_if_fail (GDAUI_IS_TREE_STORE (tree_model), FALSE);
        store = GDAUI_TREE_STORE (tree_model);
        GdauiTreeStorePrivate *priv = gdaui_tree_store_get_instance_private (store);
        g_return_val_if_fail (priv->tree, FALSE);
        g_return_val_if_fail (iter, FALSE);
	g_return_val_if_fail (iter->stamp == priv->stamp, FALSE);

	node = (GdaTreeNode*) iter->user_data;
	if (node == NOT_A_NODE)
		return FALSE;
	if (gda_tree_node_get_child_index (node, 0))
		return TRUE;
	else {
		const GValue *cv;
		cv = gda_tree_node_get_node_attribute (node,
						       GDA_ATTRIBUTE_TREE_NODE_UNKNOWN_CHILDREN);
		if (cv && (G_VALUE_TYPE (cv) == G_TYPE_BOOLEAN) &&
		    g_value_get_boolean (cv))
			return TRUE;
		else
			return FALSE;
	}
}

static gint
tree_store_iter_n_children (GtkTreeModel *tree_model, GtkTreeIter *iter)
{
        GdauiTreeStore *store;
	GSList *list = NULL;
	GdaTreeNode *node = NULL;

        g_return_val_if_fail (GDAUI_IS_TREE_STORE (tree_model), -1);
        store = GDAUI_TREE_STORE (tree_model);
        GdauiTreeStorePrivate *priv = gdaui_tree_store_get_instance_private (store);
        g_return_val_if_fail (priv->tree, 0);

        if (iter) {
		g_return_val_if_fail (iter->stamp == priv->stamp, FALSE);
		node = (GdaTreeNode*) iter->user_data;
		if (node != NOT_A_NODE)
			list = gda_tree_node_get_children (GDA_TREE_NODE (iter->user_data));
	}
	else
                list = gda_tree_get_nodes_in_path (priv->tree, NULL, FALSE);

	if (list) {
		gint retval;
		retval = g_slist_length (list);
		g_slist_free (list);
		return retval;
	}
	else if (node && (node != NOT_A_NODE)) {
		const GValue *cv;
		cv = gda_tree_node_get_node_attribute (node,
						       GDA_ATTRIBUTE_TREE_NODE_UNKNOWN_CHILDREN);
		if (cv && (G_VALUE_TYPE (cv) == G_TYPE_BOOLEAN) &&
		    g_value_get_boolean (cv))
			return 1;
	}
	return 0;
}

static gboolean
tree_store_iter_nth_child (GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreeIter *parent, gint n)
{
	GdauiTreeStore *store;
	GSList *list = NULL;
	GdaTreeNode *node = NULL;

	g_return_val_if_fail (GDAUI_IS_TREE_STORE (tree_model), FALSE);
	store = GDAUI_TREE_STORE (tree_model);
	GdauiTreeStorePrivate *priv = gdaui_tree_store_get_instance_private (store);
	g_return_val_if_fail (priv->tree, FALSE);
        g_return_val_if_fail (iter, FALSE);
	if (parent) {
		g_return_val_if_fail (parent->stamp == priv->stamp, FALSE);
		node = (GdaTreeNode*) parent->user_data;
		if (node != NOT_A_NODE)
			list = gda_tree_node_get_children (node);
	}
	else
		list = gda_tree_get_nodes_in_path (priv->tree, NULL, FALSE);
	
	if (list) {
		node = (GdaTreeNode*) g_slist_nth_data (list, n);
		g_slist_free (list);
		if (node) {
			iter->stamp = priv->stamp;
			iter->user_data = (gpointer) node;
			iter->user_data2 = NULL;
			return TRUE;
		}
	}
	else if (node && (node != NOT_A_NODE) && (n == 0)) {
		const GValue *cv;
		cv = gda_tree_node_get_node_attribute (node,
						       GDA_ATTRIBUTE_TREE_NODE_UNKNOWN_CHILDREN);
		if (cv && (G_VALUE_TYPE (cv) == G_TYPE_BOOLEAN) &&
		    g_value_get_boolean (cv)) {
			iter->stamp = priv->stamp;
			iter->user_data = (gpointer) NOT_A_NODE;
			iter->user_data2 = (gpointer) node;
			return TRUE;
		}
	}

	iter->stamp = 0;
	iter->user_data = NULL;
	iter->user_data2 = NULL;
	return FALSE;
}

static gboolean
tree_store_iter_parent (GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreeIter *child)
{
        GdauiTreeStore *store;
	GdaTreeNode *node, *parent;

        g_return_val_if_fail (GDAUI_IS_TREE_STORE (tree_model), FALSE);
        store = GDAUI_TREE_STORE (tree_model);
        GdauiTreeStorePrivate *priv = gdaui_tree_store_get_instance_private (store);
        g_return_val_if_fail (priv->tree, FALSE);
        g_return_val_if_fail (iter, FALSE);
        g_return_val_if_fail (child, FALSE);
        g_return_val_if_fail (child->stamp == priv->stamp, FALSE);

	node = (GdaTreeNode*) child->user_data;
	if (node == NOT_A_NODE) {
		parent = (GdaTreeNode*) child->user_data2;
		g_assert (GDA_IS_TREE_NODE (parent));
	}
	else
		parent = gda_tree_node_get_parent (node);

	if (parent) {
		iter->stamp = priv->stamp;
		iter->user_data = (gpointer) parent;
                iter->user_data2 = NULL;
		return TRUE;
	}

	iter->stamp = 0;
	iter->user_data = NULL;
	iter->user_data2 = NULL;
	return FALSE;
}


static void
tree_node_changed_cb (GdaTree *tree, GdaTreeNode *node, GdauiTreeStore *store)
{
	GtkTreePath *path;
	GtkTreeIter iter;
	gchar *path_str;
	GdauiTreeStorePrivate *priv = gdaui_tree_store_get_instance_private (store);
	
	priv->stamp = g_random_int_range (1, G_MAXINT);

	path_str = gda_tree_get_node_path (tree, node);
	path = gtk_tree_path_new_from_string (path_str);
	g_free (path_str);
	
	memset (&iter, 0, sizeof (GtkTreeIter));
	iter.stamp = priv->stamp;
	iter.user_data = (gpointer) node;
	iter.user_data2 = NULL;
	
	gtk_tree_model_row_changed (GTK_TREE_MODEL (store), path, &iter);
	/*g_print ("GdauiTreeStore::changed %s (node %p)\n", gtk_tree_path_to_string (path), node);*/
	gtk_tree_path_free (path);
}

static void
tree_node_inserted_cb (GdaTree *tree, GdaTreeNode *node, GdauiTreeStore *store)
{
	GtkTreePath *path;
	GtkTreeIter iter;
	gchar *path_str;
	GdauiTreeStorePrivate *priv = gdaui_tree_store_get_instance_private (store);
	
	priv->stamp = g_random_int_range (1, G_MAXINT);

	path_str = gda_tree_get_node_path (tree, node);
	path = gtk_tree_path_new_from_string (path_str);
	g_free (path_str);
	
	memset (&iter, 0, sizeof (GtkTreeIter));
	iter.stamp = priv->stamp;
	iter.user_data = (gpointer) node;
	iter.user_data2 = NULL;
	
	gtk_tree_model_row_inserted (GTK_TREE_MODEL (store), path, &iter);
	/*g_print ("GdauiTreeStore::row_inserted %s (node %p)\n", gtk_tree_path_to_string (path), node);*/
	gtk_tree_path_free (path);
}

static void
tree_node_has_child_toggled_cb (GdaTree *tree, GdaTreeNode *node, GdauiTreeStore *store)
{
	GtkTreePath *path;
	GtkTreeIter iter;
	gchar *path_str;
	GdauiTreeStorePrivate *priv = gdaui_tree_store_get_instance_private (store);
	
	priv->stamp = g_random_int_range (1, G_MAXINT);

	path_str = gda_tree_get_node_path (tree, node);
	path = gtk_tree_path_new_from_string (path_str);
	g_free (path_str);
	
	memset (&iter, 0, sizeof (GtkTreeIter));
	iter.stamp = priv->stamp;
	iter.user_data = (gpointer) node;
	iter.user_data2 = NULL;
	
	gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (store), path, &iter);
	/*g_print ("GdauiTreeStore::row_has_child %s (node %p)\n", gtk_tree_path_to_string (path), node);*/
	gtk_tree_path_free (path);
}

static void
tree_node_deleted_cb (G_GNUC_UNUSED GdaTree *tree, const gchar *node_path, GdauiTreeStore *store)
{
	GtkTreePath *path;
	GdauiTreeStorePrivate *priv = gdaui_tree_store_get_instance_private (store);

	priv->stamp = g_random_int_range (1, G_MAXINT);
	
	path = gtk_tree_path_new_from_string (node_path);
	
	gtk_tree_model_row_deleted (GTK_TREE_MODEL (store), path);
	/*g_print ("GdauiTreeStore::deleted %s\n", gtk_tree_path_to_string (path));*/
	gtk_tree_path_free (path);	
}

/*
 * GtkTreeDragSource interface
 */
static gboolean
tree_store_row_draggable (GtkTreeDragSource *drag_dest, GtkTreePath *dest_path)
{
	GdauiTreeStore *store;
	gboolean retval;
	gchar *str;

	store = GDAUI_TREE_STORE (drag_dest);
	str = gtk_tree_path_to_string (dest_path);
	g_signal_emit (store, gdaui_tree_store_signals [DRAG_CAN_DRAG], 0, str, &retval);
	g_free (str);
	/*g_print ("signalled DRAG_CAN_DRAG, got %s\n", retval ? "TRUE" : "FALSE");*/
	return retval;
}

static gboolean
tree_store_drag_data_get (GtkTreeDragSource *drag_dest, GtkTreePath *dest_path,
			  GtkSelectionData  *selection_data)
{
	GdauiTreeStore *store;
	gboolean retval;
	gchar *str;

	store = GDAUI_TREE_STORE (drag_dest);
	str = gtk_tree_path_to_string (dest_path);
	g_signal_emit (store, gdaui_tree_store_signals [DRAG_GET], 0, str, selection_data, &retval);
	g_free (str);
	/*g_print ("signalled DRAG_GET, got %s\n", retval ? "TRUE" : "FALSE");*/
	return retval;
}

static gboolean
tree_store_drag_data_delete (GtkTreeDragSource *drag_dest, GtkTreePath *dest_path)
{
	GdauiTreeStore *store;
	gboolean retval;
	gchar *str;

	store = GDAUI_TREE_STORE (drag_dest);
	str = gtk_tree_path_to_string (dest_path);
	g_signal_emit (store, gdaui_tree_store_signals [DRAG_DELETE], 0, str, &retval);
	g_free (str);
	/*g_print ("signalled DRAG_DELETE, got %s\n", retval ? "TRUE" : "FALSE");*/
	return retval;
}

/*
 * GtkTreeDragDest interface
 */
static gboolean
tree_store_drag_data_received (GtkTreeDragDest   *drag_dest,
			       GtkTreePath       *dest_path,
			       GtkSelectionData  *selection_data)
{
	GdauiTreeStore *store;
	gboolean retval;
	gchar *str;

	store = GDAUI_TREE_STORE (drag_dest);
	str = gtk_tree_path_to_string (dest_path);
	g_signal_emit (store, gdaui_tree_store_signals [DRAG_DROP], 0, str, selection_data, &retval);
	g_free (str);
	/*g_print ("signalled DRAG_DROP, got %s\n", retval ? "TRUE" : "FALSE");*/
	return retval;
}

static gboolean
tree_store_row_drop_possible  (GtkTreeDragDest   *drag_dest,
			       GtkTreePath       *dest_path,
			       GtkSelectionData  *selection_data)
{
	GdauiTreeStore *store;
	gboolean retval;
	gchar *str;

	store = GDAUI_TREE_STORE (drag_dest);
	str = gtk_tree_path_to_string (dest_path);
	g_signal_emit (store, gdaui_tree_store_signals [DRAG_CAN_DROP], 0, str, selection_data, &retval);
	g_free (str);
	/*g_print ("signalled DRAG_CAN_DROP, got %s\n", retval ? "TRUE" : "FALSE");*/
	return retval;
}
