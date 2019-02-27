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

static void gda_pstmt_class_init (GdaPStmtClass *klass);
static void gda_pstmt_init       (GdaPStmt *pstmt, GdaPStmtClass *klass);
static void gda_pstmt_dispose    (GObject *object);
static void gda_pstmt_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

struct _GdaPStmtPrivate {
	GRecMutex mutex;
	GWeakRef gda_stmt_ref; /* holds a weak reference to #GdaStatement, or %NULL */
	gchar        *sql; /* actual SQL code used for this prepared statement, mem freed by GdaPStmt */
	GSList       *param_ids; /* list of parameters' IDs (as gchar *), mem freed by GdaPStmt */

	/* meta data */
	gint          ncols;
	GType        *types; /* array of ncols types */
	GSList       *tmpl_columns; /* list of #GdaColumn objects which data models created from this prep. statement can copy */
};

/**
 * gda_pstmt_get_type:
 *
 * Returns: the #GType of GdaPStmt.
 */
GType
gda_pstmt_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GMutex registering;
		static const GTypeInfo info = {
			sizeof (GdaPStmtClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_pstmt_class_init,
			NULL,
			NULL,
			sizeof (GdaPStmt),
			0,
			(GInstanceInitFunc) gda_pstmt_init,
			0
		};

		g_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (G_TYPE_OBJECT, "GdaPStmt", &info, G_TYPE_FLAG_ABSTRACT);
		g_mutex_unlock (&registering);
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
	g_rec_mutex_init (& pstmt->priv->mutex);
	g_weak_ref_init (&pstmt->priv->gda_stmt_ref, NULL);
	pstmt->priv->sql = NULL;
	pstmt->priv->param_ids = NULL;
	pstmt->priv->ncols = -1;
	pstmt->priv->types = NULL;
	pstmt->priv->tmpl_columns = NULL;
}

/*
 * @stmt may be %NULL
 */
static void
gda_stmt_reset_cb (GdaStatement *stmt, GdaPStmt *pstmt)
{
	g_rec_mutex_lock (& pstmt->priv->mutex);
	if (stmt) {
		g_object_ref (stmt);
		g_signal_handlers_disconnect_by_func (G_OBJECT (stmt),
						      G_CALLBACK (gda_stmt_reset_cb), pstmt);
		g_object_unref (stmt);
	}
	else {
		GdaStatement *stm = g_weak_ref_get (& pstmt->priv->gda_stmt_ref);
		if (stm) {
			g_signal_handlers_disconnect_by_func (G_OBJECT (stm),
							      G_CALLBACK (gda_stmt_reset_cb), pstmt);
			g_object_unref (stm);
		}
	}

	g_weak_ref_set (& pstmt->priv->gda_stmt_ref, NULL);
	g_rec_mutex_unlock (& pstmt->priv->mutex);
}

static void
gda_pstmt_dispose (GObject *object)
{
	GdaPStmt *pstmt = (GdaPStmt *) object;

	gda_stmt_reset_cb (NULL, pstmt);

	/* chain to parent class */
	parent_class->dispose (object);
}

static void
gda_pstmt_finalize (GObject *object)
{
	GdaPStmt *pstmt = (GdaPStmt *) object;

	/* free memory */
	g_rec_mutex_clear (& pstmt->priv->mutex);
	g_weak_ref_clear (&pstmt->priv->gda_stmt_ref);

	if (pstmt->priv->sql != NULL) {
		g_free (pstmt->priv->sql);
		pstmt->priv->sql = NULL;
	}
	if (pstmt->priv->param_ids != NULL) {
		g_slist_foreach (pstmt->priv->param_ids, (GFunc) g_free, NULL);
		g_slist_free (pstmt->priv->param_ids);
		pstmt->priv->param_ids = NULL;
	}
	if (pstmt->priv->types != NULL) {
		g_free (pstmt->priv->types);
		pstmt->priv->types = NULL;
	}
	if (pstmt->priv->tmpl_columns != NULL) {
		g_slist_foreach (pstmt->priv->tmpl_columns, (GFunc) g_object_unref, NULL);
		g_slist_free (pstmt->priv->tmpl_columns);
		pstmt->priv->tmpl_columns = NULL;
	}
	g_free (pstmt->priv);

	/* chain to parent class */
	parent_class->finalize (object);
}

/**
 * gda_pstmt_set_gda_statement:
 * @pstmt: a #GdaPStmt object
 * @stmt: (nullable): a #GdaStatement object, or %NULL
 *
 * Informs @pstmt that it corresponds to the preparation of the @stmt statement
 */
void
gda_pstmt_set_gda_statement (GdaPStmt *pstmt, GdaStatement *stmt)
{
	g_return_if_fail (GDA_IS_PSTMT (pstmt));
	g_return_if_fail (!stmt || GDA_IS_STATEMENT (stmt));

	g_rec_mutex_lock (& pstmt->priv->mutex);
	g_object_ref (stmt);

	GdaStatement *estmt;
	estmt = g_weak_ref_get (& pstmt->priv->gda_stmt_ref);
	if (estmt == stmt) {
		if (estmt)
			g_object_unref (estmt);
		g_rec_mutex_unlock (& pstmt->priv->mutex);
		return;
	}

	gda_stmt_reset_cb (NULL, pstmt);

	g_weak_ref_set (& pstmt->priv->gda_stmt_ref, stmt);
	g_signal_connect (G_OBJECT (stmt), "reset", G_CALLBACK (gda_stmt_reset_cb), pstmt);
	g_object_unref (stmt);
	g_rec_mutex_unlock (& pstmt->priv->mutex);
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

	g_rec_mutex_lock (& src->priv->mutex);
	g_rec_mutex_lock (& dest->priv->mutex);

	g_free (dest->priv->sql);
	dest->priv->sql = NULL;
	if (src->priv->sql)
		dest->priv->sql = g_strdup (src->priv->sql);
	if (dest->priv->param_ids) {
		g_slist_foreach (dest->priv->param_ids, (GFunc) g_free, NULL);
		g_slist_free (dest->priv->param_ids);
		dest->priv->param_ids = NULL;
	}
	for (list = src->priv->param_ids; list; list = list->next)
		dest->priv->param_ids = g_slist_append (dest->priv->param_ids, g_strdup ((gchar *) list->data));
	dest->priv->ncols = src->priv->ncols;
	if (dest->priv->types != NULL) {
		g_free (dest->priv->types);
	}
	dest->priv->types = NULL;
	if (src->priv->types) {
		dest->priv->types = g_new (GType, dest->priv->ncols);
		memcpy (dest->priv->types, src->priv->types, sizeof (GType) * dest->priv->ncols); /* Flawfinder: ignore */
	}
	if (src->priv->tmpl_columns) {
		GSList *list;
		for (list = src->priv->tmpl_columns; list; list = list->next)
			dest->priv->tmpl_columns = g_slist_append (dest->priv->tmpl_columns,
							     gda_column_copy (GDA_COLUMN (list->data)));
	}
	GdaStatement *stmt;
	stmt = g_weak_ref_get (& src->priv->gda_stmt_ref);
	if (stmt) {
		gda_pstmt_set_gda_statement (dest, stmt);
		g_object_unref (stmt);
	}

	g_rec_mutex_unlock (& src->priv->mutex);
	g_rec_mutex_unlock (& dest->priv->mutex);

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
 * Returns: (transfer full): the #GdaStatement
 */
GdaStatement *
gda_pstmt_get_gda_statement (GdaPStmt *pstmt)
{
	g_return_val_if_fail (GDA_IS_PSTMT (pstmt), NULL);
	g_rec_mutex_lock (& pstmt->priv->mutex);
	GdaStatement *stmt;
	stmt = g_weak_ref_get (& pstmt->priv->gda_stmt_ref);
	g_rec_mutex_unlock (& pstmt->priv->mutex);
	return stmt;
}
/**
 * gda_pstmt_get_sql:
 * Actual SQL code used for this prepared statement, mem freed by GdaPStmt
 */
gchar*
gda_pstmt_get_sql (GdaPStmt *ps) {
	return ps->priv->sql;
}
/**
 * gda_pstmt_set_sql:
 *
 * Set SQL code used for this prepared statement, mem freed by GdaPStmt
 */
void
gda_pstmt_set_sql (GdaPStmt *ps, const gchar *sql) {
	ps->priv->sql = g_strdup (sql);
}
/**
 *gda_pstmt_get_param_ids:
 *
 * List of parameters' IDs (as gchar *)
 */
GSList*
gda_pstmt_get_param_ids (GdaPStmt *ps) {
	return ps->priv->param_ids;
}

/**
 *gda_pstmt_set_param_ids:
 *
 * List of parameters' IDs (as gchar *), list is stolen
 */
void
gda_pstmt_set_param_ids    (GdaPStmt *ps, GSList* params) {
	ps->priv->param_ids = params;
}
/**
 * gda_pstmt_get_ncols:
 */
gint
gda_pstmt_get_ncols (GdaPStmt *ps) {
	return ps->priv->ncols;
}
/**
 * gda_pstmt_get_types:
 *
 * Set column's types for statement. @types is stalen and should
 * no be modified
 */
GType*
gda_pstmt_get_types (GdaPStmt *ps) {
	return ps->priv->types;
}
/**
 * gda_pstmt_set_cols:
 *
 * Set column's types for statement. @types is stalen and should
 * no be modified
 */
void
gda_pstmt_set_cols (GdaPStmt *ps, gint ncols, GType *types) {
	ps->priv->ncols = ncols;
	ps->priv->types = types;
}
/**
 *gda_pstmt_get_tmpl_columns:
 *
 * List of #GdaColumn objects which data models created from this
 * prepared statement can copy
 */
GSList*
gda_pstmt_get_tmpl_columns (GdaPStmt *ps) {
	return ps->priv->tmpl_columns;
}
/**
 *gda_pstmt_set_tmpl_columns:
 *
 * Set the list of #GdaColumn objects which data models created from this
 * prepared statement can copy. The list is stolen, so you should not
 * free it.
 */
void
gda_pstmt_set_tmpl_columns (GdaPStmt *ps, GSList* columns) {
	ps->priv->tmpl_columns = columns;
}
