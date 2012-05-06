/*
 * Copyright (C) 2009 - 2011 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_WEB_RECORDSET_H__
#define __GDA_WEB_RECORDSET_H__

#include <libgda/libgda.h>
#include <providers-support/gda-data-select-priv.h>
#include "gda-web-pstmt.h"

G_BEGIN_DECLS

#define GDA_TYPE_WEB_RECORDSET            (gda_web_recordset_get_type())
#define GDA_WEB_RECORDSET(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_WEB_RECORDSET, GdaWebRecordset))
#define GDA_WEB_RECORDSET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_WEB_RECORDSET, GdaWebRecordsetClass))
#define GDA_IS_WEB_RECORDSET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_WEB_RECORDSET))
#define GDA_IS_WEB_RECORDSET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_WEB_RECORDSET))

typedef struct _GdaWebRecordset        GdaWebRecordset;
typedef struct _GdaWebRecordsetClass   GdaWebRecordsetClass;
typedef struct _GdaWebRecordsetPrivate GdaWebRecordsetPrivate;

struct _GdaWebRecordset {
	GdaDataSelect           model;
	GdaWebRecordsetPrivate *priv;
};

struct _GdaWebRecordsetClass {
	GdaDataSelectClass      parent_class;
};

GType         gda_web_recordset_get_type  (void) G_GNUC_CONST;
GdaDataModel *gda_web_recordset_new       (GdaConnection *cnc, GdaWebPStmt *ps, GdaSet *exec_params,
					   GdaDataModelAccessFlags flags, GType *col_types,
					   const gchar *session_id, xmlNodePtr data_node, GError **error);

gboolean      gda_web_recordset_store     (GdaWebRecordset *rs, xmlNodePtr data_node, GError **error);

G_END_DECLS

#endif
