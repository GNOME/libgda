/*
 * Copyright (C) 2009 - 2010 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2009 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 Jonh Wendell <jwendell@gnome.org>
 * Copyright (C) 2011 Daniel Espinosa <despinosa@src.gnome.org>
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

#ifndef __GDA_SQL_BUILDER_H_
#define __GDA_SQL_BUILDER_H_

#include <glib-object.h>
#include <libgda/sql-parser/gda-sql-statement.h>
#include <libgda/gda-data-handler.h>

G_BEGIN_DECLS

#define GDA_TYPE_SQL_BUILDER          (gda_sql_builder_get_type())
G_DECLARE_DERIVABLE_TYPE (GdaSqlBuilder,gda_sql_builder,GDA,SQL_BUILDER,GObject)

/* error reporting */
extern GQuark gda_sql_builder_error_quark (void);
#define GDA_SQL_BUILDER_ERROR gda_sql_builder_error_quark ()

typedef enum {
	GDA_SQL_BUILDER_WRONG_TYPE_ERROR,
	GDA_SQL_BUILDER_MISUSE_ERROR
} GdaSqlBuilderError;


/* struct for the object's data */
/*struct _GdaSqlBuilder
{
	GObject               object;
	GdaSqlBuilderPrivate  *priv;
};
*/
/* struct for the object's class */
struct _GdaSqlBuilderClass
{
	GObjectClass              parent_class;

	/*< private >*/
	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

typedef guint GdaSqlBuilderId;

/**
 * SECTION:gda-sql-builder
 * @short_description: Factory object for statements
 * @title: GdaSqlBuilder
 * @stability: Stable
 * @see_also: #GdaSqlParser, #GdaSqlStatement and #GdaStatement
 *
 * The #GdaSqlBuilder can be used to build a #GdaStatement from its structural description,
 * much in the same way a #GdaSqlParser can be used to build a #GdaStatement from an SQL
 * string.
 *
 * The #GdaSqlBuilder internally constructs a #GdaSqlStatement and uses it when requested to produce
 * a #GdaStatement (see gda_sql_builder_get_statement()), or a #GdaSqlStatement (see
 * gda_sql_builder_get_sql_statement()).
 *
 * During the building process, some pieces of the statement are constructed, and assembled into the
 * final statement. Each of these pieces can be reused anytime in the same #GdaSqlBuilder object, and each
 * is identified using a single unsigned integer ID. That ID is dynamically allocated by the object.
 *
 * The following example builds the equivalent of the <![CDATA["name='joe' AND age >= ##ageparam::int"]]> expression:
 * <programlisting><![CDATA[
 *GdaSqlBuilder *b=...
 *guint id_field = gda_sql_builder_add_id (b, "name"); // build the "name" SQL identifier
 *guint id_value = gda_sql_builder_add_expr (b, NULL, G_TYPE_STRING, "joe"); // 'joe' expression
 *guint id_cond1 = gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_EQ, id_field, id_value, 0); // "name='joe'"
 *
 *guint id_cond2 = gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_GT,
 *      gda_sql_builder_add_id (b, "age"), // build the "age" SQL identifier
 *      gda_sql_builder_add_param (b, "ageparam", G_TYPE_INT, FALSE), // parameter
 *      0);
 *guint id_cond_and = gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_AND, id_cond1, id_cond2, 0); // whole expression
 *]]></programlisting>
 *
 * For more examples, see the <link linkend="howto-sqlbuilder">Build statements without using a parser</link> section.
 */

GdaSqlBuilder    *gda_sql_builder_new            (GdaSqlStatementType stmt_type);
GdaStatement     *gda_sql_builder_get_statement (GdaSqlBuilder *builder, GError **error);
GdaSqlStatement  *gda_sql_builder_get_sql_statement (GdaSqlBuilder *builder);

/* Expression API */
GdaSqlBuilderId gda_sql_builder_add_id (GdaSqlBuilder *builder, const gchar *str);
GdaSqlBuilderId gda_sql_builder_add_field_id (GdaSqlBuilder *builder, const gchar *field_name, const gchar *table_name);
GdaSqlBuilderId gda_sql_builder_add_expr (GdaSqlBuilder *builder, GdaDataHandler *dh, GType type, ...);
GdaSqlBuilderId gda_sql_builder_add_expr_value (GdaSqlBuilder *builder, const GValue *value);
GdaSqlBuilderId gda_sql_builder_add_param (GdaSqlBuilder *builder, const gchar *param_name, GType type, gboolean nullok);

GdaSqlBuilderId gda_sql_builder_add_cond (GdaSqlBuilder *builder, GdaSqlOperatorType op,
                                          GdaSqlBuilderId op1, GdaSqlBuilderId op2, GdaSqlBuilderId op3);
GdaSqlBuilderId gda_sql_builder_add_cond_v (GdaSqlBuilder *builder, GdaSqlOperatorType op,
                                            const GdaSqlBuilderId *op_ids, gint op_ids_size);
GdaSqlBuilderId gda_sql_builder_add_function (GdaSqlBuilder *builder, const gchar *func_name, ...);
GdaSqlBuilderId gda_sql_builder_add_function_v (GdaSqlBuilder *builder, const gchar *func_name,
                                                const GdaSqlBuilderId *args, gint args_size);
GdaSqlBuilderId gda_sql_builder_add_sub_select (GdaSqlBuilder *builder, GdaSqlStatement *sqlst);
GdaSqlBuilderId gda_sql_builder_add_case (GdaSqlBuilder *builder, GdaSqlBuilderId test_expr, GdaSqlBuilderId else_expr, ...);
GdaSqlBuilderId gda_sql_builder_add_case_v (GdaSqlBuilder *builder, GdaSqlBuilderId test_expr, GdaSqlBuilderId else_expr,
                                            const GdaSqlBuilderId *when_array, const GdaSqlBuilderId *then_array, gint args_size);


/* General Statement API */
void              gda_sql_builder_add_field_value (GdaSqlBuilder *builder, const gchar *field_name, GType type, ...);
void              gda_sql_builder_add_field_value_as_gvalue (GdaSqlBuilder *builder, const gchar *field_name,
                                                             const GValue *value);
void              gda_sql_builder_add_field_value_id (GdaSqlBuilder *builder, GdaSqlBuilderId field_id, GdaSqlBuilderId value_id);

void              gda_sql_builder_set_table (GdaSqlBuilder *builder, const gchar *table_name);
void              gda_sql_builder_set_where (GdaSqlBuilder *builder, GdaSqlBuilderId cond_id);

/* SELECT Statement API */
GdaSqlBuilderId gda_sql_builder_select_add_field (GdaSqlBuilder *builder, const gchar *field_name,
                                                  const gchar *table_name, const gchar *alias);
GdaSqlBuilderId gda_sql_builder_select_add_target (GdaSqlBuilder *builder, const gchar *table_name, const gchar *alias);
GdaSqlBuilderId gda_sql_builder_select_add_target_id (GdaSqlBuilder *builder, GdaSqlBuilderId table_id, const gchar *alias);
GdaSqlBuilderId gda_sql_builder_select_join_targets (GdaSqlBuilder *builder,
                                                     GdaSqlBuilderId left_target_id, GdaSqlBuilderId right_target_id,
                                                     GdaSqlSelectJoinType join_type,
                                                     GdaSqlBuilderId join_expr);
void              gda_sql_builder_join_add_field (GdaSqlBuilder *builder, GdaSqlBuilderId join_id, const gchar *field_name);
void              gda_sql_builder_select_order_by (GdaSqlBuilder *builder, GdaSqlBuilderId expr_id,
                                                   gboolean asc, const gchar *collation_name);
void              gda_sql_builder_select_set_distinct (GdaSqlBuilder *builder,
                                                       gboolean distinct, GdaSqlBuilderId expr_id);
void              gda_sql_builder_select_set_limit (GdaSqlBuilder *builder,
                                                    GdaSqlBuilderId limit_count_expr_id, GdaSqlBuilderId limit_offset_expr_id);

void              gda_sql_builder_select_set_having (GdaSqlBuilder *builder, GdaSqlBuilderId cond_id);
void              gda_sql_builder_select_group_by (GdaSqlBuilder *builder, GdaSqlBuilderId expr_id);

/* COMPOUND SELECT Statement API */
void              gda_sql_builder_compound_set_type (GdaSqlBuilder *builder, GdaSqlStatementCompoundType compound_type);
void              gda_sql_builder_compound_add_sub_select (GdaSqlBuilder *builder, GdaSqlStatement *sqlst);
void              gda_sql_builder_compound_add_sub_select_from_builder (GdaSqlBuilder *builder, GdaSqlBuilder *subselect);

/* import/Export API */
GdaSqlExpr       *gda_sql_builder_export_expression (GdaSqlBuilder *builder, GdaSqlBuilderId id);
GdaSqlBuilderId gda_sql_builder_import_expression (GdaSqlBuilder *builder, GdaSqlExpr *expr);
GdaSqlBuilderId gda_sql_builder_import_expression_from_builder (GdaSqlBuilder *builder, GdaSqlBuilder *query, GdaSqlBuilderId expr_id);

G_END_DECLS

#endif
