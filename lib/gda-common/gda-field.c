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
#include "gda-common-private.h"
#include "gda-field.h"

struct _GdaFieldPrivate {
	GNOME_Database_Field *corba_field;
};

static void gda_field_class_init (GdaFieldClass *klass);
static void gda_field_init       (GdaField *field);
static void gda_field_finalize   (GObject *object);

GType
gda_field_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaFieldClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_field_class_init,
			NULL,
			NULL,
			sizeof (GdaField),
			0,
			(GInstanceInitFunc) gda_field_init
		};
		type = g_type_register_static (G_TYPE_OBJECT, "GdaField", &info, 0);
	}
	return type;
}

static void
gda_field_class_init (GdaFieldClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gda_field_finalize;
}

static void
gda_field_init (GdaField *field)
{
	field->priv = g_new0 (GdaFieldPrivate, 1);
	field->priv->corba_field = GNOME_Database_Field__alloc ();
	CORBA_any_set_release (&field->priv->corba_field->value, TRUE);
}

static void
gda_field_finalize (GObject *object)
{
	GObjectClass *parent_class;
	GdaField *field = (GdaField *) object;

	g_return_if_fail (GDA_IS_FIELD (field));

	CORBA_free (field->priv->corba_field);
	g_free (field->priv);
	field->priv = NULL;

	parent_class = g_type_peek_parent_class (G_TYPE_OBJECT);
	if (parent_class && parent_class->finalize)
		parent_class->finalize (object);
}

/**
 * gda_field_new
 */
GdaField *
gda_field_new (void)
{
	GdaField *field;

	field = GDA_FIELD (g_object_new (GDA_TYPE_FIELD, NULL));
	return field;
}

/**
 * gda_field_free
 */
void
gda_field_free (GdaField *field)
{
	g_object_unref (G_OBJECT (field));
}

/**
 * gda_field_get_actual_size
 */
glong
gda_field_get_actual_size (GdaField *field)
{
	g_return_val_if_fail (GDA_IS_FIELD (field), -1);
	return field->priv->corba_field->actualSize;
}

/**
 * gda_field_set_actual_size
 */
void
gda_field_set_actual_size (GdaField *field, glong size)
{
	g_return_if_fail (GDA_IS_FIELD (field));
	field->priv->corba_field->actualSize = size;
}

/**
 * gda_field_get_defined_size
 */
glong
gda_field_get_defined_size (GdaField *field)
{
	g_return_val_if_fail (GDA_IS_FIELD (field), -1);
	return field->priv->corba_field->attributes.definedSize;
}

/**
 * gda_field_set_defined_size
 */
void
gda_field_set_defined_size (GdaField *field, glong size)
{
	g_return_if_fail (GDA_IS_FIELD (field));
	field->priv->corba_field->attributes.definedSize = size;
}

/**
 * gda_field_get_name
 */
const gchar *
gda_field_get_name (GdaField *field)
{
	g_return_val_if_fail (GDA_IS_FIELD (field), NULL);
	return (const gchar *) field->priv->corba_field->attributes.name;
}

/**
 * gda_field_set_name
 */
void
gda_field_set_name (GdaField *field, const gchar *name)
{
	g_return_if_fail (GDA_IS_FIELD (field));
	g_return_if_fail (name != NULL);

	if (field->priv->corba_field->attributes.name != NULL)
		CORBA_free (field->priv->corba_field->attributes.name);

	field->priv->corba_field->attributes.name = CORBA_string_dup (name);
}

/**
 * gda_field_get_scale
 */
glong
gda_field_get_scale (GdaField *field)
{
	g_return_val_if_fail (GDA_IS_FIELD (field), -1);
	return field->priv->corba_field->attributes.scale;
}

/**
 * gda_field_set_scale
 */
void
gda_field_set_scale (GdaField *field, glong scale)
{
	g_return_if_fail (GDA_IS_FIELD (field));
	field->priv->corba_field->attributes.scale = scale;
}

/**
 * gda_field_get_gdatype
 */
GNOME_Database_ValueType
gda_field_get_gdatype (GdaField *field)
{
	g_return_val_if_fail (GDA_IS_FIELD (field), GNOME_Database_TypeNull);
	return field->priv->corba_field->attributes.gdaType;
}

/**
 * gda_field_set_gdatype
 */
void
gda_field_set_gdatype (GdaField *field, GNOME_Database_ValueType type)
{
	g_return_if_fail (GDA_IS_FIELD (field));
	field->priv->corba_field->attributes.gdaType = type;
}

/**
 * gda_field_get_ctype
 */
gint
gda_field_get_ctype (GdaField *field)
{
	g_return_val_if_fail (GDA_IS_FIELD (field), -1);
	return field->priv->corba_field->attributes.cType;
}

/**
 * gda_field_set_ctype
 */
void
gda_field_set_ctype (GdaField *field, gint type)
{
	g_return_if_fail (GDA_IS_FIELD (field));
	field->priv->corba_field->attributes.cType = type;
}

/**
 * gda_field_get_nativetype
 */
gint
gda_field_get_nativetype (GdaField *field)
{
	g_return_val_if_fail (GDA_IS_FIELD (field), -1);
	return field->priv->corba_field->attributes.nativeType;
}

/**
 * gda_field_set_nativetype
 */
void
gda_field_set_nativetype (GdaField *field, gint type)
{
	g_return_if_fail (GDA_IS_FIELD (field));
	field->priv->corba_field->attributes.nativeType = type;
}

/**
 * gda_field_get_bigint_value
 */
long long
gda_field_get_bigint_value (GdaField *field)
{
	g_return_val_if_fail (GDA_IS_FIELD (field), -1);
	return BONOBO_ARG_GET_LONGLONG (field->priv->corba_field->value);
}

/**
 * gda_field_set_bigint_value
 */
void
gda_field_set_bigint_value (GdaField *field, long long value)
{
	g_return_if_fail (GDA_IS_FIELD (field));

	field->priv->corba_field->attributes.gdaType = GNOME_Database_TypeBigint;
	field->priv->corba_field->actualSize = sizeof (long long);

	if (field->priv->corba_field->value._value)
		CORBA_free (field->priv->corba_field->value._value);

	field->priv->corba_field->value._value = ORBit_copy_value (&value, BONOBO_ARG_LONGLONG);
}

/**
 * gda_field_get_binary_value
 */
gconstpointer
gda_field_get_binary_value (GdaField *field)
{
}

/**
 * gda_field_set_binary_value
 */
void
gda_field_set_binary_value (GdaField *field, gconstpointer value, glong size)
{
}

/**
 * gda_field_get_boolean_value
 */
gboolean
gda_field_get_boolean_value (GdaField *field)
{
	g_return_val_if_fail (GDA_IS_FIELD (field), FALSE);
	return BONOBO_ARG_GET_BOOLEAN (&field->priv->corba_field->value);
}

/**
 * gda_field_set_boolean_value
 */
void
gda_field_set_boolean_value (GdaField *field, gboolean value)
{
	g_return_if_fail (GDA_IS_FIELD (field));

	field->priv->corba_field->attributes.gdaType = GNOME_Database_TypeBoolean;
	field->priv->corba_field->actualSize = sizeof (gboolean);

	if (field->priv->corba_field->value._value)
		CORBA_free (field->priv->corba_field->value._value);

	field->priv->corba_field->value._value = ORBit_copy_value (&value, BONOBO_ARG_BOOLEAN);
}

/**
 * gda_field_get_date_value
 */
GDate *
gda_field_get_date_value (GdaField *field)
{
	GNOME_Database_Date *corba_date;

	g_return_val_if_fail (GDA_IS_FIELD (field), NULL);

	corba_date = BONOBO_ARG_GET_GENERAL (&field->priv->corba_field->value,
					     TC_GNOME_Database_Date,
					     GNOME_Database_Date,
					     NULL);
	if (corba_date)
		return g_date_new_dmy (corba_date->day, corba_date->month, corba_date->year);

	return NULL;
}

/**
 * gda_field_set_date_value
 */
void
gda_field_set_date_value (GdaField *field, GDate *date)
{
	GNOME_Database_Date corba_date;

	g_return_if_fail (GDA_IS_FIELD (field));

	if (date) {
		corba_date.year = 0;
		corba_date.month = 0;
		corba_date.day = 0;
	}
	else {
		corba_date.year = g_date_get_year (date);
		corba_date.month = g_date_get_month (date);
		corba_date.day = g_date_get_day (date);
	}

	field->priv->corba_field->attributes.gdaType = GNOME_Database_TypeDate;
	field->priv->corba_field->actualSize = sizeof (GNOME_Database_Date);

	/* copy the value to our internal CORBA_any */
	if (field->priv->corba_field->value._value)
		CORBA_free (field->priv->corba_field->value._value);

	field->priv->corba_field->value._value = ORBit_copy_value (
		&corba_date, TC_GNOME_Database_Date);
}

/**
 * gda_field_get_double_value
 */
gdouble
gda_field_get_double_value (GdaField *field)
{
	g_return_val_if_fail (GDA_IS_FIELD (field), -1);
	return BONOBO_ARG_GET_DOUBLE (&field->priv->corba_field->value);
}

/**
 * gda_field_set_double_value
 */
void
gda_field_set_double_value (GdaField *field, gdouble value)
{
	g_return_if_fail (GDA_IS_FIELD (field));

	field->priv->corba_field->attributes.gdaType = GNOME_Database_TypeDouble;
	field->priv->corba_field->actualSize = sizeof (gdouble);

	if (field->priv->corba_field->value._value)
		CORBA_free (field->priv->corba_field->value._value);

	field->priv->corba_field->value._value = ORBit_copy_value (&value, BONOBO_ARG_DOUBLE);
}

/**
 * gda_field_get_integer_value
 */
gint
gda_field_get_integer_value (GdaField *field)
{
	g_return_val_if_fail (GDA_IS_FIELD (field), -1);
	return BONOBO_ARG_GET_INT (&field->priv->corba_field->value);
}

/**
 * gda_field_set_integer_value
 */
void
gda_field_set_integer_value (GdaField *field, gint value)
{
	g_return_if_fail (GDA_IS_FIELD (field));

	field->priv->corba_field->attributes.gdaType = GNOME_Database_TypeInteger;
	field->priv->corba_field->actualSize = sizeof (gint);

	if (field->priv->corba_field->value._value)
		CORBA_free (field->priv->corba_field->value._value);

	field->priv->corba_field->value._value = ORBit_copy_value (&value, BONOBO_ARG_INT);
}

/**
 * gda_field_get_single_value
 */
gfloat
gda_field_get_single_value (GdaField *field)
{
	g_return_val_if_fail (GDA_IS_FIELD (field), -1);
	return BONOBO_ARG_GET_FLOAT (&field->priv->corba_field->value);
}

/**
 * gda_field_set_single_value
 */
void
gda_field_set_single_value (GdaField *field, gfloat value)
{
	g_return_if_fail (GDA_IS_FIELD (field));

	field->priv->corba_field->attributes.gdaType = GNOME_Database_TypeSingle;
	field->priv->corba_field->actualSize = sizeof (gfloat);

	if (field->priv->corba_field->value._value)
		CORBA_free (field->priv->corba_field->value._value);

	field->priv->corba_field->value._value = ORBit_copy_value (&value, BONOBO_ARG_FLOAT);
}

/**
 * gda_field_get_smallint_value
 */
gshort
gda_field_get_smallint_value (GdaField *field)
{
	g_return_val_if_fail (GDA_IS_FIELD (field), -1);
	return BONOBO_ARG_GET_SHORT (&field->priv->corba_field->value);
}

/**
 * gda_field_set_smallint_value
 */
void
gda_field_set_smallint_value (GdaField *field, gshort value)
{
	g_return_if_fail (GDA_IS_FIELD (field));

	field->priv->corba_field->attributes.gdaType = GNOME_Database_TypeSmallint;
	field->priv->corba_field->actualSize = sizeof (gshort);

	if (field->priv->corba_field->value._value)
		CORBA_free (field->priv->corba_field->value._value);

	field->priv->corba_field->value._value = ORBit_copy_value (&value, BONOBO_ARG_SHORT);
}

/**
 * gda_field_get_string_value
 */
const gchar *
gda_field_get_string_value (GdaField *field)
{
	BonoboArg *arg;

	g_return_val_if_fail (GDA_IS_FIELD (field), NULL);

	arg = &field->priv->corba_field->value;
	return BONOBO_ARG_GET_STRING (arg);
}

/**
 * gda_field_set_string_value
 */
void
gda_field_set_string_value (GdaField *field, const gchar *value)
{
	BonoboArg *arg;

	g_return_if_fail (GDA_IS_FIELD (field));

	field->priv->corba_field->attributes.gdaType = GNOME_Database_TypeVarchar;
	field->priv->corba_field->actualSize = value ? strlen (value) : 0;

	arg = &field->priv->corba_field->value;
	BONOBO_ARG_SET_STRING (arg, value);
}

/**
 * gda_field_get_time_value
 */
GTime
gda_field_get_time_value (GdaField *field)
{
	GNOME_Database_Time *corba_time;

	g_return_val_if_fail (GDA_IS_FIELD (field), -1);

	corba_time = BONOBO_ARG_GET_GENERAL (&field->priv->corba_field->value,
					     TC_GNOME_Database_Time,
					     GNOME_Database_Time,
					     NULL);
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
 * gda_field_set_time_value
 */
void
gda_field_set_time_value (GdaField *field, GTime value)
{
	/* FIXME: implement */
}

/**
 * gda_field_get_timestamp_value
 */
time_t
gda_field_get_timestamp_value (GdaField *field)
{
	GNOME_Database_Timestamp corba_timet;

	g_return_val_if_fail (GDA_IS_FIELD (field), -1);

	corba_timet = BONOBO_ARG_GET_GENERAL (&field->priv->corba_field->value,
					      TC_GNOME_Database_Timestamp,
					      GNOME_Database_Timestamp,
					      NULL);
	if (corba_timet) {
		struct tm stm;

		stm.tm_year = corba_timet->year;
		stm.tm_month = corba_timet->month;
		stm.tm_day = corba_timet->day;
		stm.tm_hour = corba_timet->hour;
		stm.tm_min = corba_timet->minute;
		stm.tm_sec = corba_timet->second;

		return mktime (&stm);
	}

	return -1;
}

/**
 * gda_field_set_timestamp_value
 */
void
gda_field_set_timestamp_value (GdaField *field, time_t value)
{
	struct tm *stm;
	GNOME_Database_Timestamp corba_timet;

	g_return_if_fail (GDA_IS_FIELD (field));

	stm = localtime (value);
	if (!stm)
		return;

	corba_timet.year = stm->tm_year;
	corba_timet.month = stm->tm_month;
	corba_timet.day = stm->tm_day;
	corba_timet.hour = stm->tm_hour;
	corba_timet.minute = stm->tm_min;
	corba_timet.second = stm->tm_sec;
	corba_timet.fraction = 0;

	field->priv->corba_field->attributes.gdaType = GNOME_Database_TypeTimestamp;
	field->priv->corba_field->actualSize = sizeof (GNOME_Database_Timestamp);

	/* copy the value to our internal CORBA_any */
	if (field->priv->corba_field->value._value)
		CORBA_free (field->priv->corba_field->value._value);

	field->priv->corba_field->value._value = ORBit_copy_value (
		&corba_timet, TC_GNOME_Database_Timestamp);
}

/**
 * gda_field_get_tinyint_value
 */
gchar
gda_field_get_tinyint_value (GdaField *field)
{
	g_return_val_if_fail (GDA_IS_FIELD (field), -1);
	return BONOBO_ARG_GET_CHAR (&field->priv->corba_field->value);
}

/**
 * gda_field_set_tinyint_value
 */
void
gda_field_set_tinyint_value (GdaField *field, gchar value)
{
	g_return_if_fail (GDA_IS_FIELD (field));

	field->priv->corba_field->attributes.gdaType = GNOME_Database_TypeTinyint;
	field->priv->corba_field->actualSize = sizeof (gchar);

	if (field->priv->corba_field->value._value)
		CORBA_free (field->priv->corba_field->value._value);

	field->priv->corba_field->value._value = ORBit_copy_value (&value, BONOBO_ARG_CHAR);
}
