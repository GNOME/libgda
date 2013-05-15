/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 2 -*- */
/*
 * libgdadata
 * Copyright (C) Daniel Espinosa Ortiz 2013 <esodan@gmail.com>
 *
 * libgda is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libgda is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <gda-sql-expression.h>
#include <sql-parser/gda-statement-struct-parts.h>

G_DEFINE_TYPE (GdaSqlExpression, gda_sql_expression, G_TYPE_OBJECT);

#define GDA_SQL_EXPRESSION_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GDA_TYPE_SQL_EXPRESSION, GdaSqlExpressionPrivate))

struct _GdaSqlExpressionPrivate
{
  GdaSqlBuilder        *builder;
  GdaSqlBuilderId      id;
  GdaSqlExpressionType type;
};

/* properties */
enum
{
        PROP_0,
        PROP_VALUE,
        PROP_IS_IDENT,
        PROP_EXPR_TYPE
};

static void
gda_sql_expression_class_init (GdaSqlExpressionClass *klass)
{
  g_type_class_add_private (klass, sizeof (GdaSqlExpressionPrivate));
}

static void
gda_sql_expression_init (GdaSqlExpression *self)
{
  GdaSqlExpressionPrivate *priv;

  self->priv = priv = GDA_SQL_EXPRESSION_GET_PRIVATE (self);

  priv->builder = gda_sql_builder_new (GDA_SQL_STATEMENT_SELECT);
  priv->type = GDA_SQL_EXPRESSION_TYPE_VALUE;
}


/**
 * gda_sql_expression_new:
 * @type: a #GdaSqlExpressionType
 *
 * Returns: (transfer-full): a new #GdaSqlExpression object
 */
GdaSqlExpression *
gda_sql_expression_new (GdaSqlExpressionType type)
{}

/**
 * gda_sql_expression_new_from_string:
 * @str: a string describing an expression
 *
 * Returns: (transfer-full): a new #GdaSqlExpression object
 */
GdaSqlExpression*
gda_sql_expression_new_from_string (const gchar* str)
{}

/**
 * gda_sql_expression_to_string:
 * @expr: a #GdaSqlExpression
 *
 * Returns: (transfer-full): a new string representing a #GdaSqlExpression
 */
gchar*
gda_sql_expression_to_string (GdaSqlExpression *expr)
{}

/**
 * gda_sql_expression_check_clean:
 * @expr: a #GdaSqlExpression
 *
 */
void
gda_sql_expression_check_clean (GdaSqlExpression *expr)
{}

/**
 * gda_sql_expression_set_value:
 * @expr: a #GdaSqlExpression
 * @val: a #GValue to be used by @expr
 *
 * Sets @val to be used by a #GdaSqlExpression of type
 * #GDA_SQL_EXPRESSION_TYPE_VALUE
 */
void
gda_sql_expression_set_value (GdaSqlExpression *expr,
                              const GValue *val)
{}

/**
 * gda_sql_expression_get_value:
 * @expr: a #GdaSqlExpression
 *
 * Returns: (transfer-none): the #GValue used as an expresion
 */
const GValue*
gda_sql_expression_get_value (GdaSqlExpression *expr)
{}

/**
 * gda_sql_expression_set_variable_pspec:
 * @expr: a #GdaSqlExpression
 * @spec: a #GdaSqlParamSpec
 *
 * To be used by a #GdaSqlExpression of type #GDA_SQL_EXPRESSION_TYPE_VALUE.
 */
void
gda_sql_expression_set_variable_pspec (GdaSqlExpression *expr,
                                       const GdaSqlParamSpec *spec)
{}

/**
 * gda_sql_expression_get_variable_pspec:
 * @expr: a #GdaSqlExpression
 *
 * Returns: (transfer-full): a #GdaSqlParamSpec used by @expr of type
 * #GDA_SQL_EXPRESSION_TYPE_VALUE
 */
GdaSqlParamSpec*
gda_sql_expression_get_variable_pspec (GdaSqlExpression *expr)
{}

/**
 * gda_sql_expression_set_function_name:
 * @expr: a #GdaSqlExpression
 * @name: a string with the function's name
 *
 * Sets a function's name of a #GdaSqlExpression of type
 * #GDA_SQL_EXPRESSION_TYPE_FUNCTION
 */
void
gda_sql_expression_set_function_name (GdaSqlExpression *expr,
                                      const gchar *name)
{}

/**
 * gda_sql_expression_set_function_args:
 * @expr: a #GdaSqlExpression
 * @args: (element-type Gda.SqlExpression): a list of arguments for a function
 *
 * Sets arguments of a function to a #GdaSqlExpression of type
 * #GDA_SQL_EXPRESSION_TYPE_FUNCTION.
 */
void
gda_sql_expression_set_function_args (GdaSqlExpression *expr,
                                      const GSList *args)
{}

/**
 * gda_sql_expression_set_operator_type:
 * @expr: a #GdaSqlExpression
 * @optype: a #GdaSqlExpressionType
 *
 * Sets operator's type of a #GdaSqlExpression of type
 * #GDA_SQL_EXPRESSION_TYPE_CONDITION
 */
void
gda_sql_expression_set_operator_type  (GdaSqlExpression *expr,
                                       GdaSqlOperatorType optype)
{}

/**
 * gda_sql_expression_set_operator_operands:
 * @expr: a #GdaSqlExpression
 * @operands: (element-type Gda.SqlExpression): a lists of #GdaSqlExpression
 * expresions
 *
 */
void
gda_sql_expression_set_operator_operands (GdaSqlExpression *expr,
                                          const GSList *operands)
{}

/**
 * gda_sql_expression_set_select:
 * @expr: a #GdaSqlExpression
 * @select: a #GdaStatement to be used as a sub-select expression
 *
 */
void
gda_sql_expression_set_select (GdaSqlExpression *expr,
                               GdaStatement *select)
{}

/**
 * gda_sql_expression_get_select:
 * @expr: a #GdaSqlExpression
 *
 * Returns: a #GdaStatement of type #GDA_SQL_STATEMENT_SELECT
 */
GdaStatement*
gda_sql_expression_get_select (GdaSqlExpression *expr)
{}

/**
 * gda_sql_expression_set_compound:
 * @expr: a #GdaSqlExpression
 * @compound: a #GdaStatement
 *
 * Uses a #GdaStatement of type #GDA_SQL_STATEMENT_COMPOUND as an expression
 * when @expr is of type #GDA_SQL_EXPRESSION_TYPE_COMPOUND.
 */
void
gda_sql_expression_set_compound (GdaSqlExpression *expr,
                                 GdaStatement *compound)
{}

/**
 * gda_sql_expression_get_compound:
 * @expr: a #GdaSqlExpression
 *
 * Used to get a #GdaStatement of type #GDA_SQL_STATEMENT_COMPOUND used as an
 * expression when @expr is of type #GDA_SQL_EXPRESSION_TYPE_COMPOUND.
 *
 * Returns: (transfer-none): a #GdaStatement of type #GDA_SQL_STATEMENT_COMPOUND
 */
GdaStatement*
gda_sql_expression_get_compound (GdaSqlExpression *expr)
{}

/**
 * gda_sql_expression_set_case_expr:
 * @expr: a #GdaSqlExpression
 * @case_expr: a #GdaSqlExpression used by a CASE expresion
 *
 * Sets a #GdaSqlExpression to be used as the expression by an CASE clausure,
 * when @expr is of type #GDA_SQL_EXPRESSION_TYPE_CASE.
 */
void
gda_sql_expression_set_case_expr (GdaSqlExpression *expr,
                                  GdaSqlExpression *case_expr)
{}

/**
 * gda_sql_expression_set_case_when:
 * @expr: a #GdaSqlExpression
 * @when_exprs: (element-type Gda.SqlExpression): a list of #GdaSqlExpression
 *
 * Sets the expressions in the WHEN clause of a CASE, when @expr is of type
 * #GDA_SQL_EXPRESSION_TYPE_CASE.
 */
void
gda_sql_expression_set_case_when (GdaSqlExpression *expr,
                                  const GSList *when_exprs)
{}

/**
 * gda_sql_expression_set_case_then:
 * @expr: a #GdaSqlExpression
 * @then_exprs: (element-type Gda.SqlExpression): a list of #GdaSqlExpression
 *
 * Sets the expressions in the THEN clause of a CASE, when @expr is of type
 * #GDA_SQL_EXPRESSION_TYPE_CASE.
 */
void
gda_sql_expression_set_case_then (GdaSqlExpression *expr,
                                  const GSList *then_exprs)
{}

/**
 * gda_sql_expression_set_case_else:
 * @expr: a #GdaSqlExpression
 * @else_exprs: (element-type Gda.SqlExpression): a list of #GdaSqlExpression
 *
 * Sets the expressions in the ELSE clause of a CASE, when @expr is of type
 * #GDA_SQL_EXPRESSION_TYPE_CASE.
 *
 */
void
gda_sql_expression_set_case_else (GdaSqlExpression *expr,
                                  const GSList *else_exprs)
{}

/**
 * gda_sql_expression_set_cast_as:
 * @expr: a #GdaSqlExpression
 * @cast_as: a #GType representing a data type to cast to
 *
 */
void
gda_sql_expression_set_cast_as (GdaSqlExpression *expr,
                                GType cast_as)
{}
