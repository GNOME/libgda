/* gda-query-parsing.h
 *
 * Copyright (C) 2004 Vivien Malerba
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifndef __GDA_QUERY_PARSING__
#define __GDA_QUERY_PARSING__

#include "gda-decl.h"
#include <libsql/sql_parser.h>
#include "sql-delimiter/gda-sql-delimiter.h"

G_BEGIN_DECLS

/*
 * Structure for parding functions
 */
typedef struct {
	GSList     *prev_targets;/* list of targets existing in the query before a new parsing */
	GSList     *prev_fields; /* list of GdaQueryField objects existing in the query before a new parsing */
	GSList     *parsed_targets; /* list of targets in the INVERTED order in which they were parsed */
	GHashTable *new_targets; /* KEY= target's alias or target's represented entity's name, VAL=GdaQueryTarget */
	GHashTable *sql_table_targets; /* KEY=sql_table struct, VAL=GdaQueryTarget */
} ParseData;

ParseData   *parse_data_new                  (GdaQuery *query);
void         parse_data_destroy              (ParseData *pdata);
void         parse_data_compute_targets_hash (GdaQuery *query, ParseData *pdata);

gboolean     parsed_create_select_query       (GdaQuery *query, sql_select_statement *select, GError **error);
gboolean     parsed_create_update_query       (GdaQuery *query, sql_update_statement *update, GError **error);
gboolean     parsed_create_insert_query       (GdaQuery *query, sql_insert_statement *insert, GError **error);
gboolean     parsed_create_delete_query       (GdaQuery *query, sql_delete_statement *delete, GError **error);

GdaEntityField    *parsed_create_global_query_field (GdaQuery *query, gboolean add_to_query,
						     ParseData *pdata, sql_field *field,
						     gboolean try_existing_field, gboolean *new_field, 
						     GdaQueryTarget **target_return, GError **error);

GdaQueryCondition *parsed_create_complex_condition (GdaQuery *query, ParseData *pdata, sql_where *where,
						    gboolean try_existing_field, 
						    GSList **targets_return, GError **error);

GdaQueryField     *gda_query_get_field_by_sql_naming_fields (GdaQuery *query, const gchar *sql_name, GSList *fields_list);

G_END_DECLS

#endif



