/*
 * Copyright (C) 2000 Reinhard MÃ¼ller <reinhard@src.gnome.org>
 * Copyright (C) 2000 - 2002 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2001 Carlos Perelló Marín <carlos@gnome-db.org>
 * Copyright (C) 2001 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2002 - 2003 Gonzalo Paniagua Javier <gonzalo@ximian.com>
 * Copyright (C) 2005 Bas Driessen <bas.driessen@xobas.com>
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

#ifndef __GDA_MYSQL_RECORDSET_H__
#define __GDA_MYSQL_RECORDSET_H__

#include <libgda/libgda.h>
#include <providers-support/gda-data-select-priv.h>
#include "gda-mysql-pstmt.h"

G_BEGIN_DECLS

#define GDA_TYPE_MYSQL_RECORDSET            (gda_mysql_recordset_get_type())
#define GDA_MYSQL_RECORDSET(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_MYSQL_RECORDSET, GdaMysqlRecordset))
#define GDA_MYSQL_RECORDSET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_MYSQL_RECORDSET, GdaMysqlRecordsetClass))
#define GDA_IS_MYSQL_RECORDSET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_MYSQL_RECORDSET))
#define GDA_IS_MYSQL_RECORDSET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_MYSQL_RECORDSET))

typedef struct _GdaMysqlRecordset        GdaMysqlRecordset;
typedef struct _GdaMysqlRecordsetClass   GdaMysqlRecordsetClass;
typedef struct _GdaMysqlRecordsetPrivate GdaMysqlRecordsetPrivate;

struct _GdaMysqlRecordset {
	GdaDataSelect              model;
	GdaMysqlRecordsetPrivate  *priv;
};

struct _GdaMysqlRecordsetClass {
	GdaDataSelectClass         parent_class;
};

GType gda_mysql_recordset_get_type  (void) G_GNUC_CONST;
GdaDataModel *gda_mysql_recordset_new       (GdaConnection *cnc, GdaMysqlPStmt *ps,
					     GdaSet *exec_params, GdaDataModelAccessFlags flags, 
					     GType *col_types);
GdaDataModel *gda_mysql_recordset_new_direct (GdaConnection *cnc, GdaDataModelAccessFlags flags, 
					      GType *col_types);

gint gda_mysql_recordset_get_chunk_size (GdaMysqlRecordset  *recset);
void gda_mysql_recordset_set_chunk_size (GdaMysqlRecordset  *recset, gint chunk_size);
gint gda_mysql_recordset_get_chunks_read (GdaMysqlRecordset  *recset);

G_END_DECLS

#endif
