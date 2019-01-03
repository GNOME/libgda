/*
 * Copyright (C) 2008 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2015 Corentin Noël <corentin@elementary.io>
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

#ifndef __GDA_STATEMENT_EXTRA__
#define __GDA_STATEMENT_EXTRA__

#include <libgda/sql-parser/gda-sql-statement.h>

G_BEGIN_DECLS

/* private information to implement custom 
 * SQL renderers for GdaStatement objects
 */
typedef struct _GdaSqlRenderingContext GdaSqlRenderingContext;

/**
 * GdaSqlRenderingFunc:
 * @node: a #GdaSqlAnyPart pointer, to be cast to the correct type depending on which part the function has to render
 * @context: the rendering context
 * @error: a place to store errors, or %NULL
 * @Returns: a new string, or %NULL if an error occurred
 *
 * Function to render any #GdaSqlAnyPart.
 */
typedef gchar *(*GdaSqlRenderingFunc)      (GdaSqlAnyPart *node, GdaSqlRenderingContext *context, GError **error);

/**
 * GdaSqlRenderingExpr:
 * @expr: #GdaSqlExpr to render
 * @context: the rendering context
 * @is_default: pointer to a #gboolean which is set to TRUE if value should be considered as a default value
 * @is_null: pointer to a #gboolean which is set to TRUE if value should be considered as NULL
 * @error: a place to store errors, or %NULL
 * @Returns: a new string, or %NULL if an error occurred
 *
 * Rendering function type to render a #GdaSqlExpr
 */
typedef gchar *(*GdaSqlRenderingExpr)      (GdaSqlExpr *expr, GdaSqlRenderingContext *context, 
					    gboolean *is_default, gboolean *is_null, 
					    GError **error);

/**
 * GdaSqlRenderingPSpecFunc:
 * @pspec: #GdaSqlParamSpec to render
 * @expr: (nullable): #GdaSqlExpr which may hold the default value for the parameter, or %NULL
 * @context: the rendering context
 * @is_default: pointer to a #gboolean which is set to TRUE if value should be considered as a default value
 * @is_null: pointer to a #gboolean which is set to TRUE if value should be considered as NULL
 * @error: a place to store errors, or %NULL
 * @Returns: a new string, or %NULL if an error occurred
 *
 * Rendering function type to render a #GdaSqlParamSpec
 */
typedef gchar *(*GdaSqlRenderingPSpecFunc) (GdaSqlParamSpec *pspec, GdaSqlExpr *expr, GdaSqlRenderingContext *context, 
					    gboolean *is_default, gboolean *is_null, 
					    GError **error);

/**
 * GdaSqlRenderingValue:
 * @value: the #GValue to render
 * @context: the rendering context
 * @error: a place to store errors, or %NULL
 * @Returns: a new string, or %NULL if an error occurred
 *
 * Rendering function type to render a #GValue
 */
typedef gchar *(*GdaSqlRenderingValue)     (const GValue *value, GdaSqlRenderingContext *context, GError **error);

/**
 * GdaSqlRenderingContext:
 * @flags: Global rendering options
 * @params: Parameters to be used while doing the rendering
 * @params_used: (element-type GdaHolder): When rendering is complete, contains the ordered list of parameters which have been used while doing the rendering
 * @provider: Pointer to the server provider to be used
 * @cnc: Pointer to the connection to be used
 * @render_value: function to render a #GValue
 * @render_param_spec: function to render a #GdaSqlParamSpec
 * @render_expr: function to render a #GdaSqlExpr
 * @render_unknown: function to render a #GdaSqlStatementUnknown
 * @render_begin: function to render a BEGIN #GdaSqlStatementTransaction
 * @render_rollback: function to render a ROLLBACK #GdaSqlStatementTransaction
 * @render_commit: function to render a COMMIT #GdaSqlStatementTransaction
 * @render_savepoint: function to render a ADD SAVEPOINT #GdaSqlStatementTransaction
 * @render_rollback_savepoint: function to render a ROLBACK SAVEPOINT #GdaSqlStatementTransaction
 * @render_delete_savepoint: function to render a DELETE SAVEPOINT #GdaSqlStatementTransaction
 * @render_select: function to render a #GdaSqlStatementSelect
 * @render_insert: function to render a #GdaSqlStatementInsert
 * @render_delete: function to render a #GdaSqlStatementDelete
 * @render_update: function to render a #GdaSqlStatementUpdate
 * @render_compound: function to render a #GdaSqlStatementCompound
 * @render_field: function to render a #GdaSqlField
 * @render_table: function to render a #GdaSqlTable
 * @render_function: function to render a #GdaSqlFunction
 * @render_operation: function to render a #GdaSqlOperation
 * @render_case: function to render a #GdaSqlCase
 * @render_select_field: function to render a #GdaSqlSelectField
 * @render_select_target: function to render a #GdaSqlSelectTarget
 * @render_select_join: function to render a #GdaSqlSelectJoin
 * @render_select_from: function to render a #GdaSqlSelectFrom
 * @render_select_order: function to render a #GdaSqlSelectOrder
 * @render_distinct: function to render the DISTINCT clause in a SELECT
 *
 * Specifies the context in which a #GdaSqlStatement is being converted to SQL.
 */
struct _GdaSqlRenderingContext {
	GdaStatementSqlFlag      flags;
	GdaSet                  *params;
	GSList                  *params_used;
	GdaServerProvider       *provider; /* may be NULL */
	GdaConnection           *cnc;      /* may be NULL */

	/* rendering functions */
	GdaSqlRenderingValue     render_value;
	GdaSqlRenderingPSpecFunc render_param_spec; 
	GdaSqlRenderingExpr      render_expr;

	GdaSqlRenderingFunc      render_unknown;

	GdaSqlRenderingFunc      render_begin;
	GdaSqlRenderingFunc      render_rollback;
	GdaSqlRenderingFunc      render_commit;
	GdaSqlRenderingFunc      render_savepoint;
	GdaSqlRenderingFunc      render_rollback_savepoint;
	GdaSqlRenderingFunc      render_delete_savepoint;

	GdaSqlRenderingFunc      render_select;
	GdaSqlRenderingFunc      render_insert;
	GdaSqlRenderingFunc      render_delete;
	GdaSqlRenderingFunc      render_update;
	GdaSqlRenderingFunc      render_compound;

	GdaSqlRenderingFunc      render_field;
	GdaSqlRenderingFunc      render_table;
	GdaSqlRenderingFunc      render_function;
	GdaSqlRenderingFunc      render_operation;
	GdaSqlRenderingFunc      render_case;
	GdaSqlRenderingFunc      render_select_field;
	GdaSqlRenderingFunc      render_select_target;
	GdaSqlRenderingFunc      render_select_join;
	GdaSqlRenderingFunc      render_select_from;
	GdaSqlRenderingFunc      render_select_order;
	GdaSqlRenderingFunc      render_distinct;

	/*< private >*/
	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
	void (*_gda_reserved5) (void);
	void (*_gda_reserved6) (void);
	void (*_gda_reserved7) (void);
};

/**
 * SECTION:provider-support-sql
 * @short_description: Adapting the SQL to the database's own SQL dialect
 * @title: SQL rendering API
 * @stability: Stable
 * @see_also:
 *
 * &LIBGDA; is able to render a #GdaStatement statement to SQL in a generic way (as close as possible to the SQL
 * standard). However as each database has ultimately its own SQL dialect, some parts of the rendering has
 * to be specialized.
 *
 * Customization is achieved by providing custom implementations of SQL rendering functions for each kind of
 * part in a #GdaSqlStatement structure, all packed in a #GdaSqlRenderingContext context structure. Functions
 * which are not customized will be implemented by the default ones.
 */

gchar *gda_statement_to_sql_real (GdaStatement *stmt, GdaSqlRenderingContext *context, GError **error);

G_END_DECLS

#endif



