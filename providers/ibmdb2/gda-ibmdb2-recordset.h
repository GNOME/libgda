/* GDA DB IBM DB2 provider
 * Copyright (C) 1998-2003 The GNOME Foundation
 *
 * AUTHORS:
 *	Sergey N. Belinsky <sergey_be@mail.ru>
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

#if !defined(__gda_ibmdb2_recordset_h__)
#  define __gda_ibmdb2_recordset_h__

#include <libgda/gda-data-model-hash.h>
#include <libgda/gda-value.h>
#include <sqlcli1.h>

G_BEGIN_DECLS

#define GDA_TYPE_IBMDB2_RECORDSET            (gda_ibmdb2_recordset_get_type())
#define GDA_IBMDB2_RECORDSET(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_IBMDB2_RECORDSET, GdaIBMDB2Recordset))
#define GDA_IBMDB2_RECORDSET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_IBMDB2_RECORDSET, GdaIBMDB2RecordsetClass))
#define GDA_IS_IBMDB2_RECORDSET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_IBMDB2_RECORDSET))
#define GDA_IS_IBMDB2_RECORDSET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_IBMDB2_RECORDSET))

typedef struct _GdaIBMDB2Recordset        GdaIBMDB2Recordset;
typedef struct _GdaIBMDB2RecordsetClass   GdaIBMDB2RecordsetClass;
typedef struct _GdaIBMDB2RecordsetPrivate GdaIBMDB2RecordsetPrivate;
typedef struct _GdaIBMDB2Field            GdaIBMDB2Field;

struct _GdaIBMDB2Recordset {
	GdaDataModel model;
	GdaIBMDB2RecordsetPrivate *priv;
};

struct _GdaIBMDB2RecordsetClass {
	GdaDataModelClass parent_class;
};

struct _GdaIBMDB2RecordsetPrivate
{
        GdaConnection *cnc;
	GdaIBMDB2ConnectionData *conn_data;
	//      GdaValueType *column_types;
		
	SQLHANDLE hstmt;
		
	gint ncols;
	gint nrows;
	gint num_fetched_row;
	gboolean fetched_all_results;
	gint ntypes;

	GPtrArray *columns;
	GPtrArray *rows;
};

struct _GdaIBMDB2Field {
        SQLCHAR      column_name[32];
        SQLSMALLINT  column_name_len;
        SQLSMALLINT  column_type;
	SQLUINTEGER  column_size;
        SQLSMALLINT  column_scale;
	SQLSMALLINT  column_nullable;
        SQLPOINTER   column_data;
	GdaValueType gda_type;
};
																		

GType gda_ibmdb2_recordset_get_type (void);
GdaDataModel *gda_ibmdb2_recordset_new (GdaConnection *cnc, SQLHANDLE hstmt);

G_END_DECLS

#endif

