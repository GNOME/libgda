/* GDA MySQL provider
 * Copyright (C) 1998-2005 The GNOME Foundation.
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

GdaValueType
gda_mysql_type_to_gda (enum enum_field_types mysql_type, gboolean is_unsigned)
{
	switch (mysql_type) {
	case FIELD_TYPE_DATE :
		return GDA_VALUE_TYPE_DATE;
	case FIELD_TYPE_DECIMAL :
	case FIELD_TYPE_DOUBLE :
		return GDA_VALUE_TYPE_DOUBLE;
	case FIELD_TYPE_FLOAT :
		return GDA_VALUE_TYPE_SINGLE;
	case FIELD_TYPE_LONG :
		if (is_unsigned)
			return GDA_VALUE_TYPE_UINTEGER;
	case FIELD_TYPE_YEAR :
		return GDA_VALUE_TYPE_INTEGER;
	case FIELD_TYPE_LONGLONG :
	case FIELD_TYPE_INT24 :
		if (is_unsigned)
			return GDA_VALUE_TYPE_BIGUINT;
		return GDA_VALUE_TYPE_BIGINT;
	case FIELD_TYPE_SHORT :
		if (is_unsigned)
			return GDA_VALUE_TYPE_SMALLUINT;
		return GDA_VALUE_TYPE_SMALLINT;
	case FIELD_TYPE_TIME :
		return GDA_VALUE_TYPE_TIME;
	case FIELD_TYPE_TIMESTAMP :
	case FIELD_TYPE_DATETIME :
		return GDA_VALUE_TYPE_TIMESTAMP;
	case FIELD_TYPE_TINY :
		if (is_unsigned)
			return GDA_VALUE_TYPE_TINYUINT;
		return GDA_VALUE_TYPE_TINYINT;
	case FIELD_TYPE_TINY_BLOB :
	case FIELD_TYPE_MEDIUM_BLOB :
	case FIELD_TYPE_LONG_BLOB :
	case FIELD_TYPE_BLOB :
		return GDA_VALUE_TYPE_BINARY;
	case FIELD_TYPE_VAR_STRING :
	case FIELD_TYPE_STRING :
		/* FIXME: Check for BINARY flags and use blob */
		return GDA_VALUE_TYPE_STRING;
	case FIELD_TYPE_NULL :
	case FIELD_TYPE_NEWDATE :
	case FIELD_TYPE_ENUM :
	case FIELD_TYPE_SET :
		return GDA_VALUE_TYPE_STRING;
	}

	return GDA_VALUE_TYPE_UNKNOWN;
}

gchar *
gda_mysql_type_from_gda (const GdaValueType type)
{
	switch (type) {
	case GDA_VALUE_TYPE_NULL :
		return g_strdup_printf ("text");
	case GDA_VALUE_TYPE_BIGINT :
		return g_strdup_printf ("bigint");
	case GDA_VALUE_TYPE_BIGUINT :
		return g_strdup_printf ("bigint");
	case GDA_VALUE_TYPE_BINARY :
		return g_strdup_printf ("binary");
	case GDA_VALUE_TYPE_BLOB :
		return g_strdup_printf ("blob");
	case GDA_VALUE_TYPE_BOOLEAN :
		return g_strdup_printf ("tinyint");
	case GDA_VALUE_TYPE_DATE :
		return g_strdup_printf ("date");
	case GDA_VALUE_TYPE_DOUBLE :
		return g_strdup_printf ("double");
	case GDA_VALUE_TYPE_GEOMETRIC_POINT :
		return g_strdup_printf ("text");
	case GDA_VALUE_TYPE_GOBJECT :
		return g_strdup_printf ("text");
	case GDA_VALUE_TYPE_INTEGER :
		return g_strdup_printf ("integer");
	case GDA_VALUE_TYPE_LIST :
		return g_strdup_printf ("text");
	case GDA_VALUE_TYPE_MONEY :
		return g_strdup_printf ("char");
	case GDA_VALUE_TYPE_NUMERIC :
		return g_strdup_printf ("numeric");
	case GDA_VALUE_TYPE_SINGLE :
		return g_strdup_printf ("float");
	case GDA_VALUE_TYPE_SMALLINT :
		return g_strdup_printf ("smallint");
	case GDA_VALUE_TYPE_SMALLUINT :
		return g_strdup_printf ("smallint");
	case GDA_VALUE_TYPE_STRING :
		return g_strdup_printf ("varchar");
	case GDA_VALUE_TYPE_TIME :
		return g_strdup_printf ("time");
	case GDA_VALUE_TYPE_TIMESTAMP :
		return g_strdup_printf ("timestamp");
	case GDA_VALUE_TYPE_TINYINT :
		return g_strdup_printf ("tinyint");
	case GDA_VALUE_TYPE_TINYUINT :
		return g_strdup_printf ("tinyint");
	case GDA_VALUE_TYPE_TYPE :
		return g_strdup_printf ("smallint");
        case GDA_VALUE_TYPE_UINTEGER :
		return g_strdup_printf ("integer");
	case GDA_VALUE_TYPE_UNKNOWN :
		return g_strdup_printf ("text");
	default :
		return g_strdup_printf ("text");
	}

	return g_strdup_printf ("text");
}

