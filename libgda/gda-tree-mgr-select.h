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

#ifndef __GDA_TREE_MGR_SELECT_H__
#define __GDA_TREE_MGR_SELECT_H__

#include <libgda/gda-connection.h>
#include "gda-tree-manager.h"

G_BEGIN_DECLS

#define GDA_TYPE_TREE_MGR_SELECT            (gda_tree_mgr_select_get_type())
G_DECLARE_DERIVABLE_TYPE(GdaTreeMgrSelect, gda_tree_mgr_select, GDA, TREE_MGR_SELECT, GdaTreeManager)
struct _GdaTreeMgrSelectClass {
	GdaTreeManagerClass   object_class;
};

/**
 * SECTION:gda-tree-mgr-select
 * @short_description: A tree manager which creates a node for each row resulting from the execution of a SELECT statement
 * @title: GdaTreeMgrSelect
 * @stability: Stable
 * @see_also:
 *
 * The #GdaTreeMgrSelect is a #GdaTreeManager object which executes a SELECT statement and
 * creates a node for each row in the result.
 *
 * The #GdaConnection and SELECT #GdaStatement to be used need to be specified when the object is created.
 * If the SELECT statement to be used needs some parameters, then it is possible to give values to some of them
 * when constructing the object, but not necessary.
 *
 * If the SELECT statement needs some parameters which have not been provided during the construction, then
 * these parameters will be fetched from the #GdaTreeNode below which the nodes will be placed (using
 * gda_tree_node_fetch_attribute()).
 *
 * For each node created, an attribute is set for each column in the SELECT statement: the attribute name is
 * the column name and the attribute value is the value if that column.
 */

GdaTreeManager*    gda_tree_mgr_select_new                      (GdaConnection *cnc, GdaStatement *stmt, GdaSet *params);

G_END_DECLS

#endif
