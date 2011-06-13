/*
 * Copyright (C) 2009 - 2010 Vivien Malerba <malerba@gnome-db.org>
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
#define GDA_TREE_MGR_SCHEMAS(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_TREE_MGR_SCHEMAS, GdaTreeMgrSchemas))
#define GDA_TREE_MGR_SCHEMAS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_TREE_MGR_SCHEMAS, GdaTreeMgrSchemasClass))
#define GDA_IS_TREE_MGR_SCHEMAS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_TREE_MGR_SCHEMAS))
#define GDA_IS_TREE_MGR_SCHEMAS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_TREE_MGR_SCHEMAS))
#define GDA_TREE_MGR_SCHEMAS_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GDA_TYPE_TREE_MGR_SCHEMAS, GdaTreeMgrSchemasClass))

typedef struct _GdaTreeMgrSchemas GdaTreeMgrSchemas;
typedef struct _GdaTreeMgrSchemasPriv GdaTreeMgrSchemasPriv;
typedef struct _GdaTreeMgrSchemasClass GdaTreeMgrSchemasClass;

struct _GdaTreeMgrSchemas {
	GdaTreeManager        object;
	GdaTreeMgrSchemasPriv *priv;
};

struct _GdaTreeMgrSchemasClass {
	GdaTreeManagerClass   object_class;
};

GType              gda_tree_mgr_schemas_get_type                 (void) G_GNUC_CONST;
GdaTreeManager*    gda_tree_mgr_schemas_new                      (GdaConnection *cnc);

G_END_DECLS

#endif
