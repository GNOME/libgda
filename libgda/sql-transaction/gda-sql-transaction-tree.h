/* gda-sql-transaction-tree.h
 *
 * Copyright (C) 2006 Vivien Malerba
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifndef GDA_SQL_TRANSACTION_TREE_H
#define GDA_SQL_TRANSACTION_TREE_H

#include "gda-sql-transaction-parser.h"

GdaSqlTransaction  *gda_sql_transaction_build (GdaSqlTransactionType type, gchar *trans_name, GdaTransactionIsolation level);
void                gda_sql_transaction_destroy (GdaSqlTransaction *trans);

#endif
