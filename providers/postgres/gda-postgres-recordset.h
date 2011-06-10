/*
 * Copyright (C) 2000 Reinhard MÃ¼ller <reinhard@src.gnome.org>
 * Copyright (C) 2000 - 2004 Rodrigo Moya <rodrigo@ximian.com>
 * Copyright (C) 2001 Carlos Perelló Marín <carlos@gnome-db.org>
 * Copyright (C) 2001 - 2008 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2002 Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
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

#ifndef __GDA_POSTGRES_RECORDSET_H__
#define __GDA_POSTGRES_RECORDSET_H__

#include <libgda/libgda.h>
#include <providers-support/gda-data-select-priv.h>
#include "gda-postgres-pstmt.h"

G_BEGIN_DECLS

#define GDA_TYPE_POSTGRES_RECORDSET            (gda_postgres_recordset_get_type())
#define GDA_POSTGRES_RECORDSET(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_POSTGRES_RECORDSET, GdaPostgresRecordset))
#define GDA_POSTGRES_RECORDSET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_POSTGRES_RECORDSET, GdaPostgresRecordsetClass))
#define GDA_IS_POSTGRES_RECORDSET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_POSTGRES_RECORDSET))
#define GDA_IS_POSTGRES_RECORDSET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_POSTGRES_RECORDSET))

typedef struct _GdaPostgresRecordset        GdaPostgresRecordset;
typedef struct _GdaPostgresRecordsetClass   GdaPostgresRecordsetClass;
typedef struct _GdaPostgresRecordsetPrivate GdaPostgresRecordsetPrivate;

struct _GdaPostgresRecordset {
	GdaDataSelect                    model;
	GdaPostgresRecordsetPrivate     *priv;
};

struct _GdaPostgresRecordsetClass {
	GdaDataSelectClass               parent_class;
};

GType         gda_postgres_recordset_get_type   (void) G_GNUC_CONST;
GdaDataModel *gda_postgres_recordset_new_random (GdaConnection *cnc, GdaPostgresPStmt *ps, GdaSet *exec_params, PGresult *pg_res, GType *col_types);
GdaDataModel *gda_postgres_recordset_new_cursor (GdaConnection *cnc, GdaPostgresPStmt *ps, GdaSet *exec_params, gchar *cursor_name, GType *col_types);


G_END_DECLS

#endif
