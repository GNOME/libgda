/*
 * Copyright (C) 2008 - 2013 Vivien Malerba <malerba@gnome-db.org>
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

static void _gda_sqlite_pstmt_dispose    (GObject *object);

typedef struct {
	GWeakRef        provider;
	sqlite3_stmt   *sqlite_stmt;
	gboolean        stmt_used; /* TRUE if a recorset already uses this prepared statement,
				    * necessary because only one recordset can use sqlite_stmt at a time */
	GHashTable      *rowid_hash;
	gint             nb_rowid_columns;
} GdaSqlitePStmtPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GdaSqlitePStmt, _gda_sqlite_pstmt, GDA_TYPE_PSTMT)

static void 
_gda_sqlite_pstmt_class_init (GdaSqlitePStmtClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	/* virtual functions */
	object_class->finalize = _gda_sqlite_pstmt_dispose;
}

static void
_gda_sqlite_pstmt_init (GdaSqlitePStmt *pstmt)
{
	GdaSqlitePStmtPrivate *priv = _gda_sqlite_pstmt_get_instance_private (pstmt);
	priv->sqlite_stmt = NULL;
	priv->stmt_used = FALSE;
	priv->rowid_hash = NULL;
	priv->nb_rowid_columns = 0;
	g_weak_ref_init (&priv->provider, NULL);
}

static void
_gda_sqlite_pstmt_dispose (GObject *object)
{
	GdaSqlitePStmt *pstmt = (GdaSqlitePStmt *) object;
	GdaSqlitePStmtPrivate *priv = _gda_sqlite_pstmt_get_instance_private (pstmt);

	g_return_if_fail (GDA_IS_PSTMT (pstmt));

	/* free memory */
	if (priv->sqlite_stmt != NULL) {
		GdaSqliteProvider *prov = g_weak_ref_get (&priv->provider);
		if (prov != NULL) {
			SQLITE3_CALL (prov, sqlite3_finalize) (priv->sqlite_stmt);
			g_object_unref (prov);
		}
		priv->sqlite_stmt = NULL;
	}

	if (priv->rowid_hash != NULL) {
		g_hash_table_destroy (priv->rowid_hash);
		priv->rowid_hash = NULL;
	}
	g_weak_ref_clear (&priv->provider);

	/* chain to parent class */
	G_OBJECT_CLASS (_gda_sqlite_pstmt_parent_class)->finalize (object);
}

GdaSqlitePStmt *
_gda_sqlite_pstmt_new (GdaSqliteProvider *provider, sqlite3_stmt *sqlite_stmt)
{
	GdaSqlitePStmt *pstmt;

	pstmt = (GdaSqlitePStmt*) g_object_new (GDA_TYPE_SQLITE_PSTMT, NULL);

	GdaSqlitePStmtPrivate *priv = _gda_sqlite_pstmt_get_instance_private (pstmt);

	priv->sqlite_stmt = sqlite_stmt;
	g_weak_ref_set (&priv->provider, provider);
	return pstmt;
}

/**
 * Returns: (transfer full): current #GdaSqliteProvider
 */
GdaSqliteProvider*
_gda_sqlite_pstmt_get_provider (GdaSqlitePStmt *pstmt)
{
	g_return_val_if_fail (pstmt != NULL, NULL);
	g_return_val_if_fail (GDA_IS_SQLITE_PSTMT (pstmt), NULL);
	GdaSqlitePStmtPrivate *priv = _gda_sqlite_pstmt_get_instance_private (pstmt);

	return g_weak_ref_get (&priv->provider);
}
sqlite3_stmt*
_gda_sqlite_pstmt_get_stmt (GdaSqlitePStmt *pstmt)
{
	g_return_val_if_fail (pstmt != NULL, NULL);
	g_return_val_if_fail (GDA_IS_SQLITE_PSTMT (pstmt), NULL);
	GdaSqlitePStmtPrivate *priv = _gda_sqlite_pstmt_get_instance_private (pstmt);

	return priv->sqlite_stmt;
}
gboolean
_gda_sqlite_pstmt_get_is_used  (GdaSqlitePStmt *pstmt)
{
	g_return_val_if_fail (pstmt != NULL, FALSE);
	g_return_val_if_fail (GDA_IS_SQLITE_PSTMT (pstmt), FALSE);
	GdaSqlitePStmtPrivate *priv = _gda_sqlite_pstmt_get_instance_private (pstmt);

	return priv->stmt_used;
}
void
_gda_sqlite_pstmt_set_is_used (GdaSqlitePStmt *pstmt,
                               gboolean used)
{
	g_return_if_fail (pstmt != NULL);
	g_return_if_fail (GDA_IS_SQLITE_PSTMT (pstmt));
	GdaSqlitePStmtPrivate *priv = _gda_sqlite_pstmt_get_instance_private (pstmt);
	priv->stmt_used = used;
}

GHashTable*
_gda_sqlite_pstmt_get_rowid_hash (GdaSqlitePStmt *pstmt)
{
	g_return_val_if_fail (pstmt != NULL, NULL);
	g_return_val_if_fail (GDA_IS_SQLITE_PSTMT (pstmt), NULL);
	GdaSqlitePStmtPrivate *priv = _gda_sqlite_pstmt_get_instance_private (pstmt);

	return priv->rowid_hash;
}
void
_gda_sqlite_pstmt_set_rowid_hash (GdaSqlitePStmt *pstmt,
                                  GHashTable *hash)
{
	g_return_if_fail (pstmt != NULL);
	g_return_if_fail (GDA_IS_SQLITE_PSTMT (pstmt));
	GdaSqlitePStmtPrivate *priv = _gda_sqlite_pstmt_get_instance_private (pstmt);

	if (priv->rowid_hash != NULL) {
		g_hash_table_unref (priv->rowid_hash);
		priv->rowid_hash = NULL;
	}
	if (hash != NULL) {
		priv->rowid_hash = g_hash_table_ref (hash);
	} else {
		priv->rowid_hash = NULL;
	}
}
gint
_gda_sqlite_pstmt_get_nb_rowid_columns (GdaSqlitePStmt *pstmt)
{
	g_return_val_if_fail (pstmt != NULL, -1);
	g_return_val_if_fail (GDA_IS_SQLITE_PSTMT (pstmt), -1);
	GdaSqlitePStmtPrivate *priv = _gda_sqlite_pstmt_get_instance_private (pstmt);

	return priv->nb_rowid_columns;
}
void
_gda_sqlite_pstmt_set_nb_rowid_columns (GdaSqlitePStmt *pstmt,
                                        gint nb)
{
	g_return_if_fail (pstmt != NULL);
	g_return_if_fail (GDA_IS_SQLITE_PSTMT (pstmt));
	GdaSqlitePStmtPrivate *priv = _gda_sqlite_pstmt_get_instance_private (pstmt);

	priv->nb_rowid_columns = nb;
}

