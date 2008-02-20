/* GDA postgres provider
 * Copyright (C) 2008 The GNOME Foundation.
 *
 * AUTHORS:
 *         Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_POSTGRES_PSTMT_H__
#define __GDA_POSTGRES_PSTMT_H__

#include <providers-support/gda-pstmt.h>
#include "gda-postgres.h"

G_BEGIN_DECLS

#define GDA_TYPE_POSTGRES_PSTMT            (gda_postgres_pstmt_get_type())
#define GDA_POSTGRES_PSTMT(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_POSTGRES_PSTMT, GdaPostgresPStmt))
#define GDA_POSTGRES_PSTMT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_POSTGRES_PSTMT, GdaPostgresPStmtClass))
#define GDA_IS_POSTGRES_PSTMT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_POSTGRES_PSTMT))
#define GDA_IS_POSTGRES_PSTMT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_POSTGRES_PSTMT))

typedef struct _GdaPostgresPStmt        GdaPostgresPStmt;
typedef struct _GdaPostgresPStmtClass   GdaPostgresPStmtClass;

struct _GdaPostgresPStmt {
	GdaPStmt        object;
	
	/* PostgreSQL identifies a prepared statement by its name, which we'll keep here,
	 * along with a pointer to a GdaConnection and a PGconn because when the prepared
	 * statement is destroyed, we need to call "DEALLOCATE..."
	 */
	GdaConnection  *cnc;
	PGconn         *pconn;
	gchar          *prep_name;
};

struct _GdaPostgresPStmtClass {
	GdaPStmtClass  parent_class;
};

GType             gda_postgres_pstmt_get_type  (void) G_GNUC_CONST;
GdaPostgresPStmt *gda_postgres_pstmt_new       (GdaConnection *cnc, PGconn *pconn, const gchar *prep_name);

G_END_DECLS

#endif
