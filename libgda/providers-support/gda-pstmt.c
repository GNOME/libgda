/*
 * Copyright (C) 2008 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2010 Jonh Wendell <jwendell@gnome.org>
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
#include <libgda/gda-column.h>
#include "gda-pstmt.h"
#include <string.h>

static void gda_pstmt_dispose    (GObject *object);
static void gda_pstmt_finalize    (GObject *object);

typedef struct {
	gchar        *sql; /* actual SQL code used for this prepared statement, mem freed by GdaPStmt */
	GSList       *param_ids; /* list of parameters' IDs (as gchar *), mem freed by GdaPStmt */

	/* meta data */
	gint          ncols;
	GType        *types; /* array of ncols types */
	GSList       *tmpl_columns; /* list of #GdaColumn objects which data models created from this prep. statement
				     * can copy */

	GRecMutex mutex;
	GWeakRef gda_stmt_ref; /* holds a weak reference to #GdaStatement, or %NULL */
} GdaPStmtPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GdaPStmt, gda_pstmt, G_TYPE_OBJECT)

static void 
gda_pstmt_class_init (GdaPStmtClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	/* virtual functions */
	object_class->dispose = gda_pstmt_dispose;
	object_class->finalize = gda_pstmt_finalize;
}

static void
gda_pstmt_init (GdaPStmt *pstmt)
{
	g_return_if_fail (GDA_IS_PSTMT (pstmt));
	GdaPStmtPrivate *priv = gda_pstmt_init_get_instance_private (pstmt);
	g_rec_mutex_init (& priv->mutex);
	g_weak_ref_init (&priv->gda_stmt_ref, NULL);
	priv->sql = NULL;
	priv->param_ids = NULL;
	priv->ncols = -1;
	priv->types = NULL;
	priv->tmpl_columns = NULL;
}

/*
 * @stmt may be %NULL
 */
static void
gda_stmt_reset_cb (GdaStatement *stmt, GdaPStmt *pstmt)
{
	GdaPStmtPrivate *priv = gda_pstmt_init_get_instance_private (pstmt);
	g_rec_mutex_lock (& priv->mutex);
	if (stmt) {
		g_object_ref (stmt);
		g_signal_handlers_disconnect_by_func (G_OBJECT (stmt),
						      G_CALLBACK (gda_stmt_reset_cb), pstmt);
		g_object_unref (stmt);
	}
	else {
		GdaStatement *stm = g_weak_ref_get (& priv->gda_stmt_ref);
		if (stm) {
			g_signal_handlers_disconnect_by_func (G_OBJECT (stm),
							      G_CALLBACK (gda_stmt_reset_cb), pstmt);
			g_object_unref (stm);
		}
	}

	g_weak_ref_set (& priv->gda_stmt_ref, NULL);
	g_rec_mutex_unlock (& priv->mutex);
}

static void
gda_pstmt_dispose (GObject *object)
{
	GdaPStmt *pstmt = (GdaPStmt *) object;

	gda_stmt_reset_cb (NULL, pstmt);

	/* chain to parent class */
	G_OBJECT_CLASS (gda_pstmt_parent_class)->dispose (object);
}

static void
gda_pstmt_finalize (GObject *object)
{
	GdaPStmt *pstmt = (GdaPStmt *) object;
	GdaPStmtPrivate *priv = gda_pstmt_init_get_instance_private (pstmt);

	/* free memory */
	g_rec_mutex_clear (& priv->mutex);
	g_weak_ref_clear (&priv->gda_stmt_ref);

	if (priv->sql) {
		g_free (priv->sql);
		priv->sql = NULL;
	}
	if (priv->param_ids) {
		g_slist_foreach (priv->param_ids, (GFunc) g_free, NULL);
		g_slist_free (priv->param_ids);
		priv->param_ids = NULL;
	}
	if (priv->types) {
		g_free (priv->types);
		priv->types = NULL;
	}
	if (priv->tmpl_columns) {
		g_slist_foreach (priv->tmpl_columns, (GFunc) g_object_unref, NULL);
		g_slist_free (priv->tmpl_columns);
	}

	/* chain to parent class */
	G_OBJECT_CLASS (gda_pstmt_parent_class)->finalize (object);
}

/**
 * gda_pstmt_set_gda_statement:
 * @pstmt: a #GdaPStmt object
 * @stmt: (allow-none): a #GdaStatement object, or %NULL
 *
 * Informs @pstmt that it corresponds to the preparation of the @stmt statement
 */
void
gda_pstmt_set_gda_statement (GdaPStmt *pstmt, GdaStatement *stmt)
{
	g_return_if_fail (GDA_IS_PSTMT (pstmt));
	g_return_if_fail (!stmt || GDA_IS_STATEMENT (stmt));
	GdaPStmtPrivate *priv = gda_pstmt_init_get_instance_private (pstmt);

	g_rec_mutex_lock (& priv->mutex);
	g_object_ref (stmt);

	GdaStatement *estmt;
	estmt = g_weak_ref_get (& priv->gda_stmt_ref);
	if (estmt == stmt) {
		if (estmt)
			g_object_unref (estmt);
		g_rec_mutex_unlock (& priv->mutex);
		return;
	}

	gda_stmt_reset_cb (NULL, pstmt);

	g_weak_ref_set (& priv->gda_stmt_ref, stmt);
	g_signal_connect (G_OBJECT (stmt), "reset", G_CALLBACK (gda_stmt_reset_cb), pstmt);
	g_object_unref (stmt);
	g_rec_mutex_unlock (& priv->mutex);
}

/**
 * gda_pstmt_copy_contents:
 * @src: a #GdaPStmt object
 * @dest: a #GdaPStmt object
 *
 * Copies @src's data to @dest 
 */
void
gda_pstmt_copy_contents (GdaPStmt *src, GdaPStmt *dest)
{
	GSList *list;
	g_return_if_fail (GDA_IS_PSTMT (src));
	g_return_if_fail (GDA_IS_PSTMT (dest));
	GdaPStmtPrivate *spriv = gda_pstmt_init_get_instance_private (src);
	GdaPStmtPrivate *dpriv = gda_pstmt_init_get_instance_private (dest);

	g_rec_mutex_lock (& spriv->mutex);
	g_rec_mutex_lock (& dpriv->mutex);

	g_free (dpriv->sql);
	dpriv->sql = NULL;
	if (spriv->sql)
		dpriv->sql = g_strdup (spriv->sql);
	if (dpriv->param_ids) {
		g_slist_foreach (dpriv->param_ids, (GFunc) g_free, NULL);
		g_slist_free (dpriv->param_ids);
		dpriv->param_ids = NULL;
	}
	for (list = spriv->param_ids; list; list = list->next)
		dpriv->param_ids = g_slist_append (dpriv->param_ids, g_strdup ((gchar *) list->data));
	dpriv->ncols = spriv->ncols;

	g_free (dpriv->types);
	dpriv->types = NULL;
	if (spriv->types) {
		dpriv->types = g_new (GType, dpriv->ncols);
		memcpy (dpriv->types, spriv->types, sizeof (GType) * dpriv->ncols); /* Flawfinder: ignore */
	}
	if (spriv->tmpl_columns) {
		GSList *list;
		for (list = spriv->tmpl_columns; list; list = list->next)
			dpriv->tmpl_columns = g_slist_append (dpriv->tmpl_columns,
							     gda_column_copy (GDA_COLUMN (list->data)));
	}
	GdaStatement *stmt;
	stmt = g_weak_ref_get (& spriv->gda_stmt_ref);
	if (stmt) {
		gda_pstmt_set_gda_statement (dest, stmt);
		g_object_unref (stmt);
	}

	g_rec_mutex_unlock (& spriv->mutex);
	g_rec_mutex_unlock (& dpriv->mutex);

}

/**
 * gda_pstmt_get_gda_statement:
 * @pstmt: a #GdaPStmt object
 *
 * Get a pointer to the #GdaStatement which led to the creation of this prepared statement.
 * 
 * Note: if that statement has been modified since the creation of @pstmt, then this method
 * will return %NULL
 *
 * Returns: (transfer none): the #GdaStatement
 */
GdaStatement *
gda_pstmt_get_gda_statement (GdaPStmt *pstmt)
{
	g_return_val_if_fail (GDA_IS_PSTMT (pstmt), NULL);
	GdaPStmtPrivate *priv = gda_pstmt_init_get_instance_private (pstmt);
	g_rec_mutex_lock (& priv->mutex);
	GdaStatement *stmt;
	stmt = g_weak_ref_get (& priv->gda_stmt_ref);
	if (stmt)
		g_object_unref (stmt);
	g_rec_mutex_unlock (& priv->mutex);
	return stmt;
}

/**
 * gda_pstmt_get_ncols:
 * @pstmt: a #GdaPStmt
 *
 * Returns: number of columns in preprared statement
 */
gint
gda_pstmt_get_ncols (GdaPStmt *pstmt) {
	g_return_val_if_fail (GDA_IS_PSTMT (pstmt), -1);
	GdaPStmtPrivate *priv = gda_pstmt_init_get_instance_private (pstmt);
	return priv->ncols;
}
/**
 * gda_pstmt_get_tmpl_columns:
 * @pstmt: a #GdaPStmt
 *
 * Returns: (transfer none) (element-type Gda.Column): list of columns
 */
GSList*
gda_pstmt_get_tmpl_columns (GdaPStmt *pstmt) {
	g_return_val_if_fail (GDA_IS_PSTMT (pstmt), NULL);
	GdaPStmtPrivate *priv = gda_pstmt_init_get_instance_private (pstmt);
	return priv->tmpl_columns;
}
/**
 * gda_pstmt_has_types:
 * @pstmt: a #GdaPStmt
 *
 * List of types of the columns.
 *
 * Returns: (transfer none) (array) (element-type GLib.Type): list of columns' types
 */
GType*
gda_pstmt_get_types (GdaPStmt *pstmt) {
	g_return_val_if_fail (GDA_IS_PSTMT (pstmt), FALSE);
	GdaPStmtPrivate *priv = gda_pstmt_init_get_instance_private (pstmt);
	return priv->types;
}

/**
 * gda_pstmt_has_types:
 * @pstmt: a #GdaPStmt
 * @ncols: number of columns to set
 *
 * Set the number of columns in the prepared statement. Resets this statement
 * and its column's types to #GDA_TYPE_NULL
 *
 * Returns: (transfer none) (array) (element-type GLib.Type): list of columns' types
 */
void
gda_pstmt_set_columns (GdaPStmt *pstmt, gint ncols) {
	g_return_if_fail (GDA_IS_PSTMT (pstmt));
	g_return_if_fail (ncols > 0);
	GdaPStmtPrivate *priv = gda_pstmt_init_get_instance_private (pstmt);
	gda_stmt_reset_cb (NULL, pstmt);

	priv->ncols = ncols;
	priv->types = g_new0 (GType, ncols);
	for (int i = 0; i < ncols; i++) {
		priv->types[i] = GDA_TYPE_NULL;
	}
}
