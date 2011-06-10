/*
 * Copyright (C) 2008 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_JDBC_RECORDSET_H__
#define __GDA_JDBC_RECORDSET_H__

#include <libgda/libgda.h>
#include <providers-support/gda-data-select-priv.h>
#include "gda-jdbc-pstmt.h"

G_BEGIN_DECLS

#define GDA_TYPE_JDBC_RECORDSET            (gda_jdbc_recordset_get_type())
#define GDA_JDBC_RECORDSET(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_JDBC_RECORDSET, GdaJdbcRecordset))
#define GDA_JDBC_RECORDSET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_JDBC_RECORDSET, GdaJdbcRecordsetClass))
#define GDA_IS_JDBC_RECORDSET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_JDBC_RECORDSET))
#define GDA_IS_JDBC_RECORDSET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_JDBC_RECORDSET))

typedef struct _GdaJdbcRecordset        GdaJdbcRecordset;
typedef struct _GdaJdbcRecordsetClass   GdaJdbcRecordsetClass;
typedef struct _GdaJdbcRecordsetPrivate GdaJdbcRecordsetPrivate;

struct _GdaJdbcRecordset {
	GdaDataSelect            model;
	GdaJdbcRecordsetPrivate *priv;
};

struct _GdaJdbcRecordsetClass {
	GdaDataSelectClass       parent_class;
};

GType         gda_jdbc_recordset_get_type  (void) G_GNUC_CONST;
GdaDataModel *gda_jdbc_recordset_new       (GdaConnection *cnc, GdaJdbcPStmt *ps, GdaSet *exec_params,
					    JNIEnv *jenv, GValue *rs_value,
					    GdaDataModelAccessFlags flags, GType *col_types);

G_END_DECLS

#endif
