/* GDA common library
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *	Michael Lausch <michael@lausch.at>
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *	Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
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

#include <string.h>
#include <stdlib.h>
#include <config.h>
#include <time.h>
#include <glib/gdate.h>
#include <glib/gmessages.h>
#include <glib/gstrfuncs.h>
#include <glib/gstring.h>
#include <libgda/gda-intl.h>
#include <libgda/gda-value.h>
#include <libgda/gda-util.h>

/*
 * Private functions
 */

static void
clear_value (GdaValue *value)
{
	g_return_if_fail (value != NULL);

	switch (value->type) {
	case GDA_VALUE_TYPE_LIST :
		g_list_foreach (value->value.v_list, (GFunc) gda_value_free, NULL);
		g_list_free (value->value.v_list);
		value->value.v_list = NULL;
		break;
	case GDA_VALUE_TYPE_STRING :
		g_free (value->value.v_string);
		value->value.v_string = NULL;
		break;
	default :
		break;
	}

	value->type = GDA_VALUE_TYPE_UNKNOWN;
}

/* Makes a point from a string like "(3.2,5.6)" */
static void
make_point (GdaGeometricPoint *point, const gchar *value)
{
	value++;
	point->x = atof (value);
	value = strchr (value, ',');
	value++;
	point->y = atof (value);
}

/* Makes a GdaTime from a string like "12:30:15+01" */
static void
make_time (GdaTime *timegda, const gchar *value)
{
	timegda->hour = atoi (value);
	value += 3;
	timegda->minute = atoi (value);
	value += 3;
	timegda->second = atoi (value);
	value += 2;
	if (*value)
		timegda->timezone = atoi (value);
	else
		timegda->timezone = TIMEZONE_INVALID;
}

/* Makes a GdaTimestamp from a string like "2003-12-13 13:12:01.12+01" */
static void
make_timestamp (GdaTimestamp *timestamp, const gchar *value)
{
	timestamp->year = atoi (value);
	value += 5;
	timestamp->month = atoi (value);
	value += 3;
	timestamp->day = atoi (value);
	value += 3;
	timestamp->hour = atoi (value);
	value += 3;
	timestamp->minute = atoi (value);
	value += 3;
	timestamp->second = atoi (value);
	value += 3;
	timestamp->fraction = atol (value) * 10; // I have only hundredths of second
	value += 3;
	timestamp->timezone = atol (value) * 60 * 60;
}

static gboolean
set_from_string (GdaValue *value, const gchar *as_string)
{
	GDate *gdate;
	GdaDate date;
	GdaTime timegda;
	GdaTimestamp timestamp;
	GdaGeometricPoint point;
	GdaNumeric numeric;
	gboolean retval;
	gchar *endptr [1];
	gdouble dvalue;
	glong lvalue;

	g_return_val_if_fail (value != NULL, FALSE);

	retval = FALSE;
	switch (value->type) {
	case GDA_VALUE_TYPE_BOOLEAN :
		if (g_strcasecmp (as_string, "true") == 0){
			gda_value_set_boolean (value, TRUE);
			retval = TRUE;
		}
		else if (g_strcasecmp (as_string, "false") == 0){
			gda_value_set_boolean (value, FALSE);
			retval = TRUE;
		}
		break;
	case GDA_VALUE_TYPE_BINARY :
		gda_value_set_binary (value, as_string, strlen (as_string));
		break;
	case GDA_VALUE_TYPE_BIGINT :
		/* Use g_strtod instead of strtoll */
		dvalue = g_strtod (as_string, endptr);
		if (*as_string != '\0' && **endptr == '\0'){
			gda_value_set_bigint (value, (gint64) dvalue);
			retval = TRUE;
		}
		break;
	case GDA_VALUE_TYPE_INTEGER :
		lvalue = strtol (as_string, endptr, 10);
		if (*as_string != '\0' && **endptr == '\0'){
			gda_value_set_integer (value, (gint32) lvalue);
			retval = TRUE;
		}
		break;
	case GDA_VALUE_TYPE_SMALLINT :
		lvalue = strtol (as_string, endptr, 10);
		if (*as_string != '\0' && **endptr == '\0'){
			gda_value_set_smallint (value, (gint16) lvalue);
			retval = TRUE;
		}
		break;
	case GDA_VALUE_TYPE_SINGLE :
		dvalue = g_strtod (as_string, endptr);
		if (*as_string != '\0' && **endptr == '\0'){
			gda_value_set_single (value, (gfloat) dvalue);
			retval = TRUE;
		}
		break;
	case GDA_VALUE_TYPE_DOUBLE :
		dvalue = g_strtod (as_string, endptr);
		if (*as_string != '\0' && **endptr == '\0'){
			gda_value_set_double (value, dvalue);
			retval = TRUE;
		}
		break;
	case GDA_VALUE_TYPE_NUMERIC :
		//FIXME: what test whould i do for numeric?
		numeric.number = g_strdup (as_string);
		numeric.precision = 0; //FIXME
		numeric.width = 0; //FIXME
		gda_value_set_numeric (value, &numeric);
		g_free (numeric.number);
		retval = TRUE;
		break;
	case GDA_VALUE_TYPE_DATE :
		gdate = g_date_new ();
		g_date_set_parse (gdate, as_string);
		if (g_date_valid (gdate)) {
			date.day = g_date_get_day (gdate);
			date.month = g_date_get_month (gdate);
			date.year = g_date_get_year (gdate);
			gda_value_set_date (value, &date);
			retval = TRUE;
		}
		g_date_free (gdate);
		break;
	case GDA_VALUE_TYPE_GEOMETRIC_POINT :
		//FIXME: add more checks to make_point
		make_point (&point, as_string);
		gda_value_set_geometric_point (value, &point);
		break;
	case GDA_VALUE_TYPE_NULL :
		gda_value_set_null (value);
		break;
	case GDA_VALUE_TYPE_TIMESTAMP :
		//FIXME: add more checks to make_timestamp
		make_timestamp (&timestamp, as_string);
		gda_value_set_timestamp (value, &timestamp);
		break;
	case GDA_VALUE_TYPE_TIME :
		//FIXME: add more checks to make_time
		make_time (&timegda, as_string);
		gda_value_set_time (value, &timegda);
		break;
	case GDA_VALUE_TYPE_TYPE :
		value->value.v_type = gda_type_from_string (as_string);
		break;
	case GDA_VALUE_TYPE_LIST : //FIXME
	default :
		gda_value_set_string (value, as_string);
		retval = TRUE;
	}

	return retval;
}

/*
 * Public functions
 */

/**
 * gda_value_new_null
 *
 * Make a new #GdaValue of type #GDA_VALUE_TYPE_NULL.
 *
 * Returns: The newly created #GdaValue.
 */
GdaValue *
gda_value_new_null (void)
{
	GdaValue *value;

	value = g_new0 (GdaValue, 1);
	value->type = GDA_VALUE_TYPE_NULL;

	return value;
}

/**
 * gda_value_new_bigint
 * @val: value to set for the new #GdaValue.
 *
 * Make a new #GdaValue of type #GDA_VALUE_TYPE_BIGINT with value @val.
 *
 * Returns: The newly created #GdaValue.
 */
GdaValue *
gda_value_new_bigint (gint64 val)
{
	GdaValue *value;

	value = g_new0 (GdaValue, 1);
	gda_value_set_bigint (value, val);

	return value;
}

/**
 * gda_value_new_binary
 * @val: value to set for the new #GdaValue.
 *
 * Make a new #GdaValue of type #GDA_VALUE_TYPE_BINARY with value @val.
 *
 * Returns: The newly created #GdaValue.
 */
GdaValue *
gda_value_new_binary (gconstpointer val, glong size)
{
	GdaValue *value;

	value = g_new0 (GdaValue, 1);
	gda_value_set_binary (value, val, size);

	return value;
}

/**
 * gda_value_new_boolean
 * @val: value to set for the new #GdaValue.
 *
 * Make a new #GdaValue of type #GDA_VALUE_TYPE_BOOLEAN with value @val.
 *
 * Returns: The newly created #GdaValue.
 */
GdaValue *
gda_value_new_boolean (gboolean val)
{
	GdaValue *value;

	value = g_new0 (GdaValue, 1);
	gda_value_set_boolean (value, val);

	return value;
}

/**
 * gda_value_new_date
 * @val: value to set for the new #GdaValue.
 *
 * Make a new #GdaValue of type #GDA_VALUE_TYPE_DATE with value @val.
 *
 * Returns: The newly created #GdaValue.
 */
GdaValue *
gda_value_new_date (const GdaDate *val)
{
	GdaValue *value;

	value = g_new0 (GdaValue, 1);
	gda_value_set_date (value, val);

	return value;
}

/**
 * gda_value_new_double
 * @val: value to set for the new #GdaValue.
 *
 * Make a new #GdaValue of type #GDA_VALUE_TYPE_DOUBLE with value @val.
 *
 * Returns: The newly created #GdaValue.
 */
GdaValue *
gda_value_new_double (gdouble val)
{
	GdaValue *value;

	value = g_new0 (GdaValue, 1);
	gda_value_set_double (value, val);

	return value;
}

/**
 * gda_value_new_geometric_point
 * @val: value to set for the new #GdaValue.
 *
 * Make a new #GdaValue of type #GDA_VALUE_TYPE_GEOMETRIC_POINT with value
 * @val.
 *
 * Returns: The newly created #GdaValue.
 */
GdaValue *
gda_value_new_geometric_point (const GdaGeometricPoint *val)
{
	GdaValue *value;

	value = g_new0 (GdaValue, 1);
	gda_value_set_geometric_point (value, val);

	return value;
}

/**
 * gda_value_new_integer
 * @val: value to set for the new #GdaValue.
 *
 * Make a new #GdaValue of type #GDA_VALUE_TYPE_INTEGER with value @val.
 *
 * Returns: The newly created #GdaValue.
 */
GdaValue *
gda_value_new_integer (gint val)
{
	GdaValue *value;

	value = g_new0 (GdaValue, 1);
	gda_value_set_integer (value, val);

	return value;
}

/**
 * gda_value_new_list
 * @val: value to set for the new #GdaValue.
 *
 * Make a new #GdaValue of type #GDA_VALUE_TYPE_LIST with value @val.
 *
 * Returns: The newly created #GdaValue.
 */
GdaValue *
gda_value_new_list (const GdaValueList *val)
{
	GdaValue *value;

	value = g_new0 (GdaValue, 1);
	gda_value_set_list (value, val);

	return value;
}

/**
 * gda_value_new_numeric
 * @val: value to set for the new #GdaValue.
 *
 * Make a new #GdaValue of type #GDA_VALUE_TYPE_NUMERIC with value @val.
 *
 * Returns: The newly created #GdaValue.
 */
GdaValue *
gda_value_new_numeric (const GdaNumeric *val)
{
	GdaValue *value;

	value = g_new0 (GdaValue, 1);
	gda_value_set_numeric (value, val);

	return value;
}

/**
 * gda_value_new_single
 * @val: value to set for the new #GdaValue.
 *
 * Make a new #GdaValue of type #GDA_VALUE_TYPE_SINGLE with value @val.
 *
 * Returns: The newly created #GdaValue.
 */
GdaValue *
gda_value_new_single (gfloat val)
{
	GdaValue *value;

	value = g_new0 (GdaValue, 1);
	gda_value_set_single (value, val);

	return value;
}

/**
 * gda_value_new_smallint
 * @val: value to set for the new #GdaValue.
 *
 * Make a new #GdaValue of type #GDA_VALUE_TYPE_SMALLINT with value @val.
 *
 * Returns: The newly created #GdaValue.
 */
GdaValue *
gda_value_new_smallint (gshort val)
{
	GdaValue *value;

	value = g_new0 (GdaValue, 1);
	gda_value_set_smallint (value, val);

	return value;
}

/**
 * gda_value_new_string
 * @val: value to set for the new #GdaValue.
 *
 * Make a new #GdaValue of type #GDA_VALUE_TYPE_STRING with value @val.
 *
 * Returns: The newly created #GdaValue.
 */
GdaValue *
gda_value_new_string (const gchar *val)
{
	GdaValue *value;

	value = g_new0 (GdaValue, 1);
	gda_value_set_string (value, val);

	return value;
}

/**
 * gda_value_new_time
 * @val: value to set for the new #GdaValue.
 *
 * Make a new #GdaValue of type #GDA_VALUE_TYPE_TIME with value @val.
 *
 * Returns: The newly created #GdaValue.
 */
GdaValue *
gda_value_new_time (const GdaTime *val)
{
	GdaValue *value;

	value = g_new0 (GdaValue, 1);
	gda_value_set_time (value, val);

	return value;
}

/**
 * gda_value_new_timestamp
 * @val: value to set for the new #GdaValue.
 *
 * Make a new #GdaValue of type #GDA_VALUE_TYPE_TIMESTAMP with value @val.
 *
 * Returns: The newly created #GdaValue.
 */
GdaValue *
gda_value_new_timestamp (const GdaTimestamp *val)
{
	GdaValue *value;

	value = g_new0 (GdaValue, 1);
	gda_value_set_timestamp (value, val);

	return value;
}

/**
 * gda_value_new_tinyint
 * @val: value to set for the new #GdaValue.
 *
 * Make a new #GdaValue of type #GDA_VALUE_TYPE_TINYINT with value @val.
 *
 * Returns: The newly created #GdaValue.
 */
GdaValue *
gda_value_new_tinyint (gchar val)
{
	GdaValue *value;

	value = g_new0 (GdaValue, 1);
	gda_value_set_tinyint (value, val);

	return value;
}

/**
 * gda_value_new_type
 * @val: Value to set for the new #GdaValue.
 *
 * Make a new #GdaValue of type #GDA_VALUE_TYPE_TYPE with value @val.
 *
 * Returns: the newly created #GdaValue.
 */
GdaValue *
gda_value_new_type (GdaValueType val)
{
	GdaValue *value;

	value = g_new0 (GdaValue, 1);
	value->type = GDA_VALUE_TYPE_TYPE;
	value->value.v_type = val;

	return value;
}

/**
 * gda_value_new_from_string
 * @as_string: stringified representation of the value.
 * @type: tha new value type.
 *
 * Make a new #GdaValue of type @type from its string representation.
 *
 * Returns: The newly created #GdaValue or NULL if the string representation
 * cannot be converted to the specified @type.
 */
GdaValue *
gda_value_new_from_string (const gchar *as_string, GdaValueType type)
{
	GdaValue *value;

	g_return_val_if_fail (as_string != NULL, NULL);
	value = g_new0 (GdaValue, 1);
	if (!gda_value_set_from_string (value, as_string, type)){
		g_free (value);
		value = NULL;
	}

	return value;
}

/**
 * gda_value_free
 * @value: the resource to free.
 *
 * Deallocates all memory associated to  a #GdaValue.
 */
void
gda_value_free (GdaValue *value)
{
	g_return_if_fail (value != NULL);

	clear_value (value);
	g_free (value);
}

/**
 * gda_value_get_type
 * @value: value to get the type from.
 *
 * Retrieve the type of the given value.
 *
 * Returns: the #GdaValueType of the value.
 */
GdaValueType
gda_value_get_type (GdaValue *value)
{
	g_return_val_if_fail (value != NULL, GDA_VALUE_TYPE_UNKNOWN);
	return value->type;
}

/**
 * gda_value_is_null
 * @value: value to test.
 *
 * Tests if a given @value is of type #GDA_VALUE_TYPE_NULL.
 *
 * Returns: a boolean that says whether or not @value is of type
 * #GDA_VALUE_TYPE_NULL.
 */
gboolean
gda_value_is_null (GdaValue *value)
{
	g_return_val_if_fail (value != NULL, FALSE);
	return value->type == GDA_VALUE_TYPE_NULL;
}

/**
 * gda_value_copy
 * @value: value to get a copy from.
 *
 * Creates a new #GdaValue from an existing one.
 * 
 * Returns: a newly allocated #GdaValue with a copy of the data in @value.
 */
GdaValue *
gda_value_copy (const GdaValue *value)
{
	GdaValue *copy;
	GList *l;

	g_return_val_if_fail (value != NULL, NULL);

	copy = g_new0 (GdaValue, 1);
	copy->type = value->type;

	switch (value->type) {
	case GDA_VALUE_TYPE_BIGINT :
		copy->value.v_bigint = value->value.v_bigint;
		break;
	case GDA_VALUE_TYPE_BINARY :
		copy->value.v_binary = g_malloc0 (value->binary_length);
		copy->binary_length = value->binary_length;
		memcpy (copy->value.v_binary, value->value.v_binary, value->binary_length);
		break;
	case GDA_VALUE_TYPE_BOOLEAN :
		copy->value.v_boolean = value->value.v_boolean;
		break;
	case GDA_VALUE_TYPE_DATE :
		memcpy (&copy->value.v_date, &value->value.v_date, sizeof (GdaDate));
		break;
	case GDA_VALUE_TYPE_DOUBLE :
		copy->value.v_double = value->value.v_double;
		break;
	case GDA_VALUE_TYPE_GEOMETRIC_POINT :
		memcpy (&copy->value.v_point, &value->value.v_point, sizeof (GdaGeometricPoint));
		break;
	case GDA_VALUE_TYPE_INTEGER :
		copy->value.v_integer = value->value.v_integer;
		break;
	case GDA_VALUE_TYPE_LIST :
		copy->value.v_list = NULL;
		for (l = value->value.v_list; l != NULL; l = l->next) {
			GdaValue *v = (GdaValue *) l->data;
			copy->value.v_list = g_list_append (copy->value.v_list,
							    gda_value_copy (v));
		}
		break;
	case GDA_VALUE_TYPE_NUMERIC :
		memcpy (&copy->value.v_numeric, &value->value.v_numeric, sizeof (GdaNumeric));
		break;
	case GDA_VALUE_TYPE_SINGLE :
		copy->value.v_single = value->value.v_single;
		break;
	case GDA_VALUE_TYPE_SMALLINT :
		copy->value.v_smallint = value->value.v_smallint;
		break;
	case GDA_VALUE_TYPE_STRING :
		copy->value.v_string = g_strdup (value->value.v_string);
		break;
	case GDA_VALUE_TYPE_TIME :
		memcpy (&copy->value.v_time, &value->value.v_time, sizeof (GdaTime));
		break;
	case GDA_VALUE_TYPE_TIMESTAMP :
		memcpy (&copy->value.v_timestamp, &value->value.v_timestamp, sizeof (GdaTimestamp));
		break;
	case GDA_VALUE_TYPE_TINYINT :
		copy->value.v_tinyint = value->value.v_tinyint;
		break;
	case GDA_VALUE_TYPE_TYPE :
		copy->value.v_type = value->value.v_type;
		break;
	default :
		memset (&copy->value, 0, sizeof (copy->value));
		break;
	}

	return copy;
}


/**
 * gda_value_get_bigint
 * @value: a #GdaValue whose value we want to get.
 *
 * Gets the value stored in @value.
 * 
 * Returns: the value contained in @value.
 */
gint64
gda_value_get_bigint (GdaValue *value)
{
	g_return_val_if_fail (value != NULL, -1);
	g_return_val_if_fail (gda_value_isa (value, GDA_VALUE_TYPE_BIGINT), -1);
	return value->value.v_bigint;
}

/**
 * gda_value_set_bigint
 * @value: a #GdaValue that will store @val.
 * @val: value to be stored in @value.
 *
 * Stores @val into @value.
 */
void
gda_value_set_bigint (GdaValue *value, gint64 val)
{
	g_return_if_fail (value != NULL);

	clear_value (value);
	value->type = GDA_VALUE_TYPE_BIGINT;
	value->value.v_bigint = val;
}

/**
 * gda_value_get_binary
 * @value: a #GdaValue whose value we want to get.
 * @size: holder for length of data.
 *
 * Gets the value stored in @value.
 * 
 * Returns: the value contained in @value.
 */
gconstpointer
gda_value_get_binary (GdaValue *value, glong *size)
{
	gconstpointer val;

	g_return_val_if_fail (value != NULL, NULL);
	g_return_val_if_fail (gda_value_isa (value, GDA_VALUE_TYPE_BINARY), NULL);

	val = value->value.v_binary;
	if (size)
		*size = value->binary_length;

	return val;
}

/**
 * gda_value_set_binary
 * @value: a #GdaValue that will store @val.
 * @val: value to be stored in @value.
 * @size: the size of the memory pool pointed to by @val.
 *
 * Stores @val into @value.
 */
void
gda_value_set_binary (GdaValue *value, gconstpointer val, glong size)
{
	g_return_if_fail (value != NULL);

	clear_value (value);
	value->type = GDA_VALUE_TYPE_BINARY;
	value->value.v_binary = g_malloc0 (size);
	value->binary_length = size;
	memcpy (value->value.v_binary, val, size);
}

/**
 * gda_value_get_boolean
 * @value: a #GdaValue whose value we want to get.
 *
 * Gets the value stored in @value.
 * 
 * Returns: the value contained in @value.
 */
gboolean
gda_value_get_boolean (GdaValue *value)
{
	g_return_val_if_fail (value != NULL, FALSE);
	g_return_val_if_fail (gda_value_isa (value, GDA_VALUE_TYPE_BOOLEAN), FALSE);
	return value->value.v_boolean;
}

/**
 * gda_value_set_boolean
 * @value: a #GdaValue that will store @val.
 * @val: value to be stored in @value.
 *
 * Stores @val into @value.
 */
void
gda_value_set_boolean (GdaValue *value, gboolean val)
{
	g_return_if_fail (value != NULL);

	clear_value (value);
	value->type = GDA_VALUE_TYPE_BOOLEAN;
	value->value.v_boolean = val;
}

/**
 * gda_value_get_date
 * @value: a #GdaValue whose value we want to get.
 *
 * Gets the value stored in @value.
 * 
 * Returns: the value contained in @value.
 */
const GdaDate *
gda_value_get_date (GdaValue *value)
{
	g_return_val_if_fail (value != NULL, NULL);
	g_return_val_if_fail (gda_value_isa (value, GDA_VALUE_TYPE_DATE), NULL);
	return (const GdaDate *) &value->value.v_date;
}

/**
 * gda_value_set_date
 * @value: a #GdaValue that will store @val.
 * @val: value to be stored in @value.
 *
 * Stores @val into @value.
 */
void
gda_value_set_date (GdaValue *value, const GdaDate *val)
{
	g_return_if_fail (value != NULL);
	g_return_if_fail (val != NULL);

	clear_value (value);
	value->type = GDA_VALUE_TYPE_DATE;
	value->value.v_date.year = val->year;
	value->value.v_date.month = val->month;
	value->value.v_date.day = val->day;
}

/**
 * gda_value_get_double
 * @value: a #GdaValue whose value we want to get.
 *
 * Gets the value stored in @value.
 * 
 * Returns: the value contained in @value.
 */
gdouble
gda_value_get_double (GdaValue *value)
{
	g_return_val_if_fail (value != NULL, -1);
	g_return_val_if_fail (gda_value_isa (value, GDA_VALUE_TYPE_DOUBLE), -1);
	return value->value.v_double;
}

/**
 * gda_value_set_double
 * @value: a #GdaValue that will store @val.
 * @val: value to be stored in @value.
 *
 * Stores @val into @value.
 */
void
gda_value_set_double (GdaValue *value, gdouble val)
{
	g_return_if_fail (value != NULL);

	clear_value (value);
	value->type = GDA_VALUE_TYPE_DOUBLE;
	value->value.v_double = val;
}

/**
 * gda_value_get_geometric_point
 * @value: a #GdaValue whose value we want to get.
 *
 * Gets the value stored in @value.
 * 
 * Returns: the value contained in @value.
 */
const GdaGeometricPoint *
gda_value_get_geometric_point (GdaValue *value)
{
	g_return_val_if_fail (value != NULL, NULL);
	g_return_val_if_fail (gda_value_isa (value, GDA_VALUE_TYPE_GEOMETRIC_POINT), NULL);
	return (const GdaGeometricPoint *) &value->value;
}

/**
 * gda_value_set_geometric_point
 * @value: a #GdaValue that will store @val.
 * @val: value to be stored in @value.
 *
 * Stores @val into @value.
 */
void
gda_value_set_geometric_point (GdaValue *value, const GdaGeometricPoint *val)
{
	g_return_if_fail (value != NULL);
	g_return_if_fail (val != NULL);

	clear_value (value);
	value->type = GDA_VALUE_TYPE_GEOMETRIC_POINT;
	value->value.v_point.x = val->x;
	value->value.v_point.y = val->y;
}

/**
 * gda_value_get_integer
 * @value: a #GdaValue whose value we want to get.
 *
 * Gets the value stored in @value.
 * 
 * Returns: the value contained in @value.
 */
gint
gda_value_get_integer (GdaValue *value)
{
	g_return_val_if_fail (value != NULL, -1);
	g_return_val_if_fail (gda_value_isa (value, GDA_VALUE_TYPE_INTEGER), -1);
	return value->value.v_integer;
}

/**
 * gda_value_set_integer
 * @value: a #GdaValue that will store @val.
 * @val: value to be stored in @value.
 *
 * Stores @val into @value.
 */
void
gda_value_set_integer (GdaValue *value, gint val)
{
	g_return_if_fail (value != NULL);

	clear_value (value);
	value->type = GDA_VALUE_TYPE_INTEGER;
	value->value.v_integer = val;
}

/**
 * gda_value_get_list
 * @value: a #GdaValue whose value we want to get.
 *
 * Gets the value stored in @value.
 * 
 * Returns: the value contained in @value.
 */
const GdaValueList *
gda_value_get_list (GdaValue *value)
{
	g_return_val_if_fail (value != NULL, NULL);
	g_return_val_if_fail (gda_value_isa (value, GDA_VALUE_TYPE_LIST), NULL);
	return (const GdaValueList *) value->value.v_list;
}

/**
 * gda_value_set_list
 * @value: a #GdaValue that will store @val.
 * @val: value to be stored in @value.
 *
 * Stores @val into @value.
 */
void
gda_value_set_list (GdaValue *value, const GdaValueList *val)
{
	g_return_if_fail (value != NULL);
	g_return_if_fail (val != NULL);

	/* FIXME: implement */
}

/**
 * gda_value_set_null
 * @value: a #GdaValue that will store a value of type #GDA_VALUE_TYPE_NULL.
 *
 * Sets the type of @value to #GDA_VALUE_TYPE_NULL.
 */
void
gda_value_set_null (GdaValue *value)
{
	g_return_if_fail (value != NULL);

	clear_value (value);
	value->type = GDA_VALUE_TYPE_NULL;
}

/**
 * gda_value_get_numeric
 * @value: a #GdaValue whose value we want to get.
 *
 * Gets the value stored in @value.
 * 
 * Returns: the value contained in @value.
 */
const GdaNumeric *
gda_value_get_numeric (GdaValue *value)
{
	g_return_val_if_fail (value != NULL, NULL);
	g_return_val_if_fail (gda_value_isa (value, GDA_VALUE_TYPE_NUMERIC), NULL);
	return (const GdaNumeric *) &value->value.v_numeric;
}

/**
 * gda_value_set_numeric
 * @value: a #GdaValue that will store @val.
 * @val: value to be stored in @value.
 *
 * Stores @val into @value.
 */
void
gda_value_set_numeric (GdaValue *value, const GdaNumeric *val)
{
	g_return_if_fail (value != NULL);
	g_return_if_fail (val != NULL);

	clear_value (value);
	value->type = GDA_VALUE_TYPE_NUMERIC;
	value->value.v_numeric.number = g_strdup (val->number);
	value->value.v_numeric.precision = val->precision;
	value->value.v_numeric.width = val->width;
}

/**
 * gda_value_get_single
 * @value: a #GdaValue whose value we want to get.
 *
 * Gets the value stored in @value.
 * 
 * Returns: the value contained in @value.
 */
gfloat
gda_value_get_single (GdaValue *value)
{
	g_return_val_if_fail (value != NULL, -1);
	g_return_val_if_fail (gda_value_isa (value, GDA_VALUE_TYPE_SINGLE), -1);
	return value->value.v_single;
}

/**
 * gda_value_set_single
 * @value: a #GdaValue that will store @val.
 * @val: value to be stored in @value.
 *
 * Stores @val into @value.
 */
void
gda_value_set_single (GdaValue *value, gfloat val)
{
	g_return_if_fail (value != NULL);

	clear_value (value);
	value->type = GDA_VALUE_TYPE_SINGLE;
	value->value.v_single = val;
}

/**
 * gda_value_get_smallint
 * @value: a #GdaValue whose value we want to get.
 *
 * Gets the value stored in @value.
 * 
 * Returns: the value contained in @value.
 */
gshort
gda_value_get_smallint (GdaValue *value)
{
	g_return_val_if_fail (value != NULL, -1);
	g_return_val_if_fail (gda_value_isa (value, GDA_VALUE_TYPE_SMALLINT), -1);
	return value->value.v_smallint;
}

/**
 * gda_value_set_smallint
 * @value: a #GdaValue that will store @val.
 * @val: value to be stored in @value.
 *
 * Stores @val into @value.
 */
void
gda_value_set_smallint (GdaValue *value, gshort val)
{
	g_return_if_fail (value != NULL);

	clear_value (value);
	value->type = GDA_VALUE_TYPE_SMALLINT;
	value->value.v_smallint = val;
}

/**
 * gda_value_get_string
 * @value: a #GdaValue whose value we want to get.
 *
 * Gets the value stored in @value.
 * 
 * Returns: the value contained in @value.
 */
const gchar *
gda_value_get_string (GdaValue *value)
{
	g_return_val_if_fail (value != NULL, NULL);
	g_return_val_if_fail (gda_value_isa (value, GDA_VALUE_TYPE_STRING), NULL);
	return (const gchar *) value->value.v_string;
}

/**
 * gda_value_set_string
 * @value: a #GdaValue that will store @val.
 * @val: value to be stored in @value.
 *
 * Stores @val into @value.
 */
void
gda_value_set_string (GdaValue *value, const gchar *val)
{
	g_return_if_fail (value != NULL);

	clear_value (value);
	value->type = GDA_VALUE_TYPE_STRING;
	value->value.v_string = g_strdup (val);
}

/**
 * gda_value_get_time
 * @value: a #GdaValue whose value we want to get.
 *
 * Gets the value stored in @value.
 * 
 * Returns: the value contained in @value.
 */
const GdaTime *
gda_value_get_time (GdaValue *value)
{
	g_return_val_if_fail (value != NULL, NULL);
	g_return_val_if_fail (gda_value_isa (value, GDA_VALUE_TYPE_TIME), NULL);
	return (const GdaTime *) &value->value.v_time;
}

/**
 * gda_value_set_time
 * @value: a #GdaValue that will store @val.
 * @val: value to be stored in @value.
 *
 * Stores @val into @value.
 */
void
gda_value_set_time (GdaValue *value, const GdaTime *val)
{
	g_return_if_fail (value != NULL);
	g_return_if_fail (val != NULL);

	clear_value (value);
	value->type = GDA_VALUE_TYPE_TIME;
	value->value.v_time.hour = val->hour;
	value->value.v_time.minute = val->minute;
	value->value.v_time.second = val->second;
	value->value.v_time.timezone = val->timezone;
}

/**
 * gda_value_get_timestamp
 * @value: a #GdaValue whose value we want to get.
 *
 * Gets the value stored in @value.
 * 
 * Returns: the value contained in @value.
 */
const GdaTimestamp *
gda_value_get_timestamp (GdaValue *value)
{
	g_return_val_if_fail (value != NULL, NULL);
	g_return_val_if_fail (gda_value_isa (value, GDA_VALUE_TYPE_TIMESTAMP), NULL);
	return (const GdaTimestamp *) &value->value.v_timestamp;
}

/**
 * gda_value_set_timestamp
 * @value: a #GdaValue that will store @val.
 * @val: value to be stored in @value.
 *
 * Stores @val into @value.
 */
void
gda_value_set_timestamp (GdaValue *value, const GdaTimestamp *val)
{
	g_return_if_fail (value != NULL);
	g_return_if_fail (val != NULL);

	clear_value (value);
	value->type = GDA_VALUE_TYPE_TIMESTAMP;
	value->value.v_timestamp.year = val->year;
	value->value.v_timestamp.month = val->month;
	value->value.v_timestamp.day = val->day;
	value->value.v_timestamp.hour = val->hour;
	value->value.v_timestamp.minute = val->minute;
	value->value.v_timestamp.second = val->second;
	value->value.v_timestamp.fraction = val->fraction;
	value->value.v_timestamp.timezone = val->timezone;
}

/**
 * gda_value_get_tinyint
 * @value: a #GdaValue whose value we want to get.
 *
 * Gets the value stored in @value.
 * 
 * Returns: the value contained in @value.
 */
gchar
gda_value_get_tinyint (GdaValue *value)
{
	g_return_val_if_fail (value != NULL, -1);
	g_return_val_if_fail (gda_value_isa (value, GDA_VALUE_TYPE_TINYINT), -1);
	return value->value.v_tinyint;
}

/**
 * gda_value_set_tinyint
 * @value: a #GdaValue that will store @val.
 * @val: value to be stored in @value.
 *
 * Stores @val into @value.
 */
void
gda_value_set_tinyint (GdaValue *value, gchar val)
{
	g_return_if_fail (value != NULL);

	clear_value (value);
	value->type = GDA_VALUE_TYPE_TINYINT;
	value->value.v_tinyint = val;
}

/**
 * gda_value_get_vtype
 * @value: a #GdaValue whose value we want to get.
 * 
 * Gets the value stored in @value.
 * 
 * Returns: the value contained in @value.
 **/
GdaValueType
gda_value_get_vtype (GdaValue *value)
{
	g_return_val_if_fail (value != NULL, GDA_VALUE_TYPE_UNKNOWN);
	g_return_val_if_fail (gda_value_isa (value, GDA_VALUE_TYPE_TYPE), GDA_VALUE_TYPE_UNKNOWN);
	return value->value.v_type;
}


/**
 * gda_value_set_vtype
 * @value: a #GdaValue that will store @type.
 * @type: value to be stored in @value.
 *
 * Stores @type into @value.
 */
void
gda_value_set_vtype (GdaValue *value, GdaValueType type)
{
	g_return_if_fail (value != NULL);

	clear_value (value);
	value->type = GDA_VALUE_TYPE_TYPE;
	value->value.v_type = type;
}

/**
 * gda_value_set_from_string
 * @value: a #GdaValue that will store @val.
 * @as_string: the stringified representation of the value.
 * @type: the type of the value
 *
 * Stores the value data from its string representation as @type.
 *
 * Returns: TRUE if the value has been properly converted to @type from
 * its string representation. FALSE otherwise.
 */
gboolean
gda_value_set_from_string (GdaValue *value, 
			   const gchar *as_string,
			   GdaValueType type)
{
	g_return_val_if_fail (value != NULL, FALSE);
	g_return_val_if_fail (as_string != NULL, FALSE);

	clear_value (value);
	value->type = type;
	return set_from_string (value, as_string);
}

/**
 * gda_value_set_from_value
 * @value: a #GdaValue.
 * @from: the value to copy from.
 *
 * Set the value of a #GdaValue from another #GdaValue. This
 * is different from #gda_value_copy, which creates a new #GdaValue.
 * #gda_value_set_from_value, on the other hand, copies the contents
 * of @copy into @value, which must already be allocated.
 *
 * Returns: TRUE if successful, FALSE otherwise.
 */
gboolean
gda_value_set_from_value (GdaValue *value, const GdaValue *from)
{
	g_return_val_if_fail (value != NULL, FALSE);
	g_return_val_if_fail (from != NULL, FALSE);

	switch (from->type) {
	case GDA_VALUE_TYPE_NULL :
		gda_value_set_null (value);
		break;
	case GDA_VALUE_TYPE_BIGINT :
		gda_value_set_bigint (value, gda_value_get_bigint ((GdaValue *) from));
		break;
	case GDA_VALUE_TYPE_BINARY :
		gda_value_set_binary (value, from->value.v_binary, from->binary_length);
		break;
	case GDA_VALUE_TYPE_BOOLEAN :
		gda_value_set_boolean (value, gda_value_get_boolean ((GdaValue *) from));
		break;
	case GDA_VALUE_TYPE_DATE :
		gda_value_set_date (value, gda_value_get_date ((GdaValue *) from));
		break;
	case GDA_VALUE_TYPE_DOUBLE :
		gda_value_set_double (value, gda_value_get_double ((GdaValue *) from));
		break;
	case GDA_VALUE_TYPE_GEOMETRIC_POINT :
		gda_value_set_geometric_point (value, gda_value_get_geometric_point ((GdaValue *) from));
		break;
	case GDA_VALUE_TYPE_INTEGER :
		gda_value_set_integer (value, gda_value_get_integer ((GdaValue *) from));
		break;
	case GDA_VALUE_TYPE_LIST :
		gda_value_set_list (value, gda_value_get_list ((GdaValue *) from));
		break;
	case GDA_VALUE_TYPE_NUMERIC :
		gda_value_set_numeric (value, gda_value_get_numeric ((GdaValue *) from));
		break;
	case GDA_VALUE_TYPE_SINGLE :
		gda_value_set_single (value, gda_value_get_single ((GdaValue *) from));
		break;
	case GDA_VALUE_TYPE_SMALLINT :
		gda_value_set_smallint (value, gda_value_get_smallint ((GdaValue *) from));
		break;
	case GDA_VALUE_TYPE_STRING :
		gda_value_set_string (value, gda_value_get_string ((GdaValue *) from));
		break;
	case GDA_VALUE_TYPE_TIME :
		gda_value_set_time (value, gda_value_get_time ((GdaValue *) from));
		break;
	case GDA_VALUE_TYPE_TIMESTAMP :
		gda_value_set_timestamp (value, gda_value_get_timestamp ((GdaValue *) from));
		break;
	case GDA_VALUE_TYPE_TINYINT :
		gda_value_set_tinyint (value, gda_value_get_tinyint ((GdaValue *) from));
		break;
	case GDA_VALUE_TYPE_TYPE :
		clear_value (value);
		value->type = GDA_VALUE_TYPE_TYPE;
		value->value.v_type = from->value.v_type;
		break;
	default :
		return FALSE;
	}

	return TRUE;
}

/**
 * gda_value_stringify
 * @value: a #GdaValue.
 *
 * Converts a GdaValue to its string representation as indicated by this
 * table:
 *
 * Returns: a string formatted according to the printf() style indicated in
 * the preceding table.
 */
gchar *
gda_value_stringify (GdaValue *value)
{
	const GdaTime *gdatime;
	const GdaDate *gdadate;
	const GdaTimestamp *timestamp;
	const GdaGeometricPoint *point;
	const GdaValueList *list;
	const GdaNumeric *numeric;
	GList *l;
	GString *str = NULL;
	gchar *retval = NULL;

	g_return_val_if_fail (value != NULL, NULL);

	switch (value->type){
	case GDA_VALUE_TYPE_BIGINT:
		retval = g_strdup_printf ("%lld", gda_value_get_bigint (value));
		break;
	case GDA_VALUE_TYPE_BINARY :
		retval = g_malloc0 (value->binary_length + 1);
		memcpy (retval, value->value.v_binary, value->binary_length);
		retval[value->binary_length] = '\0';
		break;
	case GDA_VALUE_TYPE_BOOLEAN:
		if (gda_value_get_boolean (value))
			retval = g_strdup (_("TRUE"));
		else
			retval = g_strdup (_("FALSE"));
		break;
	case GDA_VALUE_TYPE_STRING:
		retval = g_strdup (gda_value_get_string (value));
		break;
	case GDA_VALUE_TYPE_INTEGER:
		retval = g_strdup_printf ("%d", gda_value_get_integer (value));
		break;
	case GDA_VALUE_TYPE_SMALLINT:
		retval = g_strdup_printf ("%d", gda_value_get_smallint (value));
		break;
	case GDA_VALUE_TYPE_TINYINT:
		retval = g_strdup_printf ("%d", gda_value_get_tinyint (value));
		break;
	case GDA_VALUE_TYPE_SINGLE:
		retval = g_strdup_printf ("%f", gda_value_get_single (value));
		break;
	case GDA_VALUE_TYPE_DOUBLE:
		retval = g_strdup_printf ("%f", gda_value_get_double (value));
		break;
	case GDA_VALUE_TYPE_TIME:
		gdatime = gda_value_get_time (value);
		retval = g_strdup_printf (gdatime->timezone == TIMEZONE_INVALID ?
					  "%02u:%02u:%02u" : "%02u:%02u:%02u%+03d", 
					  gdatime->hour,
					  gdatime->minute,
					  gdatime->second,
					  gdatime->timezone / 3600);
		break;
	case GDA_VALUE_TYPE_DATE:
		gdadate = gda_value_get_date (value);
		retval = g_strdup_printf ("%04u-%02u-%02u",
					  gdadate->year,
					  gdadate->month,
					  gdadate->day);
		break;
	case GDA_VALUE_TYPE_TIMESTAMP:
		timestamp = gda_value_get_timestamp (value);
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
		break;
	case GDA_VALUE_TYPE_GEOMETRIC_POINT:
		point = gda_value_get_geometric_point (value);
		retval = g_strdup_printf ("(%.*g,%.*g)",
					  DBL_DIG,
					  point->x,
					  DBL_DIG,
					  point->y);
		break;
	case GDA_VALUE_TYPE_LIST:
		list = gda_value_get_list (value);
		for (l = (GList *) list; l != NULL; l = l->next) {
			gchar *s;

			s = gda_value_stringify ((GdaValue *) l->data);
			if (!str) {
				str = g_string_new ("{ \"");
				str = g_string_append (str, s);
				str = g_string_append (str, "\"");
			}
			else {
				str = g_string_append (str, ", \"");
				str = g_string_append (str, s);
				str = g_string_append (str, "\"");
			}
			g_free (s);
		}

		if (str) {
			str = g_string_append (str, " }");
			retval = str->str;
			g_string_free (str, FALSE);
		}
		else
			retval = g_strdup ("");
		break;
	case GDA_VALUE_TYPE_NUMERIC:
		numeric = gda_value_get_numeric (value);
		retval = g_strdup (numeric->number);
		break;
	case GDA_VALUE_TYPE_NULL:
		retval = g_strdup ("NULL");
		break;
	case GDA_VALUE_TYPE_TYPE :
		retval = g_strdup (gda_type_to_string (value->value.v_type));
		break;
	default:
		retval = g_strdup ("");
	}
        	
	return retval;
}

/**
 * gda_value_compare
 * @value1: a #GdaValue to compare.
 * @value2: the other #GdaValue to be compared to @value1.
 *
 * Compares two values of the <b>same</b> type.
 *
 * Returns: if both values have the same time, return 0 if both contains
 * the same value, an integer less than 0 if @value1 is less than @value2 or
 * an integer greater than 0 if @value1 is greater than @value2.
 */
gint
gda_value_compare (const GdaValue *value1, const GdaValue *value2)
{
	GList *l1, *l2;
	gint retval;

	g_return_val_if_fail (value1 != NULL && value2 != NULL, -1);
	g_return_val_if_fail (value1->type != value2->type, -1);

	switch (value1->type) {
	case GDA_VALUE_TYPE_BIGINT :
		retval = (gint) value1->value.v_bigint - value2->value.v_bigint;
		break;
	case GDA_VALUE_TYPE_BINARY :
		//FIXME
		retval = 0;
		break;
	case GDA_VALUE_TYPE_BOOLEAN :
		retval = value1->value.v_boolean - value2->value.v_boolean;
		break;
	case GDA_VALUE_TYPE_DATE :
		retval = memcmp (&value1->value.v_date, &value2->value.v_date,
				  sizeof (GdaDate));
		break;
	case GDA_VALUE_TYPE_DOUBLE :
		retval = (gint) value1->value.v_double - value2->value.v_double;
		break;
	case GDA_VALUE_TYPE_GEOMETRIC_POINT :
		retval = memcmp (&value1->value.v_point, &value2->value.v_point,
				 sizeof (GdaGeometricPoint));
		break;
	case GDA_VALUE_TYPE_INTEGER :
		retval = value1->value.v_integer - value2->value.v_integer;
		break;
	case GDA_VALUE_TYPE_LIST :
		retval = 0;
		for (l1 = value1->value.v_list, l2 = value2->value.v_list; 
		     l1 != NULL && l2 != NULL; l1 = l1->next, l2 = l2->next){
			retval = gda_value_compare ((GdaValue *) l1->data,
						    (GdaValue *) l2->data);
			if (retval != 0) break;
		}
		if (retval == 0 && (l1 == NULL || l2 == NULL) && l1 != l2)
			retval = (l1 == NULL) ? -1 : 1;
		break;
	case GDA_VALUE_TYPE_NUMERIC :
		retval = memcmp (&value1->value.v_numeric,
				 &value2->value.v_numeric,
				 sizeof (GdaNumeric));
		break;
	case GDA_VALUE_TYPE_SINGLE :
		retval = (gint) value1->value.v_single - value2->value.v_single;
		break;
	case GDA_VALUE_TYPE_SMALLINT :
		retval = value1->value.v_smallint - value2->value.v_smallint;
		break;
	case GDA_VALUE_TYPE_STRING :
		retval = strcmp (value1->value.v_string, value2->value.v_string);
		break;
	case GDA_VALUE_TYPE_TIME :
		retval = memcmp (&value1->value.v_time, &value2->value.v_time,
				 sizeof (GdaTime));
		break;
	case GDA_VALUE_TYPE_TIMESTAMP :
		retval = memcmp (&value1->value.v_timestamp,
				 &value2->value.v_timestamp,
				 sizeof (GdaTimestamp));
		break;
	case GDA_VALUE_TYPE_TINYINT :
		retval = value1->value.v_tinyint - value2->value.v_tinyint;
		break;
	case GDA_VALUE_TYPE_TYPE :
		retval = value1->value.v_type == value2->value.v_type ? 0 : -1;
		break;
	default :
		//FIXME
		retval = -1;
		break;
	}

	return retval;
}

