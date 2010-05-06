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

#ifndef __GDA_TREE_MGR_TABLES_H__
#define __GDA_TREE_MGR_TABLES_H__

#include <libgda/gda-connection.h>
#include "gda-tree-manager.h"

G_BEGIN_DECLS

#define GDA_TYPE_TREE_MGR_TABLES            (gda_tree_mgr_tables_get_type())
#define GDA_TREE_MGR_TABLES(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_TREE_MGR_TABLES, GdaTreeMgrTables))
#define GDA_TREE_MGR_TABLES_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_TREE_MGR_TABLES, GdaTreeMgrTablesClass))
#define GDA_IS_TREE_MGR_TABLES(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_TREE_MGR_TABLES))
#define GDA_IS_TREE_MGR_TABLES_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_TREE_MGR_TABLES))
#define GDA_TREE_MGR_TABLES_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GDA_TYPE_TREE_MGR_TABLES, GdaTreeMgrTablesClass))

typedef struct _GdaTreeMgrTables GdaTreeMgrTables;
typedef struct _GdaTreeMgrTablesPriv GdaTreeMgrTablesPriv;
typedef struct _GdaTreeMgrTablesClass GdaTreeMgrTablesClass;

struct _GdaTreeMgrTables {
	GdaTreeManager        object;
	GdaTreeMgrTablesPriv *priv;
};

struct _GdaTreeMgrTablesClass {
	GdaTreeManagerClass   object_class;
};

GType              gda_tree_mgr_tables_get_type                 (void) G_GNUC_CONST;
GdaTreeManager*    gda_tree_mgr_tables_new                      (GdaConnection *cnc, const gchar *schema);

G_END_DECLS

#endif
