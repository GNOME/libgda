/*
 * Copyright (C) 2008 - 2009 Vivien Malerba <malerba@gnome-db.org>
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

#include <sql-parser/gda-sql-statement.h>

G_BEGIN_DECLS

/* private information to implement custom 
 * SQL renderers for GdaStatement objects
 */

typedef struct _GdaSqlRenderingContext GdaSqlRenderingContext;
typedef gchar *(*GdaSqlRenderingFunc)      (GdaSqlAnyPart *node, GdaSqlRenderingContext *context, GError **error);
typedef gchar *(*GdaSqlRenderingExpr)      (GdaSqlExpr *expr, GdaSqlRenderingContext *context, 
					    gboolean *is_default, gboolean *is_null, 
					    GError **error);
typedef gchar *(*GdaSqlRenderingPSpecFunc) (GdaSqlParamSpec *pspec, GdaSqlExpr *expr, GdaSqlRenderingContext *context, 
					    gboolean *is_default, gboolean *is_null, 
					    GError **error);
typedef gchar *(*GdaSqlRenderingValue)     (const GValue *value, GdaSqlRenderingContext *context, GError **error);

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

	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
	void (*_gda_reserved5) (void);
	void (*_gda_reserved6) (void);
	void (*_gda_reserved7) (void);
	void (*_gda_reserved8) (void);
};

gchar *gda_statement_to_sql_real (GdaStatement *stmt, GdaSqlRenderingContext *context, GError **error);

G_END_DECLS

#endif



