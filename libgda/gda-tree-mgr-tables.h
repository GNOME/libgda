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

#ifndef __GDA_TREE_MGR_TABLES_H__
#define __GDA_TREE_MGR_TABLES_H__

#include <libgda/gda-connection.h>
#include "gda-tree-manager.h"

G_BEGIN_DECLS

#define GDA_TYPE_TREE_MGR_TABLES            (gda_tree_mgr_tables_get_type())
G_DECLARE_DERIVABLE_TYPE (GdaTreeMgrTables, gda_tree_mgr_tables, GDA, TREE_MGR_TABLES, GdaTreeManager)

struct _GdaTreeMgrTablesClass {
	GdaTreeManagerClass   object_class;
};

/**
 * SECTION:gda-tree-mgr-tables
 * @short_description: A tree manager which creates a node for each table in a schema
 * @title: GdaTreeMgrTables
 * @stability: Stable
 * @see_also:
 *
 * The #GdaTreeMgrTables is a #GdaTreeManager object which creates a node for
 * each table in a database schema.
 *
 * It uses the #GdaMetaStore associated to a #GdaConnection to get the tables list
 * in database schema; it's up to the
 * caller to make sure the data in the #GdaMetaStore is up to date.
 *
 * The #GdaConnection to be used needs to be specified when the object is created. The
 * schema can however be specified when the object is created, and if not, is
 * fetched from the #GdaTreeNode below which the nodes will be placed (using
 * gda_tree_node_fetch_attribute()).
 */

GdaTreeManager*    gda_tree_mgr_tables_new                      (GdaConnection *cnc, const gchar *schema);

G_END_DECLS

#endif
