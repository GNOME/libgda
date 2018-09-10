/*
 * Copyright (C) 2009 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2012, 2018 Daniel Espinosa <esodan@gmail.com>
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

#ifndef __GDAUI_TREE_STORE__
#define __GDAUI_TREE_STORE__

#include <gtk/gtk.h>
#include <libgda/gda-tree.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_TREE_STORE          (gdaui_tree_store_get_type())
G_DECLARE_DERIVABLE_TYPE(GdauiTreeStore, gdaui_tree_store, GDAUI, TREE_STORE, GObject)

/* struct for the object's class */
struct _GdauiTreeStoreClass
{
	GObjectClass         parent_class;

	/* signals */
	gboolean           (*drag_can_drag) (GdauiTreeStore *store, const gchar *path);
	gboolean           (*drag_get)      (GdauiTreeStore *store, const gchar *path, GtkSelectionData *selection_data);
	gboolean           (*drag_can_drop) (GdauiTreeStore *store, const gchar *path, GtkSelectionData *selection_data);
	gboolean           (*drag_drop)     (GdauiTreeStore *store, const gchar *path, GtkSelectionData *selection_data);
	gboolean           (*drag_delete)   (GdauiTreeStore *store, const gchar *path);
	gpointer           padding[12];
};

/**
 * SECTION:gdaui-tree-store
 * @short_description: Bridge between a #GdaTree and a #GtkTreeModel
 * @title: GdauiTreeStore
 * @stability: Stable
 * @see_also: #GdaTree
 *
 * The #GdauiTreeStore implements the #GtkTreeModel interface required
 * to display data from a #GdaTree in a #GtkTreeView widget.
 *
 * To allow a tree to be populated only on request (ie. when the user expands a row), each
 * #GdaTreeNode can give the attribute named #GDA_ATTRIBUTE_TREE_NODE_UNKNOWN_CHILDREN
 * a boolean %TRUE #GValue to tell the #GdauiTreeStore data model to artificially add
 * a dummy child for the row corresponding to the #GdaTreeNode. Then the programmer
 * can connect to the <link linkend="GtkTreeView-test-expand-row">GtkTreeView::test-expand-row</link>
 * signal and update the requested children.
 */

GtkTreeModel   *gdaui_tree_store_new       					(GdaTree *tree, guint n_columns, ...);
GtkTreeModel   *gdaui_tree_store_newv      					(GdaTree *tree, guint n_columns,
					    			GType *types, const gchar **attribute_names);
GdaTreeNode    *gdaui_tree_store_get_node  					(GdauiTreeStore *store, GtkTreeIter *iter);
gboolean				gdaui_tree_store_get_iter_from_node (GdauiTreeStore *store, GtkTreeIter *iter, GdaTreeNode *node);

G_END_DECLS

#endif
