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

#ifndef _GDA_STATEMENT_STRUCT_H_
#define _GDA_STATEMENT_STRUCT_H_

#include <glib.h>
#include <glib-object.h>
#include <sql-parser/gda-statement-struct-decl.h>
#include <libgda/gda-meta-store.h>

struct _GdaSqlStatement {
	gchar               *sql;
	GdaSqlStatementType  stmt_type;
	gpointer             contents; /* depends on stmt_type */
	GdaMetaStruct       *validity_meta_struct; /* set when gda_sql_statement_check_validity() was last called */

	/* Padding for future expansion */
	gpointer         _gda_reserved1;
	gpointer         _gda_reserved2;
};

#define GDA_TYPE_SQL_STATEMENT (gda_sql_statement_get_type())

GType                        gda_sql_statement_get_type  (void) G_GNUC_CONST;
GdaSqlStatement             *gda_sql_statement_new       (GdaSqlStatementType type);
GdaSqlStatement             *gda_sql_statement_copy      (GdaSqlStatement *stmt);
void                         gda_sql_statement_free      (GdaSqlStatement *stmt);
gchar                       *gda_sql_statement_serialize (GdaSqlStatement *stmt);

const gchar                 *gda_sql_statement_type_to_string (GdaSqlStatementType type);
GdaSqlStatementType          gda_sql_statement_string_to_type (const gchar *type);

gboolean                     gda_sql_statement_check_structure (GdaSqlStatement *stmt, GError **error);
gboolean                     gda_sql_statement_check_validity  (GdaSqlStatement *stmt, GdaConnection *cnc, GError **error);
void                         gda_sql_statement_check_clean     (GdaSqlStatement *stmt);
gboolean                     gda_sql_statement_normalize       (GdaSqlStatement *stmt, GdaConnection *cnc, GError **error);

GdaSqlStatementContentsInfo *gda_sql_statement_get_contents_infos (GdaSqlStatementType type) ;

#endif
