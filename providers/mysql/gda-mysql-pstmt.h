/* GDA Mysql library
 * Copyright (C) 2008 The GNOME Foundation.
 *
 * AUTHORS:
 *      Carlos Savoretti <csavoretti@gmail.com>
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

#ifndef __GDA_MYSQL_PSTMT_H__
#define __GDA_MYSQL_PSTMT_H__

#include <providers-support/gda-pstmt.h>
#include "gda-mysql.h"

G_BEGIN_DECLS

#define GDA_TYPE_MYSQL_PSTMT            (gda_mysql_pstmt_get_type())
#define GDA_MYSQL_PSTMT(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_MYSQL_PSTMT, GdaMysqlPStmt))
#define GDA_MYSQL_PSTMT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_MYSQL_PSTMT, GdaMysqlPStmtClass))
#define GDA_IS_MYSQL_PSTMT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_MYSQL_PSTMT))
#define GDA_IS_MYSQL_PSTMT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_MYSQL_PSTMT))

typedef struct _GdaMysqlPStmt        GdaMysqlPStmt;
typedef struct _GdaMysqlPStmtClass   GdaMysqlPStmtClass;

struct _GdaMysqlPStmt {
	GdaPStmt        object;

	GdaConnection  *cnc;
	MYSQL          *mysql;
	MYSQL_STMT     *mysql_stmt;
	
};

struct _GdaMysqlPStmtClass {
	GdaPStmtClass  parent_class;
};

GType
gda_mysql_pstmt_get_type  (void) G_GNUC_CONST;
/* TO_ADD: helper function to create a GdaMysqlPStmt such as gda_mysql_pstmt_new() with some specific arguments */

GdaMysqlPStmt *
gda_mysql_pstmt_new       (GdaConnection  *cnc,
			   MYSQL          *mysql,
			   MYSQL_STMT     *mysql_stmt);


G_END_DECLS

#endif
