/*
 * Copyright (C) 2008 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2008 - 2010 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2009 - 2010 Murray Cumming <murrayc@murrayc.com>
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

#ifndef _GDA_STATEMENT_STRUCT_PARTS_H
#define _GDA_STATEMENT_STRUCT_PARTS_H

#include <glib.h>
#include <glib-object.h>
#include <sql-parser/gda-statement-struct.h>
#include <sql-parser/gda-statement-struct-pspec.h>
#include <sql-parser/gda-statement-struct-decl.h>
#include <libgda/gda-meta-struct.h>

G_BEGIN_DECLS

typedef struct _GdaSqlExpr      GdaSqlExpr;
typedef struct _GdaSqlField     GdaSqlField;
typedef struct _GdaSqlTable     GdaSqlTable;
typedef struct _GdaSqlFunction  GdaSqlFunction;
typedef struct _GdaSqlOperation GdaSqlOperation;
typedef struct _GdaSqlCase GdaSqlCase;
typedef struct _GdaSqlSelectField GdaSqlSelectField;
typedef struct _GdaSqlSelectTarget GdaSqlSelectTarget;
typedef struct _GdaSqlSelectJoin GdaSqlSelectJoin;
typedef struct _GdaSqlSelectFrom GdaSqlSelectFrom;
typedef struct _GdaSqlSelectOrder GdaSqlSelectOrder;

/*
 * Any Expression
 */
/**
 * GdaSqlExpr:
 * @any: inheritance structure
 * @value: (allow-none): a #GValue, or %NULL. Please see specific note about this field. 
 * @param_spec: (allow-none): a #GdaSqlParamSpec, or %NULL if this is not a variable
 * @func: (allow-none): not %NULL if expression is a function or aggregate
 * @cond: (allow-none): not %NULL if expression is a condition or an operation
 * @select: (allow-none): not %NULL if expression is a sub select statement (#GdaSqlStatementSelect or #GdaSqlStatementCompound)
 * @case_s: (allow-none): not %NULL if expression is a CASE WHEN ... expression
 * @cast_as: (allow-none): not %NULL if expression must be cast to another data type
 * @value_is_ident: Please see specific note about the @value field
 *
 * This structure contains any expression, either as a value (the @value part is set),
 * a variable (the @param_spec is set), or as other types of expressions.
 *
 * Note 1 about the @value field: if the expression represents a string value in the SQL statement,
 * the string itself must be represented as it would be in the actual SQL, ie. it should be
 * escaped (accordingly to the escaping rules of the database which will use the SQL). For 
 * example a string representing the <userinput>'joe'</userinput> value should be
 * <userinput>"'joe'"</userinput> and not <userinput>"joe"</userinput>.
 *
 * Note 2 about the @value field: if the expression represents an SQL identifier (such as a table
 * or field name), then the @value_is_ident should be set to %0x01, and @value should be a string
 * which may contain double quotes around SQL identifiers which also are reserved keywords or which
 * are case sensitive.
 */
struct _GdaSqlExpr {
	GdaSqlAnyPart    any;
	GValue          *value;
	GdaSqlParamSpec *param_spec;
	GdaSqlFunction  *func;
	GdaSqlOperation *cond;
	GdaSqlAnyPart   *select; /* SELECT OR COMPOUND statements: GdaSqlStatementSelect or GdaSqlStatementCompound */
	GdaSqlCase      *case_s;

	gchar           *cast_as;

	gpointer         value_is_ident; /* pointer to a boolean to keep ABI from 4.0.
					  * Non NULL if @value represents an SQL identifier
					  * Mem in _NOT_ allocated!
					  */

	/*< private >*/
	/* Padding for future expansion */
	gpointer         _gda_reserved2;
};

#define GDA_TYPE_SQL_EXPR (gda_sql_expr_get_type())

GType            gda_sql_expr_get_type       (void) G_GNUC_CONST;
GdaSqlExpr      *gda_sql_expr_new            (GdaSqlAnyPart *parent);
void             gda_sql_expr_free           (GdaSqlExpr *expr);
GdaSqlExpr      *gda_sql_expr_copy           (GdaSqlExpr *expr);
gchar           *gda_sql_expr_serialize      (GdaSqlExpr *expr);
void             _gda_sql_expr_check_clean   (GdaSqlExpr *expr);

void             gda_sql_expr_take_select    (GdaSqlExpr *expr, GdaSqlStatement *stmt);

/*
 * Any Table's field
 */
struct _GdaSqlField {
	GdaSqlAnyPart       any;
	gchar              *field_name;

	/* validity check with a connection */
	GdaMetaTableColumn *validity_meta_table_column;

	/*< private >*/
	/* Padding for future expansion */
	gpointer         _gda_reserved1;
	gpointer         _gda_reserved2;
};

GdaSqlField     *gda_sql_field_new            (GdaSqlAnyPart *parent);
void             gda_sql_field_free           (GdaSqlField *field);
GdaSqlField     *gda_sql_field_copy           (GdaSqlField *field);
gchar           *gda_sql_field_serialize      (GdaSqlField *field);
void             _gda_sql_field_check_clean   (GdaSqlField *field);

void             gda_sql_field_take_name      (GdaSqlField *field, GValue *value);

/*
 * Any table
 */
struct _GdaSqlTable
{
	GdaSqlAnyPart       any;
	gchar              *table_name;

	/* validity check with a connection */
	GdaMetaDbObject    *validity_meta_object;

	/*< private >*/
	/* Padding for future expansion */
	gpointer         _gda_reserved1;
	gpointer         _gda_reserved2;
};

GdaSqlTable     *gda_sql_table_new            (GdaSqlAnyPart *parent);
void             gda_sql_table_free           (GdaSqlTable *table);
GdaSqlTable     *gda_sql_table_copy           (GdaSqlTable *table);
gchar           *gda_sql_table_serialize      (GdaSqlTable *table);
void             _gda_sql_table_check_clean   (GdaSqlTable *table);

void             gda_sql_table_take_name      (GdaSqlTable *table, GValue *value);

/*
 * A function with any number of arguments
 */
struct _GdaSqlFunction {
	GdaSqlAnyPart       any;
	gchar              *function_name;
	GSList             *args_list;

	/*< private >*/
	/* Padding for future expansion */
	gpointer         _gda_reserved1;
	gpointer         _gda_reserved2;
};

GdaSqlFunction  *gda_sql_function_new            (GdaSqlAnyPart *parent);
void             gda_sql_function_free           (GdaSqlFunction *function);
GdaSqlFunction  *gda_sql_function_copy           (GdaSqlFunction *function);
gchar           *gda_sql_function_serialize      (GdaSqlFunction *function);
void             gda_sql_function_check_clean    (GdaSqlFunction *function);

void             gda_sql_function_take_name      (GdaSqlFunction *function, GValue *value);
void             gda_sql_function_take_args_list (GdaSqlFunction *function, GSList *args);

/*
 * An operation on one or more expressions
 */
/**
 * GdaSqlOperatorType:
 * @GDA_SQL_OPERATOR_TYPE_AND: 
 * @GDA_SQL_OPERATOR_TYPE_OR: 
 * @GDA_SQL_OPERATOR_TYPE_EQ: 
 * @GDA_SQL_OPERATOR_TYPE_IS: 
 * @GDA_SQL_OPERATOR_TYPE_LIKE:
 * @GDA_SQL_OPERATOR_TYPE_ILIKE:  
 * @GDA_SQL_OPERATOR_TYPE_BETWEEN: 
 * @GDA_SQL_OPERATOR_TYPE_GT: 
 * @GDA_SQL_OPERATOR_TYPE_LT: 
 * @GDA_SQL_OPERATOR_TYPE_GEQ: 
 * @GDA_SQL_OPERATOR_TYPE_LEQ: 
 * @GDA_SQL_OPERATOR_TYPE_DIFF: 
 * @GDA_SQL_OPERATOR_TYPE_REGEXP: 
 * @GDA_SQL_OPERATOR_TYPE_REGEXP_CI: 
 * @GDA_SQL_OPERATOR_TYPE_NOT_REGEXP: 
 * @GDA_SQL_OPERATOR_TYPE_NOT_REGEXP_CI: 
 * @GDA_SQL_OPERATOR_TYPE_SIMILAR: 
 * @GDA_SQL_OPERATOR_TYPE_ISNULL: 
 * @GDA_SQL_OPERATOR_TYPE_ISNOTNULL: 
 * @GDA_SQL_OPERATOR_TYPE_NOT: 
 * @GDA_SQL_OPERATOR_TYPE_IN: 
 * @GDA_SQL_OPERATOR_TYPE_NOTIN: 
 * @GDA_SQL_OPERATOR_TYPE_CONCAT: 
 * @GDA_SQL_OPERATOR_TYPE_PLUS: 
 * @GDA_SQL_OPERATOR_TYPE_MINUS: 
 * @GDA_SQL_OPERATOR_TYPE_STAR: 
 * @GDA_SQL_OPERATOR_TYPE_DIV: 
 * @GDA_SQL_OPERATOR_TYPE_REM: 
 * @GDA_SQL_OPERATOR_TYPE_BITAND: 
 * @GDA_SQL_OPERATOR_TYPE_BITOR: 
 * @GDA_SQL_OPERATOR_TYPE_BITNOT:
 */
typedef enum {
	GDA_SQL_OPERATOR_TYPE_AND,
	GDA_SQL_OPERATOR_TYPE_OR,

	GDA_SQL_OPERATOR_TYPE_EQ,
	GDA_SQL_OPERATOR_TYPE_IS,
	GDA_SQL_OPERATOR_TYPE_LIKE,
	GDA_SQL_OPERATOR_TYPE_BETWEEN,
	GDA_SQL_OPERATOR_TYPE_GT,
	GDA_SQL_OPERATOR_TYPE_LT,
	GDA_SQL_OPERATOR_TYPE_GEQ,
	GDA_SQL_OPERATOR_TYPE_LEQ,
	GDA_SQL_OPERATOR_TYPE_DIFF,
	GDA_SQL_OPERATOR_TYPE_REGEXP,
	GDA_SQL_OPERATOR_TYPE_REGEXP_CI,
	GDA_SQL_OPERATOR_TYPE_NOT_REGEXP,
	GDA_SQL_OPERATOR_TYPE_NOT_REGEXP_CI,
	GDA_SQL_OPERATOR_TYPE_SIMILAR,
	GDA_SQL_OPERATOR_TYPE_ISNULL,
	GDA_SQL_OPERATOR_TYPE_ISNOTNULL,
	GDA_SQL_OPERATOR_TYPE_NOT,
	GDA_SQL_OPERATOR_TYPE_IN,
	GDA_SQL_OPERATOR_TYPE_NOTIN,

	GDA_SQL_OPERATOR_TYPE_CONCAT,
	GDA_SQL_OPERATOR_TYPE_PLUS,
	GDA_SQL_OPERATOR_TYPE_MINUS,
	GDA_SQL_OPERATOR_TYPE_STAR,
	GDA_SQL_OPERATOR_TYPE_DIV,
	GDA_SQL_OPERATOR_TYPE_REM,
	GDA_SQL_OPERATOR_TYPE_BITAND,
	GDA_SQL_OPERATOR_TYPE_BITOR,
	GDA_SQL_OPERATOR_TYPE_BITNOT,
	GDA_SQL_OPERATOR_TYPE_ILIKE
} GdaSqlOperatorType;

struct _GdaSqlOperation {
	GdaSqlAnyPart       any;
	GdaSqlOperatorType  operator_type;
	GSList             *operands;

	/*< private >*/
	/* Padding for future expansion */
	gpointer         _gda_reserved1;
	gpointer         _gda_reserved2;
};

GdaSqlOperation  *gda_sql_operation_new            (GdaSqlAnyPart *parent);
void              gda_sql_operation_free           (GdaSqlOperation *operation);
GdaSqlOperation  *gda_sql_operation_copy           (GdaSqlOperation *operation);
gchar            *gda_sql_operation_serialize      (GdaSqlOperation *operation);
const gchar      *gda_sql_operation_operator_to_string (GdaSqlOperatorType op);
GdaSqlOperatorType    gda_sql_operation_operator_from_string (const gchar *op);

/*
 * A CASE expression
 */
struct _GdaSqlCase
{
	GdaSqlAnyPart    any;
	GdaSqlExpr      *base_expr;
	GSList          *when_expr_list;
	GSList          *then_expr_list;
	GdaSqlExpr      *else_expr;

	/*< private >*/
	/* Padding for future expansion */
	gpointer         _gda_reserved1;
	gpointer         _gda_reserved2;
};

GdaSqlCase        *gda_sql_case_new            (GdaSqlAnyPart *parent);
void               gda_sql_case_free           (GdaSqlCase *sc);
GdaSqlCase        *gda_sql_case_copy           (GdaSqlCase *sc);
gchar             *gda_sql_case_serialize      (GdaSqlCase *sc);

/*
 * Any expression in a SELECT ... before the FROM clause
 */
struct _GdaSqlSelectField
{
	GdaSqlAnyPart       any;
	GdaSqlExpr         *expr;
	gchar              *field_name; /* may be NULL if expr does not refer to a table.field, can also be "*" */
	gchar              *table_name; /* may be NULL if expr does not refer to a table.field */
	gchar              *as;

	/* validity check with a connection */
	GdaMetaDbObject    *validity_meta_object;
	GdaMetaTableColumn *validity_meta_table_column;

	/*< private >*/
	/* Padding for future expansion */
	gpointer         _gda_reserved1;
	gpointer         _gda_reserved2;
};

GdaSqlSelectField *gda_sql_select_field_new            (GdaSqlAnyPart *parent);
void               gda_sql_select_field_free           (GdaSqlSelectField *field);
GdaSqlSelectField *gda_sql_select_field_copy           (GdaSqlSelectField *field);
gchar             *gda_sql_select_field_serialize      (GdaSqlSelectField *field);
void               _gda_sql_select_field_check_clean   (GdaSqlSelectField *field);

void               gda_sql_select_field_take_star_value(GdaSqlSelectField *field, GValue *value);
void               gda_sql_select_field_take_expr      (GdaSqlSelectField *field, GdaSqlExpr *expr);
void               gda_sql_select_field_take_alias     (GdaSqlSelectField *field, GValue *alias);

/*
 * Any TARGET ... in a SELECT statement
 */
struct _GdaSqlSelectTarget
{
	GdaSqlAnyPart       any;
	GdaSqlExpr         *expr;
	gchar              *table_name; /* may be NULL if expr does not refer to a table */
	gchar              *as;

	/* validity check with a connection */
	GdaMetaDbObject    *validity_meta_object;

	/*< private >*/
	/* Padding for future expansion */
	gpointer         _gda_reserved1;
	gpointer         _gda_reserved2;
};

GdaSqlSelectTarget *gda_sql_select_target_new            (GdaSqlAnyPart *parent);
void                gda_sql_select_target_free           (GdaSqlSelectTarget *target);
GdaSqlSelectTarget *gda_sql_select_target_copy           (GdaSqlSelectTarget *target);
gchar              *gda_sql_select_target_serialize      (GdaSqlSelectTarget *target);
void                _gda_sql_select_target_check_clean   (GdaSqlSelectTarget *target);

void                gda_sql_select_target_take_table_name (GdaSqlSelectTarget *target, GValue *value);
void                gda_sql_select_target_take_select (GdaSqlSelectTarget *target, GdaSqlStatement *stmt);
void                gda_sql_select_target_take_alias (GdaSqlSelectTarget *target, GValue *alias);

/*
 * Any JOIN ... in a SELECT statement
 */
typedef enum {
	GDA_SQL_SELECT_JOIN_CROSS,
	GDA_SQL_SELECT_JOIN_NATURAL,
	GDA_SQL_SELECT_JOIN_INNER,
	GDA_SQL_SELECT_JOIN_LEFT,
	GDA_SQL_SELECT_JOIN_RIGHT,
	GDA_SQL_SELECT_JOIN_FULL
} GdaSqlSelectJoinType;

/**
 * GdaSqlSelectJoin:
 * @any: inheritance structure
 * @type: type of join
 * @position: represents a join between a target at (pos &lt; @position) and the one at @position
 * @expr: (allow-none): joining expression, or %NULL
 * @use: (allow-none): list of #GdaSqlField pointers to use when joining, or %NULL
 *
 * This structure represents a join between two targets in a SELECT statement.
 */
struct _GdaSqlSelectJoin
{
	GdaSqlAnyPart         any;
	GdaSqlSelectJoinType  type;
	gint                  position; /* between a target at (pos < @position) and the one @position */
	GdaSqlExpr           *expr;
	GSList               *use; /* list of GdaSqlField pointers */

	/*< private >*/
	/* Padding for future expansion */
	gpointer         _gda_reserved1;
	gpointer         _gda_reserved2;
};

GdaSqlSelectJoin  *gda_sql_select_join_new            (GdaSqlAnyPart *parent);
void               gda_sql_select_join_free           (GdaSqlSelectJoin *join);
GdaSqlSelectJoin  *gda_sql_select_join_copy           (GdaSqlSelectJoin *join);
gchar             *gda_sql_select_join_serialize      (GdaSqlSelectJoin *join);

const gchar       *gda_sql_select_join_type_to_string (GdaSqlSelectJoinType type);


/*
 * Any FROM ... in a SELECT statement
 */
/**
 * GdaSqlSelectFrom:
 * @any: inheritance structure
 * @targets: (element-type Gda.SqlSelectTarget): list of #GdaSqlSelectTarget
 * @joins: (element-type Gda.SqlSelectJoin): list of #GdaSqlSelectJoin
 *
 * This structure represents the FROM clause of a SELECT statement, it lists targets and joins
 */
struct _GdaSqlSelectFrom
{
	GdaSqlAnyPart    any;
	GSList          *targets;
	GSList          *joins;

	/*< private >*/
	/* Padding for future expansion */
	gpointer         _gda_reserved1;
	gpointer         _gda_reserved2;
};

GdaSqlSelectFrom  *gda_sql_select_from_new            (GdaSqlAnyPart *parent);
void               gda_sql_select_from_free           (GdaSqlSelectFrom *from);
GdaSqlSelectFrom  *gda_sql_select_from_copy           (GdaSqlSelectFrom *from);
gchar             *gda_sql_select_from_serialize      (GdaSqlSelectFrom *from);

void               gda_sql_select_from_take_new_target(GdaSqlSelectFrom *from, GdaSqlSelectTarget *target);
void               gda_sql_select_from_take_new_join  (GdaSqlSelectFrom *from, GdaSqlSelectJoin *join);

/*
 * Any expression in a SELECT ... after the ORDER BY
 */
struct _GdaSqlSelectOrder
{
	GdaSqlAnyPart    any;
	GdaSqlExpr      *expr;
	gboolean         asc;
	gchar           *collation_name;

	/*< private >*/
	/* Padding for future expansion */
	gpointer         _gda_reserved1;
	gpointer         _gda_reserved2;
};

GdaSqlSelectOrder *gda_sql_select_order_new            (GdaSqlAnyPart *parent);
void               gda_sql_select_order_free           (GdaSqlSelectOrder *order);
GdaSqlSelectOrder *gda_sql_select_order_copy           (GdaSqlSelectOrder *order);
gchar             *gda_sql_select_order_serialize      (GdaSqlSelectOrder *order);

G_END_DECLS

#endif
