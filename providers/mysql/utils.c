/* GDA MySQL provider
 * Copyright (C) 1998-2001 The Free Software Foundation
 *
 * AUTHORS:
 *      Michael Lausch <michael@lausch.at>
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *      Vivien Malerba <malerba@gnome-db.org>
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

GdaError *
gda_mysql_make_error (MYSQL *handle)
{
	GdaError *error;

	error = gda_error_new ();
	if (handle != NULL) {
		gda_error_set_description (error, mysql_error (handle));
		gda_error_set_number (error, mysql_errno (handle));
	}
	else {
		gda_error_set_description (error, "NO DESCRIPTION");
		gda_error_set_number (error, -1);
	}

	gda_error_set_source (error, "gda-mysql");
	gda_error_set_sqlstate (error, "Not available");

	return error;
}

GdaType
gda_mysql_type_to_gda (enum enum_field_types mysql_type)
{
	switch (mysql_type) {
	case FIELD_TYPE_DATE :
		return GDA_TYPE_DATE;
	case FIELD_TYPE_DECIMAL :
	case FIELD_TYPE_DOUBLE :
		return GDA_TYPE_DOUBLE;
	case FIELD_TYPE_FLOAT :
		return GDA_TYPE_SINGLE;
	case FIELD_TYPE_LONG :
	case FIELD_TYPE_YEAR :
		return GDA_TYPE_INTEGER;
	case FIELD_TYPE_LONGLONG :
	case FIELD_TYPE_INT24 :
		return GDA_TYPE_BIGINT;
	case FIELD_TYPE_SHORT :
		return GDA_TYPE_SMALLINT;
	case FIELD_TYPE_TIME :
		return GDA_TYPE_TIME;
	case FIELD_TYPE_TIMESTAMP :
	case FIELD_TYPE_DATETIME :
		return GDA_TYPE_TIMESTAMP;
	case FIELD_TYPE_TINY :
		return GDA_TYPE_TINYINT;
	case FIELD_TYPE_TINY_BLOB :
	case FIELD_TYPE_MEDIUM_BLOB :
	case FIELD_TYPE_LONG_BLOB :
	case FIELD_TYPE_BLOB :
		return GDA_TYPE_BINARY;
	case FIELD_TYPE_VAR_STRING :
	case FIELD_TYPE_STRING :
		return GDA_TYPE_STRING;
	case FIELD_TYPE_NULL :
	case FIELD_TYPE_NEWDATE :
	case FIELD_TYPE_ENUM :
	case FIELD_TYPE_SET :
		break;
	}

	return GDA_TYPE_UNKNOWN;
}
