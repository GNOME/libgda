/* gda-enums.h
 *
 * Copyright (C) 2003 - 2009 Vivien Malerba
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

#ifndef __GDA_ENUMS__
#define __GDA_ENUMS__

/* Isolation state of a transaction */
typedef enum {
	GDA_TRANSACTION_ISOLATION_UNKNOWN,
	GDA_TRANSACTION_ISOLATION_READ_COMMITTED,
	GDA_TRANSACTION_ISOLATION_READ_UNCOMMITTED,
	GDA_TRANSACTION_ISOLATION_REPEATABLE_READ,
	GDA_TRANSACTION_ISOLATION_SERIALIZABLE
} GdaTransactionIsolation;

/* status of a value */
typedef enum  {
	GDA_VALUE_ATTR_NONE           = 0,
        GDA_VALUE_ATTR_IS_NULL        = 1 << 0,
        GDA_VALUE_ATTR_CAN_BE_NULL    = 1 << 1,
        GDA_VALUE_ATTR_IS_DEFAULT     = 1 << 2,
        GDA_VALUE_ATTR_CAN_BE_DEFAULT = 1 << 3,
        GDA_VALUE_ATTR_IS_UNCHANGED   = 1 << 4,
        GDA_VALUE_ATTR_ACTIONS_SHOWN  = 1 << 5,
        GDA_VALUE_ATTR_DATA_NON_VALID = 1 << 6,
        GDA_VALUE_ATTR_HAS_VALUE_ORIG = 1 << 7,
	GDA_VALUE_ATTR_NO_MODIF       = 1 << 8,
	GDA_VALUE_ATTR_UNUSED         = 1 << 9
} GdaValueAttribute;

/* how SQL identifiers are represented */
typedef enum {
	GDA_SQL_IDENTIFIERS_LOWER_CASE = 1 << 0,
	GDA_SQL_IDENTIFIERS_UPPER_CASE = 1 << 1
} GdaSqlIdentifierStyle;

/* possible different keywords used when qualifying a table's column's extra attributes */
#define GDA_EXTRA_AUTO_INCREMENT "AUTO_INCREMENT"

#endif



