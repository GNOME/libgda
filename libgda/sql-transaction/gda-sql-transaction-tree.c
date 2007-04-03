/* gda-sql-transaction-parser.c
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

#include <stdio.h>
#include <stdlib.h>

#include "gda-sql-transaction-parser.h"
#include "gda-sql-transaction-tree.h"

GdaSqlTransaction *
gda_sql_transaction_build (GdaSqlTransactionType type, gchar *trans_name, GdaTransactionIsolation level)
{
	GdaSqlTransaction *retval;

	retval = g_new0 (GdaSqlTransaction, 1);
	retval->trans_type = type;
	retval->trans_name = trans_name;
	retval->isolation_level = level;

	return retval;
}

void
gda_sql_transaction_destroy (GdaSqlTransaction *trans)
{
	g_free (trans->full_query);
	g_free (trans->trans_name);
	g_free (trans);
}
