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

#ifndef __GDA_TREE_NODE_H__
#define __GDA_TREE_NODE_H__

#include <glib-object.h>
#include "gda-decl.h"

G_BEGIN_DECLS

#define GDA_TYPE_TREE_NODE            (gda_tree_node_get_type())
#define GDA_TREE_NODE(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_TREE_NODE, GdaTreeNode))
#define GDA_TREE_NODE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_TREE_NODE, GdaTreeNodeClass))
#define GDA_IS_TREE_NODE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_TREE_NODE))
#define GDA_IS_TREE_NODE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_TREE_NODE))

/* error reporting */
extern GQuark gda_tree_node_error_quark (void);
#define GDA_TREE_NODE_ERROR gda_tree_node_error_quark ()

typedef enum {
	GDA_TREE_NODE_UNKNOWN_ERROR
} GdaTreeNodeError;

struct _GdaTreeNode {
	GObject             object;
	GdaTreeNodePrivate *priv;
};

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

	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

GType              gda_tree_node_get_type          (void) G_GNUC_CONST;
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
