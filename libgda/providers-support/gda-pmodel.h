/* GDA common library
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

#ifndef __GDA_PMODEL_H__
#define __GDA_PMODEL_H__

#include <glib-object.h>
#include <libgda/gda-row.h>
#include <libgda/providers-support/gda-pstmt.h>
#include <sql-parser/gda-sql-statement.h>

G_BEGIN_DECLS

#define GDA_TYPE_PMODEL            (gda_pmodel_get_type())
#define GDA_PMODEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_PMODEL, GdaPModel))
#define GDA_PMODEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_PMODEL, GdaPModelClass))
#define GDA_IS_PMODEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_PMODEL))
#define GDA_IS_PMODEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_PMODEL))

typedef struct _GdaPModel        GdaPModel;
typedef struct _GdaPModelClass   GdaPModelClass;
typedef struct _GdaPModelPrivate GdaPModelPrivate;

/* error reporting */
extern GQuark gda_pmodel_error_quark (void);
#define GDA_PMODEL_ERROR gda_pmodel_error_quark ()

enum {
	GDA_PMODEL_MODIFICATION_STATEMENT_ERROR,
	GDA_PMODEL_MISSING_MODIFICATION_STATEMENT_ERROR,
	GDA_PMODEL_CONNECTION_ERROR,
	GDA_PMODEL_VALUE_ERROR,
	GDA_PMODEL_ACCESS_ERROR,
	GDA_PMODEL_SQL_ERROR
};

struct _GdaPModel {
	GObject           object;
	GdaPModelPrivate *priv;

	/* read only information */
	GdaPStmt         *prep_stmt; /* use the "prepared-stmt" property to set this */
	gint              nb_stored_rows; /* number of GdaRow objects currently stored */
	gint              advertized_nrows; /* set when the number of rows becomes known, -1 untill then */
};

/*
 * Depending on model access flags, the implementations are:
 *
 * if GDA_DATA_MODEL_ACCESS_RANDOM: 
 *     REQUIRED: fetch_nb_rows, fetch_random
 * if GDA_STATEMENT_MODEL_CURSOR_FORWARD:
 *     REQUIRED: fetch_next
 *     OPTIONAL: fetch_at
 * if GDA_STATEMENT_MODEL_CURSOR_BACKWARD:
 *     REQUIRED: fetch_prev
 *     OPTIONAL: fetch_at
 */
struct _GdaPModelClass {
	GObjectClass      parent_class;

	/* GDA_DATA_MODEL_ACCESS_RANDOM */
	gint             (*fetch_nb_rows) (GdaPModel *model);
	gboolean         (*fetch_random)  (GdaPModel *model, GdaRow **prow, gint rownum, GError **error);
	gboolean         (*store_all)     (GdaPModel *model, GError **error);

	/* GDA_STATEMENT_MODEL_CURSOR_* */
	gboolean         (*fetch_next)    (GdaPModel *model, GdaRow **prow, gint rownum, GError **error);
	gboolean         (*fetch_prev)    (GdaPModel *model, GdaRow **prow, gint rownum, GError **error);
	gboolean         (*fetch_at)      (GdaPModel *model, GdaRow **prow, gint rownum, GError **error);
};

GType          gda_pmodel_get_type                     (void) G_GNUC_CONST;

/* API reserved to provider's implementations */
void           gda_pmodel_take_row                     (GdaPModel *model, GdaRow *row, gint rownum);
GdaRow        *gda_pmodel_get_stored_row               (GdaPModel *model, gint rownum);
GdaConnection *gda_pmodel_get_connection               (GdaPModel *model);

/* public API */
gboolean       gda_pmodel_set_row_selection_condition     (GdaPModel *model, GdaSqlExpr *expr, GError **error);
gboolean       gda_pmodel_set_row_selection_condition_sql (GdaPModel *model, const gchar *sql_where, GError **error);
gboolean       gda_pmodel_compute_row_selection_condition (GdaPModel *model, GError **error);

gboolean       gda_pmodel_set_modification_statement      (GdaPModel *model, GdaStatement *mod_stmt, GError **error);
gboolean       gda_pmodel_set_modification_statement_sql  (GdaPModel *model, const gchar *sql, GError **error);
gboolean       gda_pmodel_compute_modification_statements (GdaPModel *model, gboolean require_pk, GError **error);

G_END_DECLS

#endif
