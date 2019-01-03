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

#ifndef _GDA_STATEMENT_STRUCT_TRANS_H_
#define _GDA_STATEMENT_STRUCT_TRANS_H_

#include <glib.h>
#include <glib-object.h>
#include <libgda/gda-enums.h>
#include <libgda/sql-parser/gda-statement-struct-decl.h>
#include <libgda/sql-parser/gda-statement-struct-parts.h>

G_BEGIN_DECLS

/*
 * Structure definition
 */
/**
 * GdaSqlStatementTransaction:
 * @any: inheritance structure
 * @isolation_level: isolation level as a #GdaTransactionIsolation
 * @trans_mode: transaction mode (DEFERRED, IMMEDIATE, EXCLUSIVE, READ_WRITE, READ_ONLY)
 * @trans_name: transaction name
 *
 * The statement is a transaction management related statement (BEGIN, ROLLBACK, etc). The #GdaSqlStatementTransaction structure
 * does not hold enough information to reconstruct the complete SQL statement (some information may be missing) - the aim of this
 * structure is to identify a minimum set of information in the transaction statement. Note that the complete SQL which created the
 * statement should be available in the #GdaSqlStatement structure which encapsulates this structure.
 */
struct _GdaSqlStatementTransaction {
	GdaSqlAnyPart           any;
	GdaTransactionIsolation isolation_level;
	gchar                  *trans_mode; /* DEFERRED, IMMEDIATE, EXCLUSIVE, READ_WRITE, READ_ONLY */
	gchar                  *trans_name;

	/*< private >*/
	/* Padding for future expansion */
	gpointer         _gda_reserved1;
	gpointer         _gda_reserved2;
};

/*
 * Common operations
 */
GdaSqlStatementContentsInfo *_gda_sql_statement_begin_get_infos (void);
GdaSqlStatementContentsInfo *_gda_sql_statement_commit_get_infos (void);
GdaSqlStatementContentsInfo *_gda_sql_statement_rollback_get_infos (void);

GdaSqlStatementContentsInfo *_gda_sql_statement_savepoint_get_infos (void);
GdaSqlStatementContentsInfo *_gda_sql_statement_rollback_savepoint_get_infos (void);
GdaSqlStatementContentsInfo *_gda_sql_statement_delete_savepoint_get_infos (void);

/*
 * Functions used by the parser
 */
void   gda_sql_statement_trans_take_mode (GdaSqlStatement *stmt, GValue *value);
void   gda_sql_statement_trans_set_isol_level (GdaSqlStatement *stmt, GdaTransactionIsolation level);
void   gda_sql_statement_trans_take_name (GdaSqlStatement *stmt, GValue *value);

G_END_DECLS

#endif
