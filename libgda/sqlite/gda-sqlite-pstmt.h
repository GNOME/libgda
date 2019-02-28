/*
 * Copyright (C) 2008 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2012 Jasper Lievisse Adriaanse <jasper@humppa.nl>
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

#ifndef __GDA_SQLITE_PSTMT_H__
#define __GDA_SQLITE_PSTMT_H__

#include <libgda/sqlite/gda-sqlite-provider.h>
#include <libgda/providers-support/gda-pstmt.h>
#ifdef STATIC_SQLITE
#include "sqlite-src/sqlite3.h"
#else
#  ifdef HAVE_SQLITE
#    include <sqlite3.h>
#  else
#    include "sqlite-src/sqlite3.h"
#endif
#endif

G_BEGIN_DECLS

#define GDA_TYPE_SQLITE_PSTMT            (_gda_sqlite_pstmt_get_type())

G_DECLARE_DERIVABLE_TYPE (GdaSqlitePStmt, _gda_sqlite_pstmt, GDA, SQLITE_PSTMT, GdaPStmt)

struct _GdaSqlitePStmtClass {
	GdaPStmtClass  parent_class;
};

GdaSqlitePStmt                 *_gda_sqlite_pstmt_new          (GdaSqliteProvider *provider,
                                                                sqlite3_stmt *sqlite_stmt);

GdaSqliteProvider              *_gda_sqlite_pstmt_get_provider (GdaSqlitePStmt *pstmt);
sqlite3_stmt                   *_gda_sqlite_pstmt_get_stmt     (GdaSqlitePStmt *pstmt);
gboolean                       _gda_sqlite_pstmt_get_is_used   (GdaSqlitePStmt *pstmt);
void                           _gda_sqlite_pstmt_set_is_used   (GdaSqlitePStmt *pstmt,
                                                                gboolean used);

GHashTable                     *_gda_sqlite_pstmt_get_rowid_hash (GdaSqlitePStmt *pstmt);
void                            _gda_sqlite_pstmt_set_rowid_hash (GdaSqlitePStmt *pstmt,
                                                                  GHashTable *hash);
gint                            _gda_sqlite_pstmt_get_nb_rowid_columns (GdaSqlitePStmt *pstmt);
void                            _gda_sqlite_pstmt_set_nb_rowid_columns (GdaSqlitePStmt *pstmt,
                                                                        gint nb);

G_END_DECLS

#endif
