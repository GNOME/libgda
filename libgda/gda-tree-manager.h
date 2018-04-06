/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 Robert Ancell <robert.ancell@canonical.com>
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

#ifndef __GDA_TREE_MANAGER_H__
#define __GDA_TREE_MANAGER_H__

#include <glib-object.h>
#include "gda-decl.h"

G_BEGIN_DECLS

#define GDA_TYPE_TREE_MANAGER (gda_tree_manager_get_type())
G_DECLARE_DERIVABLE_TYPE (GdaTreeManager, gda_tree_manager, GDA, TREE_MANAGER, GObject)

typedef GSList *(*GdaTreeManagerNodesFunc) (GdaTreeManager *manager, GdaTreeNode *node,
					    const GSList *children_nodes,
					    gboolean *out_error, GError **error);
typedef GdaTreeNode *(*GdaTreeManagerNodeFunc) (GdaTreeManager *manager, GdaTreeNode *parent, const gchar *name);

/* error reporting */
extern GQuark gda_tree_manager_error_quark (void);
#define GDA_TREE_MANAGER_ERROR gda_tree_manager_error_quark ()

typedef enum {
	GDA_TREE_MANAGER_UNKNOWN_ERROR
} GdaTreeManagerError;

struct _GdaTreeManagerClass {
	GObjectClass       parent_class;

	/* virtual methods */
	/**
	 * GdaTreeManager::update_children:
	 *
	 * Returns: NULL if an error occurred, and @out_error is set to TRUE
	 */
	GSList *(*update_children) (GdaTreeManager *manager, GdaTreeNode *node,
				    const GSList *children_nodes,
				    gboolean *out_error, GError **error);

	/*< private >*/
	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

/**
 * SECTION:gda-tree-manager
 * @short_description: Base class for all the tree managers
 * @title: GdaTreeManager
 * @stability: Stable
 * @see_also:
 *
 * A #GdaTreeManager object is responsible for creating nodes in the tree(s) for which it
 * operates.
 *
 * When creating nodes, a #GdaTreeManager object can (depending on its implementation), get some
 * named attributes from the node below which it has to create nodes, using the gda_tree_node_fetch_attribute()
 * or gda_tree_node_get_node_attribute(). For example the #GdaTreeMgrColumns manager (which creates a node for each column
 * of a table) needs the table name and the schema in which the table is; both can be specified using an
 * object's property, or, if not specified that way, are fetched as attributes.
 *
 * The #GdaTreeManager itself is an abstract type (which can't be instantiated). Use an existing sub class or subclass
 * it yourself.
 */

GdaTreeManager        *gda_tree_manager_new_with_func            (GdaTreeManagerNodesFunc update_func);
void                   gda_tree_manager_add_manager              (GdaTreeManager *manager, GdaTreeManager *sub);
const GSList          *gda_tree_manager_get_managers             (GdaTreeManager *manager);

void                   gda_tree_manager_set_node_create_func     (GdaTreeManager *manager, GdaTreeManagerNodeFunc func);
GdaTreeManagerNodeFunc gda_tree_manager_get_node_create_func     (GdaTreeManager *manager);

void                   gda_tree_manager_add_new_node_attribute   (GdaTreeManager *manager, const gchar *attribute, const GValue *value);
GdaTreeNode           *gda_tree_manager_create_node              (GdaTreeManager *manager, GdaTreeNode *parent, const gchar *name);

/* private */
void                   _gda_tree_manager_update_children         (GdaTreeManager *manager, GdaTreeNode *node,
								  const GSList *children_nodes,
								  gboolean *out_error, GError **error);


G_END_DECLS

#endif
