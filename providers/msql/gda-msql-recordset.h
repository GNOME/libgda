/* GDA mSQL Provider
 * Copyright (C) 1998-2002 The GNOME Foundation
 *
 * AUTHORS:
 * 	   Danilo Schoeneberg <dj@starfire-programming.net
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __gda_msql_recordset_h__
#define __gda_msql_recordset_h__

#include <libgda/gda-connection.h>
#include <libgda/gda-data-model.h>

G_BEGIN_DECLS

#define GDA_TYPE_MSQL_RECORDSET \
  (gda_msql_recordset_get_type())

#define GDA_MSQL_RECORDSET(o) \
  (G_TYPE_CHECK_INSTANCE_CAST(o,GDA_TYPE_MSQL_RECORDSET,GdaMsqlRecordset))

#define GDA_MSQL_RECORDSET_CLASS(cl) \
  (G_TYPE_CHECK_CLASS_CAST(cl,GDA_TYPE_MSQL_RECORDSET,GdaMsqlRecordsetClass))

#define GDA_IS_MSQL_RECORDSET(o) \
  (G_TYPE_CHECK_INSTANCE_TYPE(o,GDA_TYPE_MSQL_RECORDSET))

#define GDA_IS_MSQL_RECORDSET_CLASS(cl) \
  (G_TYPE_CHECK_CLASS_TYPE((cl),GDA_TYPE_MSQL_RECORDSET))

typedef struct _GdaMsqlRecordset      GdaMsqlRecordset;
typedef struct _GdaMsqlRecordsetClass GdaMsqlRecordsetClass;

struct _GdaMsqlRecordset {
  GdaDataModel     model;
  GPtrArray       *rows;
  GdaConnection   *cnc;
  m_result        *res;
  int              sock;
};

struct _GdaMsqlRecordsetClass {
  GdaDataModelClass parent_class;
};

GType             gda_msql_recordset_get_type();
GdaMsqlRecordset *gda_msql_recordset_new(GdaConnection *cnc,m_result *res,
                                         int sock);

G_END_DECLS

#endif
