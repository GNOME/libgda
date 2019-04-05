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

#ifndef __GDA_MYSQL_PSTMT_H__
#define __GDA_MYSQL_PSTMT_H__

#include <providers-support/gda-pstmt.h>
#include "gda-mysql.h"

G_BEGIN_DECLS

#define GDA_TYPE_MYSQL_PSTMT            (gda_mysql_pstmt_get_type())

G_DECLARE_DERIVABLE_TYPE (GdaMysqlPStmt, gda_mysql_pstmt, GDA, MYSQL_PSTMT, GdaPStmt)

struct _GdaMysqlPStmtClass {
	GdaPStmtClass  parent_class;
};

/* TO_ADD: helper function to create a GdaMysqlPStmt such as gda_mysql_pstmt_new() with some specific arguments */

GdaMysqlPStmt *gda_mysql_pstmt_new                   (GdaConnection  *cnc,
                                                      MYSQL          *mysql,
                                                      MYSQL_STMT     *mysql_stmt);
gboolean       gda_mysql_pstmt_get_stmt_used         (GdaMysqlPStmt *stmt);
void           gda_mysql_pstmt_set_stmt_used         (GdaMysqlPStmt *stmt, gboolean used);
MYSQL_STMT    *gda_mysql_pstmt_get_mysql_stmt        (GdaMysqlPStmt *stmt);
MYSQL_BIND    *gda_mysql_pstmt_get_mysql_bind_result (GdaMysqlPStmt *stmt);
void           gda_mysql_pstmt_set_mysql_bind_result (GdaMysqlPStmt *stmt, MYSQL_BIND *bind);
void           gda_mysql_pstmt_free_mysql_bind_result (GdaMysqlPStmt *stmt);


G_END_DECLS

#endif
