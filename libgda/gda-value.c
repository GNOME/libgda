/* GDA common library
 * Copyright (C) 1998-2002 The GNOME Foundation.
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
#include <libgda/gda-value.h>

/*
 * Private functions
 */

static void
clear_value (GdaValue *value)
{
	CORBA_Environment ev;

	g_return_if_fail (value != NULL);

	if (value->_type) {
		CORBA_exception_init (&ev);
		CORBA_Object_release (value->_type, &ev);
		CORBA_exception_free (&ev);
	}

	if (value->_value)
		CORBA_free (value->_value);

	value->_type = CORBA_OBJECT_NIL;
	value->_value = NULL;
}

/**
 * gda_value_new_null
 */
GdaValue *
gda_value_new_null (void)
{
	return (GdaValue *) bonobo_arg_new (GDA_VALUE_TYPE_NULL);
}

GdaValue *
gda_value_new_bigint (long long val)
{
	return (GdaValue *) bonobo_arg_new_from (GDA_VALUE_TYPE_BIGINT,
						(gconstpointer) &val);
}

GdaValue *
gda_value_new_binary (gconstpointer val)
{
	return NULL;
}

GdaValue *
gda_value_new_boolean (gboolean val)
{
	return (GdaValue *) bonobo_arg_new_from (GDA_VALUE_TYPE_BOOLEAN,
						(gconstpointer) &val);
}

GdaValue *
gda_value_new_date (GdaDate *val)
{
	g_return_val_if_fail (val != NULL, NULL);

	return (GdaValue *) bonobo_arg_new_from (GDA_VALUE_TYPE_DATE,
						(gconstpointer) val);
}

GdaValue *
gda_value_new_double (gdouble val)
{
	return (GdaValue *) bonobo_arg_new_from (GDA_VALUE_TYPE_DOUBLE,
						(gconstpointer) &val);

}

GdaValue *
gda_value_new_geometric_point (GdaGeometricPoint *val)
{
	g_return_val_if_fail (val != NULL, NULL);

	return (GdaValue *) bonobo_arg_new_from (
			GDA_VALUE_TYPE_GEOMETRIC_POINT, (gconstpointer) val);
}

GdaValue *
gda_value_new_integer (gint val)
{
	return (GdaValue *) bonobo_arg_new_from (GDA_VALUE_TYPE_INTEGER,
						(gconstpointer) &val);
}

GdaValue *
gda_value_new_single (gfloat val)
{
	return (GdaValue *) bonobo_arg_new_from (GDA_VALUE_TYPE_SINGLE,
						(gconstpointer) &val);
}

GdaValue *
gda_value_new_smallint (gshort val)
{
	return (GdaValue *) bonobo_arg_new_from (GDA_VALUE_TYPE_SMALLINT,
						(gconstpointer) &val);
}

GdaValue *
gda_value_new_string (const gchar *val)
{
	return (GdaValue *) bonobo_arg_new_from (GDA_VALUE_TYPE_STRING,
						(gconstpointer) &val);
}

GdaValue *
gda_value_new_time (GdaTime *val)
{
	g_return_val_if_fail (val != NULL, NULL);

	return (GdaValue *) bonobo_arg_new_from (GDA_VALUE_TYPE_TIME,
					(gconstpointer) val);
}

GdaValue *
gda_value_new_timestamp (GdaTimestamp *val)
{
	g_return_val_if_fail (val != NULL, NULL);

	return (GdaValue *) bonobo_arg_new_from (GDA_VALUE_TYPE_TIMESTAMP,
						(gconstpointer) val);
}

GdaValue *
gda_value_new_tinyint (gchar val)
{
	return (GdaValue *) bonobo_arg_new_from (GDA_VALUE_TYPE_TINYINT,
						(gconstpointer) &val);
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
 * gda_value_isa
 */
gboolean
gda_value_isa (const GdaValue *value, GdaValueType type)
{
	g_return_val_if_fail (value != NULL, FALSE);
	return bonobo_arg_type_is_equal (type, value->_type, NULL);
}

/**
 * gda_value_is_null
 */
gboolean
gda_value_is_null (GdaValue *value)
{
	g_return_val_if_fail (value != NULL, FALSE);
	return bonobo_arg_type_is_equal (GDA_VALUE_TYPE_NULL, value->_type, 
					NULL);
}

/**
 * gda_value_copy
 */
GdaValue *
gda_value_copy (GdaValue *value)
{
	g_return_val_if_fail (value != NULL, NULL);
	return (GdaValue *) bonobo_arg_copy (value);
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

	if (!gda_value_isa (value, GDA_VALUE_TYPE_BIGINT)) {
		clear_value (value);
		value->_type = ORBit_RootObject_duplicate (
					GDA_VALUE_TYPE_BIGINT);
	}
	else if (value->_value);
		CORBA_free (value->_value);

	value->_value = ORBit_copy_value (&val, GDA_VALUE_TYPE_BIGINT);
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

	if (!gda_value_isa (value, GDA_VALUE_TYPE_BOOLEAN)) {
		clear_value (value);
		value->_type = ORBit_RootObject_duplicate (GDA_VALUE_TYPE_BOOLEAN);
	}
	else if (value->_value);
		CORBA_free (value->_value);

	value->_value = ORBit_copy_value (&val, GDA_VALUE_TYPE_BOOLEAN);
}

/**
 * gda_value_get_date
 */
const GdaDate *
gda_value_get_date (GdaValue *value)
{
	g_return_val_if_fail (value != NULL, NULL);
	g_return_val_if_fail (gda_value_isa (value, GDA_VALUE_TYPE_DATE), NULL);

	return (const GdaDate *) value->_value;
}

/**
 gda_value_set_date
*/
void
gda_value_set_date (GdaValue *value, GdaDate *val)
{
	g_return_if_fail (value != NULL);
	g_return_if_fail (val != NULL);
	
	if (!gda_value_isa (value, GDA_VALUE_TYPE_DATE)) {
		clear_value (value);
		value->_type = ORBit_RootObject_duplicate (GDA_VALUE_TYPE_DATE);
	}
	else if (value->_value)
		CORBA_free (value->_value);

	value->_value = ORBit_copy_value (val, GDA_VALUE_TYPE_DATE);
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

	if (!gda_value_isa (value, GDA_VALUE_TYPE_DOUBLE)) {
		clear_value (value);
		value->_type = ORBit_RootObject_duplicate (GDA_VALUE_TYPE_DOUBLE);
	}
	else if (value->_value)
		CORBA_free (value->_value);

	value->_value = ORBit_copy_value (&val, GDA_VALUE_TYPE_DOUBLE);
}

/**
 * gda_value_get_geometric_point
 */
const GdaGeometricPoint *
gda_value_get_geometric_point (GdaValue *value)
{
	g_return_val_if_fail (value != NULL, NULL);
	g_return_val_if_fail (gda_value_isa (value, 
				GDA_VALUE_TYPE_GEOMETRIC_POINT), NULL);

	return (const GdaGeometricPoint *) value->_value;
}

/**
 * gda_value_set_geometric_point
 */
void
gda_value_set_geometric_point (GdaValue *value, GdaGeometricPoint *val)
{
	g_return_if_fail (value != NULL);
	g_return_if_fail (val != NULL);

	if (!gda_value_isa (value, GDA_VALUE_TYPE_GEOMETRIC_POINT)) {
		clear_value (value);
		value->_type = ORBit_RootObject_duplicate (
					GDA_VALUE_TYPE_GEOMETRIC_POINT);
	}
	else if (value->_value)
		CORBA_free (value->_value);

	value->_value = ORBit_copy_value (val, GDA_VALUE_TYPE_GEOMETRIC_POINT);
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

	if (!gda_value_isa (value, GDA_VALUE_TYPE_INTEGER)) {
		clear_value (value);
		value->_type = ORBit_RootObject_duplicate (GDA_VALUE_TYPE_INTEGER);
	}
	else if (value->_value)
		CORBA_free (value->_value);

	value->_value = ORBit_copy_value (&val, GDA_VALUE_TYPE_INTEGER);
}

/**
 * gda_value_set_null
 */
void
gda_value_set_null (GdaValue *value)
{
	g_return_if_fail (value != NULL);

	if (!gda_value_isa (value, GDA_VALUE_TYPE_NULL)) {
		clear_value (value);
		value->_type = ORBit_RootObject_duplicate (GDA_VALUE_TYPE_NULL);
	}
	else if (value->_value)
		CORBA_free (value->_value);

	value->_value = NULL;
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

	if (!gda_value_isa (value, GDA_VALUE_TYPE_SINGLE)) {
		clear_value (value);
		value->_type = ORBit_RootObject_duplicate (GDA_VALUE_TYPE_SINGLE);
	}
	else if (value->_value)
		CORBA_free (value->_value);

	value->_value = ORBit_copy_value (&val, GDA_VALUE_TYPE_SINGLE);
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

	if (!gda_value_isa (value, GDA_VALUE_TYPE_SMALLINT)) {
		clear_value (value);
		value->_type = ORBit_RootObject_duplicate (GDA_VALUE_TYPE_SMALLINT);
	}
	else if (value->_value)
		CORBA_free (value->_value);

	value->_value = ORBit_copy_value (&val, GDA_VALUE_TYPE_SMALLINT);
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

	if (!gda_value_isa (value, GDA_VALUE_TYPE_STRING)) {
		clear_value (value);
		value->_type = ORBit_RootObject_duplicate (GDA_VALUE_TYPE_STRING);
	}
	else if (value->_value)
		CORBA_free (value->_value);

	value->_value = ORBit_copy_value (&val, GDA_VALUE_TYPE_STRING);
}

/**
 * gda_value_get_time
 */
const GdaTime *
gda_value_get_time (GdaValue *value)
{
	g_return_val_if_fail (value != NULL, NULL);
	g_return_val_if_fail (gda_value_isa (value, GDA_VALUE_TYPE_TIME), NULL);

	return (const GdaTime *) value->_value;
}

/**
 * gda_value_set_time
 */
void
gda_value_set_time (GdaValue *value, GdaTime *val)
{
	g_return_if_fail (value != NULL);
	g_return_if_fail (val != NULL);

	if (!gda_value_isa (value, GDA_VALUE_TYPE_TIME)) {
		clear_value (value);
		value->_type = ORBit_RootObject_duplicate (GDA_VALUE_TYPE_TIME);
	}
	else if (value->_value)
		CORBA_free (value->_value);

	value->_value = ORBit_copy_value (val, GDA_VALUE_TYPE_TIME);
}

/**
 * gda_value_get_timestamp
 */
const GdaTimestamp *
gda_value_get_timestamp (GdaValue *value)
{
	g_return_val_if_fail (value != NULL, NULL);
	g_return_val_if_fail (gda_value_isa (value,
				GDA_VALUE_TYPE_TIMESTAMP), NULL);

	return (const GdaTimestamp *) value->_value;
}

/**
 * gda_value_set_timestamp
 */
void
gda_value_set_timestamp (GdaValue *value, GdaTimestamp *val)
{
	g_return_if_fail (value != NULL);
	g_return_if_fail (val != NULL);

	if (!gda_value_isa (value, GDA_VALUE_TYPE_TIMESTAMP)) {
		clear_value (value);
		value->_type = ORBit_RootObject_duplicate (GDA_VALUE_TYPE_TIMESTAMP);
	}
	else if (value->_value)
		CORBA_free (value->_value);

	value->_value = ORBit_copy_value (val, GDA_VALUE_TYPE_TIMESTAMP);
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

	if (!gda_value_isa (value, GDA_VALUE_TYPE_TINYINT)) {
		clear_value (value);
		value->_type = ORBit_RootObject_duplicate (GDA_VALUE_TYPE_TINYINT);
	}
	else if (value->_value)
		CORBA_free (value->_value);

	value->_value = ORBit_copy_value (&val, GDA_VALUE_TYPE_TINYINT);
}

/**
 * gda_value_stringify
 */
gchar *
gda_value_stringify (const GdaValue *value)
{
	gchar *retval = NULL;

	g_return_val_if_fail (value != NULL, NULL);

	if (gda_value_isa (value, GDA_VALUE_TYPE_BIGINT))
		retval = g_strdup_printf ("%ld", gda_value_get_bigint (value));
	else if (gda_value_isa (value, GDA_VALUE_TYPE_BOOLEAN)) {
		if (gda_value_get_boolean (value))
			retval = g_strdup (_("TRUE"));
		else
			retval = g_strdup (_("FALSE"));
	}
	else if (gda_value_isa (value, GDA_VALUE_TYPE_STRING))
		retval = g_strdup (gda_value_get_string (value));
	else if (gda_value_isa (value, GDA_VALUE_TYPE_INTEGER))
		retval = g_strdup_printf ("%d", gda_value_get_integer (value));
	else if (gda_value_isa (value, GDA_VALUE_TYPE_SMALLINT))
		retval = g_strdup_printf ("%d", gda_value_get_smallint (value));
	else if (gda_value_isa (value, GDA_VALUE_TYPE_SINGLE))
		retval = g_strdup_printf ("%f", gda_value_get_single (value));
	else if (gda_value_isa (value, GDA_VALUE_TYPE_DOUBLE))
		retval = g_strdup_printf ("%f", gda_value_get_double (value));
	else if (gda_value_isa (value, GDA_VALUE_TYPE_TIME)) {
		GdaTime *gdatime;

		gdatime = gda_value_get_time (value);
		retval = g_strdup_printf (gdatime->timezone == TIMEZONE_INVALID ?
				   "%02u:%02u:%02u" : "%02u:%02u:%02u%+03d", 
						gdatime->hour,
						gdatime->minute,
						gdatime->second,
						gdatime->timezone/3600);
	}
	else if (gda_value_isa (value, GDA_VALUE_TYPE_DATE)) {
		GdaDate *gdadate;

		gdadate = gda_value_get_date (value);
		retval = g_strdup_printf ("%04u-%02u-%02u",gdadate->year,
							   gdadate->month,
							   gdadate->day);
	}
	else if (gda_value_isa (value, GDA_VALUE_TYPE_TIMESTAMP)) {
		GdaTimestamp *timestamp;

		timestamp = gda_value_get_timestamp (value);
		CLAMP (timestamp->fraction, 0, 999);
		retval = g_strdup_printf (timestamp->timezone == TIMEZONE_INVALID ?
			"%04u-%02u-%02u %02u:%02u:%02u.%03d" :
			"%04u-%02u-%02u %02u:%02u:%02u.%03d%+03d",
						timestamp->year,
						timestamp->month,
						timestamp->day,
						timestamp->hour,
						timestamp->minute,
						timestamp->second,
						timestamp->fraction,
						timestamp->timezone/3600);
	}
	else if (gda_value_isa (value, GDA_VALUE_TYPE_GEOMETRIC_POINT)) {
		GdaGeometricPoint *point;

		point = gda_value_get_geometric_point (value);
		retval = g_strdup_printf ("(%.*lg,%.*lg)", DBL_DIG,
							point->x,
							DBL_DIG,
							point->y);
	}
	else if (gda_value_isa (value, GDA_VALUE_TYPE_NULL))
		retval = g_strdup ("NULL");
	else
		retval = g_strdup ("");
        	
	return retval;
}
