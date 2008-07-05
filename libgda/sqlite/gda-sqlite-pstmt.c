/* GDA library
 * Copyright (C) 2008 The GNOME Foundation.
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

#include <glib/gi18n-lib.h>
#include "gda-sqlite-pstmt.h"
#include <string.h>

static void gda_sqlite_pstmt_class_init (GdaSqlitePStmtClass *klass);
static void gda_sqlite_pstmt_init       (GdaSqlitePStmt *pstmt, GdaSqlitePStmtClass *klass);
static void gda_sqlite_pstmt_finalize    (GObject *object);

static GObjectClass *parent_class = NULL;

/**
 * gda_sqlite_pstmt_get_type
 *
 * Returns: the #GType of GdaSqlitePStmt.
 */
GType
gda_sqlite_pstmt_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static const GTypeInfo info = {
			sizeof (GdaSqlitePStmtClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_sqlite_pstmt_class_init,
			NULL,
			NULL,
			sizeof (GdaSqlitePStmt),
			0,
			(GInstanceInitFunc) gda_sqlite_pstmt_init
		};

		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GDA_TYPE_PSTMT, "GdaSqlitePStmt", &info, 0);
		g_static_mutex_unlock (&registering);
	}
	return type;
}

static void 
gda_sqlite_pstmt_class_init (GdaSqlitePStmtClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	/* virtual functions */
	object_class->finalize = gda_sqlite_pstmt_finalize;
}

static void
gda_sqlite_pstmt_init (GdaSqlitePStmt *pstmt, GdaSqlitePStmtClass *klass)
{
	g_return_if_fail (GDA_IS_PSTMT (pstmt));
	pstmt->sqlite_stmt = NULL;
	pstmt->stmt_used = FALSE;
}

static void
gda_sqlite_pstmt_finalize (GObject *object)
{
	GdaSqlitePStmt *pstmt = (GdaSqlitePStmt *) object;

	g_return_if_fail (GDA_IS_PSTMT (pstmt));

	/* free memory */
	if (pstmt->sqlite_stmt) 
		sqlite3_finalize (pstmt->sqlite_stmt);

	/* chain to parent class */
	parent_class->finalize (object);
}

GdaSqlitePStmt *
gda_sqlite_pstmt_new (sqlite3_stmt *sqlite_stmt)
{
	GdaSqlitePStmt *pstmt;

	pstmt = (GdaSqlitePStmt*) g_object_new (GDA_TYPE_SQLITE_PSTMT, NULL);
	pstmt->sqlite_stmt = sqlite_stmt;
	return pstmt;
}
