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

/* inclusion guard */
#ifndef __GDA_SQL_EXPRESSION_H__
#define __GDA_SQL_EXPRESSION_H__

#include <glib-object.h>
/*
 * Potentially, include other headers on which this header depends.
 */

/*
 * Type macros.
 */
#define GDA_TYPE_SQL_EXPRESSION                  (gda_sql_expression_get_type ())
#define GDA_SQL_EXPRESSION(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDA_TYPE_SQL_EXPRESSION, GdaSqlExpression))
#define GDA_IS_SQL_EXPRESSION(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDA_TYPE_SQL_EXPRESSION))
#define GDA_SQL_EXPRESSION_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GDA_TYPE_SQL_EXPRESSION, GdaSqlExpressionClass))
#define GDA_IS_SQL_EXPRESSION_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_SQL_EXPRESSION))
#define GDA_SQL_EXPRESSION_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GDA_TYPE_SQL_EXPRESSION, GdaSqlExpressionClass))

typedef struct _GdaSqlExpression        GdaSqlExpression;
typedef struct _GdaSqlExpressionClass   GdaSqlExpressionClass;
typedef struct _GdaSqlExpressionPrivate GdaSqlExpressionPrivate;

typedef enum   _GdaSqlExpressionType    GdaSqlExpressionType;

typedef enum {
	GDA_SQL_EXPRESSION_TYPE_VALUE,
	GDA_SQL_EXPRESSION_TYPE_VARIABLE,
	GDA_SQL_EXPRESSION_TYPE_FUNCTION,
	GDA_SQL_EXPRESSION_TYPE_CONDITION,
	GDA_SQL_EXPRESSION_TYPE_SELECT,
	GDA_SQL_EXPRESSION_TYPE_COMPOUND,
	GDA_SQL_EXPRESSION_TYPE_CASE,
	GDA_SQL_EXPRESSION_TYPE_CAST_AS
} _GdaSqlExpressionType;

struct _GdaSqlExpression
{
  GObject parent_instance;

  /* instance members */
  GdaSqlExpressionPrivate *priv;
};

struct _GdaSqlExpressionClass
{
  GObjectClass parent_class;

  /* class members */
};

/* used by GDA_TYPE_SQL_EXPRESSION */
GType              gda_sql_expression_get_type        (void);
GdaSqlExpression  *gda_sql_expression_new             (GdaSqlExpressionType type);
GdaSqlExpression  *gda_sql_expression_new_from_string (const gchar* str);
gchar             *gda_sql_expression_to_string       (GdaSqlExpression *expr);
void               gda_sql_expression_check_clean     (GdaSqlExpression *expr);

void               gda_sql_expression_set_value       (GdaSqlExpression *expr,
                                                    const GValue *val);
const GValue      *gda_sql_expression_get_value       (GdaSqlExpression *expr);


void               gda_sql_expression_set_variable_pspec (GdaSqlExpression *expr,
                                                    const GdaSqlParamSpec *spec);
GdaSqlParamSpec   *gda_sql_expression_get_variable_pspec (GdaSqlExpression *expr):


void               gda_sql_expression_set_function_name  (GdaSqlExpression *expr,
                                                    const gchar *name);
void               gda_sql_expression_set_function_args  (GdaSqlExpression *expr,
                                                    const GSList *args);

void               gda_sql_expression_set_operator_type  (GdaSqlExpression *expr,
                                                    GdaSqlOperatorType optype);
void               gda_sql_expression_set_operator_operands (GdaSqlExpression *expr,
                                                    const GSList *operands);

void               gda_sql_expression_set_select      (GdaSqlExpression *expr,
                                                    GdaStatement *select);
GdaSqlSelect      *gda_sql_expression_get_select      (GdaSqlExpression *expr);


void               gda_sql_expression_set_compound    (GdaSqlExpression *expr,
                                                    GdaStatement *compound);
GdaStatement      *gda_sql_expression_get_compound    (GdaSqlExpression *expr);


void               gda_sql_expression_set_case_expr   (GdaSqlExpression *expr,
                                                    GdaSqlExpression *case_expr);
void               gda_sql_expression_set_case_when   (GdaSqlExpression *expr,
                                                    const GSList *when_exprs);
void               gda_sql_expression_set_case_then   (GdaSqlExpression *expr,
                                                    const GSList *then_exprs);
void               gda_sql_expression_set_case_else   (GdaSqlExpression *expr,
                                                    const GSList *else_exprs);


void               gda_sql_expression_set_cast_as     (GdaSqlExpression *expr,
                                                    GType cast_as);

#endif /* __GDA_SQL_EXPRESSION_H__ */
