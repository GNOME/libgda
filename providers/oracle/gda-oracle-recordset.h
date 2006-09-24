/* GDA Oracle provider
 * Copyright (C) 2002 - 2004 The GNOME Foundation.
 *
 * AUTHORS:
 * 	Tim Coleman <tim@timcoleman.com>
 *
 * Borrowed from mysql-oracle-recordset.h, written by
 *      Michael Lausch <michael@lausch.at>
 *	Rodrigo Moya <rodrigo@gnome-db.org>
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
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#if !defined(__gda_oracle_recordset_h__)
#  define __gda_oracle_recordset_h__

#include <libgda/gda-data-model-hash.h>
#include <libgda/gda-value.h>
#include <oci.h>

G_BEGIN_DECLS

#define GDA_TYPE_ORACLE_RECORDSET            (gda_oracle_recordset_get_type())
#define GDA_ORACLE_RECORDSET(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_ORACLE_RECORDSET, GdaOracleRecordset))
#define GDA_ORACLE_RECORDSET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_ORACLE_RECORDSET, GdaOracleRecordsetClass))
#define GDA_IS_ORACLE_RECORDSET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_ORACLE_RECORDSET))
#define GDA_IS_ORACLE_RECORDSET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_ORACLE_RECORDSET))


typedef struct _GdaOracleRecordset        GdaOracleRecordset;
typedef struct _GdaOracleRecordsetClass   GdaOracleRecordsetClass;
typedef struct _GdaOracleRecordsetField   GdaOracleRecordsetField;
typedef struct _GdaOracleRecordsetPrivate GdaOracleRecordsetPrivate;
typedef struct _GdaOracleValue            GdaOracleValue;

struct _GdaOracleRecordset {
	GdaDataModelRow model;
	GdaOracleRecordsetPrivate *priv;
};

struct _GdaOracleRecordsetClass {
	GdaDataModelRowClass parent_class;
};

struct _GdaOracleValue {
	OCIDefine *hdef;
	OCIParam *pard;
	sb2 indicator;
	ub2 sql_type;
	ub2 defined_size;
	gpointer value;
	GType g_type;
};


GType               gda_oracle_recordset_get_type (void);
GdaOracleRecordset *gda_oracle_recordset_new (GdaConnection *cnc, GdaOracleConnectionData *cdata, OCIStmt *stmthp);

G_END_DECLS

#endif
