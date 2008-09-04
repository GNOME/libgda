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

#ifndef __GDA_DATA_SELECT_H__
#define __GDA_DATA_SELECT_H__

#include <glib-object.h>
#include <libgda/gda-row.h>
#include <providers-support/gda-pstmt.h>
#include <sql-parser/gda-sql-statement.h>

G_BEGIN_DECLS

#define GDA_TYPE_DATA_SELECT            (gda_data_select_get_type())
#define GDA_DATA_SELECT(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_DATA_SELECT, GdaDataSelect))
#define GDA_DATA_SELECT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_DATA_SELECT, GdaDataSelectClass))
#define GDA_IS_DATA_SELECT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_DATA_SELECT))
#define GDA_IS_DATA_SELECT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_DATA_SELECT))

typedef struct _GdaDataSelect        GdaDataSelect;
typedef struct _GdaDataSelectClass   GdaDataSelectClass;
typedef struct _GdaDataSelectPrivate GdaDataSelectPrivate;

/* error reporting */
extern GQuark gda_data_select_error_quark (void);
#define GDA_DATA_SELECT_ERROR gda_data_select_error_quark ()

enum {
	GDA_DATA_SELECT_MODIFICATION_STATEMENT_ERROR,
	GDA_DATA_SELECT_MISSING_MODIFICATION_STATEMENT_ERROR,
	GDA_DATA_SELECT_CONNECTION_ERROR,
	GDA_DATA_SELECT_VALUE_ERROR,
	GDA_DATA_SELECT_ACCESS_ERROR,
	GDA_DATA_SELECT_SQL_ERROR
};

struct _GdaDataSelect {
	GObject           object;
	GdaDataSelectPrivate *priv;

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
struct _GdaDataSelectClass {
	GObjectClass      parent_class;

	/* GDA_DATA_MODEL_ACCESS_RANDOM */
	gint             (*fetch_nb_rows) (GdaDataSelect *model);
	gboolean         (*fetch_random)  (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error);
	gboolean         (*store_all)     (GdaDataSelect *model, GError **error);

	/* GDA_STATEMENT_MODEL_CURSOR_* */
	gboolean         (*fetch_next)    (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error);
	gboolean         (*fetch_prev)    (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error);
	gboolean         (*fetch_at)      (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error);
};

GType          gda_data_select_get_type                     (void) G_GNUC_CONST;

gboolean       gda_data_select_set_row_selection_condition     (GdaDataSelect *model, GdaSqlExpr *expr, GError **error);
gboolean       gda_data_select_set_row_selection_condition_sql (GdaDataSelect *model, const gchar *sql_where, GError **error);
gboolean       gda_data_select_compute_row_selection_condition (GdaDataSelect *model, GError **error);

gboolean       gda_data_select_set_modification_statement      (GdaDataSelect *model, GdaStatement *mod_stmt, GError **error);
gboolean       gda_data_select_set_modification_statement_sql  (GdaDataSelect *model, const gchar *sql, GError **error);
gboolean       gda_data_select_compute_modification_statements (GdaDataSelect *model, GError **error);

G_END_DECLS

#endif
