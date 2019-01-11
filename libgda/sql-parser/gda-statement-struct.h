/*
 * Copyright (C) 2008 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 Murray Cumming <murrayc@murrayc.com>
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

#ifndef _GDA_STATEMENT_STRUCT_H_
#define _GDA_STATEMENT_STRUCT_H_

#include <glib.h>
#include <glib-object.h>
#include <libgda/sql-parser/gda-statement-struct-decl.h>
#include <libgda/gda-meta-store.h>

G_BEGIN_DECLS

/**
 * GdaSqlStatement:
 * @sql: 
 * @stmt_type: type of statement 
 * @contents: contents, cast it depending on @stmt_type (for example to a #GdaSqlStatementSelect).
 * @validity_meta_struct:
 *
 * This structure is the top level structure encapsulating several type of statements.
 */
struct _GdaSqlStatement {
	gchar               *sql;
	GdaSqlStatementType  stmt_type;
	gpointer             contents; /* depends on stmt_type */
	GdaMetaStruct       *validity_meta_struct; /* set when gda_sql_statement_check_validity() was last called */

	/*< private >*/
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
gboolean                     gda_sql_statement_check_validity_m (GdaSqlStatement *stmt,
								 GdaMetaStruct *mstruct,
								 GError **error);

void                         gda_sql_statement_check_clean     (GdaSqlStatement *stmt);
gboolean                     gda_sql_statement_normalize       (GdaSqlStatement *stmt, GdaConnection *cnc, GError **error);

GdaSqlStatementContentsInfo *gda_sql_statement_get_contents_infos (GdaSqlStatementType type) ;

G_END_DECLS

#endif
