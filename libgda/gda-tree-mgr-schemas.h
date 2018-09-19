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

#ifndef __GDA_TREE_MGR_SCHEMAS_H__
#define __GDA_TREE_MGR_SCHEMAS_H__

#include <libgda/gda-connection.h>
#include "gda-tree-manager.h"

G_BEGIN_DECLS

#define GDA_TYPE_TREE_MGR_SCHEMAS            (gda_tree_mgr_schemas_get_type())
G_DECLARE_DERIVABLE_TYPE(GdaTreeMgrSchemas, gda_tree_mgr_schemas, GDA, TREE_MGR_SCHEMAS, GdaTreeManager)
struct _GdaTreeMgrSchemasClass {
	GdaTreeManagerClass   object_class;
};

/**
 * SECTION:gda-tree-mgr-schemas
 * @short_description: A tree manager which creates a node for each schema in a database
 * @title: GdaTreeMgrSchemas
 * @stability: Stable
 * @see_also:
 *
 * The #GdaTreeMgrSchemas is a #GdaTreeManager object which creates a node for
 * each schema in a database.
 *
 * It uses the #GdaMetaStore associated to a #GdaConnection to get the schemas list; it's up to the
 * caller to make sure the data in the #GdaMetaStore is up to date.
 *
 * The #GdaConnection to be used needs to be specified when the object is created.
 */

GdaTreeManager*    gda_tree_mgr_schemas_new                      (GdaConnection *cnc);

G_END_DECLS

#endif
