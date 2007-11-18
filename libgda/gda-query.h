/* gda-query.h
 *
 * Copyright (C) 2003 - 2005 Vivien Malerba
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


#ifndef __GDA_QUERY_H_
#define __GDA_QUERY_H_

#include <libgda/gda-query-object.h>
#include "gda-decl.h"
#include "gda-enums.h"

G_BEGIN_DECLS

#define GDA_TYPE_QUERY          (gda_query_get_type())
#define GDA_QUERY(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_query_get_type(), GdaQuery)
#define GDA_QUERY_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_query_get_type (), GdaQueryClass)
#define GDA_IS_QUERY(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_query_get_type ())

/* error reporting */
extern GQuark gda_query_error_quark (void);
#define GDA_QUERY_ERROR gda_query_error_quark ()

/* different possible types for a query */
typedef enum {
        GDA_QUERY_TYPE_SELECT,
	GDA_QUERY_TYPE_INSERT,
	GDA_QUERY_TYPE_UPDATE,
	GDA_QUERY_TYPE_DELETE,
        GDA_QUERY_TYPE_UNION,
        GDA_QUERY_TYPE_INTERSECT,
	GDA_QUERY_TYPE_EXCEPT,
        GDA_QUERY_TYPE_NON_PARSED_SQL
} GdaQueryType;

typedef enum
{
	GDA_QUERY_XML_LOAD_ERROR,
	GDA_QUERY_META_DATA_UPDATE,
	GDA_QUERY_FIELDS_ERROR,
	GDA_QUERY_TARGETS_ERROR,
	GDA_QUERY_RENDER_ERROR,
	GDA_QUERY_PARSE_ERROR,
	GDA_QUERY_SYNTAX_ERROR,
	GDA_QUERY_STRUCTURE_ERROR,
	GDA_QUERY_SQL_ANALYSE_ERROR,
	GDA_QUERY_NO_CNC_ERROR,
	GDA_QUERY_CNC_CLOSED_ERROR,
	GDA_QUERY_EXEC_ERROR,
	GDA_QUERY_PARAM_TYPE_ERROR,
	GDA_QUERY_MULTIPLE_STATEMENTS_ERROR
} GdaQueryError;


/* struct for the object's data */
struct _GdaQuery
{
	GdaQueryObject         object;
	GdaQueryPrivate       *priv;
};

/* struct for the object's class */
struct _GdaQueryClass
{
	GdaQueryObjectClass    parent_class;

	/* signals */
	void   (*type_changed)         (GdaQuery *query);
	void   (*condition_changed)    (GdaQuery *query);

	void   (*target_added)         (GdaQuery *query, GdaQueryTarget *target);
	void   (*target_removed)       (GdaQuery *query, GdaQueryTarget *target);
	void   (*target_updated)       (GdaQuery *query, GdaQueryTarget *target);

	void   (*join_added)           (GdaQuery *query, GdaQueryJoin *join);
	void   (*join_removed)         (GdaQuery *query, GdaQueryJoin *join);
	void   (*join_updated)         (GdaQuery *query, GdaQueryJoin *join);
	
	void   (*sub_query_added)      (GdaQuery *query, GdaQuery *sub_query);
	void   (*sub_query_removed)    (GdaQuery *query, GdaQuery *sub_query);
	void   (*sub_query_updated)    (GdaQuery *query, GdaQuery *sub_query);
};

GType              gda_query_get_type               (void) G_GNUC_CONST;
GdaQuery          *gda_query_new                    (GdaDict *dict);
GdaQuery          *gda_query_new_copy               (GdaQuery *orig, GHashTable *replacements);
GdaQuery          *gda_query_new_from_sql           (GdaDict *dict, const gchar *sql, GError **error);

void               gda_query_set_query_type         (GdaQuery *query, GdaQueryType type);
GdaQueryType       gda_query_get_query_type         (GdaQuery *query);
const gchar       *gda_query_get_query_type_string  (GdaQuery *query);
gboolean           gda_query_is_select_query        (GdaQuery *query);
gboolean           gda_query_is_insert_query        (GdaQuery *query);
gboolean           gda_query_is_update_query        (GdaQuery *query);
gboolean           gda_query_is_delete_query        (GdaQuery *query);
gboolean           gda_query_is_modify_query         (GdaQuery *query);
gboolean           gda_query_is_well_formed         (GdaQuery *query, GdaParameterList *context, GError **error);
GdaQuery          *gda_query_get_parent_query       (GdaQuery *query);

GSList            *gda_query_get_parameters         (GdaQuery *query);
GdaParameterList  *gda_query_get_parameter_list   (GdaQuery *query);
GdaObject         *gda_query_execute                (GdaQuery *query, GdaParameterList *params,
						     gboolean iter_model_only_requested, GError **error);


/* if SQL queries */
void               gda_query_set_sql_text           (GdaQuery *query, const gchar *sql, GError **error);
gchar             *gda_query_get_sql_text           (GdaQuery *query);

/* for other types of queries */
GSList            *gda_query_get_sub_queries        (GdaQuery *query);
void               gda_query_add_sub_query          (GdaQuery *query, GdaQuery *sub_query);
void               gda_query_del_sub_query          (GdaQuery *query, GdaQuery *sub_query);
 
void               gda_query_add_param_source       (GdaQuery *query, GdaDataModel *param_source);
void               gda_query_del_param_source       (GdaQuery *query, GdaDataModel *param_source);
const GSList      *gda_query_get_param_sources      (GdaQuery *query);

gboolean           gda_query_add_target             (GdaQuery *query, GdaQueryTarget *target, GError **error);
void               gda_query_del_target             (GdaQuery *query, GdaQueryTarget *target);
GSList            *gda_query_get_targets            (GdaQuery *query);
GdaQueryTarget    *gda_query_get_target_by_xml_id   (GdaQuery *query, const gchar *xml_id);
GdaQueryTarget    *gda_query_get_target_by_alias    (GdaQuery *query, const gchar *alias_or_name);
GSList            *gda_query_get_target_pkfields    (GdaQuery *query, GdaQueryTarget *target);

GSList            *gda_query_get_joins              (GdaQuery *query);
GdaQueryJoin      *gda_query_get_join_by_targets    (GdaQuery *query, GdaQueryTarget *target1, GdaQueryTarget *target2);
gboolean           gda_query_add_join               (GdaQuery *query, GdaQueryJoin *join);
void               gda_query_del_join               (GdaQuery *query, GdaQueryJoin *join);

void               gda_query_set_condition          (GdaQuery *query, GdaQueryCondition *cond);
GdaQueryCondition *gda_query_get_condition          (GdaQuery *query);
GSList            *gda_query_get_main_conditions    (GdaQuery *query);
void               gda_query_append_condition       (GdaQuery *query, GdaQueryCondition *cond, gboolean append_as_and);

void               gda_query_set_order_by_field     (GdaQuery *query, GdaQueryField *field, gint order, gboolean ascendant);
gint               gda_query_get_order_by_field     (GdaQuery *query, GdaQueryField *field, gboolean *ascendant);

void               gda_query_set_results_limit      (GdaQuery *query, gboolean has_limit, guint limit, guint offset);
gboolean           gda_query_get_results_limit      (GdaQuery *query, guint *limit, guint *offset);

/* utility functions */
GdaQueryField     *gda_query_add_field_from_sql     (GdaQuery *query, const gchar *field, GError **error);

GSList            *gda_query_get_all_fields         (GdaQuery *query);
GdaQueryField     *gda_query_get_field_by_sql_naming        (GdaQuery *query, const gchar *sql_name);
GdaQueryField     *gda_query_get_field_by_param_name        (GdaQuery *query, const gchar *param_name);
GdaQueryField     *gda_query_get_field_by_ref_field         (GdaQuery *query, GdaQueryTarget *target, 
							     GdaEntityField *ref_field, 
							     GdaQueryFieldState field_state);
GdaQueryField     *gda_query_get_first_field_for_target     (GdaQuery *query, GdaQueryTarget *target);
GSList            *gda_query_expand_all_field               (GdaQuery *query, GdaQueryTarget *target);
void               gda_query_order_fields_using_join_conds  (GdaQuery *query);
GSList            *gda_query_get_fields_by_target           (GdaQuery *query, GdaQueryTarget *target, gboolean visible_fields_only);

void               gda_query_declare_condition      (GdaQuery *query, GdaQueryCondition *cond);
void               gda_query_undeclare_condition    (GdaQuery *query, GdaQueryCondition *cond);

G_END_DECLS

#endif
