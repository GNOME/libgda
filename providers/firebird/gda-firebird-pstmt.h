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

#ifndef __GDA_FIREBIRD_PSTMT_H__
#define __GDA_FIREBIRD_PSTMT_H__

#include <providers-support/gda-pstmt.h>
#include "gda-firebird.h"

G_BEGIN_DECLS

#define GDA_TYPE_FIREBIRD_PSTMT            (gda_firebird_pstmt_get_type())
#define GDA_FIREBIRD_PSTMT(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_FIREBIRD_PSTMT, GdaFirebirdPStmt))
#define GDA_FIREBIRD_PSTMT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_FIREBIRD_PSTMT, GdaFirebirdPStmtClass))
#define GDA_IS_FIREBIRD_PSTMT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_FIREBIRD_PSTMT))
#define GDA_IS_FIREBIRD_PSTMT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_FIREBIRD_PSTMT))

typedef struct _GdaFirebirdPStmt        GdaFirebirdPStmt;
typedef struct _GdaFirebirdPStmtClass   GdaFirebirdPStmtClass;

struct _GdaFirebirdPStmt {
	GdaPStmt        object;

	isc_stmt_handle	stmt_h;
	ISC_STATUS	status[20];
	XSQLDA 	       *sqlda;
	XSQLDA 	       *input_sqlda;
	gint		statement_type;
	gboolean	is_non_select;
};

struct _GdaFirebirdPStmtClass {
	GdaPStmtClass  parent_class;
};

GType         gda_firebird_pstmt_get_type  (void) G_GNUC_CONST;
/* TO_ADD: helper function to create a GdaFirebirdPStmt such as gda_firebird_pstmt_new() with some specific arguments */

G_END_DECLS

#endif
