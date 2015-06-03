/*
 * Copyright (C) 2008 - 2012 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_ORACLE_PSTMT_H__
#define __GDA_ORACLE_PSTMT_H__

#include <providers-support/gda-pstmt.h>
#include "gda-oracle.h"

G_BEGIN_DECLS

#define GDA_TYPE_ORACLE_PSTMT            (gda_oracle_pstmt_get_type())
#define GDA_ORACLE_PSTMT(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_ORACLE_PSTMT, GdaOraclePStmt))
#define GDA_ORACLE_PSTMT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_ORACLE_PSTMT, GdaOraclePStmtClass))
#define GDA_IS_ORACLE_PSTMT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_ORACLE_PSTMT))
#define GDA_IS_ORACLE_PSTMT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_ORACLE_PSTMT))

typedef struct _GdaOraclePStmt        GdaOraclePStmt;
typedef struct _GdaOraclePStmtClass   GdaOraclePStmtClass;

struct _GdaOraclePStmt {
	GdaPStmt        object;
	OCIStmt        *hstmt;
	GList          *ora_values; /* list of GdaOracleValue pointers, owned here */
};

struct _GdaOraclePStmtClass {
	GdaPStmtClass  parent_class;
};

GType           gda_oracle_pstmt_get_type  (void) G_GNUC_CONST;
GdaOraclePStmt *gda_oracle_pstmt_new (OCIStmt *hstmt);

G_END_DECLS

#endif
