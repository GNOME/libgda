/*
 * Copyright (C) 2008 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
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

#include <glib/gi18n-lib.h>
#include "gda-sqlite-pstmt.h"
#include "gda-sqlite.h"
#include <string.h>

static void gda_sqlite_pstmt_class_init (GdaSqlitePStmtClass *klass);
static void gda_sqlite_pstmt_init       (GdaSqlitePStmt *pstmt, GdaSqlitePStmtClass *klass);
static void gda_sqlite_pstmt_finalize    (GObject *object);

static GObjectClass *parent_class = NULL;

/**
 * _gda_sqlite_pstmt_get_type
 *
 * Returns: the #GType of GdaSqlitePStmt.
 */
GType
_gda_sqlite_pstmt_get_type (void)
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
			(GInstanceInitFunc) gda_sqlite_pstmt_init,
			0
		};

		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GDA_TYPE_PSTMT, CLASS_PREFIX "PStmt", &info, 0);
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
gda_sqlite_pstmt_init (GdaSqlitePStmt *pstmt, G_GNUC_UNUSED GdaSqlitePStmtClass *klass)
{
	g_return_if_fail (GDA_IS_PSTMT (pstmt));
	pstmt->sqlite_stmt = NULL;
	pstmt->stmt_used = FALSE;
	pstmt->rowid_hash = NULL;
	pstmt->nb_rowid_columns = 0;
}

static void
gda_sqlite_pstmt_finalize (GObject *object)
{
	GdaSqlitePStmt *pstmt = (GdaSqlitePStmt *) object;

	g_return_if_fail (GDA_IS_PSTMT (pstmt));

	/* free memory */
	if (pstmt->sqlite_stmt) 
		SQLITE3_CALL (sqlite3_finalize) (pstmt->sqlite_stmt);

	if (pstmt->rowid_hash)
		g_hash_table_destroy (pstmt->rowid_hash);

	/* chain to parent class */
	parent_class->finalize (object);
}

GdaSqlitePStmt *
_gda_sqlite_pstmt_new (sqlite3_stmt *sqlite_stmt)
{
	GdaSqlitePStmt *pstmt;

	pstmt = (GdaSqlitePStmt*) g_object_new (GDA_TYPE_SQLITE_PSTMT, NULL);
	pstmt->sqlite_stmt = sqlite_stmt;
	return pstmt;
}
