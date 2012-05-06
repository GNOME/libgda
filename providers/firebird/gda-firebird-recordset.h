/*
 * Copyright (C) 2002 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2004 Jeronimo Albi <jeronimoalbi@yahoo.com.ar>
 * Copyright (C) 2004 - 2012 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_FIREBIRD_RECORDSET_H__
#define __GDA_FIREBIRD_RECORDSET_H__

#include <libgda/libgda.h>
#include <providers-support/gda-data-select-priv.h>
#include "gda-firebird-pstmt.h"

G_BEGIN_DECLS

#define GDA_TYPE_FIREBIRD_RECORDSET            (gda_firebird_recordset_get_type())
#define GDA_FIREBIRD_RECORDSET(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_FIREBIRD_RECORDSET, GdaFirebirdRecordset))
#define GDA_FIREBIRD_RECORDSET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_FIREBIRD_RECORDSET, GdaFirebirdRecordsetClass))
#define GDA_IS_FIREBIRD_RECORDSET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_FIREBIRD_RECORDSET))
#define GDA_IS_FIREBIRD_RECORDSET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_FIREBIRD_RECORDSET))


#ifdef VMS
#define FB_ALIGN(n,b)              (n)
#endif

#ifdef sun
#ifdef sparc
#define FB_ALIGN(n,b)          ((n + b - 1) & ~(b - 1))
#endif
#endif

#ifdef hpux
#define FB_ALIGN(n,b)          ((n + b - 1) & ~(b - 1))
#endif

#ifdef _AIX
#define FB_ALIGN(n,b)          ((n + b - 1) & ~(b - 1))
#endif
 
#if (defined(_MSC_VER) && defined(WIN32)) 
#define FB_ALIGN(n,b)          ((n + b - 1) & ~(b - 1))
#endif

#ifndef FB_ALIGN
#define FB_ALIGN(n,b)          ((n+1) & ~1)
#endif


typedef struct _GdaFirebirdRecordset        GdaFirebirdRecordset;
typedef struct _GdaFirebirdRecordsetClass   GdaFirebirdRecordsetClass;
typedef struct _GdaFirebirdRecordsetPrivate GdaFirebirdRecordsetPrivate;

struct _GdaFirebirdRecordset {
	GdaDataSelect                model;
	GdaFirebirdRecordsetPrivate *priv;

};

struct _GdaFirebirdRecordsetClass {
	GdaDataSelectClass             parent_class;
};

GType         gda_firebird_recordset_get_type  (void) G_GNUC_CONST;
GdaDataModel *gda_firebird_recordset_new (GdaConnection            *cnc,
			    GdaFirebirdPStmt            *ps,
			    GdaSet                   *exec_params,
			    GdaDataModelAccessFlags   flags,
			    GType                    *col_types);

G_END_DECLS

#endif
