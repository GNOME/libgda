/* GDA common library
 * Copyright (C) 1998 - 2005 The GNOME Foundation.
 *
 * AUTHORS:
 *	Michael Lausch <michael@lausch.at>
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *	Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 *	Juan-Mariano de Goyeneche <jmseyas@dit.upm.es> (BLOB issues)
 *      Daniel Espinosa Ortiz <esodan@gmail.com> (Port to GValue)
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

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <glib/gdate.h>
#include <glib/gmessages.h>
#include <glib/gstrfuncs.h>
#include <glib/gstring.h>
#include <libgda/gda-intl.h>
#include <libgda/gda-value.h>
#include <libgda/gda-util.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#define l_g_value_unset(val) if G_IS_VALUE (val) g_value_unset (val)

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
	timestamp->fraction = atol (value) * 10; /* I have only hundredths of second */
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
        gulong ulvalue;
	GdaValueType type;
	GdaBinary binary;

	g_return_val_if_fail (value, FALSE);
	if (! G_IS_VALUE (value)) {
		g_warning ("Can't determine the GdaValueType of a NULL GdaValue");
		return FALSE;
	}

	retval = FALSE;
	switch (GDA_VALUE_TYPE (value)) {
	case GDA_VALUE_TYPE_BOOLEAN:
		if (g_strcasecmp (as_string, "true") == 0) {
			gda_value_set_boolean (value, TRUE);
			retval = TRUE;
		}
		else if (g_strcasecmp (as_string, "false") == 0) {
			gda_value_set_boolean (value, FALSE);
			retval = TRUE;
		}
		break;

	case GDA_VALUE_TYPE_BINARY:
		binary.data = as_string;
		binary.binary_length = strlen (as_string);
		gda_value_set_binary (value, &binary);
		break;
	
	case GDA_VALUE_TYPE_BIGINT:
		/* Use g_strtod instead of strtoll */
		dvalue = g_strtod (as_string, endptr);
		if (*as_string != '\0' && **endptr == '\0'){
			gda_value_set_bigint (value, (gint64) dvalue);
			retval = TRUE;
		}
		break;

	case GDA_VALUE_TYPE_BIGUINT:
 	        dvalue = g_strtod (as_string, endptr);
                if (*as_string!=0 && **endptr==0) {
			gda_value_set_biguint(value,(guint64)dvalue);
			retval = TRUE;
                }
		break;

	case GDA_VALUE_TYPE_INTEGER:
		lvalue = strtol (as_string, endptr, 10);
		if (*as_string != '\0' && **endptr == '\0'){
			gda_value_set_integer (value, (gint32) lvalue);
			retval = TRUE;
		}
		break;

	case GDA_VALUE_TYPE_UINTEGER:
		ulvalue = strtoul (as_string, endptr, 10);
		if (*as_string!=0 && **endptr==0) {
			gda_value_set_uinteger(value,(guint32)ulvalue);
			retval = TRUE;
		}
		break;

	case GDA_VALUE_TYPE_SMALLINT:
		lvalue = strtol (as_string, endptr, 10);
		if (*as_string != '\0' && **endptr == '\0'){
			gda_value_set_smallint (value, (gint16) lvalue);
			retval = TRUE;
		}
		break;

	case GDA_VALUE_TYPE_SMALLUINT:
		ulvalue = strtoul (as_string, endptr, 10);
		if (*as_string!=0 && **endptr==0) {
			gda_value_set_smalluint(value,(guint16)ulvalue);
			retval = TRUE;
		}
		break;

	case GDA_VALUE_TYPE_TINYINT:
		lvalue = strtol(as_string, endptr, 10);
		if (*as_string!=0 && **endptr==0) {
			gda_value_set_tinyint(value,(gchar)lvalue);
			retval = TRUE;
		}
		break;

	case GDA_VALUE_TYPE_TINYUINT:
		ulvalue = strtoul(as_string,endptr, 10);
		if (*as_string!=0 && **endptr==0) {
			gda_value_set_tinyuint(value,(guchar)ulvalue);
			retval = TRUE;
		}
		break;

	case GDA_VALUE_TYPE_SINGLE:
		dvalue = g_strtod (as_string, endptr);
		if (*as_string != '\0' && **endptr == '\0'){
			gda_value_set_single (value, (gfloat) dvalue);
			retval = TRUE;
		}
		break;

	case GDA_VALUE_TYPE_DOUBLE:
		dvalue = g_strtod (as_string, endptr);
		if (*as_string != '\0' && **endptr == '\0'){
			gda_value_set_double (value, dvalue);
			retval = TRUE;
		}
		break;

	case GDA_VALUE_TYPE_NUMERIC:
		/* FIXME: what test whould i do for numeric? */
		numeric.number = g_strdup (as_string);
		numeric.precision = 0; /* FIXME */
		numeric.width = 0; /* FIXME */
		gda_value_set_numeric (value, &numeric);
		g_free (numeric.number);
		retval = TRUE;
		break;

	case GDA_VALUE_TYPE_DATE:
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

	case GDA_VALUE_TYPE_GEOMETRIC_POINT:
		/* FIXME: add more checks to make_point */
		make_point (&point, as_string);
		gda_value_set_geometric_point (value, &point);
		break;

	case GDA_VALUE_TYPE_GOBJECT:
		/* FIXME */
		break;

	case GDA_VALUE_TYPE_NULL:
		gda_value_set_null (value);
		break;

	case GDA_VALUE_TYPE_TIMESTAMP:
		/* FIXME: add more checks to make_timestamp */
		make_timestamp (&timestamp, as_string);
		gda_value_set_timestamp (value, &timestamp);
		break;

	case GDA_VALUE_TYPE_TIME:
		/* FIXME: add more checks to make_time */
		make_time (&timegda, as_string);
		gda_value_set_time (value, &timegda);
		break;
	
	case GDA_VALUE_TYPE_TYPE:
		gda_value_set_gdatype (value, gda_type_from_string (as_string));
		break;

	case GDA_VALUE_TYPE_LIST:
		/* FIXME */
		break;

	case GDA_VALUE_TYPE_MONEY:
		/* FIXME */
		break;

	case GDA_VALUE_TYPE_STRING:
		gda_value_set_string (value, as_string);
		retval = TRUE;
		break;
	default:
		g_assert_not_reached ();
	}

	return retval;
}

/**
 * gda_value_new_null
 *
 * Makes a new #GdaValue of type #GDA_VALUE_TYPE_NULL.
 *
 * Returns: the newly created #GdaValue.
 */
GdaValue *
gda_value_new_null (void)
{
	GdaValue *value;

	value = g_new0 (GdaValue, 1);

	return value;
}

/**
 * gda_value_new_bigint
 * @val: value to set for the new #GdaValue.
 *
 * Makes a new #GdaValue of type #GDA_VALUE_TYPE_BIGINT with value @val.
 *
 * Returns: the newly created #GdaValue.
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
 * gda_value_new_biguint
 * @val: value to set for the new #GdaValue.
 *
 * Makes a new #GdaValue of type #GDA_VALUE_TYPE_BIGUINT with value @val.
 *
 * Returns: the newly created #GdaValue.
 */

GdaValue *gda_value_new_biguint (guint64 val)
{
	GdaValue *value;
	
	value = g_new0 (GdaValue,1);
	gda_value_set_biguint (value,val);
	return value;
}

/**
 * gda_value_new_binary
 * @val: value to set for the new #GdaValue.
 * @size: the size of the memory pool pointer to by @val.
 *
 * Makes a new #GdaValue of type #GDA_VALUE_TYPE_BINARY with value @val.
 *
 * Returns: the newly created #GdaValue.
 */
GdaValue *
gda_value_new_binary (guchar *val, glong size)
{
	GdaValue *value;
	GdaBinary binary;

	binary.data = val;
	binary.binary_length = size;

	value = g_new0 (GdaValue, 1);
	gda_value_set_binary (value, &binary);

	return value;
}

/* Register the GdaBinary type in the GType system */

GType
gda_binary_get_type (void)
{
	static GType type = 0;
	
	if (type == 0)
		type = g_boxed_type_register_static ("GdaBinary",
			(GBoxedCopyFunc) gda_binary_copy,
			(GBoxedFreeFunc) gda_binary_free);
	
	return type;
}

/**
 * gda_binary_copy
 * @src: source to get a copy from.
 *
 * Creates a new #GdaBinary structure from an existing one.

 * Returns: a newly allocated #GdaBinary which contains a copy of
 * information in @src.
 */
gpointer
gda_binary_copy (gpointer boxed)
{
	GdaBinary *src = (GdaBinary*) boxed;
	GdaBinary *copy = NULL;

	g_return_val_if_fail (src, NULL);

	copy = g_new0 (GdaBinary, 1);
	copy->data = g_memdup (src->data, src->binary_length);
	copy->binary_length = src->binary_length;

	return copy;
}

/**
 * gda_binary_free
 * @boxed: #GdaBinary to free.
 *
 * Deallocates all memory associated to the given #GdaBinary.
 */
void
gda_binary_free (gpointer boxed)
{
	GdaBinary *binary = (GdaBinary*) boxed;
	
	g_return_if_fail (binary);
		
	g_free (binary->data);
}



/**
 * gda_value_new_boolean
 * @val: value to set for the new #GdaValue.
 *
 * Makes a new #GdaValue of type #GDA_VALUE_TYPE_BOOLEAN with value @val.
 *
 * Returns: the newly created #GdaValue.
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
 * Makes a new #GdaValue of type #GDA_VALUE_TYPE_DATE with value @val.
 *
 * Returns: the newly created #GdaValue.
 */
GdaValue *
gda_value_new_date (const GdaDate *val)
{
	GdaValue *value;

	value = g_new0 (GdaValue, 1);
	gda_value_set_date (value, val);

	return value;
}

/* Register the GdaDate type in the GType system */

GType
gda_date_get_type (void)
{
	static GType type = 0;
	
	if (type == 0)
		type = g_boxed_type_register_static ("GdaDate",
			(GBoxedCopyFunc) gda_date_copy,
			(GBoxedFreeFunc) gda_date_free);
	
	return type;
}

gpointer 
gda_date_copy (gpointer boxed)
{
	GdaDate *val = (GdaDate*) boxed;
	GdaDate *copy = NULL;
	
	g_return_val_if_fail (val, NULL);
	
	copy = g_new0 (GdaDate, 1);
	copy->year = val->year;
	copy->month = val->month;
	copy->day = val->day;

	return copy;
}

void gda_date_free (gpointer boxed)
{
	
}


/**
 * gda_value_new_double
 * @val: value to set for the new #GdaValue.
 *
 * Makes a new #GdaValue of type #GDA_VALUE_TYPE_DOUBLE with value @val.
 *
 * Returns: the newly created #GdaValue.
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
 * Makes a new #GdaValue of type #GDA_VALUE_TYPE_GEOMETRIC_POINT with value
 * @val.
 *
 * Returns: the newly created #GdaValue.
 */
GdaValue *
gda_value_new_geometric_point (const GdaGeometricPoint *val)
{
	GdaValue *value;

	value = g_new0 (GdaValue, 1);
	gda_value_set_geometric_point (value, val);

	return value;
}

/* Register the GdaGeometricPoint type in the GType system */

GType
gda_geometricpoint_get_type (void)
{
	static GType type = 0;
	
	if (type == 0)
		type = g_boxed_type_register_static ("GdaGeometricPoint",
			(GBoxedCopyFunc) gda_geometricpoint_copy,
			(GBoxedFreeFunc) gda_geometricpoint_free);
	
	return type;
}

gpointer 
gda_geometricpoint_copy (gpointer boxed)
{
	GdaGeometricPoint *val = (GdaGeometricPoint*) boxed;
	
	g_return_val_if_fail( val, NULL);
		
	GdaGeometricPoint *copy = g_new0(GdaGeometricPoint, 1);
	copy->x = val->x;
	copy->y = val->y;

	return copy;
}

void gda_geometricpoint_free (gpointer boxed)
{
	
}

/**
 * gda_value_new_gobject
 * @val: value to set for the new #GdaValue.
 *
 * Makes a new #GdaValue of type #GDA_VALUE_TYPE_GOBJECT with value @val.
 *
 * Returns: the newly created #GdaValue.
 */
GdaValue *
gda_value_new_gobject (const GObject *val)
{
	GdaValue *value;

	value = g_new0 (GdaValue, 1);
	gda_value_set_gobject (value, val);

	return value;
}

/**
 * gda_value_new_integer
 * @val: value to set for the new #GdaValue.
 *
 * Makes a new #GdaValue of type #GDA_VALUE_TYPE_INTEGER with value @val.
 *
 * Returns: the newly created #GdaValue.
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
 * gda_value_new_uinteger
 * @val: value to set for the new #GdaValue.
 *
 * Makes a new #GdaValue of type #GDA_VALUE_TYPE_UINTEGER with value @val.
 *
 * Returns: the newly created #GdaValue.
 */

GdaValue *gda_value_new_uinteger (guint val)
{
	GdaValue *value;
	
	value=g_new0 (GdaValue,1);
	gda_value_set_uinteger (value,val);
	return value;
}


/**
 * gda_value_new_list
 * @val: value to set for the new #GdaValue.
 *
 * Makes a new #GdaValue of type #GDA_VALUE_TYPE_LIST with value @val.
 *
 * Returns: the newly created #GdaValue.
 */
GdaValue *
gda_value_new_list (const GdaValueList *val)
{
	GdaValue *value;

	value = g_new0 (GdaValue, 1);
	gda_value_set_list (value, val);

	return value;
}

/* Register the GdaValueList type in the GType system */
gpointer gda_value_list_copy (gpointer boxed);
void gda_value_list_free (gpointer boxed);

GType
gda_value_list_get_type(void)
{
	static GType gda_value_list_type = 0;
	
	if (gda_value_list_type == 0)
		gda_value_list_type = g_boxed_type_register_static ("GdaValueList",
			(GBoxedCopyFunc) gda_value_list_copy,
			(GBoxedFreeFunc) gda_value_list_free);
	
	return gda_value_list_type;
}

gpointer 
gda_value_list_copy (gpointer boxed)
{
	GList *list = NULL;
	const GList *values;
	
	values = (GList*) boxed;
	
	while (values) {
		list = g_list_append (list, gda_value_copy ((GdaValue *) (values->data)));
		values = values->next;
	}

	return list;
}

void gda_value_list_free (gpointer boxed)
{
	GList *l = (GList*) boxed;
	g_list_free(l);
}

/**
 + gda_value_new_money
 * @val: value to set for the new #GdaValue.
 *
 * Makes a new #GdaValue of type #GDA_VALUE_TYPE_MONEY with value @val.
 *
 * Returns: the newly created #GdaValue.
 */
GdaValue *
gda_value_new_money (const GdaMoney *val)
{
	GdaValue *value;

	value = g_new0 (GdaValue, 1);
	gda_value_set_money (value, val);

	return value;
}

/* Register the GdaMoney type in the GType system */

GType
gda_money_get_type (void)
{
	static GType type = 0;
	
	if (type == 0)
		type = g_boxed_type_register_static ("GdaMoney",
			(GBoxedCopyFunc) gda_money_copy,
			(GBoxedFreeFunc) gda_money_free);
	
	return type;
}

/**
 * gda_money_copy
 * @src: source to get a copy from.
 *
 * Creates a new #GdaMoney structure from an existing one.

 * Returns: a newly allocated #GdaMoney which contains a copy of
 * information in @src.
 */
gpointer
gda_money_copy (gpointer boxed)
{
	GdaMoney *src = (GdaMoney*) boxed;
	GdaMoney *copy = NULL;

	g_return_val_if_fail (src, NULL);

	copy = g_new0 (GdaMoney, 1);
	copy->currency = g_strdup (src->currency);
	copy->amount = src->amount;

	return copy;
}

/**
 * gda_money_free
 * @gda_money: #GdaMoney to free.
 *
 * Deallocates all memory associated to the given #GdaMoney.
 */
void
gda_money_free (gpointer boxed)
{
	GdaMoney *money = (GdaMoney*) money;
	
	g_return_if_fail (money);
		
	g_free (money->currency);
}


/**
 * gda_value_new_numeric
 * @val: value to set for the new #GdaValue.
 *
 * Makes a new #GdaValue of type #GDA_VALUE_TYPE_NUMERIC with value @val.
 *
 * Returns: the newly created #GdaValue.
 */
GdaValue *
gda_value_new_numeric (const GdaNumeric *val)
{
	GdaValue *value;

	value = g_new0 (GdaValue, 1);
	gda_value_set_numeric (value, val);

	return value;
}


/* Register the GdaNumeric type in the GType system */

GType
gda_numeric_get_type (void)
{
	static GType type = 0;
	
	if (type == 0)
		type = g_boxed_type_register_static ("GdaNumeric",
			(GBoxedCopyFunc) gda_numeric_copy,
			(GBoxedFreeFunc) gda_numeric_free);
	
	return type;
}

/**
 * gda_numeric_copy
 * @src: source to get a copy from.
 *
 * Creates a new #GdaNumeric structure from an existing one.

 * Returns: a newly allocated #GdaNumeric which contains a copy of
 * information in @src.
 */

gpointer
gda_numeric_copy (gpointer boxed)
{
	GdaNumeric *src = (GdaNumeric*) boxed;
	GdaNumeric *copy = NULL;

	g_return_val_if_fail (src, NULL);

	copy = g_new0 (GdaNumeric, 1);
	copy->number = g_strdup (src->number);
	copy->precision = src->precision;
	copy->width = src->width;  

	return copy;
}

/**
 * gda_numeric_free
 * @provider_info: provider information to free.
 *
 * Deallocates all memory associated to the given #GdaProviderInfo.
 */
void
gda_numeric_free (gpointer boxed)
{
	GdaNumeric *numeric = (GdaNumeric*) boxed;
	g_return_if_fail (numeric);

	g_free (numeric->number);

	g_free (numeric);
}



/**
 * gda_value_new_single
 * @val: value to set for the new #GdaValue.
 *
 * Makes a new #GdaValue of type #GDA_VALUE_TYPE_SINGLE with value @val.
 *
 * Returns: the newly created #GdaValue.
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
 * Makes a new #GdaValue of type #GDA_VALUE_TYPE_SMALLINT with value @val.
 *
 * Returns: the newly created #GdaValue.
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
 * gda_value_new_smalluint
 * @val: value to set for the new #GdaValue.
 *
 * Makes a new #GdaValue of type #GDA_VALUE_TYPE_SMALLUINT with value @val.
 *
 * Returns: the newly created #GdaValue.
 */
GdaValue *
gda_value_new_smalluint (gushort val)
{
	GdaValue *value;

	value = g_new0 (GdaValue, 1);
	gda_value_set_smalluint (value, val);

	return value;
}

/**
 * gda_value_new_string
 * @val: value to set for the new #GdaValue.
 *
 * Makes a new #GdaValue of type #GDA_VALUE_TYPE_STRING with value @val.
 *
 * Returns: the newly created #GdaValue.
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
 * gda_value_
 * @val: value to set for the new #GdaValue.
 *
 * Makes a new #GdaValue of type #GDA_VALUE_TYPE_TIME with value @val.
 *
 * Returns: the newly created #GdaValue.
 */
GdaValue *
gda_value_new_time (const GdaTime *val)
{
	GdaValue *value;

	value = g_new0 (GdaValue, 1);
	gda_value_set_time (value, val);

	return value;
}

/* Register the GdaTime type in the GType system */

GType
gda_time_get_type(void)
{
	static GType type = 0;
	
	if (type == 0)
		type = g_boxed_type_register_static ("GdaTime",
			(GBoxedCopyFunc) gda_time_copy,
			(GBoxedFreeFunc) gda_time_free);
	
	return type;
}

gpointer
gda_time_copy (gpointer boxed)
{
	
	GdaTime *src = (GdaTime*) boxed;
	GdaTime *copy = NULL;
	
	g_return_val_if_fail(src, NULL);
	
	copy = g_new0(GdaTime, 1);
	copy->hour = src->hour;
	copy->minute = src->minute;
	copy->second = src->second;
	copy->timezone = src->timezone;

	return copy;
}

void gda_time_free (gpointer boxed)
{
	
}


/**
 * gda_value_new_timestamp
 * @val: value to set for the new #GdaValue.
 *
 * Makes a new #GdaValue of type #GDA_VALUE_TYPE_TIMESTAMP with value @val.
 *
 * Returns: the newly created #GdaValue.
 */
GdaValue *
gda_value_new_timestamp (const GdaTimestamp *val)
{
	GdaValue *value;

	value = g_new0 (GdaValue, 1);
	gda_value_set_timestamp (value, val);

	return value;
}



/* Register the GdaTimestamp type in the GType system */


GType
gda_timestamp_get_type (void)
{
	static GType type = 0;
	
	if (type == 0)
		type = g_boxed_type_register_static ("GdaTimestamp",
			(GBoxedCopyFunc) gda_timestamp_copy,
			(GBoxedFreeFunc) gda_timestamp_free);
	
	return type;
}

gpointer 
gda_timestamp_copy (gpointer boxed)
{
	GdaTimestamp *src = (GdaTimestamp*) boxed;
	GdaTimestamp *copy;
	
	g_return_val_if_fail(src, NULL);
	
	copy = g_new0(GdaTimestamp, 1);
	copy->year = src->year;
	copy->month = src->month;
	copy->day = src->day;
	copy->hour = src->hour;
	copy->minute = src->hour;
	copy->second = src->hour;
	copy->fraction = src->fraction;
	copy->timezone = src->timezone;
	
	return copy;
}

void gda_timestamp_free (gpointer boxed)
{
	
}


/**
 * gda_value_new_timestamp_from_timet
 * @val: value to set for the new #GdaValue.
 *
 * Makes a new #GdaValue of type #GDA_VALUE_TYPE_TIMESTAMP with value @val 
 * (of type time_t).
 *
 * Returns: the newly created #GdaValue.
 */
GdaValue *
gda_value_new_timestamp_from_timet (time_t val)
{
	GdaValue *value;
	struct tm *ltm;

	value = g_new0 (GdaValue, 1);
	ltm = localtime ((const time_t *) &val);
	if (ltm) {
		GdaTimestamp tstamp;
		tstamp.year = ltm->tm_year;
		tstamp.month = ltm->tm_mon;
		tstamp.day = ltm->tm_mday;
		tstamp.hour = ltm->tm_hour;
		tstamp.minute = ltm->tm_min;
		tstamp.second = ltm->tm_sec;
		tstamp.fraction = 0;
		tstamp.timezone = 0; /* FIXME */
		gda_value_set_timestamp (value, (const GdaTimestamp *) &tstamp);
	}

	return value;
}

/**
 * gda_value_new_tinyint
 * @val: value to set for the new #GdaValue.
 *
 * Makes a new #GdaValue of type #GDA_VALUE_TYPE_TINYINT with value @val.
 *
 * Returns: the newly created #GdaValue.
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
 * gda_value_new_tinyuint
 * @val: value to set for the new #GdaValue.
 *
 * Makes a new #GdaValue of type #GDA_VALUE_TYPE_TINYUINT with value @val.
 *
 * Returns: the newly created #GdaValue.
 */
GdaValue *
gda_value_new_tinyuint (guchar val)
{
	GdaValue *value;

	value = g_new0 (GdaValue, 1);
	gda_value_set_tinyuint (value, val);

	return value;
}

/** 
 * gda_value_new_gdatype
 * @val: Value to set for the new #GdaValue.
 *
 * Makes a new #GdaValue of type #GDA_VALUE_TYPE_TYPE with value @val.
 *
 * Returns: the newly created #GdaValue.
 */
GdaValue *
gda_value_new_gdatype (GdaValueType val)
{
	GdaValue *value;

	value = g_new0 (GdaValue, 1);
	gda_value_set_gdatype (value, val);

	return value;
}


/**
 * gda_value_new_from_string
 * @as_string: stringified representation of the value.
 * @type: the new value type.
 *
 * Makes a new #GdaValue of type @type from its string representation.
 *
 * Returns: the newly created #GdaValue or %NULL if the string representation
 * cannot be converted to the specified @type.
 */
GdaValue *
gda_value_new_from_string (const gchar *as_string, GdaValueType type)
{
	GdaValue *value;

	g_return_val_if_fail (as_string, NULL);
	value = g_new0 (GdaValue, 1);
	if (!gda_value_set_from_string (value, as_string, type)){
		g_free (value);
		value = NULL;
	}

	return value;
}

/**
 * gda_value_new_from_xml
 * @node: a XML node representing the value.
 *
 * Creates a GdaValue from a XML representation of it. That XML
 * node corresponds to the following string representation:
 *    &lt;value type="gdatype"&gt;value&lt;/value&gt;
 *
 * Returns:  the newly created #GdaValue.
 */
GdaValue *
gda_value_new_from_xml (const xmlNodePtr node)
{
	GdaValue *value;

	g_return_val_if_fail (node, NULL);

	/* parse the XML */
	if (!node || !(node->name) || (node && strcmp (node->name, "value"))) 
		return NULL;

	value = g_new0 (GdaValue, 1);
	if (!gda_value_set_from_string (value,
					xmlNodeGetContent (node),
					gda_type_from_string (xmlGetProp (node, "gdatype")))) {
		g_free (value);
		value = NULL;
	}

	return value;
}

/**
 * gda_value_free
 * @value: the resource to free.
 *
 * Deallocates all memory associated to a #GdaValue.
 */
void
gda_value_free (GdaValue *value)
{
	g_return_if_fail (value);

	l_g_value_unset (value);
	g_free (value);
}

/* gda_value_reset_with_type
 * @value: the #GdaValue to be reseted
 * @type:  the #GdaValueType to set to
 *
 * Resets the #GdaValue and set a new type to #GdaValueType.
*/
void
gda_value_reset_with_type (GdaValue *value, GdaValueType type)
{
	g_return_if_fail (value);

	l_g_value_unset (value);
	if (type == GDA_VALUE_TYPE_NULL || type == GDA_VALUE_TYPE_UNKNOWN)
		return;
	else
		g_value_init (value, gda_value_convert_gdatype_to_gtype (type));
}

/**
 * gda_value_get_type
 * @value: value to get the type from.
 *
 * Retrieves the type of the given value.
 *
 * Returns: the #GType of the value.
 */
GdaValueType
gda_value_get_type (GdaValue *value)
{
	g_return_val_if_fail (value, GDA_VALUE_TYPE_NULL);
	if (! G_IS_VALUE (value))
		return GDA_VALUE_TYPE_NULL;
	
	return gda_value_convert_gtype_to_gdatype (G_VALUE_TYPE (value));
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
	g_return_val_if_fail (value, FALSE);
	return !G_IS_VALUE (value);
}

/**
 * gda_value_is_number
 * @value: a #GdaValue.
 *
 * Gets whether the value stored in the given #GdaValue is of
 * numeric type or not.
 *
 * Returns: %TRUE if a number, %FALSE otherwise.
 */
gboolean
gda_value_is_number (GdaValue *value)
{
	g_return_val_if_fail (value && G_IS_VALUE(value), FALSE);
	GdaValueType type;
	
	type = GDA_VALUE_TYPE(value);
	switch (type) {
	case GDA_VALUE_TYPE_BIGINT:
	case GDA_VALUE_TYPE_DOUBLE:
	case GDA_VALUE_TYPE_INTEGER:
	case GDA_VALUE_TYPE_MONEY:
	case GDA_VALUE_TYPE_NUMERIC:
	case GDA_VALUE_TYPE_SINGLE:
	case GDA_VALUE_TYPE_SMALLINT:
	case GDA_VALUE_TYPE_TINYINT:
	case GDA_VALUE_TYPE_TINYUINT:
		return TRUE;
	    break;
	
	default:	
		return FALSE;
    }
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
gda_value_copy (GdaValue *value)
{
	GdaValue *copy;
	GList *l;

	g_return_val_if_fail (value, NULL);

	copy = g_new0 (GdaValue, 1);

	if (G_IS_VALUE (value)) {
		g_value_init (copy, G_VALUE_TYPE (value));
		g_value_copy (value, copy);
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
	g_return_val_if_fail (value && G_IS_VALUE (value), -1);
	g_return_val_if_fail (gda_value_isa (value, GDA_VALUE_TYPE_BIGINT), -1);
	return g_value_get_int64 (value);
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
	g_return_if_fail (value);

	l_g_value_unset (value);
	g_value_init (value, G_VALUE_TYPE_BIGINT); 
	g_value_set_int64 (value, val);
}

/**
 * gda_value_get_biguint
 * @value: a #GdaValue whose value we want to get.
 *
 * Returns: the value stored in @value.
 */
guint64
gda_value_get_biguint (GdaValue *value)
{
	g_return_val_if_fail (value && G_IS_VALUE (value), -1);
	g_return_val_if_fail (gda_value_isa (value, GDA_VALUE_TYPE_BIGUINT), -1);
	return g_value_get_uint64(value);
}

/**
 * gda_value_set_biguint
 * @value: a #GdaValue that will store @val.
 * @val: value to be stored in @value.
 *
 * Stores @val into @value.
 */
void
gda_value_set_biguint (GdaValue *value, guint64 val)
{
	g_return_if_fail (value);
	
	l_g_value_unset (value);
	g_value_init (value, G_VALUE_TYPE_BIGUINT); 
	g_value_set_uint64 (value, val);
}

/**
 * gda_value_get_binary
 * @value: a #GdaValue whose value we want to get.
 *
 * Returns: the value stored in @value.
 */
G_CONST_RETURN GdaBinary *
gda_value_get_binary (GdaValue *value)
{
	GdaBinary *val;

	g_return_val_if_fail (value && G_IS_VALUE (value), NULL);
	g_return_val_if_fail (gda_value_isa (value, GDA_VALUE_TYPE_BINARY), NULL);

	val = (GdaBinary*) g_value_get_boxed (value);

	return val;
}

/**
 * gda_value_set_binary
 * @value: a #GdaValue that will store @val.
 * @binary: a #GdaBinary structure with the data and its size to be stored in @value.
 *
 * Stores @val into @value.
 */
void
gda_value_set_binary (GdaValue *value, const GdaBinary *binary)
{
	g_return_if_fail (value);
	g_return_if_fail (binary);
	
	l_g_value_unset (value);
	g_value_init (value, G_VALUE_TYPE_BINARY);
	g_value_set_boxed (value, binary);
}

/**
 * gda_value_get_blob
 * @value: a #GdaValue whose value we want to get.
 *
 * Returns: the value stored in @value.
 */
const GdaBlob *
gda_value_get_blob (GdaValue *value)
{
	g_return_val_if_fail (value && G_IS_VALUE (value), NULL);
	g_return_val_if_fail (gda_value_isa (value, GDA_VALUE_TYPE_BLOB), NULL);
	return (const GdaBlob *) g_value_get_object (value);
}

/**
 * gda_value_set_blob
 * @value: a #GdaValue that will store @val.
 * @val: value to be stored in @value.
 *
 * Stores @val into @value.
 */
void
gda_value_set_blob (GdaValue *value, const GdaBlob *val)
{
	g_return_if_fail (value);
	g_return_if_fail (GDA_IS_BLOB (val));
	
	GdaBlob *blob;
	l_g_value_unset (value);
	g_value_init (value, G_VALUE_TYPE_BLOB);
	
	blob = g_object_ref (val);
		
	g_value_set_object (value, blob);
}

/**
 * gda_value_get_boolean
 * @value: a #GdaValue whose value we want to get.
 *
 * Returns: the value stored in @value.
 */
gboolean
gda_value_get_boolean (GdaValue *value)
{
	g_return_val_if_fail (value && G_IS_VALUE (value), FALSE);
	g_return_val_if_fail (gda_value_isa (value, GDA_VALUE_TYPE_BOOLEAN), FALSE);
	return g_value_get_boolean(value);
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
	g_return_if_fail (value);

	l_g_value_unset (value);
	g_value_init (value, G_VALUE_TYPE_BOOLEAN);
	g_value_set_boolean (value, val);
}

/**
 * gda_value_get_date
 * @value: a #GdaValue whose value we want to get.
 *
 * Returns: the value stored in @value.
 */
G_CONST_RETURN GdaDate *
gda_value_get_date (GdaValue *value)
{
	g_return_val_if_fail (value && G_IS_VALUE (value), NULL);
	g_return_val_if_fail (gda_value_isa (value, GDA_VALUE_TYPE_DATE), NULL);
	return (const GdaDate *) g_value_get_boxed (value);
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
	g_return_if_fail (value);
	g_return_if_fail (val);

	l_g_value_unset (value);
	g_value_init (value, G_VALUE_TYPE_DATE);
	g_value_set_boxed (value, val);	
}

/**
 * gda_value_get_double
 * @value: a #GdaValue whose value we want to get.
 *
 * Returns: the value stored in @value.
 */
gdouble
gda_value_get_double (GdaValue *value)
{
	g_return_val_if_fail (value && G_IS_VALUE (value), -1);
	g_return_val_if_fail (gda_value_isa (value, GDA_VALUE_TYPE_DOUBLE), -1);
	return g_value_get_double(value);
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
	g_return_if_fail (value);

	l_g_value_unset (value);
	g_value_init (value, G_VALUE_TYPE_DOUBLE);
	g_value_set_double (value, val);
}

/**
 * gda_value_get_geometric_point
 * @value: a #GdaValue whose value we want to get.
 *
 * Returns: the value stored in @value.
 */
G_CONST_RETURN GdaGeometricPoint *
gda_value_get_geometric_point (GdaValue *value)
{
	g_return_val_if_fail (value && G_IS_VALUE (value), NULL);
	g_return_val_if_fail (gda_value_isa (value, GDA_VALUE_TYPE_GEOMETRIC_POINT), NULL);
	return (const GdaGeometricPoint *) g_value_get_boxed(value);
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
	g_return_if_fail (value);
	g_return_if_fail (val);

	l_g_value_unset (value);	
	g_value_init (value, G_VALUE_TYPE_GEOMETRIC_POINT);
	g_value_set_boxed (value, val);
}

/**
 * gda_value_get_gobject
 * @value: a #GdaValue whose value we want to get.
 *
 * Returns: the value stored in @value.
 */
G_CONST_RETURN GObject *
gda_value_get_gobject (GdaValue *value)
{
	g_return_val_if_fail (value && G_IS_VALUE (value), NULL);
	g_return_val_if_fail (gda_value_isa (value, GDA_VALUE_TYPE_GOBJECT), NULL);
	return (const GObject *) g_value_get_object(value);
}

/**
 * gda_value_set_gobject
 * @value: a #GdaValue that will store @val.
 * @val: value to be stored in @value.
 *
 * Stores @val into @value.
 */
void
gda_value_set_gobject (GdaValue *value, const GObject *val)
{
	g_return_if_fail (value);

	l_g_value_unset (value);
	g_value_init (value, G_VALUE_TYPE_GOBJECT);
	if (G_IS_OBJECT (val))
		g_value_set_object (value, G_OBJECT(val));
}

/**
 * gda_value_get_integer
 * @value: a #GdaValue whose value we want to get.
 *
 * Returns: the value stored in @value.
 */
gint
gda_value_get_integer (GdaValue *value)
{
	g_return_val_if_fail (value && G_IS_VALUE (value), -1);
	g_return_val_if_fail (gda_value_isa (value, GDA_VALUE_TYPE_INTEGER), -1);
	return g_value_get_int(value);
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
	g_return_if_fail (value);

	l_g_value_unset (value);
	g_value_init (value, G_VALUE_TYPE_INTEGER);
	g_value_set_int (value, val);
}

/**
 * gda_value_get_list
 * @value: a #GdaValue whose value we want to get.
 *
 * Returns: the value stored in @value.
 */
G_CONST_RETURN GdaValueList *
gda_value_get_list (GdaValue *value)
{
	g_return_val_if_fail (value && G_IS_VALUE (value), NULL);
	g_return_val_if_fail (gda_value_isa (value, GDA_VALUE_TYPE_LIST), NULL);
	return (const GdaValueList *) g_value_get_boxed(value);
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
	const GList *values;
	g_return_if_fail (value);
	g_return_if_fail (val);

	l_g_value_unset (value);
	g_value_init (value, G_VALUE_TYPE_LIST);
	
	/* See the implementation of GdaValueList as a GBoxed for the Copy function used by GValue*/
	g_value_set_boxed (value, val);
}

/**
 * gda_value_get_money
 * @value: a #GdaValue whose value we want to get.
 *
 * Returns: the value stored in @value.
 */
G_CONST_RETURN GdaMoney *
gda_value_get_money (GdaValue *value)
{
	g_return_val_if_fail (value && G_IS_VALUE (value), NULL);
	g_return_val_if_fail (gda_value_isa (value, GDA_VALUE_TYPE_MONEY), NULL);
	return (const GdaMoney *) g_value_get_boxed(value);
}

/**
 * gda_value_set_money
 * @value: a #GdaValue that will store @val.
 * @val: value to be stored in @value.
 *
 * Stores @val into @value.
 */
void
gda_value_set_money (GdaValue *value, const GdaMoney *val)
{
	g_return_if_fail (value);
	g_return_if_fail (val);

	l_g_value_unset (value);
	g_value_init (value, G_VALUE_TYPE_MONEY);
	g_value_set_boxed (value, val);
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
	g_return_if_fail (value);

	l_g_value_unset (value);
}

/**
 * gda_value_get_numeric
 * @value: a #GdaValue whose value we want to get.
 *
 * Returns: the value stored in @value.
 */
G_CONST_RETURN GdaNumeric *
gda_value_get_numeric (GdaValue *value)
{
	g_return_val_if_fail (value && G_IS_VALUE (value), NULL);
	g_return_val_if_fail (gda_value_isa (value, GDA_VALUE_TYPE_NUMERIC), NULL);
	return (const GdaNumeric *) g_value_get_boxed(value);
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
	g_return_if_fail (value);
	g_return_if_fail (val);

	l_g_value_unset (value);
	g_value_init (value, G_VALUE_TYPE_NUMERIC);
	g_value_set_boxed (value, val);
}

/**
 * gda_value_get_single
 * @value: a #GdaValue whose value we want to get.
 *
 * Returns: the value stored in @value.
 */
gfloat
gda_value_get_single (GdaValue *value)
{
	g_return_val_if_fail (value && G_IS_VALUE (value), -1);
	g_return_val_if_fail (gda_value_isa (value, GDA_VALUE_TYPE_SINGLE), -1);
	return g_value_get_float(value);
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
	g_return_if_fail (value);

	l_g_value_unset (value);
	g_value_init (value, G_VALUE_TYPE_SINGLE);
	g_value_set_float (value, val);
}

/**
 * gda_value_get_smallint
 * @value: a #GdaValue whose value we want to get.
 *
 * Returns: the value stored in @value.
 */
gshort
gda_value_get_smallint (GdaValue *value)
{
	g_return_val_if_fail (value && G_IS_VALUE (value), -1);
	g_return_val_if_fail (gda_value_isa (value, GDA_VALUE_TYPE_SMALLINT), -1);
	return (gshort) value->data[0].v_int;
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
	g_return_if_fail (value);

	l_g_value_unset (value);
	g_value_init (value, G_VALUE_TYPE_SMALLINT);
	value->data[0].v_int = val;
}

/**
 * gda_value_get_smalluint
 * @value: a #GdaValue whose value we want to get.
 *
 * Returns: the value stored in @value.
 */
gushort
gda_value_get_smalluint (GdaValue *value)
{
	g_return_val_if_fail (value && G_IS_VALUE (value), -1);
	g_return_val_if_fail (gda_value_isa (value, GDA_VALUE_TYPE_SMALLUINT), -1);
	return (gushort) value->data[0].v_uint;
}

/**
 * gda_value_set_smalluint
 * @value: a #GdaValue that will store @val.
 * @val: value to be stored in @value.
 *
 * Stores @val into @value.
 */
void
gda_value_set_smalluint (GdaValue *value, gushort val)
{
	g_return_if_fail (value);

	l_g_value_unset (value);
	g_value_init (value, G_VALUE_TYPE_SMALLUINT);
	value->data[0].v_uint = val;
}

/**
 * gda_value_get_string
 * @value: a #GdaValue whose value we want to get.
 *
 * Returns: the value stored in @value.
 */
const gchar *
gda_value_get_string (GdaValue *value)
{
	g_return_val_if_fail (value && G_IS_VALUE (value), NULL);
	g_return_val_if_fail (gda_value_isa (value, GDA_VALUE_TYPE_STRING), NULL);
	return (const gchar *) g_value_get_string(value);
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
	g_return_if_fail (value);

	l_g_value_unset (value);
	g_value_init (value, G_VALUE_TYPE_STRING);
	g_value_set_string (value, val);
}

/**
 * gda_value_get_time
 * @value: a #GdaValue whose value we want to get.
 *
 * Returns: the value stored in @value.
 */
const GdaTime *
gda_value_get_time (GdaValue *value)
{
	g_return_val_if_fail (value && G_IS_VALUE (value), NULL);
	g_return_val_if_fail (gda_value_isa (value, GDA_VALUE_TYPE_TIME), NULL);
	return (const GdaTime *) g_value_get_boxed(value);
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
	g_return_if_fail (value);
	g_return_if_fail (val);

	l_g_value_unset (value);
	g_value_init (value, G_VALUE_TYPE_TIME);
	g_value_set_boxed (value, val);
}

/**
 * gda_value_get_timestamp
 * @value: a #GdaValue whose value we want to get.
 *
 * Returns: the value stored in @value.
 */
const GdaTimestamp *
gda_value_get_timestamp (GdaValue *value)
{
	g_return_val_if_fail (value && G_IS_VALUE (value), NULL);
	g_return_val_if_fail (gda_value_isa (value, GDA_VALUE_TYPE_TIMESTAMP), NULL);
	return (const GdaTimestamp *) g_value_get_boxed(value);
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
	g_return_if_fail (value);
	g_return_if_fail (val);

	l_g_value_unset (value);
	g_value_init (value, G_VALUE_TYPE_TIMESTAMP);
	g_value_set_boxed (value, val);
}

/**
 * gda_value_get_tinyint
 * @value: a #GdaValue whose value we want to get.
 *
 * Returns: the value stored in @value.
 */
gchar
gda_value_get_tinyint (GdaValue *value)
{
	g_return_val_if_fail (value && G_IS_VALUE (value), -1);
	g_return_val_if_fail (gda_value_isa (value, GDA_VALUE_TYPE_TINYINT), -1);
	return g_value_get_char(value);
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
	g_return_if_fail (value);

	l_g_value_unset (value);
	g_value_init (value, G_VALUE_TYPE_TINYINT);
	g_value_set_char (value, val);
}

/**
 * gda_value_get_tinyuint
 * @value: a #GdaValue whose value we want to get.
 *
 * Returns: the value stored in @value.
 */
guchar
gda_value_get_tinyuint (GdaValue *value)
{
	g_return_val_if_fail (value && G_IS_VALUE (value), -1);
	g_return_val_if_fail (gda_value_isa (value, GDA_VALUE_TYPE_TINYUINT), -1);
	return g_value_get_uchar(value);
}

/**
 * gda_value_set_tinyuint
 * @value: a #GdaValue that will store @val.
 * @val: value to be stored in @value.
 *
 * Stores @val into @value.
 */
void
gda_value_set_tinyuint (GdaValue *value, guchar val)
{
	g_return_if_fail (value);

	l_g_value_unset (value);
	g_value_init (value, G_VALUE_TYPE_TINYUINT);
	g_value_set_uchar (value, val);
}

/**
 * gda_value_get_uinteger
 * @value: a #GdaValue whose value we want to get.
 *
 * Returns: the value stored in @value.
 */
guint
gda_value_get_uinteger (GdaValue *value)
{
	g_return_val_if_fail (value && G_IS_VALUE (value), -1);
	g_return_val_if_fail (gda_value_isa (value, GDA_VALUE_TYPE_UINTEGER), -1);
	return g_value_get_uint(value);
}

/**
 * gda_value_set_uinteger
 * @value: a #GdaValue that will store @val.
 * @val: value to be stored in @value.
 *
 * Stores @val into @value.
 */
void
gda_value_set_uinteger (GdaValue *value, guint val)
{
	g_return_if_fail (value);

	l_g_value_unset (value);
	g_value_init (value, G_VALUE_TYPE_UINTEGER);
	g_value_set_uint (value, val);
}


/**
 * gda_value_set_gtype
 * @value: a #GdaValue that will store @type.
 * @type: value to be stored in @value.
 *
 * Stores @type into @value.
 */
void
gda_value_set_gtype (GdaValue *value, GType type)
{
	g_return_if_fail (value);

	l_g_value_unset (value);
	g_value_init (value, type);
}

/**
 * 
 * @value: a #GdaValue that will store @val.
 * @as_string: the stringified representation of the value.
 * @type: the type of the value
 *
 * Stores the value data from its string representation as @type.
 *
 * Returns: %TRUE if the value has been properly converted to @type from
 * its string representation. %FALSE otherwise.
 */
gboolean
gda_value_set_from_string (GdaValue *value, 
			   const gchar *as_string,
			   GdaValueType type)
{
	g_return_val_if_fail (value, FALSE);
	g_return_val_if_fail (as_string, FALSE);

	l_g_value_unset (value);
	g_value_init (value, gda_value_convert_gdatype_to_gtype (type));
	return set_from_string (value, as_string);
}

/**
 * gda_value_set_from_value
 * @value: a #GdaValue.
 * @from: the value to copy from.
 *
 * Sets the value of a #GdaValue from another #GdaValue. This
 * is different from #gda_value_copy, which creates a new #GdaValue.
 * #gda_value_set_from_value, on the other hand, copies the contents
 * of @copy into @value, which must already be allocated.
 *
 * Returns: %TRUE if successful, %FALSE otherwise.
 */
gboolean
gda_value_set_from_value (GdaValue *value, const GdaValue *from)
{
	g_return_val_if_fail (value, FALSE);
	g_return_val_if_fail (from, FALSE);

	if (G_IS_VALUE (from)) {
		g_value_copy (from, value);
		return g_value_type_compatible (G_VALUE_TYPE (from), G_VALUE_TYPE (value));
	}
	else {
		l_g_value_unset (value);
		return TRUE;
	}
}

/**
 * gda_value_stringify
 * @value: a #GdaValue.
 *
 * Converts a GdaValue to its string representation as indicated by this
 * table:
 *
 * Returns: a string formatted according to the printf() style indicated in
 * the preceding table.  Free the value with a g_free() when you've finished
 * using it. 
 */
gchar *
gda_value_stringify (GdaValue *value)
{
	const GdaTime *gdatime;
	const GdaDate *gdadate;
	const GdaMoney *gdamoney;
	const GdaTimestamp *timestamp;
	const GdaGeometricPoint *point;
	const GdaValueList *list;
	const GdaNumeric *numeric;
	const GdaBlob *gdablob;
	GList *l;
	GString *str = NULL;
	gchar *retval = NULL;
	GdaValueType type;

	g_return_val_if_fail (value, NULL);

	switch (GDA_VALUE_TYPE (value)) {
	case GDA_VALUE_TYPE_BIGINT:
		retval = g_strdup_printf ("%lld", gda_value_get_bigint (value));
		break;

	case GDA_VALUE_TYPE_BIGUINT:
                retval = g_strdup_printf ("%llu", gda_value_get_biguint (value));
		break;

	case GDA_VALUE_TYPE_BINARY:
	{
		const GdaBinary *binary = gda_value_get_binary (value);
		retval = g_memdup (binary->data, binary->binary_length+1);
		retval [binary->binary_length] = '\0';
		break;
	}

	case GDA_VALUE_TYPE_BOOLEAN:
		if (gda_value_get_boolean (value))
			retval = g_strdup (_("TRUE"));
		else
			retval = g_strdup (_("FALSE"));
		break;

	case GDA_VALUE_TYPE_STRING:
		if (gda_value_get_string (value))
			retval = g_strdup (gda_value_get_string (value));
		else
			retval = g_strdup ("");
		break;

	case GDA_VALUE_TYPE_INTEGER:
		retval = g_strdup_printf ("%d", gda_value_get_integer (value));
		break;

	case GDA_VALUE_TYPE_UINTEGER:
                retval = g_strdup_printf ("%u", gda_value_get_uinteger (value));
		break;

	case GDA_VALUE_TYPE_SMALLINT:
		retval = g_strdup_printf ("%d", gda_value_get_smallint (value));
		break;

	case GDA_VALUE_TYPE_SMALLUINT:
                retval = g_strdup_printf ("%u", gda_value_get_smalluint (value));
		break;

	case GDA_VALUE_TYPE_TINYINT:
		retval = g_strdup_printf ("%d", gda_value_get_tinyint (value));
		break;

	case GDA_VALUE_TYPE_TINYUINT:
                retval = g_strdup_printf ("%u", gda_value_get_tinyuint (value));
		break;

	case GDA_VALUE_TYPE_SINGLE:
		retval = g_strdup_printf ("%.2f", gda_value_get_single (value));
		break;

	case GDA_VALUE_TYPE_DOUBLE:
		retval = g_strdup_printf ("%.2f", gda_value_get_double (value));
		break;

	case GDA_VALUE_TYPE_TIME:
		gdatime = gda_value_get_time (value);
		retval = g_strdup_printf (gdatime->timezone == TIMEZONE_INVALID ?
					  "%02u:%02u:%02u" : "%02u:%02u:%02u%+03d", 
					  gdatime->hour,
					  gdatime->minute,
					  gdatime->second,
					  (int) gdatime->timezone / 3600);
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
					  (int) timestamp->fraction,
					  (int) timestamp->timezone/3600);
		break;

	case GDA_VALUE_TYPE_GEOMETRIC_POINT:
		point = gda_value_get_geometric_point (value);
		retval = g_strdup_printf ("(%.*g,%.*g)",
					  DBL_DIG,
					  point->x,
					  DBL_DIG,
					  point->y);
		break;

	case GDA_VALUE_TYPE_GOBJECT:
		if (G_IS_OBJECT(gda_value_get_gobject(value))) 
			retval = g_strdup_printf (_("(GObject of type '%s'"),
						  g_type_name (GDA_VALUE_TYPE(value)));
		else
			retval = g_strdup_printf (_("NULL GObject"));
		break;
	
	case GDA_VALUE_TYPE_BLOB:
		gdablob = gda_value_get_blob (value);
		retval = g_strdup_printf ("%s", gda_blob_stringify (gdablob));
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

	case GDA_VALUE_TYPE_MONEY:
		gdamoney = gda_value_get_money(value);
		retval = g_strdup_printf ("%s%f", gdamoney->currency,
					  gdamoney->amount);
		break;

	case GDA_VALUE_TYPE_NUMERIC:
		numeric = gda_value_get_numeric (value);
		retval = g_strdup (numeric->number);
		break;

	case GDA_VALUE_TYPE_NULL:
		retval = g_strdup ("NULL");
		break;

	case  GDA_VALUE_TYPE_TYPE:
		retval = g_strdup (gda_type_to_string (gda_value_get_gdatype (value)));
		break;

	default:
		g_assert_not_reached ();
	}
	
	return retval;
}

/**
 * gda_value_compare
 * @value1: a #GdaValue to compare.
 * @value2: the other #GdaValue to be compared to @value1.
 *
 * Compares two values of the same type.
 *
 * Returns: if both values have the same type, returns 0 if both contain
 * the same value, an integer less than 0 if @value1 is less than @value2 or
 * an integer greater than 0 if @value1 is greater than @value2.
 */
gint
gda_value_compare (GdaValue *value1, GdaValue *value2)
{
	GList *l1, *l2;
	gint retval;
	GdaValueType type;

	g_return_val_if_fail (value1 && value2, -1);
	g_return_val_if_fail (GDA_VALUE_TYPE (value1) == GDA_VALUE_TYPE (value2), -1);

	switch (GDA_VALUE_TYPE (value1)) {
	case GDA_VALUE_TYPE_NULL:
		retval = 0;
		break;

	case GDA_VALUE_TYPE_BIGINT:
		retval = gda_value_get_bigint (value1) - gda_value_get_bigint (value2);
		break;

	case GDA_VALUE_TYPE_BIGUINT:
                retval = gda_value_get_biguint (value1) - gda_value_get_biguint (value2);
		break;

	case GDA_VALUE_TYPE_BINARY:
	{
		GdaBinary *binary1 = gda_value_get_binary (value1);
		GdaBinary *binary2 = gda_value_get_binary (value2);
		if(binary1 && binary2 && (binary1->binary_length == binary2->binary_length))
		    retval = memcmp (binary1->data, binary2->data, binary1->binary_length) ;
		else
			retval = -1;
		break;
	}

	case GDA_VALUE_TYPE_BOOLEAN:
		retval = gda_value_get_boolean (value1) - gda_value_get_boolean (value2);
		break;

	case GDA_VALUE_TYPE_BLOB:
		/* FIXME: Does compare() make sense with BLOBs? */
		retval = 0;
		break;

	case GDA_VALUE_TYPE_DATE:
		retval = memcmp (gda_value_get_date(value1), gda_value_get_date(value2),
				 sizeof (GdaDate));
		break;

	case GDA_VALUE_TYPE_DOUBLE:
	{
		gdouble v1, v2;

		v1 = gda_value_get_double (value1);
		v2 = gda_value_get_double (value2);
		
		if (v1 == v2)
			retval = 0;
		else
			retval = (v1 > v2) ? 1 : -1;
		break;
	}

	case GDA_VALUE_TYPE_GEOMETRIC_POINT:
		retval = memcmp (gda_value_get_geometric_point(value1) , 
				 gda_value_get_geometric_point(value2),
				 sizeof (GdaGeometricPoint));
		break;

	case GDA_VALUE_TYPE_GOBJECT:
		if (gda_value_get_gobject (value1) == gda_value_get_gobject (value2))
			retval = 0;
		else
			retval = -1;
		break;

	case GDA_VALUE_TYPE_INTEGER:
		retval = gda_value_get_integer (value1) - gda_value_get_integer (value2);
		break;

	case GDA_VALUE_TYPE_LIST:
		retval = 0;
		for ( l1 = (GList*) gda_value_get_list(value1), l2 = (GList*) gda_value_get_list(value2); 
		     l1 != NULL && l2 != NULL; l1 = l1->next, l2 = l2->next){
			retval = gda_value_compare ((GdaValue *) l1->data,
						    (GdaValue *) l2->data);
			if (retval != 0) break;
		}
		if (retval == 0 && (l1 == NULL || l2 == NULL) && l1 != l2)
			retval = (l1 == NULL) ? -1 : 1;
		break;

	case GDA_VALUE_TYPE_MONEY:
	{
		GdaMoney *money1, *money2;
		money1 = gda_value_get_money(value1);
		money2 = gda_value_get_money(value2);
		if (!strcmp (money1->currency ? money2->currency : "",
			     money1->currency ? money2->currency : "")) {
			retval = money1->amount == money2->amount ? 0 : 
				(gint) money1->amount  - money2->amount  ;
		} else
			retval = -1; /* FIXME: do currency conversions to compare? */
		break;
	}

	case GDA_VALUE_TYPE_NUMERIC:
	{
		GdaNumeric *num1, *num2;
		num1= gda_value_get_numeric (value1);
		num2 = gda_value_get_numeric (value2);
                if (num1) {
			if (num2)
				retval = strcmp (num1->number, num2->number);
			else
				retval = 1;
		}
		else {
			if (num2->number)
				retval = -1;
			else
				retval = 0;
		}
		break;
	}

	case  GDA_VALUE_TYPE_SINGLE:
		retval = (gint) gda_value_get_single (value1) - gda_value_get_single (value2);
		break;

	case GDA_VALUE_TYPE_SMALLINT:
		retval = gda_value_get_smallint (value1) - gda_value_get_smallint (value2);
		break;

	case GDA_VALUE_TYPE_SMALLUINT:
                retval = gda_value_get_smalluint (value1) - gda_value_get_smalluint (value2);
		break;

	case GDA_VALUE_TYPE_STRING:
	{
		gchar *str1, *str2;
		str1 = gda_value_get_string (value1);
		str2 = gda_value_get_string (value2);
		if (str1 && str2)
			retval = strcmp (str1, str2);
		else {
			if (str1)
				return 1;
			else {
				if (str2)
					return -1;
				else
					return 0;
			}
		}
		break;
	}
	
	case GDA_VALUE_TYPE_TIME:
		retval = memcmp (gda_value_get_time(value1), gda_value_get_time(value2),
				 sizeof (GdaTime));
		break;

	case GDA_VALUE_TYPE_TIMESTAMP:
		retval = memcmp (gda_value_get_timestamp(value1), 
				 gda_value_get_timestamp(value2),
				 sizeof (GdaTimestamp));
		break;

	case GDA_VALUE_TYPE_TINYINT:
		retval = gda_value_get_tinyint (value1) - gda_value_get_tinyint (value2);
		break;

        case GDA_VALUE_TYPE_TINYUINT:
                retval = gda_value_get_tinyuint (value1) - gda_value_get_tinyuint (value2);
		break;

	case GDA_VALUE_TYPE_TYPE:
		retval = GDA_VALUE_TYPE (value1) == GDA_VALUE_TYPE (value2) ? 0 : -1;
		break;

	case GDA_VALUE_TYPE_UINTEGER:
                retval = gda_value_get_uinteger (value1) - gda_value_get_uinteger(value2);
		break;

	default:
		g_error ("GDA Data type %d not handled in %s", GDA_VALUE_TYPE (value1),
			 __FUNCTION__);
	}

	return retval;
}

/**
 * gda_value_compare_ext
 * @value1: a #GdaValue to compare.
 * @value2: the other #GdaValue to be compared to @value1.
 *
 * Like gda_value_compare(), compares two values of the same type, except that NULL values and values
 * of type GDA_VALUE_TYPE_NULL are considered equals
 *
 * Returns: 0 if both contain the same value, an integer less than 0 if @value1 is less than @value2 or
 * an integer greater than 0 if @value1 is greater than @value2.
 */
gint
gda_value_compare_ext (GdaValue *value1, GdaValue *value2)
{
	if (!value1 || (GDA_VALUE_TYPE (value1) == GDA_VALUE_TYPE_NULL)) {
		/* value1 represents a NULL value */
		if (! value2 || (GDA_VALUE_TYPE (value2) == GDA_VALUE_TYPE_NULL))
			return 0;
		else
			return 1;
	}
	else {
		/* value1 does not represents a NULL value */
		if (! value2 || (GDA_VALUE_TYPE (value2) == GDA_VALUE_TYPE_NULL))
			return -1;
		else
			return gda_value_compare (value1, value2);
	}
}

/*
 * to_string
 * 
 * The exact reverse process of set_from_string(), almost the same as gda_value_stingify ()
 * because of some localization with gda_value_stingify ().
 */
static gchar *
to_string (GdaValue *value)
{
	gchar *retval = NULL;

	g_return_val_if_fail (value && G_IS_VALUE (value), NULL);

	switch (GDA_VALUE_TYPE(value)){
	case GDA_VALUE_TYPE_BOOLEAN:
		if (gda_value_get_boolean (value))
			retval = g_strdup ("true");
		else
			retval = g_strdup ("false");
		break;
	default:
		retval = gda_value_stringify (value);
	}
        	
	return retval;
}


/**
 * gda_value_to_xml
 * @value: a #GdaValue.
 *
 * Serializes the given #GdaValue to a XML node string.
 *
 * Returns: the XML node. Once not needed anymore, you should free it.
 */
xmlNodePtr
gda_value_to_xml (GdaValue *value)
{
	gchar *valstr;
	xmlNodePtr retval;

	g_return_val_if_fail (value && G_IS_VALUE (value), NULL);

	valstr = to_string (value);

	retval = xmlNewNode (NULL, "value");
	xmlSetProp (retval, "type", gda_type_to_string (GDA_VALUE_TYPE(value)));
	xmlNodeSetContent (retval, valstr);

	g_free (valstr);

	return retval;
}

GType
gda_smallint_get_type (void) 
{
	static GType type = 0;
	if(type == 0) {
		static const GTypeInfo type_info = {
    			0,			/* class_size */
    			NULL,		/* base_init */
    			NULL,		/* base_finalize */
    			NULL,		/* class_init */
    			NULL,		/* class_finalize */
    			NULL,		/* class_data */
    			0,			/* instance_size */
    			0,			/* n_preallocs */
    			NULL,		/* instance_init */
    			NULL		/* value_table */
  		};
  
  		type = g_type_register_static (G_TYPE_INT, "smallint", &type_info, 0);		

	  }
  	return type;
}

GType
gda_smalluint_get_type (void) {
	static GType type = 0;
	if(type == 0) {
		static const GTypeInfo type_info = {
    			0,			/* class_size */
    			NULL,		/* base_init */
    			NULL,		/* base_finalize */
    			NULL,		/* class_init */
    			NULL,		/* class_finalize */
    			NULL,		/* class_data */
    			0,			/* instance_size */
    			0,			/* n_preallocs */
    			NULL,		/* instance_init */
    			NULL		/* value_table */
  		};
  
  		type = g_type_register_static (G_TYPE_UINT, "smalluint", &type_info, 0);		

  	}
  	return type;
}

GType
gda_gdatype_get_type (void) {
	static GType type = 0;
	if(type == 0) {
		static const GTypeInfo type_info = {
    			0,			/* class_size */
    			NULL,		/* base_init */
    			NULL,		/* base_finalize */
    			NULL,		/* class_init */
    			NULL,		/* class_finalize */
    			NULL,		/* class_data */
    			0,			/* instance_size */
    			0,			/* n_preallocs */
    			NULL,		/* instance_init */
    			NULL		/* value_table */
  		};
  
  		type = g_type_register_static (G_TYPE_INT, "gdatype", &type_info, 0);		

	  }
  	return type;
}

/**
 * gda_value_get_gdatype
 */
GdaValueType
gda_value_get_gdatype (GValue *value)
{
	g_return_val_if_fail (value && G_IS_VALUE (value), -1);
	g_return_val_if_fail (gda_value_isa (value, GDA_VALUE_TYPE_TYPE), -1);
  
	return (GdaValueType) value->data[0].v_int;
}

/**
 * gda_value_set_gdatype
 */
void
gda_value_set_gdatype (GValue *value, GdaValueType val)
{
	g_return_if_fail (value);

	l_g_value_unset (value);
	g_value_init (value, G_VALUE_TYPE_TYPE);
	value->data[0].v_int = (int) val;
}

/**
 * gda_value_convert_gtype_to_gdatype
 * @type: a GType type
 *
 * As a #GdaValue is a #GValue, all the GValue functions returning information on the type of value
 * stored in the #GdaValue will return a GType which can be converted into a #GdaValueType.
 *
 * Converts @type to the corresponding #GdaValueType. This function does the opposite of the
 * gda_value_convert_gdatype_to_gtype() function.
 *
 * Returns: the converted type.
 */
GdaValueType
gda_value_convert_gtype_to_gdatype (GType type)
{
	if (type == G_TYPE_INT64)
		return GDA_VALUE_TYPE_BIGINT;
	
	else if (type == G_TYPE_UINT64)
		return GDA_VALUE_TYPE_BIGUINT;
	
	else if (type == gda_binary_get_type() )
		return GDA_VALUE_TYPE_BINARY;
	
	else if (type == gda_blob_get_type() )
		return GDA_VALUE_TYPE_BLOB;
	
	else if (type == G_TYPE_BOOLEAN)
		return GDA_VALUE_TYPE_BOOLEAN;
	
	else if (type == gda_date_get_type() )
		return GDA_VALUE_TYPE_DATE;
	
	else if (type == G_TYPE_DOUBLE)
		return GDA_VALUE_TYPE_DOUBLE;
	
	else if (type == gda_geometricpoint_get_type() )
		return GDA_VALUE_TYPE_GEOMETRIC_POINT;
	
	else if (type == G_TYPE_OBJECT)
		return GDA_VALUE_TYPE_GOBJECT;
	
	else if (type == G_TYPE_INT)
		return GDA_VALUE_TYPE_INTEGER;
	
	else if (type == G_TYPE_UINT)
		return GDA_VALUE_TYPE_UINTEGER;
	
	else if (type == gda_value_list_get_type() )
		return GDA_VALUE_TYPE_LIST;
	
	else if (type == gda_money_get_type() )
		return GDA_VALUE_TYPE_MONEY;
	
	else if ( type == gda_numeric_get_type() )
		return GDA_VALUE_TYPE_NUMERIC;
	
	else if ( type == G_TYPE_FLOAT)
		return GDA_VALUE_TYPE_SINGLE;
	
	else if ( type == gda_smallint_get_type() )
		return GDA_VALUE_TYPE_SMALLINT;
	
	else if ( type == gda_smalluint_get_type() )
		return GDA_VALUE_TYPE_SMALLUINT;
	
	else if ( type == G_TYPE_STRING)
		return GDA_VALUE_TYPE_STRING;
	
	else if ( type == G_TYPE_CHAR)
		return GDA_VALUE_TYPE_TINYINT;
	
	else if ( type == G_TYPE_UCHAR)
		return GDA_VALUE_TYPE_TINYUINT;
	
	else if ( type == G_TYPE_INVALID)
		return GDA_VALUE_TYPE_UNKNOWN;
	
	else if ( type == gda_time_get_type() )
		return GDA_VALUE_TYPE_TIME;
	
	else if ( type == gda_timestamp_get_type() )
		return GDA_VALUE_TYPE_TIMESTAMP;
	
	else if ( type == gda_gdatype_get_type() )
		return GDA_VALUE_TYPE_TYPE;

	g_warning ("Can't find GdaValueType type for GType `%s'", g_type_name (type));
	return GDA_VALUE_TYPE_UNKNOWN;
}

/**
 * gda_value_convert_gdatype_to_gtype
 * @type: a #GdaValueType
 * 
 * Converts @type to its GType equivalent. This function does the opposite of the
 * gda_value_convert_gtype_to_gdatype() function. See the gda_value_convert_gtype_to_gdatype()
 * function's documentation for more information.
 *
 * Returns: the converted type.
 */
GType
gda_value_convert_gdatype_to_gtype (GdaValueType type)
{
	switch (type) {
	case GDA_VALUE_TYPE_NULL:			
		return G_VALUE_TYPE_NULL;
		
	case GDA_VALUE_TYPE_BIGINT:
		return G_VALUE_TYPE_BIGINT;
		
	case GDA_VALUE_TYPE_BIGUINT:
		return G_VALUE_TYPE_BIGUINT;
		
	case GDA_VALUE_TYPE_BINARY:
		return G_VALUE_TYPE_BINARY;
		
	case GDA_VALUE_TYPE_BLOB:
		return G_VALUE_TYPE_BLOB;
		
	case GDA_VALUE_TYPE_BOOLEAN:
		return G_VALUE_TYPE_BOOLEAN;
		
	case GDA_VALUE_TYPE_DATE:
		return G_VALUE_TYPE_DATE;
		
	case GDA_VALUE_TYPE_DOUBLE:
		return G_VALUE_TYPE_DOUBLE;
		
	case GDA_VALUE_TYPE_GEOMETRIC_POINT:
		return G_VALUE_TYPE_GEOMETRIC_POINT;
		
	case GDA_VALUE_TYPE_GOBJECT:
		return G_VALUE_TYPE_GOBJECT;
		
	case GDA_VALUE_TYPE_INTEGER:
		return G_VALUE_TYPE_INTEGER;
		
	case GDA_VALUE_TYPE_LIST:
		return G_VALUE_TYPE_LIST;
		
	case GDA_VALUE_TYPE_MONEY:
		return G_VALUE_TYPE_MONEY;
		
	case GDA_VALUE_TYPE_NUMERIC:
		return G_VALUE_TYPE_NUMERIC;
		
	case GDA_VALUE_TYPE_SINGLE:
		return G_VALUE_TYPE_SINGLE;
		
	case GDA_VALUE_TYPE_SMALLINT:
		return G_VALUE_TYPE_SMALLINT;
		
	case GDA_VALUE_TYPE_SMALLUINT:
		return G_VALUE_TYPE_SMALLUINT;
		
	case GDA_VALUE_TYPE_STRING:
		return G_VALUE_TYPE_STRING;
		
	case GDA_VALUE_TYPE_TIME:
		return G_VALUE_TYPE_TIME;
		
	case GDA_VALUE_TYPE_TIMESTAMP:
		return G_VALUE_TYPE_TIMESTAMP;
		
	case GDA_VALUE_TYPE_TINYINT:
		return G_VALUE_TYPE_TINYINT;
		
	case GDA_VALUE_TYPE_TINYUINT:
		return G_VALUE_TYPE_TINYUINT;
		
	case GDA_VALUE_TYPE_TYPE:
		return G_VALUE_TYPE_TYPE;
		
        case GDA_VALUE_TYPE_UINTEGER:
		return G_VALUE_TYPE_UINTEGER;
		
	case GDA_VALUE_TYPE_UNKNOWN:
		return G_VALUE_TYPE_UNKNOWN;
	default:
		g_assert_not_reached ();
	}
}
