/* GDA Server Library
 * Copyright (C) 2000 Rodrigo Moya
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

#include <time.h>
#include "gda-server.h"
#include "gda-server-private.h"

/**
 * gda_server_field_new
 */
GdaServerField *
gda_server_field_new (void)
{
	GdaServerField *field = g_new0 (GdaServerField, 1);
	field->value = g_new0 (GDA_Value, 1);
	return field;
}

/**
 * gda_server_field_set_name
 */
void
gda_server_field_set_name (GdaServerField * field, const gchar * name)
{
	g_return_if_fail (field != NULL);

	if (field->name)
		g_free ((gpointer) field->name);
	field->name = g_strdup (name);
}

/**
 * gda_server_field_get_sql_type
 */
gulong
gda_server_field_get_sql_type (GdaServerField * field)
{
	g_return_val_if_fail (field != NULL, 0);
	return field->sql_type;
}

/**
 * gda_server_field_set_sql_type
 */
void
gda_server_field_set_sql_type (GdaServerField * field, gulong sql_type)
{
	g_return_if_fail (field != NULL);
	field->sql_type = sql_type;
}

/**
 * gda_server_field_set_defined_length
 */
void
gda_server_field_set_defined_length (GdaServerField * field, glong length)
{
	g_return_if_fail (field != NULL);
	field->defined_length = length;
}

/**
 * gda_server_field_set_actual_length
 */
void
gda_server_field_set_actual_length (GdaServerField * field, glong length)
{
	g_return_if_fail (field != NULL);
	field->actual_length = length;
}

/**
 * gda_server_field_set_scale
 */
void
gda_server_field_set_scale (GdaServerField * field, gshort scale)
{
	g_return_if_fail (field != NULL);
	field->num_scale = scale;
}

/**
 * gda_server_field_get_user_data
 */
gpointer
gda_server_field_get_user_data (GdaServerField * field)
{
	g_return_val_if_fail (field != NULL, NULL);
	return field->user_data;
}

/**
 * gda_server_field_set_user_data
 */
void
gda_server_field_set_user_data (GdaServerField * field, gpointer user_data)
{
	g_return_if_fail (field != NULL);
	field->user_data = user_data;
}

/**
 * gda_server_field_free
 */
void
gda_server_field_free (GdaServerField * field)
{
	g_return_if_fail (field != NULL);

	if (field->name)
		g_free ((gpointer) field->name);
	//if (field->value) g_free((gpointer) field->value);
	g_free ((gpointer) field);
}

/**
 * gda_server_field_set_boolean
 */
void
gda_server_field_set_boolean (GdaServerField * field, gboolean val)
{
	g_return_if_fail (field != NULL);

	field->value->_d = GDA_TypeBoolean;
	field->value->_u.b = val;
	field->actual_length = sizeof (CORBA_boolean);
}

/**
 * gda_server_field_set_date
 */
void
gda_server_field_set_date (GdaServerField * field, GDate * val)
{
	g_return_if_fail (field != NULL);

	field->value->_d = GDA_TypeDbDate;
	if (val) {
		field->value->_u.dbd.year = g_date_year (val);
		field->value->_u.dbd.month = g_date_month (val);
		field->value->_u.dbd.day = g_date_day (val);
		field->actual_length = sizeof (GDA_DbDate);
	}
	else {
		field->value->_u.dbd.year = 0;
		field->value->_u.dbd.month = 0;
		field->value->_u.dbd.day = 0;
		field->actual_length = 0;
	}
}

/**
 * gda_server_field_set_time
 */
void
gda_server_field_set_time (GdaServerField * field, GTime val)
{
	struct tm *stm;

	g_return_if_fail (field != NULL);

	field->value->_d = GDA_TypeDbTime;
	stm = localtime (&val);
	if (stm) {
		field->value->_u.dbt.hour = stm->tm_hour;
		field->value->_u.dbt.minute = stm->tm_min;
		field->value->_u.dbt.second = stm->tm_sec;
		field->actual_length = sizeof (GDA_DbTime);
	}
	else {
		field->value->_u.dbt.hour = 0;
		field->value->_u.dbt.minute = 0;
		field->value->_u.dbt.second = 0;
		field->actual_length = 0;
	}
}

/**
 * gda_server_field_set_timestamp
 */
void
gda_server_field_set_timestamp (GdaServerField * field, GDate * dat,
				GTime tim)
{
	struct tm *stm;

	g_return_if_fail (field != 0);

	field->value->_d = GDA_TypeDbTimestamp;
	stm = localtime (&tim);
	memset (&field->value->_u.dbtstamp, 0,
		sizeof (field->value->_u.dbtstamp));
	if (dat) {
		field->value->_u.dbtstamp.year = g_date_year (dat);
		field->value->_u.dbtstamp.month = g_date_month (dat);
		field->value->_u.dbtstamp.day = g_date_day (dat);
	}
	if (stm) {
		field->value->_u.dbtstamp.hour = stm->tm_hour;
		field->value->_u.dbtstamp.minute = stm->tm_min;
		field->value->_u.dbtstamp.second = stm->tm_sec;
		field->value->_u.dbtstamp.fraction = 0;
	}
	field->actual_length = sizeof (GDA_DbTimestamp);
}

/**
 * gda_server_field_set_smallint
 */
void
gda_server_field_set_smallint (GdaServerField * field, gshort val)
{
	g_return_if_fail (field != NULL);

	field->value->_d = GDA_TypeSmallint;
	field->value->_u.si = val;
	field->actual_length = sizeof (CORBA_short);
}

/**
 * gda_server_field_set_integer
 */
void
gda_server_field_set_integer (GdaServerField * field, gint val)
{
	g_return_if_fail (field != NULL);

	field->value->_d = GDA_TypeInteger;
	field->value->_u.i = val;
	field->actual_length = sizeof (CORBA_short);
}

/**
 * gda_server_set_long_varchar
 */
void
gda_server_field_set_longvarchar (GdaServerField * field, gchar * val)
{
	g_return_if_fail (field != NULL);

	field->value->_d = GDA_TypeLongvarchar;
	if (val) {
		field->value->_u.lvc = CORBA_string_dup (val);
		field->actual_length = strlen (val);
		field->malloced = TRUE;
	}
	else {
		field->value->_u.lvc = 0;
		field->actual_length = 0;
	}
}

/**
 * gda_server_field_set_char
 */
void
gda_server_field_set_char (GdaServerField * field, gchar * val)
{
	g_return_if_fail (field != NULL);

	field->value->_d = GDA_TypeChar;
	if (val) {
		field->value->_u.lvc = CORBA_string_dup (val);
		field->actual_length = strlen (val);
		field->malloced = TRUE;
	}
	else {
		field->value->_u.lvc = 0;
		field->actual_length = 0;
	}
}

/**
 * gda_server_field_set_varchar
 */
void
gda_server_field_set_varchar (GdaServerField * field, gchar * val)
{
	g_return_if_fail (field != NULL);

	field->value->_d = GDA_TypeVarchar;
	if (val) {
		field->value->_u.lvc = CORBA_string_dup (val);
		field->actual_length = strlen (val);
		field->malloced = TRUE;
	}
	else {
		field->value->_u.lvc = 0;
		field->actual_length = 0;
	}
}

/**
 * gda_server_field_set_single
 */
void
gda_server_field_set_single (GdaServerField * field, gfloat val)
{
	g_return_if_fail (field != NULL);

	field->value->_d = GDA_TypeSingle;
	field->value->_u.f = val;
	field->actual_length = sizeof (CORBA_float);
}

/**
 * gda_server_field_set_double
 */
void
gda_server_field_set_double (GdaServerField * field, gdouble val)
{
	g_return_if_fail (field != NULL);

	field->value->_d = GDA_TypeDouble;
	field->value->_u.dp = val;
	field->actual_length = sizeof (CORBA_double);
}

/**
 * gda_server_field_set_varbin
 */
void
gda_server_field_set_varbin (GdaServerField * field, gpointer val, glong size)
{
	g_return_if_fail (field != NULL);

	field->value->_d = GDA_TypeVarbin;
	if (val) {
	}
	else {
		field->value->_u.lvb._buffer = NULL;
		field->value->_u.lvb._maximum = 0;
		field->value->_u.lvb._length = 0;
		field->actual_length = sizeof (GDA_VarBinString);
	}
}
