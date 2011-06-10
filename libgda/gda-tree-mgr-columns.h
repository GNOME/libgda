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

#ifndef __GDA_TREE_MGR_COLUMNS_H__
#define __GDA_TREE_MGR_COLUMNS_H__

#include <libgda/gda-connection.h>
#include "gda-tree-manager.h"

G_BEGIN_DECLS

#define GDA_TYPE_TREE_MGR_COLUMNS            (gda_tree_mgr_columns_get_type())
#define GDA_TREE_MGR_COLUMNS(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_TREE_MGR_COLUMNS, GdaTreeMgrColumns))
#define GDA_TREE_MGR_COLUMNS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_TREE_MGR_COLUMNS, GdaTreeMgrColumnsClass))
#define GDA_IS_TREE_MGR_COLUMNS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_TREE_MGR_COLUMNS))
#define GDA_IS_TREE_MGR_COLUMNS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_TREE_MGR_COLUMNS))
#define GDA_TREE_MGR_COLUMNS_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GDA_TYPE_TREE_MGR_COLUMNS, GdaTreeMgrColumnsClass))

typedef struct _GdaTreeMgrColumns GdaTreeMgrColumns;
typedef struct _GdaTreeMgrColumnsPriv GdaTreeMgrColumnsPriv;
typedef struct _GdaTreeMgrColumnsClass GdaTreeMgrColumnsClass;

struct _GdaTreeMgrColumns {
	GdaTreeManager        object;
	GdaTreeMgrColumnsPriv *priv;
};

struct _GdaTreeMgrColumnsClass {
	GdaTreeManagerClass   object_class;
};

/**
 * SECTION:gda-tree-mgr-columns
 * @short_description: A tree manager which creates a node for each column of a table
 * @title: GdaTreeMgrColumns
 * @stability: Stable
 * @see_also:
 *
 * The #GdaTreeMgrColumns is a #GdaTreeManager object which creates a node for
 * each column in a table.
 *
 * It uses the #GdaMetaStore associated to a #GdaConnection to get the columns list
 * for a table designated by its name and the database schema it is in; it's up to the
 * caller to make sure the data in the #GdaMetaStore is up to date.
 *
 * The #GdaConnection to be used needs to be specified when the object is created. The table
 * name and schema can however be specified when the object is created, and if not, are
 * fetched from the #GdaTreeNode below which the nodes will be placed (using
 * gda_tree_node_fetch_attribute()).
 */

GType              gda_tree_mgr_columns_get_type                 (void) G_GNUC_CONST;
GdaTreeManager*    gda_tree_mgr_columns_new                      (GdaConnection *cnc, const gchar *schema,
								  const gchar *table_name);

G_END_DECLS

#endif
