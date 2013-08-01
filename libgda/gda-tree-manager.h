/*
 * Copyright (C) 2009 - 2010 Vivien Malerba <malerba@gnome-db.org>
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

#define GDA_TYPE_TREE_MANAGER            (gda_tree_manager_get_type())
#define GDA_TREE_MANAGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_TREE_MANAGER, GdaTreeManager))
#define GDA_TREE_MANAGER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_TREE_MANAGER, GdaTreeManagerClass))
#define GDA_IS_TREE_MANAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_TREE_MANAGER))
#define GDA_IS_TREE_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_TREE_MANAGER))
#define GDA_TREE_MANAGER_GET_CLASS(o)	 (G_TYPE_INSTANCE_GET_CLASS ((o), GDA_TYPE_TREE_MANAGER, GdaTreeManagerClass))

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

struct _GdaTreeManager {
	GObject            object;
	GdaTreeManagerPrivate *priv;
};

struct _GdaTreeManagerClass {
	GObjectClass       object_class;

	/* virtual methods */
	/**
	 * update_children:
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

GType                  gda_tree_manager_get_type                 (void) G_GNUC_CONST;

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
