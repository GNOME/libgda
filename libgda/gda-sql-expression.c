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

GdaSqlExpression *
gda_sql_expression_new (GdaSqlExpressionType type)
{}

GdaSqlExpression*
gda_sql_expression_new_from_string (const gchar* str)
{}

gchar*
gda_sql_expression_to_string (GdaSqlExpression *expr)
{}

void
gda_sql_expression_check_clean (GdaSqlExpression *expr)
{}

void
gda_sql_expression_take_select (GdaSqlExpression *expr, 
                                GdaStatement *stm)
{}

void
gda_sql_expression_set_value (GdaSqlExpression *expr,
                              const GValue *val)
{}

const GValue*
gda_sql_expression_get_value (GdaSqlExpression *expr)
{}


void
gda_sql_expression_set_variable_pspec (GdaSqlExpression *expr,
                                       const GdaSqlParamSpec *spec)
{}

GdaSqlParamSpec*
gda_sql_expression_get_variable_pspec (GdaSqlExpression *expr)
{}


void
gda_sql_expression_set_function_name (GdaSqlExpression *expr,
                                      const gchar *name)
{}

void
gda_sql_expression_set_function_args (GdaSqlExpression *expr,
                                      const GSList *args)
{}

void
gda_sql_expression_set_operator_type  (GdaSqlExpression *expr,
                                       GdaSqlOperatorType optype)
{}

void
gda_sql_expression_set_operator_operands (GdaSqlExpression *expr,
                                          const GSList *operands)
{}

void
gda_sql_expression_set_select (GdaSqlExpression *expr,
                               GdaStatement *select)
{}

GdaSqlSelect*
gda_sql_expression_get_select (GdaSqlExpression *expr)
{}

void
gda_sql_expression_set_compound (GdaSqlExpression *expr,
                                 GdaStatement *compound)
{}

GdaSqlCompound*
gda_sql_expression_set_compound (GdaSqlExpression *expr)
{}

void
gda_sql_expression_set_case_expr (GdaSqlExpression *expr,
                                  GdaSqlExpression *e)
{}

void
gda_sql_expression_set_case_when (GdaSqlExpression *expr,
                                  const GSList *when)
{}

void
gda_sql_expression_set_case_then (GdaSqlExpression *expr,
                                  const GSList *then)
{}

void
gda_sql_expression_set_case_else (GdaSqlExpression *expr,
                                  const GSList *_else)
{}

void
gda_sql_expression_set_cast_as (GdaSqlExpression *expr,
                                const gchar *cast_as)
{}
