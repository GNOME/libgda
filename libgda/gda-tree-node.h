/*
 * Copyright (C) 2009 - 2011 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_TREE_NODE_H__
#define __GDA_TREE_NODE_H__

#include <glib-object.h>
#include <libgda/gda-tree-manager.h>
#include "gda-decl.h"

G_BEGIN_DECLS

#define GDA_TYPE_TREE_NODE            (gda_tree_node_get_type())
G_DECLARE_DERIVABLE_TYPE (GdaTreeNode, gda_tree_node, GDA, TREE_NODE, GObject)


struct _GdaTreeNodeClass {
	GObjectClass        object_class;

	/* signals */
	void         (* node_changed)           (GdaTreeNode *reporting, GdaTreeNode *node);
	void         (* node_inserted)          (GdaTreeNode *reporting, GdaTreeNode *node);
	void         (* node_has_child_toggled) (GdaTreeNode *reporting, GdaTreeNode *node);
	void         (* node_deleted)           (GdaTreeNode *reporting, const gchar *relative_path);

	/* virtual methods */
	gchar              *(*dump_header) (GdaTreeNode *node);
	void                (*dump_children) (GdaTreeNode *node, const gchar *prefix, GString *in_string);

	/*< private >*/
	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

/* error reporting */
extern GQuark gda_tree_node_error_quark (void);
#define GDA_TREE_NODE_ERROR gda_tree_node_error_quark ()

typedef enum {
	GDA_TREE_NODE_UNKNOWN_ERROR
} GdaTreeNodeError;

/**
 * SECTION:gda-tree-node
 * @short_description: A node in a #GdaTree
 * @title: GdaTreeNode
 * @stability: Stable
 * @see_also:
 *
 * Every node in a #GdaTree tree is represented by a single #GdaTreeNode object. There is no distinction
 * between nodes which have children and those which don't (leaf nodes).
 *
 * The #GdaTreeNode is very basic as it only has a "name" attribute: users are encouraged to subclass it to
 * add more features if needed (and make use of them by defining a #GdaTreeManagerNodeFunc function and 
 * calling gda_tree_manager_set_node_create_func()).
 */

GdaTreeNode*       gda_tree_node_new               (const gchar *name);

GdaTreeNode       *gda_tree_node_get_parent        (GdaTreeNode *node);
GSList            *gda_tree_node_get_children      (GdaTreeNode *node);
GdaTreeNode       *gda_tree_node_get_child_index   (GdaTreeNode *node, gint index);
GdaTreeNode       *gda_tree_node_get_child_name    (GdaTreeNode *node, const gchar *name);

void               gda_tree_node_set_node_attribute(GdaTreeNode *node, const gchar *attribute, const GValue *value,
						    GDestroyNotify destroy);
const GValue      *gda_tree_node_get_node_attribute(GdaTreeNode *node, const gchar *attribute);
const GValue      *gda_tree_node_fetch_attribute   (GdaTreeNode *node, const gchar *attribute);

/* private */
void               _gda_tree_node_add_children     (GdaTreeNode *node, GdaTreeManager *mgr, const GSList *children);
const GSList      *_gda_tree_node_get_children_for_manager (GdaTreeNode *node, GdaTreeManager *mgr);
GSList            *_gda_tree_node_get_managers_for_children (GdaTreeNode *node);
GdaTreeManager    *_gda_tree_node_get_manager_for_child (GdaTreeNode *node, GdaTreeNode *child);

G_END_DECLS

#endif
