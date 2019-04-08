/*
 * Copyright (C) 2000 Reinhard Müller <reinhard@src.gnome.org>
 * Copyright (C) 2000 - 2004 Rodrigo Moya <rodrigo@ximian.com>
 * Copyright (C) 2001 Carlos Perelló Marín <carlos@gnome-db.org>
 * Copyright (C) 2001 - 2012 Vivien Malerba <malerba@gnome-db.org>
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
G_DECLARE_DERIVABLE_TYPE(GdaPostgresRecordset, gda_postgres_recordset, GDA, POSTGRES_RECORDSET, GdaDataSelect)

struct _GdaPostgresRecordsetClass {
	GdaDataSelectClass               parent_class;
};

GdaDataModel *gda_postgres_recordset_new_random (GdaConnection *cnc, GdaPostgresPStmt *ps, GdaSet *exec_params, PGresult *pg_res, GType *col_types);
GdaDataModel *gda_postgres_recordset_new_cursor (GdaConnection *cnc, GdaPostgresPStmt *ps, GdaSet *exec_params, gchar *cursor_name, GType *col_types);


G_END_DECLS

#endif
