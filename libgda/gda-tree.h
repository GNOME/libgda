/*
 * Copyright (C) 2009 - 2011 The GNOME Foundation.
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

#ifndef __GDA_TREE_H__
#define __GDA_TREE_H__

#include <glib-object.h>
#include <stdio.h>
#include "gda-decl.h"

G_BEGIN_DECLS

#define GDA_TYPE_TREE            (gda_tree_get_type())
#define GDA_TREE(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_TREE, GdaTree))
#define GDA_TREE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_TREE, GdaTreeClass))
#define GDA_IS_TREE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_TREE))
#define GDA_IS_TREE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_TREE))
#define GDA_TREE_GET_CLASS(o)	 (G_TYPE_INSTANCE_GET_CLASS ((o), GDA_TYPE_TREE, GdaTreeClass))

/* error reporting */
extern GQuark gda_tree_error_quark (void);
#define GDA_TREE_ERROR gda_tree_error_quark ()

typedef enum {
	GDA_TREE_UNKNOWN_ERROR
} GdaTreeError;

struct _GdaTree {
	GObject           object;
	GdaTreePrivate   *priv;
};

struct _GdaTreeClass {
	GObjectClass      object_class;

	/* signals */
	void         (* node_changed)           (GdaTree *tree, GdaTreeNode *node);
	void         (* node_inserted)          (GdaTree *tree, GdaTreeNode *node);
	void         (* node_has_child_toggled) (GdaTree *tree, GdaTreeNode *node);
	void         (* node_deleted)           (GdaTree *tree, const gchar *node_path);

	/*< private >*/
	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

/**
 * SECTION:gda-tree
 * @short_description: A tree-structure
 * @title: GdaTree
 * @stability: Stable
 * @see_also:
 *
 * The #GdaTree is the top level object representing hierarchically structured data. From this object it
 * is also possible (depending on the tree managers it uses), to clean (remove all the nodes) the whole tree,
 * or to request a complete or partial update of the nodes.
 *
 * It is also possible to set attributes to the tree itself (as it is possible to do for tree nodes),
 * or to dump the whole or part of a tree in an indented and easy to read fashion.
 */

GType              gda_tree_get_type      (void) G_GNUC_CONST;
GdaTree*           gda_tree_new           (void);
void               gda_tree_add_manager   (GdaTree *tree, GdaTreeManager *manager);

void               gda_tree_clean         (GdaTree *tree);
gboolean           gda_tree_update_all    (GdaTree *tree, GError **error);
gboolean           gda_tree_update_part   (GdaTree *tree, GdaTreeNode *node, GError **error);
gboolean           gda_tree_update_children (GdaTree *tree, GdaTreeNode *node, GError **error);

GSList            *gda_tree_get_nodes_in_path (GdaTree *tree, const gchar *tree_path, gboolean use_names);
GdaTreeNode       *gda_tree_get_node      (GdaTree *tree, const gchar *tree_path, gboolean use_names);
gchar             *gda_tree_get_node_path (GdaTree *tree, GdaTreeNode *node);
GdaTreeManager    *gda_tree_get_node_manager (GdaTree *tree, GdaTreeNode *node);

void               gda_tree_set_attribute (GdaTree *tree, const gchar *attribute, const GValue *value,
					   GDestroyNotify destroy);

void               gda_tree_dump          (GdaTree *tree, GdaTreeNode *node, FILE *stream);

G_END_DECLS

#endif
