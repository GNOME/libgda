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
#include <libgda/gda-column.h>
#include "gda-pstmt.h"
#include <string.h>

static void gda_pstmt_class_init (GdaPStmtClass *klass);
static void gda_pstmt_init       (GdaPStmt *pstmt, GdaPStmtClass *klass);
static void gda_pstmt_dispose    (GObject *object);
static void gda_pstmt_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

struct _GdaPStmtPrivate {
	GdaStatement *gda_stmt; /* GdaPStmt object holds a weak reference on this stmt object, may be NULL */
};

/**
 * gda_pstmt_get_type
 *
 * Returns: the #GType of GdaPStmt.
 */
GType
gda_pstmt_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static const GTypeInfo info = {
			sizeof (GdaPStmtClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_pstmt_class_init,
			NULL,
			NULL,
			sizeof (GdaPStmt),
			0,
			(GInstanceInitFunc) gda_pstmt_init
		};

		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (G_TYPE_OBJECT, "GdaPStmt", &info, G_TYPE_FLAG_ABSTRACT);
		g_static_mutex_unlock (&registering);
	}
	return type;
}

static void 
gda_pstmt_class_init (GdaPStmtClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	/* virtual functions */
	object_class->dispose = gda_pstmt_dispose;
	object_class->finalize = gda_pstmt_finalize;
}

static void
gda_pstmt_init (GdaPStmt *pstmt, G_GNUC_UNUSED GdaPStmtClass *klass)
{
	g_return_if_fail (GDA_IS_PSTMT (pstmt));
	pstmt->priv = g_new0 (GdaPStmtPrivate, 1);
	pstmt->priv->gda_stmt = NULL;
	pstmt->sql = NULL;
	pstmt->param_ids = NULL;
	pstmt->ncols = -1;
	pstmt->types = NULL;
	pstmt->tmpl_columns = NULL;
}

static void
gda_stmt_reset_cb (GdaStatement *stmt, GdaPStmt *pstmt)
{
	g_signal_handlers_disconnect_by_func (G_OBJECT (stmt), 
					      G_CALLBACK (gda_stmt_reset_cb), pstmt);
	g_object_remove_weak_pointer ((GObject*) pstmt->priv->gda_stmt, (gpointer*) &(pstmt->priv->gda_stmt));
	pstmt->priv->gda_stmt = NULL;
}

static void
gda_pstmt_dispose (GObject *object)
{
	GdaPStmt *pstmt = (GdaPStmt *) object;

	if (pstmt->priv->gda_stmt) 
		gda_stmt_reset_cb (pstmt->priv->gda_stmt, pstmt);

	/* chain to parent class */
	parent_class->dispose (object);
}

static void
gda_pstmt_finalize (GObject *object)
{
	GdaPStmt *pstmt = (GdaPStmt *) object;

	/* free memory */
	g_free (pstmt->priv);

	if (pstmt->sql) {
		g_free (pstmt->sql);
		pstmt->sql = NULL;
	}
	if (pstmt->param_ids) {
		g_slist_foreach (pstmt->param_ids, (GFunc) g_free, NULL);
		g_slist_free (pstmt->param_ids);
		pstmt->param_ids = NULL;
	}
	if (pstmt->types) {
		g_free (pstmt->types);
		pstmt->types = NULL;
	}
	if (pstmt->tmpl_columns) {
		g_slist_foreach (pstmt->tmpl_columns, (GFunc) g_object_unref, NULL);
		g_slist_free (pstmt->tmpl_columns);
	}

	/* chain to parent class */
	parent_class->finalize (object);
}

/**
 * gda_pstmt_set_gda_statement
 * @pstmt: a #GdaPStmt object
 * @stmt: a #GdaStatement object
 *
 * Informs @pstmt that it corresponds to the preparation of the @stmt statement
 */
void
gda_pstmt_set_gda_statement (GdaPStmt *pstmt, GdaStatement *stmt)
{
	g_return_if_fail (GDA_IS_PSTMT (pstmt));
	g_return_if_fail (!stmt || GDA_IS_STATEMENT (stmt));

	if (pstmt->priv->gda_stmt == stmt)
		return;
	if (pstmt->priv->gda_stmt) 
		gda_stmt_reset_cb (pstmt->priv->gda_stmt, pstmt);

	pstmt->priv->gda_stmt = stmt;
	if (stmt) {
		g_object_add_weak_pointer ((GObject*) stmt, (gpointer*) &(pstmt->priv->gda_stmt));
		g_signal_connect (G_OBJECT (stmt), "reset", G_CALLBACK (gda_stmt_reset_cb), pstmt);
	}
}

/**
 * gda_pstmt_copy_contents
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

	g_free (dest->sql);
	dest->sql = NULL;
	if (src->sql)
		dest->sql = g_strdup (src->sql);
	if (dest->param_ids) {
		g_slist_foreach (dest->param_ids, (GFunc) g_free, NULL);
		g_slist_free (dest->param_ids);
		dest->param_ids = NULL;
	}
	for (list = src->param_ids; list; list = list->next) 
		dest->param_ids = g_slist_append (dest->param_ids, g_strdup ((gchar *) list->data));
	dest->ncols = src->ncols;

	g_free (dest->types);
	dest->types = NULL;
	if (src->types) {
		dest->types = g_new (GType, dest->ncols);
		memcpy (dest->types, src->types, sizeof (GType) * dest->ncols);
	}
	if (src->tmpl_columns) {
		GSList *list;
		for (list = src->tmpl_columns; list; list = list->next) 
			dest->tmpl_columns = g_slist_append (dest->tmpl_columns, 
							     gda_column_copy (GDA_COLUMN (list->data)));
	}
	if (src->priv->gda_stmt)
		gda_pstmt_set_gda_statement (dest, src->priv->gda_stmt);
}

/**
 * gda_pstmt_get_gda_statement
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
	return pstmt->priv->gda_stmt;
}
