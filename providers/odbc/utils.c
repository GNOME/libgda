/* GNOME DB Postgres Provider
 * Copyright (C) 1998-2002 The GNOME Foundation
 *
 * AUTHORS:
 *         Nick Gorham <nick@lurcher.org>
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

#include <libgda/gda-intl.h>
#include <stdlib.h>
#include <string.h>
#include "gda-odbc.h"

/*
 * Create a GdaError from the ODBC connection, checking for errors on 
 * any of the handles
 */

static GdaError *
gda_odbc_make_error( SQLHANDLE env,
			SQLHANDLE con,
			SQLHANDLE stmt )
{
	SQLRETURN rc;
	SQLCHAR sqlstate[ 6 ];
	SQLINTEGER native;
	SQLCHAR msg[ SQL_MAX_MESSAGE_LENGTH ];
	SQLSMALLINT txt_len;

	rc = SQLError( env, 
			con, 
			stmt, 
			sqlstate,
			&native,
			msg,
			sizeof( msg ),
			&txt_len );

	if ( rc == SQL_NO_DATA && con != SQL_NULL_HANDLE ) {
		rc = SQLError( env, 
				con, 
				SQL_NULL_HANDLE, 
				sqlstate,
				&native,
				msg,
				sizeof( msg ),
				&txt_len );

		if ( rc == SQL_NO_DATA && env != SQL_NULL_HANDLE ) {
			rc = SQLError( env, 
					SQL_NULL_HANDLE, 
					SQL_NULL_HANDLE, 
					sqlstate,
					&native,
					msg,
					sizeof( msg ),
					&txt_len );
		}
	}

	if ( SQL_SUCCEEDED( rc )) {
		GdaError *error;

		error = gda_error_new ();

		gda_error_set_description (error, msg);
		gda_error_set_number (error, native);
		gda_error_set_source (error, "gda-odbc");
		gda_error_set_sqlstate (error, sqlstate);

		return error;
	}
	else {
		return NULL;
	}
}

/*
 * Generate a GList of GdaErrors, we may as well use the ODBC 2 SQLError as we 
 * don't need any of the ODBC 3 bits
 */

void gda_odbc_emit_error ( GdaConnection *cnc,
				SQLHANDLE env, 
				SQLHANDLE con, 
				SQLHANDLE stmt )
{
	GdaError *error;
	GList *list = NULL;

	while (( error = gda_odbc_make_error( env, con, stmt ))) {
		list = g_list_append( list, error );
	}

	gda_connection_add_error_list( cnc, list );
}

/*
 * Map from a ODBC type to a GdaValueType
 */

GdaValueType
odbc_to_gda_type ( int odbc_type )
{
	switch ( odbc_type ) {
	case SQL_CHAR:
	case SQL_WCHAR:
	case SQL_VARCHAR:
	case SQL_WVARCHAR:
	case SQL_LONGVARCHAR:
	case SQL_WLONGVARCHAR:
		return GDA_VALUE_TYPE_STRING;

	case SQL_DECIMAL:
	case SQL_NUMERIC:
		return GDA_VALUE_TYPE_NUMERIC;

	case SQL_BINARY:
	case SQL_VARBINARY:
	case SQL_LONGVARBINARY:
		return GDA_VALUE_TYPE_BINARY;

	case SQL_INTEGER:
	case SQL_C_ULONG:
	case SQL_C_SLONG:
		return GDA_VALUE_TYPE_INTEGER;

	case SQL_BIGINT:
	case SQL_C_UBIGINT:
	case SQL_C_SBIGINT:
		return GDA_VALUE_TYPE_BIGINT;

	case SQL_SMALLINT:
	case SQL_C_USHORT:
	case SQL_C_SSHORT:
		return GDA_VALUE_TYPE_SMALLINT;

	case SQL_TINYINT:
	case SQL_C_UTINYINT:
	case SQL_C_STINYINT:
		return GDA_VALUE_TYPE_TINYINT;

	case SQL_C_BIT:
		return GDA_VALUE_TYPE_BOOLEAN;

	case SQL_REAL:
	case SQL_DOUBLE:
	case SQL_FLOAT:
		return GDA_VALUE_TYPE_DOUBLE;

	case SQL_TYPE_DATE:
	case SQL_DATE:
		return GDA_VALUE_TYPE_DATE;

	case SQL_TYPE_TIME:
	case SQL_TIME:
		return GDA_VALUE_TYPE_TIME;

	case SQL_TYPE_TIMESTAMP:
	case SQL_TIMESTAMP:
		return GDA_VALUE_TYPE_TIMESTAMP;

	default:
		return GDA_VALUE_TYPE_UNKNOWN;
	}
}

