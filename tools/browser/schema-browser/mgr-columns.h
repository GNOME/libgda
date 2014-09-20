/*
 * Copyright (C) 2009 - 2014 Vivien Malerba <malerba@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef ___MGR_COLUMNS_H__
#define ___MGR_COLUMNS_H__

#include <libgda/gda-tree-manager.h>
#include "t-connection.h"

G_BEGIN_DECLS

#define MGR_COLUMNS_TYPE            (mgr_columns_get_type())
#define MGR_COLUMNS(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, MGR_COLUMNS_TYPE, MgrColumns))
#define MGR_COLUMNS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, MGR_COLUMNS_TYPE, MgrColumnsClass))
#define IS_MGR_COLUMNS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, MGR_COLUMNS_TYPE))
#define IS_MGR_COLUMNS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), MGR_COLUMNS_TYPE))

typedef struct _MgrColumns MgrColumns;
typedef struct _MgrColumnsPriv MgrColumnsPriv;
typedef struct _MgrColumnsClass MgrColumnsClass;

/* error reporting */
extern GQuark mgr_columns_error_quark (void);
#define MGR_COLUMNS_ERROR mgr_columns_error_quark ()

typedef enum
{
        MGR_COLUMNS_TABLE_NOT_FOUND,
        MGR_COLUMNS_WRONG_OBJ_TYPE,
        MGR_COLUMNS_NO_META_STRUCT
} MgrColumnsError;

struct _MgrColumns {
	GdaTreeManager      object;
	MgrColumnsPriv     *priv;
};

struct _MgrColumnsClass {
	GdaTreeManagerClass object_class;
};

GType           mgr_columns_get_type                 (void) G_GNUC_CONST;
GdaTreeManager* mgr_columns_new                      (TConnection *tcnc, const gchar *schema, const gchar *table);

#define MGR_COLUMNS_COL_NAME_ATT_NAME "markup"
#define MGR_COLUMNS_COL_TYPE_ATT_NAME "type"
#define MGR_COLUMNS_COL_NOTNULL_ATT_NAME "notnull"
#define MGR_COLUMNS_COL_DEFAULT_ATT_NAME "default"
#define MGR_COLUMNS_COL_DETAILS "details"

G_END_DECLS

#endif
