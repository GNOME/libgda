/* GDA common library
 * Copyright (C) 1998-2001 The Free Software Foundation
 *
 * AUTHORS:
 *	Michael Lausch <michael@lausch.at>
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

#include <time.h>
#include <bonobo/bonobo-arg.h>
#include <bonobo/bonobo-i18n.h>
#include "gda-value.h"

/**
 * gda_value_new
 *
 * Create a new #GdaValue object.
 *
 * Returns: a pointer to the newly allocated object.
 */
GdaValue *
gda_value_new (void)
{
	GdaValue *value;

	value = GNOME_Database_Value__alloc ();
	CORBA_any_set_release (value, TRUE);
}

/**
 * gda_value_free
 */
void
gda_value_free (GdaValue *value)
{
	g_return_if_fail (value != NULL);
	CORBA_free (value);
}

/**
 * gda_value_get_bigint
 */
long long
gda_value_get_bigint (GdaValue *value)
{
	g_return_val_if_fail (value != NULL, -1);
	return BONOBO_ARG_GET_LONGLONG (value);
}

/**
 * gda_value_set_bigint
 */
void
gda_value_set_bigint (GdaValue *value, long long val)
{
	g_return_if_fail (value != NULL);

	if (value->_value)
		CORBA_free (value->_value);
	value->_value = ORBit_copy_value (&val, BONOBO_ARG_LONGLONG);
}

/**
 * gda_value_get_binary
 */
gconstpointer
gda_value_get_binary (GdaValue *value)
{
}

/**
 * gda_value_set_binary
 */
void
gda_value_set_binary (GdaValue *value, gconstpointer val, glong size)
{
}

/**
 * gda_value_get_boolean
 */
gboolean
gda_value_get_boolean (GdaValue *value)
{
	g_return_val_if_fail (value != NULL, FALSE);
	return BONOBO_ARG_GET_BOOLEAN (value);
}

/**
 * gda_value_set_boolean
 */
void
gda_value_set_boolean (GdaValue *value, gboolean val)
{
	g_return_if_fail (value != NULL);

	if (value->_value)
		CORBA_free (value->_value);
	value->_value = ORBit_copy_value (&val, BONOBO_ARG_BOOLEAN);
}

/**
 * gda_value_get_date
 */
GDate *
gda_value_get_date (GdaValue *value)
{
	GNOME_Database_Date *corba_date;

	g_return_val_if_fail (value != NULL, NULL);

	corba_date = (GNOME_Database_Date *) value->_value;
	if (corba_date)
		return g_date_new_dmy (corba_date->day, corba_date->month, corba_date->year);

	return NULL;
}

/**
 gda_value_set_date
*/
void
gda_value_set_date (GdaValue *value, GDate *val)
{
	GNOME_Database_Date corba_date;

	
	if (!val) {
		corba_date.year = 0;
		corba_date.month = 0;
		corba_date.day = 0;
	}
	else {
		corba_date.year = g_date_get_year (val);
		corba_date.month = g_date_get_month (val);
		corba_date.day = g_date_get_day (val);
	}

	if (value->_value)
		CORBA_free (value->_value);

	value->_value = ORBit_copy_value (&corba_date, TC_GNOME_Database_Date);
}

/**
 * gda_value_get_double
 */
gdouble
gda_value_get_double (GdaValue *value)
{
	g_return_val_if_fail (value != NULL, -1);
	return BONOBO_ARG_GET_DOUBLE (value);
}

/**
 * gda_value_set_double
 */
void
gda_value_set_double (GdaValue *value, gdouble val)
{
	g_return_if_fail (value != NULL);

	if (value->_value)
		CORBA_free (value->_value);
	value->_value = ORBit_copy_value (&val, BONOBO_ARG_DOUBLE);
}

/**
 * gda_value_get_integer
 */
gint
gda_value_get_integer (GdaValue *value)
{
	g_return_val_if_fail (value != NULL, -1);
	return BONOBO_ARG_GET_INT (value);
}

/**
 * gda_value_set_integer
 */
void
gda_value_set_integer (GdaValue *value, gint val)
{
	g_return_if_fail (value != NULL);

	if (value->_value)
		CORBA_free (value->_value);
	value->_value = ORBit_copy_value (&val, BONOBO_ARG_INT);
}

/**
 * gda_value_get_single
 */
gfloat
gda_value_get_single (GdaValue *value)
{
	g_return_val_if_fail (value != NULL, -1);
	return BONOBO_ARG_GET_FLOAT (value);
}

/**
 * gda_value_set_single
 */
void
gda_value_set_single (GdaValue *value, gfloat val)
{
	g_return_if_fail (value != NULL);

	if (value->_value)
		CORBA_free (value->_value);
	value->_value = ORBit_copy_value (&val, BONOBO_ARG_FLOAT);
}

/**
 * gda_value_get_smallint
 */
gshort
gda_value_get_smallint (GdaValue *value)
{
	g_return_val_if_fail (value != NULL, -1);
	return BONOBO_ARG_GET_SHORT (value);
}

/**
 * gda_value_set_smallint
 */
void
gda_value_set_smallint (GdaValue *value, gshort val)
{
	g_return_if_fail (value != NULL);

	if (value->_value)
		CORBA_free (value->_value);
	value->_value = ORBit_copy_value (&val, BONOBO_ARG_SHORT);
}

/**
 * gda_value_get_string
 */
const gchar *
gda_value_get_string (GdaValue *value)
{
	g_return_val_if_fail (value != NULL, NULL);
	return BONOBO_ARG_GET_STRING (value);
}

/**
 * gda_value_set_string
 */
void
gda_value_set_string (GdaValue *value, const gchar *val)
{
	g_return_if_fail (value != NULL);
	BONOBO_ARG_SET_STRING (value, val);
}

/**
 * gda_value_get_time
 */
GTime
gda_value_get_time (GdaValue *value)
{
	GNOME_Database_Time *corba_time;

	g_return_val_if_fail (value != NULL, -1);

	corba_time = (GNOME_Database_Time *) value->_value;
	if (corba_time) {
		struct tm stm;

		memset (&stm, 0, sizeof (stm));
		stm.tm_hour = corba_time->hour;
		stm.tm_min = corba_time->minute;
		stm.tm_sec = corba_time->second;

		return mktime (&stm);
	}

	return -1;
}

/**
 * gda_value_set_time
 */
void
gda_value_set_time (GdaValue *value, GTime val)
{
}

/**
 * gda_value_get_timestamp
 */
time_t
gda_value_get_timestamp (GdaValue *value)
{
	GNOME_Database_Timestamp *corba_timet;

	g_return_val_if_fail (value != NULL, -1);

	corba_timet = (GNOME_Database_Timestamp *) value->_value;
	if (corba_timet) {
		struct tm stm;

		stm.tm_year = corba_timet->year;
		stm.tm_mon = corba_timet->month;
		stm.tm_mday = corba_timet->day;
		stm.tm_hour = corba_timet->hour;
		stm.tm_min = corba_timet->minute;
		stm.tm_sec = corba_timet->second;

		return mktime (&stm);
	}

	return -1;
}

/**
 * gda_value_set_timestamp
 */
void
gda_value_set_timestamp (GdaValue *value, time_t val)
{
	struct tm *stm;
	GNOME_Database_Timestamp corba_timet;

	g_return_if_fail (value != NULL);

	stm = localtime (&value);
	if (!stm)
		return;

	corba_timet.year = stm->tm_year;
	corba_timet.month = stm->tm_mon;
	corba_timet.day = stm->tm_mday;
	corba_timet.hour = stm->tm_hour;
	corba_timet.minute = stm->tm_min;
	corba_timet.second = stm->tm_sec;
	corba_timet.fraction = 0;

	if (value->_value)
		CORBA_free (value->_value);
	value->_value = ORBit_copy_value (&corba_timet, TC_GNOME_Database_Timestamp);
}

/**
 * gda_value_get_tinyint
 */
gchar
gda_value_get_tinyint (GdaValue *value)
{
	g_return_val_if_fail (value != NULL, -1);
	return BONOBO_ARG_GET_CHAR (value);
}

/**
 * gda_value_set_tinyint
 */
void
gda_value_set_tinyint (GdaValue *value, gchar val)
{
	g_return_if_fail (value != NULL);

	if (value->_value)
		CORBA_free (value->_value);
	value->_value = ORBit_copy_value (&val, BONOBO_ARG_CHAR);
}

/**
 * gda_value_stringify
 */
gchar *
gda_value_stringify (GdaValue *value)
{
	gchar *retval = NULL;

	g_return_val_if_fail (value != NULL, NULL);

	if (bonobo_arg_type_is_equal (value->_type, TC_null, NULL))
		retval = g_strdup (_("<Unknown GDA Type(NULL)>"));
	else if (bonobo_arg_type_is_equal (value->_type, TC_CORBA_long_long, NULL))
		retval = g_strdup_printf ("%ld", gda_value_get_bigint (value));
	else if (bonobo_arg_type_is_equal (value->_type, TC_CORBA_boolean, NULL)) {
		if (gda_value_get_boolean (value))
			retval = g_strdup (_("TRUE"));
		else
			retval = g_strdup (_("FALSE"));
	}
	else if (bonobo_arg_type_is_equal (value->_type, TC_CORBA_string, NULL))
		retval = g_strdup (gda_value_get_string (value));
	else if (bonobo_arg_type_is_equal (value->_type, TC_CORBA_long, NULL))
		retval = g_strdup_printf ("%d", gda_value_get_integer (value));
	else if (bonobo_arg_type_is_equal (value->_type, TC_CORBA_short, NULL))
		retval = g_strdup_printf ("%d", gda_value_get_smallint (value));
	else if (bonobo_arg_type_is_equal (value->_type, TC_CORBA_float, NULL))
		retval = g_strdup_printf ("%f", gda_value_get_single (value));
	else if (bonobo_arg_type_is_equal (value->_type, TC_CORBA_double, NULL))
		retval = g_strdup_printf ("%f", gda_value_get_double (value));
	else if (bonobo_arg_type_is_equal (value->_type, TC_GNOME_Database_Time, NULL)) {
	}
	else if (bonobo_arg_type_is_equal (value->_type, TC_GNOME_Database_Date, NULL)) {
	}
	else if (bonobo_arg_type_is_equal (value->_type, TC_GNOME_Database_Timestamp, NULL)) {
		/* FIXME: implement, and add all missing ones */
	}
        	
	return retval;


}
