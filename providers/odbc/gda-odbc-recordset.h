/* GDA DB Postgres provider
 * Copyright (C) 1998 - 2004 The GNOME Foundation
 *
 * AUTHORS:
 *      Michael Lausch <michael@lausch.at>
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *      Vivien Malerba <malerba@gnome-db.org>
 *      Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
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

#if !defined(__gda_odbc_recordset_h__)
#  define __gda_odbc_recordset_h__

#include <libgda/gda-data-model-hash.h>
#include <libgda/gda-value.h>

G_BEGIN_DECLS

#define GDA_TYPE_ODBC_RECORDSET            (gda_odbc_recordset_get_type())
#define GDA_ODBC_RECORDSET(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_ODBC_RECORDSET, GdaOdbcRecordset))
#define GDA_ODBC_RECORDSET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_ODBC_RECORDSET, GdaOdbcRecordsetClass))
#define GDA_IS_ODBC_RECORDSET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_ODBC_RECORDSET))
#define GDA_IS_ODBC_RECORDSET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_ODBC_RECORDSET))

typedef struct _GdaOdbcRecordset        GdaOdbcRecordset;
typedef struct _GdaOdbcRecordsetClass   GdaOdbcRecordsetClass;
typedef struct _GdaOdbcRecordsetPrivate GdaOdbcRecordsetPrivate;

struct _GdaOdbcRecordset {
	GdaDataModelHash model;
	GdaOdbcRecordsetPrivate *priv;
};

struct _GdaOdbcRecordsetClass {
	GdaDataModelHashClass parent_class;
};

GType          gda_odbc_recordset_get_type (void);
GdaDataModel *gda_odbc_recordset_new (GdaConnection *cnc, SQLHANDLE stmt );

G_END_DECLS

#endif

