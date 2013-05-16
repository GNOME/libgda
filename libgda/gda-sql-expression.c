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
  GdaSqlExpressionType expr_type;
};

/* properties */
enum
{
        PROP_0
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
  priv->expr_type = GDA_SQL_EXPRESSION_TYPE_VALUE;
}

/**
 * gda_sql_expression_new:
 * @type: a #GdaSqlExpressionType
 *
 * Since: 5.2
 *
 * Returns: (transfer-full): a new #GdaSqlExpression object
 */
GdaSqlExpression *
gda_sql_expression_new (GdaSqlExpressionType type)
{
	GdaSqlExpression *obj;
	GdaSqlExpressionPrivate *priv;
	obj = GDA_SQL_EXPRESSION (g_object_new (GDA_TYPE_SQL_EXPRESSION, NULL));
	priv = GDA_SQL_EXPRESSION_GET_PRIVATE (obj);
	priv->expr_type = type;
	return obj;
}

/**
 * gda_sql_expression_new_from_string:
 * @etype: a #GdaSqlExpressionType
 * @str: a string describing an expression
 * @cnc: a #GdaConnection
 *
 * Since: 5.2
 *
 * Returns: (transfer-full): a new #GdaSqlExpression object
 */
GdaSqlExpression*
gda_sql_expression_new_from_string (GdaSqlExpressionType etype,
                                    const gchar *str,
                                    GdaConnection *cnc)
{
	return NULL;
}

/**
 * gda_sql_expression_to_string:
 * @expr: a #GdaSqlExpression
 *
 * Since: 5.2
 *
 * Returns: (transfer-full): a new string representing a #GdaSqlExpression
 */
gchar*
gda_sql_expression_to_string (GdaSqlExpression *expr)
{
	return g_strdup ("GdaSqlExpression");
}

/**
 * gda_sql_expression_check_clean:
 * @expr: a #GdaSqlExpression
 *
 * Since: 5.2
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
 *
 * Since: 5.2
 *
 */
void
gda_sql_expression_set_value (GdaSqlExpression *expr,
                              const GValue *val)
{
	g_return_if_fail (GDA_IS_SQL_EXPRESSION (expr));
	GdaSqlExpressionPrivate *priv = GDA_SQL_EXPRESSION_GET_PRIVATE (expr);
	g_return_if_fail (priv->expr_type == GDA_SQL_EXPRESSION_TYPE_VALUE);

	GdaDataHandler *dh = gda_data_handler_get_default (G_VALUE_TYPE (val));
	priv->expr_id = gda_sql_builder_add_expr_value (priv->builder, dh, val);
}

/**
 * gda_sql_expression_get_value:
 * @expr: a #GdaSqlExpression
 *
 * Since: 5.2
 *
 * Returns: (transfer-full): the #GValue used as an expresion
 */
GValue*
gda_sql_expression_get_value (GdaSqlExpression *expr)
{
	g_return_if_fail (GDA_IS_SQL_EXPRESSION (expr));
	GdaSqlExpressionPrivate *priv = GDA_SQL_EXPRESSION_GET_PRIVATE (expr);
	g_return_if_fail (priv->expr_type == GDA_SQL_EXPRESSION_TYPE_VALUE);

	GdaSqlExpr *e = gda_sql_builder_export_expression (priv->builder, priv->expr_id);
	if (e) {
		GValue *v = gda_value_copy (e->value_expr);
		gda_sql_expr_free (e);
		return v;
	}
	return NULL;
}

/**
 * gda_sql_expression_set_variable_pspec:
 * @expr: a #GdaSqlExpression
 * @spec: a #GdaSqlParamSpec
 *
 * To be used by a #GdaSqlExpression of type #GDA_SQL_EXPRESSION_TYPE_VARIABLE.
 *
 * Since: 5.2
 *
 */
void
gda_sql_expression_set_variable_pspec (GdaSqlExpression *expr,
                                       const GdaSqlParamSpec *spec)
{
	g_return_if_fail (GDA_IS_SQL_EXPRESSION (expr));
	GdaSqlExpressionPrivate *priv = GDA_SQL_EXPRESSION_GET_PRIVATE (expr);
	g_return_if_fail (priv->expr_type == GDA_SQL_EXPRESSION_TYPE_VARIABLE);
	
}

/**
 * gda_sql_expression_get_variable_pspec:
 * @expr: a #GdaSqlExpression
 *
 * Returns: (transfer-full): a #GdaSqlParamSpec used by @expr of type
 * #GDA_SQL_EXPRESSION_TYPE_VALUE
 *
 * Since: 5.2
 *
 */
GdaSqlParamSpec*
gda_sql_expression_get_variable_pspec (GdaSqlExpression *expr)
{
	g_return_if_fail (GDA_IS_SQL_EXPRESSION (expr));
	GdaSqlExpressionPrivate *priv = GDA_SQL_EXPRESSION_GET_PRIVATE (expr);
	g_return_if_fail (priv->expr_type == GDA_SQL_EXPRESSION_TYPE_VARIABLE);
}

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
{
	g_return_if_fail (GDA_IS_SQL_EXPRESSION (expr));
	GdaSqlExpressionPrivate *priv = GDA_SQL_EXPRESSION_GET_PRIVATE (expr);
	g_return_if_fail (priv->expr_type == GDA_SQL_EXPRESSION_TYPE_FUNCTION);
}

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
{
	g_return_if_fail (GDA_IS_SQL_EXPRESSION (expr));
	GdaSqlExpressionPrivate *priv = GDA_SQL_EXPRESSION_GET_PRIVATE (expr);
	g_return_if_fail (priv->expr_type == GDA_SQL_EXPRESSION_TYPE_FUNCTION);
}

/**
 * gda_sql_expression_set_operator_type:
 * @expr: a #GdaSqlExpression
 * @optype: a #GdaSqlExpressionType
 *
 * Sets operator's type of a #GdaSqlExpression of type
 * #GDA_SQL_EXPRESSION_TYPE_CONDITION.
 *
 * Since: 5.2
 *
 */
void
gda_sql_expression_set_operator_type  (GdaSqlExpression *expr,
                                       GdaSqlOperatorType optype)
{
	g_return_if_fail (GDA_IS_SQL_EXPRESSION (expr));
	GdaSqlExpressionPrivate *priv = GDA_SQL_EXPRESSION_GET_PRIVATE (expr);
	g_return_if_fail (priv->expr_type == GDA_SQL_EXPRESSION_TYPE_CONDITION);
}

/**
 * gda_sql_expression_set_operator_operands:
 * @expr: a #GdaSqlExpression
 * @operands: (element-type Gda.SqlExpression): a lists of #GdaSqlExpression
 * expresions
 *
 * Sets operator's operands of a #GdaSqlExpression of type
 * #GDA_SQL_EXPRESSION_TYPE_CONDITION.
 *
 * Since: 5.2
 *
 */
void
gda_sql_expression_set_operator_operands (GdaSqlExpression *expr,
                                          const GSList *operands)
{
	g_return_if_fail (GDA_IS_SQL_EXPRESSION (expr));
	GdaSqlExpressionPrivate *priv = GDA_SQL_EXPRESSION_GET_PRIVATE (expr);
	g_return_if_fail (priv->expr_type == GDA_SQL_EXPRESSION_TYPE_CONDITION);
}

/**
 * gda_sql_expression_set_select:
 * @expr: a #GdaSqlExpression
 * @select: a #GdaStatement to be used as a sub-select expression
 *
 * Since: 5.2
 *
 */
void
gda_sql_expression_set_select (GdaSqlExpression *expr,
                               GdaStatement *select)
{
	g_return_if_fail (GDA_IS_SQL_EXPRESSION (expr));
	GdaSqlExpressionPrivate *priv = GDA_SQL_EXPRESSION_GET_PRIVATE (expr);
	g_return_if_fail (priv->expr_type == GDA_SQL_EXPRESSION_TYPE_SELECT);
}

/**
 * gda_sql_expression_get_select:
 * @expr: a #GdaSqlExpression
 *
 * Since: 5.2
 *
 * Returns: a #GdaStatement of type #GDA_SQL_STATEMENT_SELECT
 */
GdaStatement*
gda_sql_expression_get_select (GdaSqlExpression *expr)
{
	g_return_if_fail (GDA_IS_SQL_EXPRESSION (expr));
	GdaSqlExpressionPrivate *priv = GDA_SQL_EXPRESSION_GET_PRIVATE (expr);
	g_return_if_fail (priv->expr_type == GDA_SQL_EXPRESSION_TYPE_SELECT);
}

/**
 * gda_sql_expression_set_compound:
 * @expr: a #GdaSqlExpression
 * @compound: a #GdaStatement
 *
 * Uses a #GdaStatement of type #GDA_SQL_STATEMENT_COMPOUND as an expression
 * when @expr is of type #GDA_SQL_EXPRESSION_TYPE_COMPOUND.
 *
 * Since: 5.2
 *
 */
void
gda_sql_expression_set_compound (GdaSqlExpression *expr,
                                 GdaStatement *compound)
{
	g_return_if_fail (GDA_IS_SQL_EXPRESSION (expr));
	GdaSqlExpressionPrivate *priv = GDA_SQL_EXPRESSION_GET_PRIVATE (expr);
	g_return_if_fail (priv->expr_type == GDA_SQL_EXPRESSION_TYPE_COMPOUND);
}

/**
 * gda_sql_expression_get_compound:
 * @expr: a #GdaSqlExpression
 *
 * Used to get a #GdaStatement of type #GDA_SQL_STATEMENT_COMPOUND used as an
 * expression when @expr is of type #GDA_SQL_EXPRESSION_TYPE_COMPOUND.
 *
 * Since: 5.2
 *
 * Returns: (transfer-none): a #GdaStatement of type #GDA_SQL_STATEMENT_COMPOUND
 */
GdaStatement*
gda_sql_expression_get_compound (GdaSqlExpression *expr)
{
	g_return_if_fail (GDA_IS_SQL_EXPRESSION (expr));
	GdaSqlExpressionPrivate *priv = GDA_SQL_EXPRESSION_GET_PRIVATE (expr);
	g_return_if_fail (priv->expr_type == GDA_SQL_EXPRESSION_TYPE_COMPOUND);
}

/**
 * gda_sql_expression_set_case_expr:
 * @expr: a #GdaSqlExpression
 * @case_expr: a #GdaSqlExpression used by a CASE expresion
 *
 * Sets a #GdaSqlExpression to be used as the expression by an CASE clausure,
 * when @expr is of type #GDA_SQL_EXPRESSION_TYPE_CASE.
 *
 * Since: 5.2
 *
 */
void
gda_sql_expression_set_case_expr (GdaSqlExpression *expr,
                                  GdaSqlExpression *case_expr)
{
	g_return_if_fail (GDA_IS_SQL_EXPRESSION (expr));
	GdaSqlExpressionPrivate *priv = GDA_SQL_EXPRESSION_GET_PRIVATE (expr);
	g_return_if_fail (priv->expr_type == GDA_SQL_EXPRESSION_TYPE_CASE);
}

/**
 * gda_sql_expression_set_case_when:
 * @expr: a #GdaSqlExpression
 * @when_exprs: (element-type Gda.SqlExpression): a list of #GdaSqlExpression
 *
 * Sets the expressions in the WHEN clause of a CASE, when @expr is of type
 * #GDA_SQL_EXPRESSION_TYPE_CASE.
 *
 * Since: 5.2
 *
 */
void
gda_sql_expression_set_case_when (GdaSqlExpression *expr,
                                  const GSList *when_exprs)
{
	g_return_if_fail (GDA_IS_SQL_EXPRESSION (expr));
	GdaSqlExpressionPrivate *priv = GDA_SQL_EXPRESSION_GET_PRIVATE (expr);
	g_return_if_fail (priv->expr_type == GDA_SQL_EXPRESSION_TYPE_CASE);
}

/**
 * gda_sql_expression_set_case_then:
 * @expr: a #GdaSqlExpression
 * @then_exprs: (element-type Gda.SqlExpression): a list of #GdaSqlExpression
 *
 * Sets the expressions in the THEN clause of a CASE, when @expr is of type
 * #GDA_SQL_EXPRESSION_TYPE_CASE.
 *
 * Since: 5.2
 *
 */
void
gda_sql_expression_set_case_then (GdaSqlExpression *expr,
                                  const GSList *then_exprs)
{
	g_return_if_fail (GDA_IS_SQL_EXPRESSION (expr));
	GdaSqlExpressionPrivate *priv = GDA_SQL_EXPRESSION_GET_PRIVATE (expr);
	g_return_if_fail (priv->expr_type == GDA_SQL_EXPRESSION_TYPE_CASE);
}

/**
 * gda_sql_expression_set_case_else:
 * @expr: a #GdaSqlExpression
 * @else_exprs: (element-type Gda.SqlExpression): a list of #GdaSqlExpression
 *
 * Sets the expressions in the ELSE clause of a CASE, when @expr is of type
 * #GDA_SQL_EXPRESSION_TYPE_CASE.
 *
 * Since: 5.2
 *
 */
void
gda_sql_expression_set_case_else (GdaSqlExpression *expr,
                                  const GSList *else_exprs)
{
	g_return_if_fail (GDA_IS_SQL_EXPRESSION (expr));
	GdaSqlExpressionPrivate *priv = GDA_SQL_EXPRESSION_GET_PRIVATE (expr);
	g_return_if_fail (priv->expr_type == GDA_SQL_EXPRESSION_TYPE_CASE);
}

/**
 * gda_sql_expression_set_cast_as:
 * @expr: a #GdaSqlExpression
 * @cast_as: a #GType representing a data type to cast to
 *
 * Since: 5.2
 *
 */
void
gda_sql_expression_set_cast_as (GdaSqlExpression *expr,
                                GType cast_as)
{
	g_return_if_fail (GDA_IS_SQL_EXPRESSION (expr));
	GdaSqlExpressionPrivate *priv = GDA_SQL_EXPRESSION_GET_PRIVATE (expr);
	g_return_if_fail (priv->expr_type == GDA_SQL_EXPRESSION_TYPE_CAST_AS);
}
