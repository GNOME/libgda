/*
 * Copyright (C) 2000 Reinhard Müller <reinhard@src.gnome.org>
 * Copyright (C) 2000 - 2002 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2001 - 2002 Carlos Perelló Marín <carlos@gnome-db.org>
 * Copyright (C) 2001 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2002 Gonzalo Paniagua Javier <gonzalo@src.gnome.org>
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

#ifndef __GDA_SQLITE_RECORDSET_H__
#define __GDA_SQLITE_RECORDSET_H__

#include <libgda/gda-value.h>
#include <libgda/gda-connection.h>
#include <libgda/providers-support/gda-data-select-priv.h>
#include "gda-sqlite-pstmt.h"

G_BEGIN_DECLS

#define GDA_TYPE_SQLITE_RECORDSET            (gda_sqlite_recordset_get_type())

G_DECLARE_DERIVABLE_TYPE(GdaSqliteRecordset, gda_sqlite_recordset, GDA, SQLITE_RECORDSET, GdaDataSelect)

struct _GdaSqliteRecordsetClass {
	GdaDataSelectClass             parent_class;
};

GdaDataModel *_gda_sqlite_recordset_new       (GdaConnection *cnc, GdaSqlitePStmt *ps, GdaSet *exec_params,
					       GdaDataModelAccessFlags flags, GType *col_types,
					       gboolean force_empty);

G_END_DECLS

#endif
