/* 
 * Copyright (C) 2007 Vivien Malerba
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

#ifndef _GDA_STATEMENT_STRUCT_H_
#define _GDA_STATEMENT_STRUCT_H_

#include <glib.h>
#include <glib-object.h>
#include <sql-parser/gda-statement-struct-decl.h>

typedef void (*GdaSqlStatementFunc) (GdaSqlAnyPart*, gpointer);

struct _GdaSqlStatement {
	gchar               *sql;
	GdaSqlStatementType  stmt_type;
	gpointer             contents; /* depends on stmt_type */
};

GdaSqlStatement             *gda_sql_statement_new       (GdaSqlStatementType type);
GdaSqlStatement             *gda_sql_statement_copy      (GdaSqlStatement *stmt);
void                         gda_sql_statement_free      (GdaSqlStatement *stmt);
gchar                       *gda_sql_statement_serialize (GdaSqlStatement *stmt);

const gchar                 *gda_sql_statement_type_to_string (GdaSqlStatementType type);
GdaSqlStatementType          gda_sql_statement_string_to_type (const gchar *type);

gboolean                     gda_sql_statement_check_structure   (GdaSqlStatement *stmt, GError **error);
gboolean                     gda_sql_statement_check_with_dict   (GdaSqlStatement *stmt, 
								  GdaDict *dict, GdaSqlStatementFunc func, gpointer func_data, 
								  GError **error);
void                         gda_sql_statement_check_clean       (GdaSqlStatement *stmt);

GdaSqlStatementContentsInfo *gda_sql_statement_get_contents_infos (GdaSqlStatementType type) ;

#endif
