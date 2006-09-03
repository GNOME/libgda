/* GDA MySQL provider
 * Copyright (C) 1998 - 2006 The GNOME Foundation.
 *
 * AUTHORS:
 *      Michael Lausch <michael@lausch.at>
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *      Vivien Malerba <malerba@gnome-db.org>
 *	Bas Driessen <bas.driessen@xobas.com>
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

#include "gda-mysql.h"

GdaConnectionEvent *
gda_mysql_make_error (MYSQL *handle)
{
	GdaConnectionEvent *error;

	error = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);
	if (handle != NULL) {
		gda_connection_event_set_description (error, mysql_error (handle));
		gda_connection_event_set_code (error, mysql_errno (handle));
	}
	else {
		gda_connection_event_set_description (error, "NO DESCRIPTION");
		gda_connection_event_set_code (error, -1);
	}

	gda_connection_event_set_source (error, "gda-mysql");
	gda_connection_event_set_sqlstate (error, "Not available");

	return error;
}

GType
gda_mysql_type_to_gda (enum enum_field_types mysql_type, gboolean is_unsigned)
{
	switch (mysql_type) {
	case FIELD_TYPE_DATE :
		return G_TYPE_DATE;
	case FIELD_TYPE_DECIMAL :
	case FIELD_TYPE_DOUBLE :
		return G_TYPE_DOUBLE;
	case FIELD_TYPE_FLOAT :
		return G_TYPE_FLOAT;
	case FIELD_TYPE_LONG :
		if (is_unsigned)
			return G_TYPE_UINT;
	case FIELD_TYPE_YEAR :
		return G_TYPE_INT;
	case FIELD_TYPE_LONGLONG :
	case FIELD_TYPE_INT24 :
		if (is_unsigned)
			return G_TYPE_UINT64;
		return G_TYPE_INT64;
	case FIELD_TYPE_SHORT :
		if (is_unsigned)
			return GDA_TYPE_USHORT;
		return GDA_TYPE_SHORT;
	case FIELD_TYPE_TIME :
		return GDA_TYPE_TIME;
	case FIELD_TYPE_TIMESTAMP :
	case FIELD_TYPE_DATETIME :
		return GDA_TYPE_TIMESTAMP;
	case FIELD_TYPE_TINY :
		if (is_unsigned)
			return G_TYPE_UCHAR;
		return G_TYPE_CHAR;
	case FIELD_TYPE_TINY_BLOB :
	case FIELD_TYPE_MEDIUM_BLOB :
	case FIELD_TYPE_LONG_BLOB :
	case FIELD_TYPE_BLOB :
		return GDA_TYPE_BINARY;
	case FIELD_TYPE_VAR_STRING :
	case FIELD_TYPE_STRING :
		/* FIXME: Check for BINARY flags and use blob */
		return G_TYPE_STRING;
	case FIELD_TYPE_NULL :
	case FIELD_TYPE_NEWDATE :
	case FIELD_TYPE_ENUM :
	case FIELD_TYPE_SET :
		return G_TYPE_STRING;
#if NDB_VERSION_MAJOR >= 5
	case MYSQL_TYPE_VARCHAR:
		return G_TYPE_STRING;
	case MYSQL_TYPE_BIT:
		if (is_unsigned)
			return G_TYPE_UCHAR;
		return G_TYPE_CHAR;
	case MYSQL_TYPE_NEWDECIMAL:
		return G_TYPE_DOUBLE;
	case MYSQL_TYPE_GEOMETRY:
		return G_TYPE_STRING;
		break;
#endif
	}

	return G_TYPE_INVALID;
}
