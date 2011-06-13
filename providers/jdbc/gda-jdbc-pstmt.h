/*
 * Copyright (C) 2008 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_JDBC_PSTMT_H__
#define __GDA_JDBC_PSTMT_H__

#include <providers-support/gda-pstmt.h>
#include "gda-jdbc.h"

G_BEGIN_DECLS

#define GDA_TYPE_JDBC_PSTMT            (gda_jdbc_pstmt_get_type())
#define GDA_JDBC_PSTMT(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_JDBC_PSTMT, GdaJdbcPStmt))
#define GDA_JDBC_PSTMT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_JDBC_PSTMT, GdaJdbcPStmtClass))
#define GDA_IS_JDBC_PSTMT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_JDBC_PSTMT))
#define GDA_IS_JDBC_PSTMT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_JDBC_PSTMT))

typedef struct _GdaJdbcPStmt        GdaJdbcPStmt;
typedef struct _GdaJdbcPStmtClass   GdaJdbcPStmtClass;

struct _GdaJdbcPStmt {
	GdaPStmt        object;
	GValue         *pstmt_obj; /* JAVA PreparedStatement object */
};

struct _GdaJdbcPStmtClass {
	GdaPStmtClass  parent_class;
};

GType         gda_jdbc_pstmt_get_type  (void) G_GNUC_CONST;
GdaJdbcPStmt *gda_jdbc_pstmt_new (GValue *pstmt_obj);


G_END_DECLS

#endif
