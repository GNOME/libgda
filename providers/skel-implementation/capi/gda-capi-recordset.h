/*
 * Copyright (C) YEAR The GNOME Foundation.
 *
 * AUTHORS:
 *      TO_ADD: your name and email
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
 * write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef __GDA_CAPI_RECORDSET_H__
#define __GDA_CAPI_RECORDSET_H__

#include <libgda/libgda.h>
#include <providers-support/gda-data-select-priv.h>
#include "gda-capi-pstmt.h"

G_BEGIN_DECLS

#define GDA_TYPE_CAPI_RECORDSET            (gda_capi_recordset_get_type())
G_DECLARE_DERIVABLE_TYPE (GdaCapiRecordset, gda_capi_recordset, GDA, CAPI_RECORDSET, GdaDataSelect)
struct _GdaCapiRecordsetClass {
	GdaDataSelectClass             parent_class;
};

GdaDataModel *gda_capi_recordset_new       (GdaConnection *cnc, GdaCapiPStmt *ps, GdaSet *exec_params,
					    GdaDataModelAccessFlags flags, GType *col_types);

G_END_DECLS

#endif
