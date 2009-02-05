/* 
 * Copyright (C) 2007 - 2009 Vivien Malerba
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

#ifndef _GDA_STATEMENT_STRUCT_SELECT_H_
#define _GDA_STATEMENT_STRUCT_SELECT_H_

#include <glib.h>
#include <glib-object.h>
#include <sql-parser/gda-statement-struct-decl.h>
#include <sql-parser/gda-statement-struct-parts.h>

/*
 * Structure definition
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

	/* Padding for future expansion */
	gpointer         _gda_reserved1;
	gpointer         _gda_reserved2;
};

/*
 * Common operations
 */
gpointer  gda_sql_statement_select_new (void);
gpointer  gda_sql_statement_select_copy (gpointer src);
void      gda_sql_statement_select_free (gpointer stmt);
gchar    *gda_sql_statement_select_serialize (gpointer stmt);
GdaSqlStatementContentsInfo *gda_sql_statement_select_get_infos (void);

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
#endif
