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

GdaConnectionEvent *
gda_ldap_make_error (LDAP *handle)
{
	GdaConnectionEvent *error;

	error = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);
/*	if (handle != NULL) {
		gda_connection_event_set_description (error, ldap_error (handle));
		gda_connection_event_set_code (error, ldap_errno (handle));
	}
	else {
		gda_connection_event_set_description (error, "NO DESCRIPTION");
		gda_connection_event_set_code (error, -1);
	}
*/
	gda_connection_event_set_source (error, "gda-ldap");
	gda_connection_event_set_sqlstate (error, "Not available");

	return error;
}

/*GType
gda_ldap_type_to_gda (enum enum_field_types ldap_type)
{

	switch (ldap_type) {
	case FIELD_TYPE_DATE :
		return G_TYPE_DATE;
	case FIELD_TYPE_DECIMAL :
	case FIELD_TYPE_DOUBLE :
		return G_TYPE_DOUBLE;
	case FIELD_TYPE_FLOAT :
		return G_TYPE_FLOAT;
	case FIELD_TYPE_LONG :
	case FIELD_TYPE_YEAR :
		return G_TYPE_INT;
	case FIELD_TYPE_LONGLONG :
	case FIELD_TYPE_INT24 :
		return G_TYPE_INT64;
	case FIELD_TYPE_SHORT :
		return GDA_TYPE_SHORT;
	case FIELD_TYPE_TIME :
		return GDA_TYPE_TIME;
	case FIELD_TYPE_TIMESTAMP :
	case FIELD_TYPE_DATETIME :
		return GDA_TYPE_TIMESTAMP;
	case FIELD_TYPE_TINY :
		return G_TYPE_CHAR;
	case FIELD_TYPE_TINY_BLOB :
	case FIELD_TYPE_MEDIUM_BLOB :
	case FIELD_TYPE_LONG_BLOB :
	case FIELD_TYPE_BLOB :
		return GDA_TYPE_BINARY;
	case FIELD_TYPE_VAR_STRING :
	case FIELD_TYPE_STRING :
		return G_TYPE_STRING;
	case FIELD_TYPE_NULL :
	case FIELD_TYPE_NEWDATE :
	case FIELD_TYPE_ENUM :
	case FIELD_TYPE_SET :
		return G_TYPE_STRING;
	}

	return G_TYPE_INVALID;
}
*/
gchar *
gda_ldap_value_to_sql_string (GValue *value)
{
	gchar *val_str;
	gchar *ret;
	GType type;

	g_return_val_if_fail (value != NULL, NULL);

	val_str = gda_value_stringify (value);
	if (!val_str)
		return NULL;

	type = G_VALUE_TYPE (value);

	if ((type == G_TYPE_INT64) ||
	    (type == G_TYPE_DOUBLE) ||
	    (type == G_TYPE_INT) ||
	    (type == GDA_TYPE_NUMERIC) ||
	    (type == G_TYPE_FLOAT) ||
	    (type == GDA_TYPE_SHORT) ||
	    (type == G_TYPE_CHAR))
		ret = g_strdup (val_str);
	else
		ret = g_strdup_printf ("\"%s\"", val_str);

	return ret;
}
