/* GDA MDB provider
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
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

#include "gda-mdb.h"

GdaValueType
gda_mdb_type_to_gda (int col_type)
{
	switch (col_type) {
	case MDB_BOOL : return GDA_VALUE_TYPE_BOOLEAN;
	case MDB_BYTE : return GDA_VALUE_TYPE_TINYINT;
	case MDB_DOUBLE : return GDA_VALUE_TYPE_DOUBLE;
	case MDB_FLOAT : return GDA_VALUE_TYPE_SINGLE;
	case MDB_INT : return GDA_VALUE_TYPE_INTEGER;
	case MDB_LONGINT : return GDA_VALUE_TYPE_BIGINT;
	case MDB_MEMO : return GDA_VALUE_TYPE_BINARY;
	case MDB_MONEY : return GDA_VALUE_TYPE_DOUBLE;
	case MDB_NUMERIC : return GDA_VALUE_TYPE_NUMERIC;
	case MDB_OLE : return GDA_VALUE_TYPE_BINARY;
	case MDB_REPID : return GDA_VALUE_TYPE_BINARY;
	case MDB_SDATETIME : return GDA_VALUE_TYPE_TIMESTAMP;
	case MDB_TEXT : return GDA_VALUE_TYPE_STRING;
	}

	return GDA_VALUE_TYPE_UNKNOWN;
}
