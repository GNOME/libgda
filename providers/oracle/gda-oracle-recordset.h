/*
 * Copyright (C) 2002 Rodrigo Moya <rodrigo@src.gnome.org>
 * Copyright (C) 2002 Tim Coleman <tim@timcoleman.com>
 * Copyright (C) 2005 - 2012 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_ORACLE_RECORDSET_H__
#define __GDA_ORACLE_RECORDSET_H__

#include <libgda/libgda.h>
#include <providers-support/gda-data-select-priv.h>
#include "gda-oracle-pstmt.h"

G_BEGIN_DECLS

#define GDA_TYPE_ORACLE_RECORDSET            (gda_oracle_recordset_get_type())
#define GDA_ORACLE_RECORDSET(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_ORACLE_RECORDSET, GdaOracleRecordset))
#define GDA_ORACLE_RECORDSET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_ORACLE_RECORDSET, GdaOracleRecordsetClass))
#define GDA_IS_ORACLE_RECORDSET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_ORACLE_RECORDSET))
#define GDA_IS_ORACLE_RECORDSET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_ORACLE_RECORDSET))

typedef struct _GdaOracleRecordset        GdaOracleRecordset;
typedef struct _GdaOracleRecordsetClass   GdaOracleRecordsetClass;
typedef struct _GdaOracleRecordsetPrivate GdaOracleRecordsetPrivate;

struct _GdaOracleRecordset {
	GdaDataSelect                model;
	GdaOracleRecordsetPrivate *priv;
};

struct _GdaOracleRecordsetClass {
	GdaDataSelectClass             parent_class;
};

GType         gda_oracle_recordset_get_type  (void) G_GNUC_CONST;
GdaDataModel *gda_oracle_recordset_new       (GdaConnection *cnc, GdaOraclePStmt *ps, GdaSet *exec_params,
					      GdaDataModelAccessFlags flags, GType *col_types);

G_END_DECLS

#endif
