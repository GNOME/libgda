/*
 * Copyright (C) 2008 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2009 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2011 Daniel Espinosa <despinosa@src.gnome.org>
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

#ifndef _GDA_STATEMENT_STRUCT_COMPOUND_H_
#define _GDA_STATEMENT_STRUCT_COMPOUND_H_

#include <glib.h>
#include <glib-object.h>
#include <libgda/sql-parser/gda-statement-struct-decl.h>
#include <libgda/sql-parser/gda-statement-struct-select.h>

G_BEGIN_DECLS

/*
 * Kinds
 */
/**
 * GdaSqlStatementCompoundType:
 * @GDA_SQL_STATEMENT_COMPOUND_UNION: 
 * @GDA_SQL_STATEMENT_COMPOUND_UNION_ALL: 
 * @GDA_SQL_STATEMENT_COMPOUND_INTERSECT: 
 * @GDA_SQL_STATEMENT_COMPOUND_INTERSECT_ALL: 
 * @GDA_SQL_STATEMENT_COMPOUND_EXCEPT: 
 * @GDA_SQL_STATEMENT_COMPOUND_EXCEPT_ALL: 
 */
typedef enum {
	GDA_SQL_STATEMENT_COMPOUND_UNION,
	GDA_SQL_STATEMENT_COMPOUND_UNION_ALL,
	GDA_SQL_STATEMENT_COMPOUND_INTERSECT,
	GDA_SQL_STATEMENT_COMPOUND_INTERSECT_ALL,
	GDA_SQL_STATEMENT_COMPOUND_EXCEPT,
	GDA_SQL_STATEMENT_COMPOUND_EXCEPT_ALL
} GdaSqlStatementCompoundType;

/*
 * Structure definition
 */
/**
 * GdaSqlStatementCompound:
 * @any: 
 * @compound_type: 
 * @stmt_list:
 */
struct _GdaSqlStatementCompound {
	GdaSqlAnyPart                any;
	GdaSqlStatementCompoundType  compound_type;
	GSList                      *stmt_list; /* list of SELECT or COMPOUND statements */

	/*< private >*/
	/* Padding for future expansion */
	gpointer         _gda_reserved1;
	gpointer         _gda_reserved2;
};

/*
 * Common operations
 */
gpointer  _gda_sql_statement_compound_copy (gpointer src);
void      _gda_sql_statement_compound_free (gpointer stmt);
gchar    *_gda_sql_statement_compound_serialize (gpointer stmt);
GdaSqlStatementContentsInfo *_gda_sql_statement_compound_get_infos (void);

/*
 * compound specific operations
 */
gint      _gda_sql_statement_compound_get_n_cols (GdaSqlStatementCompound *compound, GError **error);
GdaSqlAnyPart * _gda_sql_statement_compound_reduce (GdaSqlAnyPart *compound_or_select);

/*
 * Functions used by the parser
 */
void gda_sql_statement_compound_set_type (GdaSqlStatement *stmt, GdaSqlStatementCompoundType type);
void gda_sql_statement_compound_take_stmt (GdaSqlStatement *stmt, GdaSqlStatement *s);

G_END_DECLS

#endif
