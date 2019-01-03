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

#ifndef _GDA_STATEMENT_STRUCT_UPDATE_H_
#define _GDA_STATEMENT_STRUCT_UPDATE_H_

#include <glib.h>
#include <glib-object.h>
#include <libgda/sql-parser/gda-statement-struct-decl.h>
#include <libgda/sql-parser/gda-statement-struct-select.h>
#include <libgda/sql-parser/gda-statement-struct-parts.h>

G_BEGIN_DECLS

/*
 * Structure definition
 */
/**
 * GdaSqlStatementUpdate:
 * @any: 
 * @on_conflict: 
 * @table: 
 * @fields_list: 
 * @expr_list: 
 * @cond:
 */
struct _GdaSqlStatementUpdate {
	GdaSqlAnyPart     any;
	gchar            *on_conflict; /* conflict resolution clause */
	GdaSqlTable      *table;
	GSList           *fields_list; /* list of GdaSqlField pointers */
	GSList           *expr_list;   /* list of GdaSqlExpr pointers */
	GdaSqlExpr       *cond;

	/*< private >*/
	/* Padding for future expansion */
	gpointer         _gda_reserved1;
	gpointer         _gda_reserved2;
};

/*
 * Common operations
 */
GdaSqlStatementContentsInfo *_gda_sql_statement_update_get_infos (void);

/*
 * Functions used by the parser
 */
void gda_sql_statement_update_take_table_name (GdaSqlStatement *stmt, GValue *value);
void gda_sql_statement_update_take_on_conflict (GdaSqlStatement *stmt, GValue *value);
void gda_sql_statement_update_take_condition (GdaSqlStatement *stmt, GdaSqlExpr *cond);
void gda_sql_statement_update_take_set_value (GdaSqlStatement *stmt, GValue *fname, GdaSqlExpr *expr);

G_END_DECLS

#endif
