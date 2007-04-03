/* gda-sql-transaction-parser.h
 *
 * Copyright (C) 2006 Vivien Malerba
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

#ifndef GDA_SQL_TRANSACTION_PARSER_H
#define GDA_SQL_TRANSACTION_PARSER_H

#include <libgda/gda-enums.h>
#include <glib.h>
#define YY_NO_UNISTD_H

typedef struct _GdaSqlTransaction GdaSqlTransaction;
typedef enum {
	GDA_SQL_TRANSACTION_BEGIN,
	GDA_SQL_TRANSACTION_COMMIT,
	GDA_SQL_TRANSACTION_ROLLBACK,
	GDA_SQL_TRANSACTION_SAVEPOINT_ADD,
	GDA_SQL_TRANSACTION_SAVEPOINT_REMOVE,
	GDA_SQL_TRANSACTION_SAVEPOINT_ROLLBACK
} GdaSqlTransactionType;

struct _GdaSqlTransaction
{
	GdaSqlTransactionType    trans_type;
	gchar                   *trans_name;
	gchar                   *full_query;
	GdaTransactionIsolation  isolation_level;
};

GdaSqlTransaction *gda_sql_transaction_parse            (const char *sql_statement);
GdaSqlTransaction *gda_sql_transaction_parse_with_error (const char *sql_statement,
							 GError ** error);

#endif
