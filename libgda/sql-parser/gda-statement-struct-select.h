/*
 * Copyright (C) 2008 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2009 Murray Cumming <murrayc@murrayc.com>
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

#ifndef _GDA_STATEMENT_STRUCT_SELECT_H_
#define _GDA_STATEMENT_STRUCT_SELECT_H_

#include <glib.h>
#include <glib-object.h>
#include <libgda/sql-parser/gda-statement-struct-decl.h>
#include <libgda/sql-parser/gda-statement-struct-parts.h>

G_BEGIN_DECLS

/*
 * Structure definition
 */
/**
 * GdaSqlStatementSelect:
 * @any: 
 * @distinct: 
 * @distinct_expr: 
 * @expr_list: 
 * @from: 
 * @where_cond: 
 * @group_by: 
 * @having_cond: 
 * @order_by: 
 * @limit_count: 
 * @limit_offset: 
 */
struct _GdaSqlStatementSelect {
	GdaSqlAnyPart     any;
	gboolean          distinct;
	GdaSqlExpr       *distinct_expr;

	GSList           *expr_list;  /* list of GdaSqlSelectField pointers */
	GdaSqlSelectFrom *from;
	
	GdaSqlExpr       *where_cond; /* WHERE... */
	GSList           *group_by; /* list of GdaSqlExpr pointers */
	GdaSqlExpr       *having_cond; /* HAVING... */
	GSList           *order_by;   /* list of GdaSqlSelectOrder pointers */

	GdaSqlExpr       *limit_count;
	GdaSqlExpr       *limit_offset;

	/*< private >*/
	/* Padding for future expansion */
	gpointer         _gda_reserved1;
	gpointer         _gda_reserved2;
};

/*
 * Common operations
 */
gpointer  _gda_sql_statement_select_copy (gpointer src);
void      _gda_sql_statement_select_free (gpointer stmt);
gchar    *_gda_sql_statement_select_serialize (gpointer stmt);
GdaSqlStatementContentsInfo *_gda_sql_statement_select_get_infos (void);

/*
 * Functions used by the parser
 */
void gda_sql_statement_select_take_distinct (GdaSqlStatement *stmt, gboolean distinct, GdaSqlExpr *distinct_expr);
void gda_sql_statement_select_take_expr_list (GdaSqlStatement *stmt, GSList *expr_list);
void gda_sql_statement_select_take_from (GdaSqlStatement *stmt, GdaSqlSelectFrom *from);
void gda_sql_statement_select_take_where_cond (GdaSqlStatement *stmt, GdaSqlExpr *expr);
void gda_sql_statement_select_take_group_by (GdaSqlStatement *stmt, GSList *group_by);
void gda_sql_statement_select_take_having_cond (GdaSqlStatement *stmt, GdaSqlExpr *expr);
void gda_sql_statement_select_take_order_by (GdaSqlStatement *stmt, GSList *order_by);
void gda_sql_statement_select_take_limits (GdaSqlStatement *stmt, GdaSqlExpr *count, GdaSqlExpr *offset);

G_END_DECLS

#endif
