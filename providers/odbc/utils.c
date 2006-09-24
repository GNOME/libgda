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

#include <glib/gi18n-lib.h>
#include <stdlib.h>
#include <string.h>
#include "gda-odbc.h"

/*
 * Create a GdaConnectionEvent from the ODBC connection, checking for errors on 
 * any of the handles
 */

static GdaConnectionEvent *
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
		GdaConnectionEvent *error;

		error = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);

		gda_connection_event_set_description (error, msg);
		gda_connection_event_set_code (error, native);
		gda_connection_event_set_source (error, "gda-odbc");
		gda_connection_event_set_sqlstate (error, sqlstate);

		return error;
	}
	else {
		return NULL;
	}
}

/*
 * Generate a GList of GdaConnectionEvents, we may as well use the ODBC 2 SQLError as we 
 * don't need any of the ODBC 3 bits
 */

void
gda_odbc_emit_error ( GdaConnection *cnc,
			SQLHANDLE env, 
			SQLHANDLE con, 
			SQLHANDLE stmt )
{
	GdaConnectionEvent *error;
	GList *list = NULL;

	while (( error = gda_odbc_make_error( env, con, stmt ))) {
		list = g_list_append( list, error );
	}

	gda_connection_add_events_list( cnc, list );
}

/*
 * Map from a ODBC type to a GType
 */

GType
odbc_to_g_type ( int odbc_type )
{
	switch ( odbc_type ) {
	case SQL_CHAR:
	case SQL_WCHAR:
	case SQL_VARCHAR:
	case SQL_WVARCHAR:
	case SQL_LONGVARCHAR:
	case SQL_WLONGVARCHAR:
		return G_TYPE_STRING;

	case SQL_DECIMAL:
	case SQL_NUMERIC:
		return GDA_TYPE_NUMERIC;

	case SQL_BINARY:
	case SQL_VARBINARY:
	case SQL_LONGVARBINARY:
		return GDA_TYPE_BINARY;

	case SQL_INTEGER:
	case SQL_C_ULONG:
	case SQL_C_SLONG:
		return G_TYPE_INT;

	case SQL_BIGINT:
	case SQL_C_UBIGINT:
	case SQL_C_SBIGINT:
		return G_TYPE_INT64;

	case SQL_SMALLINT:
	case SQL_C_USHORT:
	case SQL_C_SSHORT:
		return GDA_TYPE_SHORT;

	case SQL_TINYINT:
	case SQL_C_UTINYINT:
	case SQL_C_STINYINT:
		return G_TYPE_CHAR;

	case SQL_C_BIT:
		return G_TYPE_BOOLEAN;

	case SQL_REAL:
	case SQL_DOUBLE:
	case SQL_FLOAT:
		return G_TYPE_DOUBLE;

	case SQL_TYPE_DATE:
	case SQL_DATE:
		return G_TYPE_DATE;

	case SQL_TYPE_TIME:
	case SQL_TIME:
		return GDA_TYPE_TIME;

	case SQL_TYPE_TIMESTAMP:
	case SQL_TIMESTAMP:
		return GDA_TYPE_TIMESTAMP;

	default:
		return G_TYPE_INVALID;
	}
}

