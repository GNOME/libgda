/* GDA SQLite provider
 * Copyright (C) 1998 - 2006 The GNOME Foundation.
 *
 * AUTHORS:
 *	   Rodrigo Moya <rodrigo@gnome-db.org>
 *         Carlos Perelló Marín <carlos@gnome-db.org>
 *         Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_SQLITE_RECORDSET_H__
#define __GDA_SQLITE_RECORDSET_H__

#include <libgda/gda-value.h>
#include <libgda/gda-connection.h>
#include <libgda/providers-support/gda-data-select-priv.h>
#include "gda-sqlite-pstmt.h"

G_BEGIN_DECLS

#define GDA_TYPE_SQLITE_RECORDSET            (gda_sqlite_recordset_get_type())
#define GDA_SQLITE_RECORDSET(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_SQLITE_RECORDSET, GdaSqliteRecordset))
#define GDA_SQLITE_RECORDSET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_SQLITE_RECORDSET, GdaSqliteRecordsetClass))
#define GDA_IS_SQLITE_RECORDSET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_SQLITE_RECORDSET))
#define GDA_IS_SQLITE_RECORDSET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_SQLITE_RECORDSET))

typedef struct _GdaSqliteRecordset        GdaSqliteRecordset;
typedef struct _GdaSqliteRecordsetClass   GdaSqliteRecordsetClass;
typedef struct _GdaSqliteRecordsetPrivate GdaSqliteRecordsetPrivate;

struct _GdaSqliteRecordset {
	GdaDataSelect                  model;
	GdaSqliteRecordsetPrivate *priv;
};

struct _GdaSqliteRecordsetClass {
	GdaDataSelectClass             parent_class;
};

GType         gda_sqlite_recordset_get_type  (void) G_GNUC_CONST;
GdaDataModel *gda_sqlite_recordset_new       (GdaConnection *cnc, GdaSqlitePStmt *ps, GdaSet *exec_params,
					      GdaDataModelAccessFlags flags, GType *col_types);

G_END_DECLS

#endif
