/*
 * Copyright (C) 2009 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2018 Daniel Espinosa <esodan@gmail.com>
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
G_DECLARE_DERIVABLE_TYPE(GdaTreeMgrColumns, gda_tree_mgr_columns, GDA, TREE_MGR_COLUMNS, GdaTreeManager)
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

GdaTreeManager*    gda_tree_mgr_columns_new                      (GdaConnection *cnc, const gchar *schema,
								  const gchar *table_name);

G_END_DECLS

#endif
