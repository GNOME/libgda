/* GDA common library
 * Copyright (C) 1998 - 2008 The GNOME Foundation.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
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

#ifndef __GDA_UTIL_H__
#define __GDA_UTIL_H__

#include <glib/ghash.h>
#include <glib/glist.h>
#include "gda-holder.h"
#include "gda-row.h"
#include "gda-connection.h"
#include <sql-parser/gda-sql-statement.h>

G_BEGIN_DECLS

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
gchar      **gda_completion_list_get (GdaConnection *cnc, const gchar *text, gint start, gint end);

/*
 * Param & model utilities
 */
gboolean     gda_utility_check_data_model (GdaDataModel *model, gint nbcols, ...);
void         gda_utility_data_model_dump_data_to_xml (GdaDataModel *model, xmlNodePtr parent, 
					      const gint *cols, gint nb_cols, const gint *rows, gint nb_rows,
					      gboolean use_col_ids);
void         gda_utility_holder_load_attributes (GdaHolder *holder, xmlNodePtr node, GSList *sources);

/* 
 * translate any text to an alphanumerical text 
 */
gchar       *gda_text_to_alphanum (const gchar *text);
gchar       *gda_alphanum_to_text (gchar *text);

/*
 * Statement computation from meta store 
 */
GdaSqlExpr  *gda_compute_unique_table_row_condition (GdaSqlStatementSelect *stsel, GdaMetaTable *mtable, 
						     gboolean require_pk, GError **error);
gboolean     gda_compute_dml_statements (GdaConnection *cnc, GdaStatement *select_stmt, gboolean require_pk, 
					 GdaStatement **insert_stmt, GdaStatement **update_stmt, GdaStatement **delete_stmt, 
					 GError **error);

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
gboolean     gda_parse_iso8601_time (GdaTime *timegda, const gchar *value);
gboolean     gda_parse_iso8601_timestamp (GdaTimestamp *timestamp, const gchar *value);

G_END_DECLS

#endif
