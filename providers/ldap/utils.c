/* GDA LDAP provider
 * Copyright (C) 1998-2003 The GNOME Foundation.
 *
 * AUTHORS:
 *      Michael Lausch <michael@lausch.at>
 *      Rodrigo Moya <rodrigo@gnome-db.org>
 *      Vivien Malerba <malerba@gnome-db.org>
 *      German Poo-Caaman~o <gpoo@ubiobio.cl>
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

#include "gda-ldap.h"

GdaError *
gda_ldap_make_error (LDAP *handle)
{
	GdaError *error;

	error = gda_error_new ();
/*	if (handle != NULL) {
		gda_error_set_description (error, ldap_error (handle));
		gda_error_set_number (error, ldap_errno (handle));
	}
	else {
		gda_error_set_description (error, "NO DESCRIPTION");
		gda_error_set_number (error, -1);
	}
*/
	gda_error_set_source (error, "gda-ldap");
	gda_error_set_sqlstate (error, "Not available");

	return error;
}

/*GdaValueType
gda_ldap_type_to_gda (enum enum_field_types ldap_type)
{

	switch (ldap_type) {
	case FIELD_TYPE_DATE :
		return GDA_VALUE_TYPE_DATE;
	case FIELD_TYPE_DECIMAL :
	case FIELD_TYPE_DOUBLE :
		return GDA_VALUE_TYPE_DOUBLE;
	case FIELD_TYPE_FLOAT :
		return GDA_VALUE_TYPE_SINGLE;
	case FIELD_TYPE_LONG :
	case FIELD_TYPE_YEAR :
		return GDA_VALUE_TYPE_INTEGER;
	case FIELD_TYPE_LONGLONG :
	case FIELD_TYPE_INT24 :
		return GDA_VALUE_TYPE_BIGINT;
	case FIELD_TYPE_SHORT :
		return GDA_VALUE_TYPE_SMALLINT;
	case FIELD_TYPE_TIME :
		return GDA_VALUE_TYPE_TIME;
	case FIELD_TYPE_TIMESTAMP :
	case FIELD_TYPE_DATETIME :
		return GDA_VALUE_TYPE_TIMESTAMP;
	case FIELD_TYPE_TINY :
		return GDA_VALUE_TYPE_TINYINT;
	case FIELD_TYPE_TINY_BLOB :
	case FIELD_TYPE_MEDIUM_BLOB :
	case FIELD_TYPE_LONG_BLOB :
	case FIELD_TYPE_BLOB :
		return GDA_VALUE_TYPE_BINARY;
	case FIELD_TYPE_VAR_STRING :
	case FIELD_TYPE_STRING :
		return GDA_VALUE_TYPE_STRING;
	case FIELD_TYPE_NULL :
	case FIELD_TYPE_NEWDATE :
	case FIELD_TYPE_ENUM :
	case FIELD_TYPE_SET :
		return GDA_VALUE_TYPE_STRING;
	}

	return GDA_VALUE_TYPE_UNKNOWN;
}
*/
gchar *
gda_ldap_value_to_sql_string (GdaValue *value)
{
	gchar *val_str;
	gchar *ret;

	g_return_val_if_fail (value != NULL, NULL);

	val_str = gda_value_stringify (value);
	if (!val_str)
		return NULL;

	switch (value->type) {
	case GDA_VALUE_TYPE_BIGINT :
	case GDA_VALUE_TYPE_DOUBLE :
	case GDA_VALUE_TYPE_INTEGER :
	case GDA_VALUE_TYPE_NUMERIC :
	case GDA_VALUE_TYPE_SINGLE :
	case GDA_VALUE_TYPE_SMALLINT :
	case GDA_VALUE_TYPE_TINYINT :
		ret = g_strdup (val_str);
		break;
	default :
		ret = g_strdup_printf ("\"%s\"", val_str);
	}

	g_free (val_str);

	return ret;
}
