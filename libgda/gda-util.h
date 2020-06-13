/*
 * Copyright (C) 2000 Reinhard Müller <reinhard@src.gnome.org>
 * Copyright (C) 2000 - 2002 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2001 Carlos Perelló Marín <carlos@gnome-db.org>
 * Copyright (C) 2001 - 2013 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2002 Gonzalo Paniagua Javier <gonzalo@src.gnome.org>
 * Copyright (C) 2006 - 2007 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2007 Armin Burgmeier <armin@openismus.com>
 * Copyright (C) 2008 Przemysław Grzegorczyk <pgrzegorczyk@gmail.com>
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

#ifndef __GDA_UTIL_H__
#define __GDA_UTIL_H__

#include <glib.h>
#include "gda-holder.h"
#include "gda-row.h"
#include "gda-connection.h"
#include <libgda/sql-parser/gda-sql-statement.h>
#include <libgda/gda-data-select.h>

G_BEGIN_DECLS

/**
 * SECTION:gda-util
 * @short_description: Utility functions
 * @title: Utility functions
 * @stability: Stable
 * @see_also:
 *
 * Some useful functions.
 */

/*
 * Type utilities
 */
const gchar *gda_g_type_to_string (GType type);
GType        gda_g_type_from_string (const gchar *str);

/* 
 * SQL escaping
 */
gchar       *gda_default_escape_string (const gchar *string);
gchar       *gda_default_unescape_string (const gchar *string);
guint        gda_identifier_hash (const gchar *id);
gboolean     gda_identifier_equal (const gchar *id1, const gchar *id2);
gchar      **gda_completion_list_get (GdaConnection *cnc, const gchar *sql, gint start, gint end);
gchar      **gda_sql_identifier_split (const gchar *id);
gchar       *gda_sql_identifier_quote (const gchar *id, GdaConnection *cnc, GdaServerProvider *prov,
				       gboolean meta_store_convention, gboolean force_quotes);

/*
 * Param & model utilities
 */
gboolean     gda_utility_check_data_model (GdaDataModel *model, gint nbcols, ...);
gboolean     gda_utility_check_data_model_v (GdaDataModel *model, gint nbcols, GType* types);
gboolean     gda_utility_data_model_dump_data_to_xml (GdaDataModel *model, xmlNodePtr parent, 
					      const gint *cols, gint nb_cols, const gint *rows, gint nb_rows,
					      gboolean use_col_ids);
const gchar *gda_utility_data_model_find_column_description (GdaDataSelect *model, const gchar *field_name);
gboolean     gda_utility_holder_load_attributes (GdaHolder *holder, xmlNodePtr node, GSList *sources, GError **error);

GdaSqlStatement *gda_statement_rewrite_for_default_values (GdaStatement *stmt, GdaSet *params,
							   gboolean remove, GError **error);

/* 
 * translate any text to an alphanumerical text 
 */
gchar       *gda_text_to_alphanum (const gchar *text);
gchar       *gda_alphanum_to_text (gchar *text);

/*
 * Statement computation (using data from meta store) 
 */
GdaSqlExpr      *gda_compute_unique_table_row_condition (GdaSqlStatementSelect *stsel, GdaMetaTable *mtable, 
							 gboolean require_pk, GError **error);
GdaSqlExpr      *gda_compute_unique_table_row_condition_with_cnc (GdaConnection *cnc,
								   GdaSqlStatementSelect *stsel,
								   GdaMetaTable *mtable, gboolean require_pk,
								   GError **error);

gboolean         gda_compute_dml_statements (GdaConnection *cnc, GdaStatement *select_stmt, gboolean require_pk, 
					     GdaStatement **insert_stmt, GdaStatement **update_stmt, GdaStatement **delete_stmt, 
					     GError **error);
GdaSqlStatement *gda_compute_select_statement_from_update (GdaStatement *update_stmt, GError **error);
GdaSqlStatement *gda_rewrite_sql_statement_for_null_parameters (GdaSqlStatement *sqlst, GdaSet *params,
								gboolean *out_modified, GError **error);
gboolean         gda_rewrite_statement_for_null_parameters (GdaStatement *stmt, GdaSet *params,
							    GdaStatement **out_stmt, GError **error);
void             _gda_modify_statement_param_types (GdaStatement *stmt, GdaDataModel *model);

/*
 * DSN and connection string manipulations
 */
gchar       *gda_rfc1738_encode          (const gchar *string);
gboolean     gda_rfc1738_decode          (gchar *string);
void         gda_dsn_split               (const gchar *string, gchar **out_dsn, 
					  gchar **out_username, gchar **out_password);
void         gda_connection_string_split (const gchar *string, gchar **out_cnc_params, gchar **out_provider, 
					  gchar **out_username, gchar **out_password);

/*
 * Date and time parsing from ISO 8601 (well, part of it)
 */
gboolean     gda_parse_iso8601_date (GDate *gdate, const gchar *value);
GdaTime     *gda_parse_iso8601_time (const gchar *value);

gboolean     gda_parse_formatted_date (GDate *gdate, const gchar *value,
				       GDateDMY first, GDateDMY second, GDateDMY third, gchar sep);
GdaTime     *gda_parse_formatted_time (const gchar *value, gchar sep);
GDateTime   *gda_parse_formatted_timestamp (const gchar *value,
					    GDateDMY first, GDateDMY second, GDateDMY third, gchar sep);

G_END_DECLS

#endif
