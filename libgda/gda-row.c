/* GDA library
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
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

#include <libgda/gda-row.h>

/**
 * gda_row_new
 */
GdaRow *
gda_row_new (gint count)
{
	GdaRow *row;
	gint i;

	g_return_val_if_fail (count >= 0, NULL);

	row = GNOME_Database_Row__alloc ();
	CORBA_sequence_set_release (row, TRUE);
	row->_length = count;
	row->_buffer = CORBA_sequence_GNOME_Database_Field_allocbuf (count);

	for (i = 0; i < count; i++) {
		row->_buffer[i].value._type = CORBA_OBJECT_NIL;
		row->_buffer[i].value._value = NULL;
	}

	return row;
}

/**
 * gda_row_free
 */
void
gda_row_free (GdaRow *row)
{
	g_return_if_fail (row != NULL);
	CORBA_free (row);
}

/**
 * gda_row_get_field
 */
GdaField *
gda_row_get_field (GdaRow *row, gint num)
{
	g_return_val_if_fail (row != NULL, NULL);
	g_return_val_if_fail (num >= 0, NULL);
	g_return_val_if_fail (num < row->_length, NULL);

	return &row->_buffer[num];
}

/**
 * gda_row_attributes_new
 */
GdaRowAttributes *
gda_row_attributes_new (gint count)
{
	GdaRowAttributes *attrs;

	g_return_val_if_fail (count >= 0, NULL);

	attrs = GNOME_Database_RowAttributes__alloc ();
	CORBA_sequence_set_release (attrs, TRUE);
	attrs->_length = count;
	attrs->_buffer = CORBA_sequence_GNOME_Database_FieldAttributes_allocbuf (count);

	return attrs;
}

/**
 * gda_row_attributes_free
 */
void
gda_row_attributes_free (GdaRowAttributes *attrs)
{
	g_return_if_fail (attrs != NULL);
	CORBA_free (attrs);
}

/**
 * gda_row_attributes_get_field
 */
GdaFieldAttributes *
gda_row_attributes_get_field (GdaRowAttributes *attrs, gint num)
{
	g_return_val_if_fail (attrs != NULL, NULL);
	g_return_val_if_fail (num >= 0, NULL);
	g_return_val_if_fail (num < attrs->_length, NULL);

	return &attrs->_buffer[num];
}

/**
 * gda_field_attributes_get_defined_size
 */
glong
gda_field_attributes_get_defined_size (GdaFieldAttributes *fa)
{
	g_return_val_if_fail (fa != NULL, 0);
	return fa->definedSize;
}

/**
 * gda_field_attributes_set_defined_size
 */
void
gda_field_attributes_set_defined_size (GdaFieldAttributes *fa, glong size)
{
	g_return_if_fail (fa != NULL);
	fa->definedSize = size;
}

/**
 * gda_field_attributes_get_name
 */
const gchar *
gda_field_attributes_get_name (GdaFieldAttributes *fa)
{
	g_return_val_if_fail (fa != NULL, NULL);
	return (const gchar *) fa->name;
}

/**
 * gda_field_attributes_set_name
 */
void
gda_field_attributes_set_name (GdaFieldAttributes *fa, const gchar *name)
{
	g_return_if_fail (fa != NULL);
	g_return_if_fail (name != NULL);

	if (fa->name != NULL)
		CORBA_free (fa->name);
	fa->name = CORBA_string_dup (name);
}

/**
 * gda_field_attributes_get_scale
 */
glong
gda_field_attributes_get_scale (GdaFieldAttributes *fa)
{
	g_return_val_if_fail (fa != NULL, 0);
	return fa->scale;
}

/**
 * gda_field_attributes_set_scale
 */
void
gda_field_attributes_set_scale (GdaFieldAttributes *fa, glong scale)
{
	g_return_if_fail (fa != NULL);
	fa->scale = scale;
}

/**
 * gda_field_attributes_get_gdatype
 */
GdaType
gda_field_attributes_get_gdatype (GdaFieldAttributes *fa)
{
	g_return_val_if_fail (fa != NULL, GDA_TYPE_NULL);
	return fa->gdaType;
}

/**
 * gda_field_attributes_set_gdatype
 */
void
gda_field_attributes_set_gdatype (GdaFieldAttributes *fa, GdaType type)
{
	g_return_if_fail (fa != NULL);
	fa->gdaType = type;
}

/**
 * gda_field_get_actual_size
 */
glong
gda_field_get_actual_size (GdaField *field)
{
	g_return_val_if_fail (field != NULL, -1);
	return field->actualSize;
}

/**
 * gda_field_set_actual_size
 */
void
gda_field_set_actual_size (GdaField *field, glong size)
{
	g_return_if_fail (field != NULL);
	field->actualSize = size;
}

/**
 * gda_field_get_defined_size
 */
glong
gda_field_get_defined_size (GdaField *field)
{
	g_return_val_if_fail (field != NULL, -1);
	return field->attributes.definedSize;
}

/**
 * gda_field_set_defined_size
 */
void
gda_field_set_defined_size (GdaField *field, glong size)
{
	g_return_if_fail (field != NULL);
	field->attributes.definedSize = size;
}

/**
 * gda_field_get_name
 */
const gchar *
gda_field_get_name (GdaField *field)
{
	g_return_val_if_fail (field != NULL, NULL);
	return (const gchar *) field->attributes.name;
}

/**
 * gda_field_set_name
 */
void
gda_field_set_name (GdaField *field, const gchar *name)
{
	g_return_if_fail (field != NULL);
	g_return_if_fail (name != NULL);

	if (field->attributes.name != NULL)
		CORBA_free (field->attributes.name);

	field->attributes.name = CORBA_string_dup (name);
}

/**
 * gda_field_get_scale
 */
glong
gda_field_get_scale (GdaField *field)
{
	g_return_val_if_fail (field != NULL, -1);
	return field->attributes.scale;
}

/**
 * gda_field_set_scale
 */
void
gda_field_set_scale (GdaField *field, glong scale)
{
	g_return_if_fail (field != NULL);
	field->attributes.scale = scale;
}

/**
 * gda_field_get_gdatype
 */
GdaType
gda_field_get_gdatype (GdaField *field)
{
	g_return_val_if_fail (field != NULL, GNOME_Database_TYPE_NULL);
	return field->attributes.gdaType;
}

/**
 * gda_field_set_gdatype
 */
void
gda_field_set_gdatype (GdaField *field, GdaType type)
{
	g_return_if_fail (field != NULL);
	field->attributes.gdaType = type;
}

/**
 * gda_field_is_null
 */
gboolean
gda_field_is_null (GdaField *field)
{
	g_return_val_if_fail (field != NULL, TRUE);
	return gda_value_is_null (&field->value);
}

/**
 * gda_field_get_value
 */
GdaValue *
gda_field_get_value (GdaField *field)
{
	g_return_val_if_fail (field != NULL, NULL);
	return &field->value;
}

/**
 * gda_field_set_value
 */
void
gda_field_set_value (GdaField *field, const GdaValue *value)
{
	g_return_if_fail (field != NULL);
	g_return_if_fail (value != NULL);

	if (field->value._value)
		CORBA_free (field->value._value);

	field->value._type = value->_type;
	field->value._release = value->_release;
	field->value._value = ORBit_copy_value (value->_value, value->_type);
}

/**
 * gda_field_get_bigint_value
 */
long long
gda_field_get_bigint_value (GdaField *field)
{
	g_return_val_if_fail (field != NULL, -1);
	return gda_value_get_bigint (&field->value);
}

/**
 * gda_field_set_bigint_value
 */
void
gda_field_set_bigint_value (GdaField *field, long long value)
{
	g_return_if_fail (field != NULL);

	field->attributes.gdaType = GDA_TYPE_BIGINT;
	field->actualSize = sizeof (long long);

	gda_value_set_bigint (&field->value, value);
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
	g_return_val_if_fail (field != NULL, FALSE);
	return gda_value_get_boolean (&field->value);
}

/**
 * gda_field_set_boolean_value
 */
void
gda_field_set_boolean_value (GdaField *field, gboolean value)
{
	g_return_if_fail (field != NULL);

	field->attributes.gdaType = GDA_TYPE_BOOLEAN;
	field->actualSize = sizeof (gboolean);

	gda_value_set_boolean (&field->value, value);
}

/**
 * gda_field_get_date_value
 */
const GdaDate *
gda_field_get_date_value (GdaField *field)
{
	g_return_val_if_fail (field != NULL, NULL);

	return gda_value_get_date (&field->value);
}

/**
 * gda_field_set_date_value
 */
void
gda_field_set_date_value (GdaField *field, GdaDate *date)
{
	g_return_if_fail (field != NULL);
	g_return_if_fail (date != NULL);

	field->attributes.gdaType = GDA_TYPE_DATE;
	field->actualSize = sizeof (GdaDate);

	gda_value_set_date (&field->value, date);
}

/**
 * gda_field_get_double_value
 */
gdouble
gda_field_get_double_value (GdaField *field)
{
	g_return_val_if_fail (field != NULL, -1);
	return gda_value_get_double (&field->value);
}

/**
 * gda_field_set_double_value
 */
void
gda_field_set_double_value (GdaField *field, gdouble value)
{
	g_return_if_fail (field != NULL);

	field->attributes.gdaType = GDA_TYPE_DOUBLE;
	field->actualSize = sizeof (gdouble);

	gda_value_set_double (&field->value, value);
}

/**
 * gda_field_get_geometric_point_value
 */
const GdaGeometricPoint *
gda_field_get_geometric_point_value (GdaField *field)
{
	g_return_val_if_fail (field != NULL, NULL);
	return gda_value_get_geometric_point (&field->value);
}

/**
 * gda_field_set_geometric_point_value
 */
void
gda_field_set_geometric_point_value (GdaField *field, GdaGeometricPoint *value)
{
	g_return_if_fail (field != NULL);
	g_return_if_fail (value != NULL);

	field->attributes.gdaType = GDA_TYPE_GEOMETRIC_POINT;
	field->actualSize = sizeof (GdaGeometricPoint);

	gda_value_set_geometric_point (&field->value, value);
}

/**
 * gda_field_get_integer_value
 */
gint
gda_field_get_integer_value (GdaField *field)
{
	g_return_val_if_fail (field != NULL, -1);
	return gda_value_get_integer (&field->value);
}

/**
 * gda_field_set_integer_value
 */
void
gda_field_set_integer_value (GdaField *field, gint value)
{
	g_return_if_fail (field != NULL);

	field->attributes.gdaType = GDA_TYPE_INTEGER;
	field->actualSize = sizeof (gint);

	gda_value_set_integer (&field->value, value);
}

/**
 * gda_field_set_null_value
 */
void
gda_field_set_null_value (GdaField *field)
{
	g_return_if_fail (field != NULL);

	field->actualSize = 0;
	gda_value_set_null (&field->value);
}

/**
 * gda_field_get_single_value
 */
gfloat
gda_field_get_single_value (GdaField *field)
{
	g_return_val_if_fail (field != NULL, -1);
	return gda_value_get_single (&field->value);
}

/**
 * gda_field_set_single_value
 */
void
gda_field_set_single_value (GdaField *field, gfloat value)
{
	g_return_if_fail (field != NULL);

	field->attributes.gdaType = GDA_TYPE_SINGLE;
	field->actualSize = sizeof (gfloat);

	gda_value_set_single (&field->value, value);
}

/**
 * gda_field_get_smallint_value
 */
gshort
gda_field_get_smallint_value (GdaField *field)
{
	g_return_val_if_fail (field != NULL, -1);
	return gda_value_get_smallint (&field->value);
}

/**
 * gda_field_set_smallint_value
 */
void
gda_field_set_smallint_value (GdaField *field, gshort value)
{
	g_return_if_fail (field != NULL);

	field->attributes.gdaType = GDA_TYPE_SMALLINT;
	field->actualSize = sizeof (gshort);

	gda_value_set_smallint (&field->value, value);
}

/**
 * gda_field_get_string_value
 */
const gchar *
gda_field_get_string_value (GdaField *field)
{
	g_return_val_if_fail (field != NULL, NULL);
	return gda_value_get_string (&field->value);
}

/**
 * gda_field_set_string_value
 */
void
gda_field_set_string_value (GdaField *field, const gchar *value)
{
	g_return_if_fail (field != NULL);

	field->attributes.gdaType = GDA_TYPE_STRING;
	field->actualSize = value ? strlen (value) : 0;

	gda_value_set_string (&field->value, value);
}

/**
 * gda_field_get_time_value
 */
const GdaTime *
gda_field_get_time_value (GdaField *field)
{
	g_return_val_if_fail (field != NULL, NULL);

	return gda_value_get_time (&field->value);
}

/**
 * gda_field_set_time_value
 */
void
gda_field_set_time_value (GdaField *field, GdaTime *value)
{
	g_return_if_fail (field != NULL);
	g_return_if_fail (value != NULL);
	
	field->attributes.gdaType = GDA_TYPE_TIME;
	field->actualSize = sizeof (GdaTime);

	gda_value_set_time (&field->value, value);
}

/**
 * gda_field_get_timestamp_value
 */
const GdaTimestamp *
gda_field_get_timestamp_value (GdaField *field)
{
	g_return_val_if_fail (field != NULL, NULL);

	return gda_value_get_timestamp (&field->value);
}

/**
 * gda_field_set_timestamp_value
 */
void
gda_field_set_timestamp_value (GdaField *field, GdaTimestamp *value)
{
	g_return_if_fail (field != NULL);

	field->attributes.gdaType = GDA_TYPE_TIMESTAMP;
	field->actualSize = sizeof (GdaTimestamp);

	gda_value_set_timestamp (&field->value, value);
}

/**
 * gda_field_get_tinyint_value
 */
gchar
gda_field_get_tinyint_value (GdaField *field)
{
	g_return_val_if_fail (field != NULL, -1);
	return gda_value_get_tinyint (&field->value);
}

/**
 * gda_field_set_tinyint_value
 */
void
gda_field_set_tinyint_value (GdaField *field, gchar value)
{
	g_return_if_fail (field != NULL);

	field->attributes.gdaType = GDA_TYPE_TINYINT;
	field->actualSize = sizeof (gchar);

	gda_value_set_tinyint (&field->value, value);
}

/**
 * gda_field_stringify
 */
gchar *
gda_field_stringify (const GdaField *field)
{
	g_return_val_if_fail (field != NULL, NULL);
	return gda_value_stringify (&field->value);
}
