/*
 * Copyright (C) 2008 - 2012 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_DATA_SELECT_H__
#define __GDA_DATA_SELECT_H__

#include <glib-object.h>
#include <libgda/gda-row.h>
#include <libgda/providers-support/gda-pstmt.h>
#include <libgda/sql-parser/gda-sql-statement.h>

G_BEGIN_DECLS

#define GDA_TYPE_DATA_SELECT            (gda_data_select_get_type())

G_DECLARE_DERIVABLE_TYPE (GdaDataSelect, gda_data_select, GDA, DATA_SELECT, GObject)

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

	/*< private >*/
	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};


/* error reporting */
extern GQuark gda_data_select_error_quark (void);
#define GDA_DATA_SELECT_ERROR gda_data_select_error_quark ()

typedef enum {
	GDA_DATA_SELECT_MODIFICATION_STATEMENT_ERROR,
	GDA_DATA_SELECT_MISSING_MODIFICATION_STATEMENT_ERROR,
	GDA_DATA_SELECT_CONNECTION_ERROR,
	GDA_DATA_SELECT_ACCESS_ERROR,
	GDA_DATA_SELECT_SQL_ERROR,
	GDA_DATA_SELECT_SAFETY_LOCKED_ERROR
} GdaDataSelectError;

/**
 * GdaDataSelectConditionType:
 * @GDA_DATA_SELECT_COND_PK: only primary key fields are used
 * @GDA_DATA_SELECT_COND_ALL_COLUMNS: all the columns of the tables are used
 *
 * Defines what criteria gda_data_select_compute_modification_statements_ext() uses
 * to uniquely identify a single row in a table when creating modification statements.
 */
typedef enum {
	GDA_DATA_SELECT_COND_PK,
	GDA_DATA_SELECT_COND_ALL_COLUMNS
} GdaDataSelectConditionType;

/**
 * SECTION:gda-data-select
 * @short_description: Base class for data models returned by the execution of a SELECT statement
 * @title: GdaDataSelect
 * @stability: Stable
 * @see_also: #GdaDataModel and the <link linkend="data-select">Advanced GdaDataSelect usage</link> section.
 *
 * This data model implements the <link linkend="GdaDataModel">GdaDataModel</link> interface and is the required
 *  base object when database providers implement a data model returned when a SELECT statement has been executed.
 *  As the <link linkend="GdaDataModel">GdaDataModel</link> interface is implemented, consult the API
 *  to access and modify the data held in a <link linkend="GdaDataSelect">GdaDataSelect</link> object.
 *
 * Depending on the requested data model usage (as specified by the "model_usage" parameter of the
 * <link linkend="gda-connection-statement-execute">gda_connection_statement_execute()</link> and similar
 * methods, the #GdaDataSelect will allow random access, cursor based access or both.
 *
 * Also, when later you'll be reading the data contained in a #GdaDataSelect object, depending on the actual
 * implementation (which adapts to the API providede by the database server), some calls to the database server
 * may be necessary to actually obtain the data. If this behaviour is not the one intended and if you need to
 * access the data without having to contact the database server (for example for performances reasons), then
 * you can use the <link linkend="gda_data_select_prepare_for_offline">gda_data_select_prepare_for_offline()</link>
 * method or specify the <link linkend="GDA-STATEMENT-MODEL-OFFLINE:CAPS">GDA_STATEMENT_MODEL_OFFLINE</link>
 * flag when executing the SELECT statement.
 *
 *  The default behaviour however is to disallow modifications, and this section documents how to parametrize
 *  a <link linkend="GdaDataSelect">GdaDataSelect</link> to allow modifications. Once this is done, any modification
 *  done to the data model will be propagated to the modified table in the database using INSERT, UPDATE or DELETE
 *  statements.
 *
 *  After any modification, it is still possible to read values from the data model (even values for rows which have
 *  been modified or inserted). The data model might then execute some SELECT statement to fetch some actualized values.
 *
 *  Note: there is a corner case where a modification made to a row would make the row not selected at first in the data model
 *  (for example is the original SELECT statement included a clause <![CDATA["WHERE id < 100"]]> and the modification sets the 
 *  <![CDATA["id"]]> value to 110), then the row will still be in the data model even though it would not be if the SELECT statement
 *  which execution created the data model in the first place was re-run. This is illustrated in the schema below:
 *  <mediaobject>
 *    <imageobject role="html">
 *      <imagedata fileref="writable_data_model.png" format="PNG" contentwidth="100mm"/>
 *    </imageobject>
 *    <textobject>
 *      <phrase>GdaDataSelect data model's contents after some modifications</phrase>
 *    </textobject>
 *  </mediaobject>
 */

gboolean       gda_data_select_set_row_selection_condition     (GdaDataSelect *model, GdaSqlExpr *expr, GError **error);
gboolean       gda_data_select_set_row_selection_condition_sql (GdaDataSelect *model, const gchar *sql_where, GError **error);
gboolean       gda_data_select_compute_row_selection_condition (GdaDataSelect *model, GError **error);

gboolean       gda_data_select_set_modification_statement      (GdaDataSelect *model, GdaStatement *mod_stmt, GError **error);
gboolean       gda_data_select_set_modification_statement_sql  (GdaDataSelect *model, const gchar *sql, GError **error);
gboolean       gda_data_select_compute_modification_statements (GdaDataSelect *model, GError **error);
gboolean       gda_data_select_compute_modification_statements_ext (GdaDataSelect *model,
								    GdaDataSelectConditionType cond_type,
								    GError **error);

gboolean       gda_data_select_compute_columns_attributes      (GdaDataSelect *model, GError **error);
GdaConnection *gda_data_select_get_connection                  (GdaDataSelect *model);

gboolean       gda_data_select_prepare_for_offline             (GdaDataSelect *model, GError **error);

#define GDA_TYPE_DATA_SELECT_ITER gda_data_select_iter_get_type()

G_DECLARE_DERIVABLE_TYPE(GdaDataSelectIter, gda_data_select_iter, GDA, DATA_SELECT_ITER, GdaDataModelIter)

struct _GdaDataSelectIterClass {
	GdaDataModelIterClass parent_class;
};

G_END_DECLS

#endif
