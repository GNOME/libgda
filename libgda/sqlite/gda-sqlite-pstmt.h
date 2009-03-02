/* GDA common library
 * Copyright (C) 2008 - 2009 The GNOME Foundation.
 *
 * AUTHORS:
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

#ifndef __GDA_SQLITE_PSTMT_H__
#define __GDA_SQLITE_PSTMT_H__

#include <libgda/providers-support/gda-pstmt.h>
#ifdef HAVE_SQLITE
#include <sqlite3.h>
#else
#include "sqlite-src/sqlite3.h"
#endif

G_BEGIN_DECLS

#define GDA_TYPE_SQLITE_PSTMT            (_gda_sqlite_pstmt_get_type())
#define GDA_SQLITE_PSTMT(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_PSTMT, GdaSqlitePStmt))
#define GDA_SQLITE_PSTMT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_PSTMT, GdaSqlitePStmtClass))
#define GDA_IS_SQLITE_PSTMT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_PSTMT))
#define GDA_IS_SQLITE_PSTMT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_PSTMT))

typedef struct _GdaSqlitePStmt        GdaSqlitePStmt;
typedef struct _GdaSqlitePStmtClass   GdaSqlitePStmtClass;

struct _GdaSqlitePStmt {
	GdaPStmt        object;

	sqlite3_stmt   *sqlite_stmt;
	gboolean        stmt_used; /* TRUE if a recorset already uses this prepared statement,
				    * necessary because only one recordset can use sqlite_stmt at a time */
	GHashTable      *rowid_hash;
	gint             nb_rowid_columns;
};

struct _GdaSqlitePStmtClass {
	GdaPStmtClass  parent_class;
};

GType           _gda_sqlite_pstmt_get_type  (void) G_GNUC_CONST;
GdaSqlitePStmt *_gda_sqlite_pstmt_new       (sqlite3_stmt *sqlite_stmt);

G_END_DECLS

#endif
