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

#include <glib/gmessages.h>
#include <glib/gstrfuncs.h>
#include <libgda/gda-row.h>

/**
 * gda_row_new
 * @count: number of #GdaField in the new #GdaRow.
 *
 * Creates a #GdaRow which can hold @count #GdaField.
 *
 * Returns: the newly allocated #GdaRow.
 */
GdaRow *
gda_row_new (gint count)
{
	GdaRow *row = NULL;
	gint i;

	g_return_val_if_fail (count >= 0, NULL);

	for (i = 0; i < count; i++) {
		GdaField *field = gda_field_new ();
		row = g_list_append (row, field);
	}

	return row;
}

/**
 * gda_row_free
 * @row: the resource to free.
 *
 * Deallocates all memory associated to a #GdaRow.
 */
void
gda_row_free (GdaRow *row)
{
	g_return_if_fail (row != NULL);

	g_list_foreach (row, (GFunc) gda_field_free, NULL);
	g_list_free (row);
}

/**
 * gda_row_get_field
 * @row: a #GdaRow (which contains #GdaField).
 * @num: field index.
 *
 * Gets a pointer to a #GdaField stored in a #GdaRow.
 * 
 * Returns: a pointer to the #GdaField in the position @num of @row.
 */
GdaField *
gda_row_get_field (GdaRow *row, gint num)
{
	GList *l;

	g_return_val_if_fail (row != NULL, NULL);
	g_return_val_if_fail (num >= 0, NULL);

	l = g_list_nth (row, num);
	return l ? (GdaField *) l->data : NULL;
}

/**
 * gda_row_attributes_new
 * @count: number of #GdaRowAttributes to allocate.
 *
 * Creates a new #GdaRowAttributes which can hold up to @count attributes.
 *
 * Returns: a pointer to the newly allocated row attributes.
 */
GdaRowAttributes *
gda_row_attributes_new (gint count)
{
	GdaRowAttributes *attrs = NULL;
	gint i;

	g_return_val_if_fail (count >= 0, NULL);

	for (i = 0; i < count; i++) {
		GdaFieldAttributes *fa;

		fa = gda_field_attributes_new ();
		attrs = g_list_append (attrs, fa);
	}

	return attrs;
}

/**
 * gda_row_attributes_free
 * @attrs: a #GdaRowAttributes.
 *
 * Deallocates the memory associated with @attrs.
 */
void
gda_row_attributes_free (GdaRowAttributes *attrs)
{
	g_return_if_fail (attrs != NULL);

	g_list_foreach (attrs, (GFunc) gda_field_attributes_free, NULL);
	g_list_free (attrs);
}

/**
 * gda_row_attributes_get_length
 */
gint
gda_row_attributes_get_length (GdaRowAttributes *attrs)
{
	g_return_val_if_fail (attrs != NULL, -1);
	return g_list_length (attrs);
}

/**
 * gda_row_attributes_get_field
 * @attrs: a #GdaRowAttributes.
 * @num: an index into @attrs.
 *
 * Returns: the @num'th @GdafieldAttributes in @attrs.
 */
GdaFieldAttributes *
gda_row_attributes_get_field (GdaRowAttributes *attrs, gint num)
{
	GList *l;

	g_return_val_if_fail (attrs != NULL, NULL);
	g_return_val_if_fail (num >= 0, NULL);

	l = g_list_nth (attrs, num);
	return l ? (GdaFieldAttributes *) l->data : NULL;
}

/**
 * gda_field_attributes_new
 */
GdaFieldAttributes *
gda_field_attributes_new (void)
{
	GdaFieldAttributes *fa;

	fa = g_new0 (GdaFieldAttributes, 1);
	fa->defined_size = 0;
	fa->name = NULL;
	fa->scale = 0;
	fa->gda_type = GDA_VALUE_TYPE_NULL;
	fa->allow_null = TRUE;
	fa->primary_key = FALSE;
	fa->unique_key = FALSE;
	fa->references = NULL;
	fa->auto_increment = FALSE;
	fa->auto_increment_start = 0;
	fa->auto_increment_step = 0;

	return fa;
}

/**
 * gda_field_attributes_free
 */
void
gda_field_attributes_free (GdaFieldAttributes *fa)
{
	g_return_if_fail (fa != NULL);

	g_free (fa->name);
	g_free (fa->references);
	g_free (fa);
}

/**
 * gda_field_attributes_get_defined_size
 * @fa: a @GdaRowAttributes
 *
 * Returns: the defined size of @fa.
 */
glong
gda_field_attributes_get_defined_size (GdaFieldAttributes *fa)
{
	g_return_val_if_fail (fa != NULL, 0);
	return fa->defined_size;
}

/**
 * gda_field_attributes_set_defined_size
 * @fa: a #GdaFieldAttributes.
 * @size: the defined size we want to set.
 *
 * Sets the defined size of a #GdaFieldAttributes.
 */
void
gda_field_attributes_set_defined_size (GdaFieldAttributes *fa, glong size)
{
	g_return_if_fail (fa != NULL);
	fa->defined_size = size;
}

/**
 * gda_field_attributes_get_name
 * @fa: a #GdaFieldAttributes.
 *
 * Returns: the name of @fa.
 */
const gchar *
gda_field_attributes_get_name (GdaFieldAttributes *fa)
{
	g_return_val_if_fail (fa != NULL, NULL);
	return (const gchar *) fa->name;
}

/**
 * gda_field_attributes_set_name
 * @fa: a #GdaFieldAttributes.
 * @name: the new name of @fa.
 *
 * Sets the name of @fa to @name.
 */
void
gda_field_attributes_set_name (GdaFieldAttributes *fa, const gchar *name)
{
	g_return_if_fail (fa != NULL);
	g_return_if_fail (name != NULL);

	if (fa->name != NULL)
		g_free (fa->name);
	fa->name = g_strdup (name);
}

/**
 * gda_field_attributes_get_scale
 * @fa: a #GdaFieldAttributes.
 *
 * Returns: the number of decimals of @fa.
 */
glong
gda_field_attributes_get_scale (GdaFieldAttributes *fa)
{
	g_return_val_if_fail (fa != NULL, 0);
	return fa->scale;
}

/**
 * gda_field_attributes_set_scale
 * @fa: a #GdaFieldAttributes.
 * @scale: number of decimals.
 *
 * Sets the scale of @fa to @scale.
 */
void
gda_field_attributes_set_scale (GdaFieldAttributes *fa, glong scale)
{
	g_return_if_fail (fa != NULL);
	fa->scale = scale;
}

/**
 * gda_field_attributes_get_gdatype
 * @fa: a #GdaFieldAttributes.
 *
 * Returns: the type of @fa.
 */
GdaValueType
gda_field_attributes_get_gdatype (GdaFieldAttributes *fa)
{
	g_return_val_if_fail (fa != NULL, GDA_VALUE_TYPE_NULL);
	return fa->gda_type;
}

/**
 * gda_field_attributes_set_gdatype
 * @fa: a #GdaFieldAttributes.
 * @type: the new type of @fa.
 *
 * Sets the type of @fa to @type.
 */
void
gda_field_attributes_set_gdatype (GdaFieldAttributes *fa, GdaValueType type)
{
	g_return_if_fail (fa != NULL);
	fa->gda_type = type;
}

/**
 * gda_field_attributes_get_allow_null
 * @fa: a #GdaFieldAttributes.
 *
 * Get the 'allow null' flag of the given field attributes.
 *
 * Returns: whether the given field allows null values or not.
 */
gboolean
gda_field_attributes_get_allow_null (GdaFieldAttributes *fa)
{
	g_return_val_if_fail (fa != NULL, FALSE);
	return fa->allow_null;
}

/**
 * gda_field_attributes_set_allow_null
 * @fa: a #GdaFieldAttributes.
 * @allow:
 *
 * Sets the 'allow null' flag of the given field attributes.
 */
void
gda_field_attributes_set_allow_null (GdaFieldAttributes *fa, gboolean allow)
{
	g_return_if_fail (fa != NULL);
	fa->allow_null = allow;
}

/**
 * gda_field_attributes_get_primary_key
 * @fa: a #GdaFieldAttributes.
 */
gboolean
gda_field_attributes_get_primary_key (GdaFieldAttributes *fa)
{
	g_return_val_if_fail (fa != NULL, FALSE);
	return fa->primary_key;
}

/**
 * gda_field_attributes_set_primary_key
 */
void
gda_field_attributes_set_primary_key (GdaFieldAttributes *fa, gboolean pk)
{
	g_return_if_fail (fa != NULL);
	fa->primary_key = pk;
}

/**
 * gda_field_attributes_get_unique_key
 */
gboolean
gda_field_attributes_get_unique_key (GdaFieldAttributes *fa)
{
	g_return_val_if_fail (fa != NULL, FALSE);
	return fa->unique_key;
}

/**
 * gda_field_attributes_set_unique_key
 */
void
gda_field_attributes_set_unique_key (GdaFieldAttributes *fa, gboolean uk)
{
	g_return_if_fail (fa != NULL);
	fa->unique_key = uk;
}

/**
 * gda_field_attributes_get_references
 */
const gchar *
gda_field_attributes_get_references (GdaFieldAttributes *fa)
{
	g_return_val_if_fail (fa != NULL, NULL);
	return (const gchar *) fa->references;
}

/**
 * gda_field_attributes_set_references
 */
void
gda_field_attributes_set_references (GdaFieldAttributes *fa, const gchar *ref)
{
	g_return_if_fail (fa != NULL);

	if (fa->references != NULL)
		g_free (fa->references);

	if (ref)
		fa->references = g_strdup (ref);
}

/**
 * gda_field_new
 */
GdaField *
gda_field_new (void)
{
	GdaField *field;

	field = g_new0 (GdaField, 1);
	field->actual_size = 0;
	field->value = gda_value_new_null ();
	field->attributes = gda_field_attributes_new ();

	return field;
}

/**
 * gda_field_free
 */
void
gda_field_free (GdaField *field)
{
	g_return_if_fail (field != NULL);

	gda_value_free (field->value);
	gda_field_attributes_free (field->attributes);
	g_free (field);
}

/**
 * gda_field_get_actual_size
 * @field: a #GdaField
 *
 * Returns: the actual size of @field.
 */
glong
gda_field_get_actual_size (GdaField *field)
{
	g_return_val_if_fail (field != NULL, -1);
	return field->actual_size;
}

/**
 * gda_field_set_actual_size
 * @field: a #GdaField
 * @size: the new actual size of @field.
 *
 * Sets the actual size of @field to @size.
 */
void
gda_field_set_actual_size (GdaField *field, glong size)
{
	g_return_if_fail (field != NULL);
	field->actual_size = size;
}

/**
 * gda_field_get_defined_size
 * @field: a #GdaField
 *
 * Returns: the defined size of @field.
 */
glong
gda_field_get_defined_size (GdaField *field)
{
	g_return_val_if_fail (field != NULL, -1);
	return gda_field_attributes_get_defined_size (field->attributes);
}

/**
 * gda_field_set_defined_size
 * @field: a #GdaField.
 * @size: the new defined size of @field.
 *
 * Sets the defined size of @field to @size.
 */
void
gda_field_set_defined_size (GdaField *field, glong size)
{
	g_return_if_fail (field != NULL);
	gda_field_attributes_set_defined_size (field->attributes, size);
}

/**
 * gda_field_get_name
 * @field: a #GdaField
 *
 * Returns: the name of @field.
 */
const gchar *
gda_field_get_name (GdaField *field)
{
	g_return_val_if_fail (field != NULL, NULL);
	return gda_field_attributes_get_name (field->attributes);
}

/**
 * gda_field_set_name
 * @field: a #GdaField.
 * @name: the new name of @field.
 *
 * Sets the name of @field to @name.
 */
void
gda_field_set_name (GdaField *field, const gchar *name)
{
	g_return_if_fail (field != NULL);
	g_return_if_fail (name != NULL);

	gda_field_attributes_set_name (field->attributes, name);
}

/**
 * gda_field_get_scale
 * @field: a #GdaField
 *
 * Returns: the scale (number of decimals) of @field.
 */
glong
gda_field_get_scale (GdaField *field)
{
	g_return_val_if_fail (field != NULL, -1);
	return gda_field_attributes_get_scale (field->attributes);
}

/**
 * gda_field_set_scale
 * @field: a #GdaField.
 * @scale: the new scale of @field.
 *
 * Sets the scale of @field to @scale.
 */
void
gda_field_set_scale (GdaField *field, glong scale)
{
	g_return_if_fail (field != NULL);
	gda_field_attributes_set_scale (field->attributes, scale);
}

/**
 * gda_field_get_gdatype
 * @field: a #GdaField
 *
 * Returns: the type of @field.
 */
GdaValueType
gda_field_get_gdatype (GdaField *field)
{
	g_return_val_if_fail (field != NULL, GDA_VALUE_TYPE_NULL);
	return gda_field_attributes_get_gdatype (field->attributes);
}

/**
 * gda_field_set_gdatype
 * @field: a #GdaField.
 * @type: the new type of @field.
 *
 * Sets the type of @field to @type.
 */
void
gda_field_set_gdatype (GdaField *field, GdaValueType type)
{
	g_return_if_fail (field != NULL);
	gda_field_attributes_set_gdatype (field->attributes, type);
}

/**
 * gda_field_get_allow_null
 * @field: a #GdaField.
 */
gboolean
gda_field_get_allow_null (GdaField *field)
{
	g_return_val_if_fail (field != NULL, FALSE);
	return gda_field_attributes_get_allow_null (field->attributes);
}

/**
 * gda_field_set_allow_null
 * @field: a #GdaField.
 * @allow:
 */
void
gda_field_set_allow_null (GdaField *field, gboolean allow)
{
	g_return_if_fail (field != NULL);
	gda_field_attributes_set_allow_null (field->attributes, allow);
}

/**
 * gda_field_get_primary_key
 */
gboolean
gda_field_get_primary_key (GdaField *field)
{
	g_return_val_if_fail (field != NULL, FALSE);
	return gda_field_attributes_get_primary_key (field->attributes);
}

/**
 * gda_field_set_primary_key
 */
void
gda_field_set_primary_key (GdaField *field, gboolean pk)
{
	g_return_if_fail (field != NULL);
	gda_field_attributes_set_primary_key (field->attributes, pk);
}

/**
 * gda_field_get_unique_key
 */
gboolean
gda_field_get_unique_key (GdaField *field)
{
	g_return_val_if_fail (field != NULL, FALSE);
	return gda_field_attributes_get_unique_key (field->attributes);
}

/**
 * gda_field_set_unique_key
 */
void
gda_field_set_unique_key (GdaField *field, gboolean uk)
{
	g_return_if_fail (field != NULL);
	gda_field_attributes_set_unique_key (field->attributes, uk);
}

/**
 * gda_field_get_references
 */
const gchar *
gda_field_get_references (GdaField *field)
{
	g_return_val_if_fail (field != NULL, NULL);
	return gda_field_attributes_get_references (field->attributes);
}

/**
 * gda_field_set_references
 */
void
gda_field_set_references (GdaField *field, const gchar *ref)
{
	g_return_if_fail (field != NULL);
	gda_field_attributes_set_references (field->attributes, ref);
}

/**
 * gda_field_is_null
 * @field: a #GdaField
 *
 * Returns: a gboolean indicating whether or not the value of @field is of
 * type GDA_TYPE_VALUE_NULL.
 */
gboolean
gda_field_is_null (GdaField *field)
{
	g_return_val_if_fail (field != NULL, TRUE);
	return gda_value_is_null (field->value);
}

/**
 * gda_field_get_value
 * @field: a #GdaField
 *
 * Returns: the #GdaValue stored in @field.
 */
GdaValue *
gda_field_get_value (GdaField *field)
{
	g_return_val_if_fail (field != NULL, NULL);
	return field->value;
}

/**
 * gda_field_set_value
 * @field: a #GdaField.
 * @value: the #GdaValue to store in @field.
 *
 * Sets the value of @field to @value.
 */
void
gda_field_set_value (GdaField *field, const GdaValue *value)
{
	g_return_if_fail (field != NULL);
	g_return_if_fail (value != NULL);

	if (field->value)
		gda_value_free (field->value);
	field->value = gda_value_copy (value);
}

/**
 * gda_field_get_bigint_value
 * @field: a #GdaField
 *
 * Returns: the gint64 value stored in @field.
 */
gint64
gda_field_get_bigint_value (GdaField *field)
{
	g_return_val_if_fail (field != NULL, -1);
	return gda_value_get_bigint (field->value);
}

/**
 * gda_field_set_bigint_value
 * @field: a #GdaField.
 * @value: the gint64 value to store in @field.
 *
 * Sets the value of @field to @value.
 */
void
gda_field_set_bigint_value (GdaField *field, gint64 value)
{
	g_return_if_fail (field != NULL);

	gda_field_set_gdatype (field, GDA_VALUE_TYPE_BIGINT);
	field->actual_size = sizeof (gint64);
	gda_value_set_bigint (field->value, value);
}

/**
 * gda_field_get_binary_value
 * @field: a #GdaField
 *
 * NOT IMPLEMENTED YET!
 *
 * Returns: the pointer to the binary data in @field.
 */
gconstpointer
gda_field_get_binary_value (GdaField *field)
{
	return NULL;
}

/**
 * gda_field_set_binary_value
 * @field: a #GdaField.
 * @value: the gint64 value to store in @field.
 * @size: size of the buffer pointed to by @value.
 *
 * NOT IMPLEMENTED YET!
 * Sets the value of @field to the contents pointed to by @value.
 */
void
gda_field_set_binary_value (GdaField *field, gconstpointer value, glong size)
{
}

/**
 * gda_field_get_boolean_value
 * @field: a #GdaField
 *
 * Returns: the gboolean value stored in @field.
 */
gboolean
gda_field_get_boolean_value (GdaField *field)
{
	g_return_val_if_fail (field != NULL, FALSE);
	return gda_value_get_boolean (field->value);
}

/**
 * gda_field_set_boolean_value
 * @field: a #GdaField.
 * @value: the gboolean value to store in @field.
 *
 * Sets the value of @field to @value.
 */
void
gda_field_set_boolean_value (GdaField *field, gboolean value)
{
	g_return_if_fail (field != NULL);

	gda_field_set_gdatype (field, GDA_VALUE_TYPE_BOOLEAN);
	field->actual_size = sizeof (gboolean);
	gda_value_set_boolean (field->value, value);
}

/**
 * gda_field_get_date_value
 * @field: a #GdaField
 *
 * Returns: the pointer to the #GdaDate value stored in @field.
 */
const GdaDate *
gda_field_get_date_value (GdaField *field)
{
	g_return_val_if_fail (field != NULL, NULL);

	return gda_value_get_date (field->value);
}

/**
 * gda_field_set_date_value
 * @field: a #GdaField.
 * @date: the #GdaDate value to store in @field.
 *
 * Sets the value of @field to the #GdaDate pointed to @value.
 */
void
gda_field_set_date_value (GdaField *field, GdaDate *date)
{
	g_return_if_fail (field != NULL);
	g_return_if_fail (date != NULL);

	gda_field_set_gdatype (field, GDA_VALUE_TYPE_DATE);
	field->actual_size = sizeof (GdaDate);
	gda_value_set_date (field->value, date);
}

/**
 * gda_field_get_double_value
 * @field: a #GdaField.
 *
 * Returns: the double precision value stored in @field.
 */
gdouble
gda_field_get_double_value (GdaField *field)
{
	g_return_val_if_fail (field != NULL, -1);
	return gda_value_get_double (field->value);
}

/**
 * gda_field_set_double_value
 * @field: a #GdaField.
 * @value: the double precision value to store in @field.
 *
 * Sets the value of @field to @value.
 */
void
gda_field_set_double_value (GdaField *field, gdouble value)
{
	g_return_if_fail (field != NULL);

	gda_field_set_gdatype (field, GDA_VALUE_TYPE_DOUBLE);
	field->actual_size = sizeof (gdouble);
	gda_value_set_double (field->value, value);
}

/**
 * gda_field_get_geometric_point_value
 * @field: a #GdaField.
 *
 * Returns: the a pointer to the #GdaGeometricPoint value stored in @field.
 */
const GdaGeometricPoint *
gda_field_get_geometric_point_value (GdaField *field)
{
	g_return_val_if_fail (field != NULL, NULL);
	return gda_value_get_geometric_point (field->value);
}

/**
 * gda_field_set_geometric_point_value
 * @field: a #GdaField.
 * @value: the #GdaGeometricPoint value to store in @field.
 *
 * Sets the value of @field to @value.
 */
void
gda_field_set_geometric_point_value (GdaField *field, GdaGeometricPoint *value)
{
	g_return_if_fail (field != NULL);
	g_return_if_fail (value != NULL);

	gda_field_set_gdatype (field, GDA_VALUE_TYPE_GEOMETRIC_POINT);
	field->actual_size = sizeof (GdaGeometricPoint);
	gda_value_set_geometric_point (field->value, value);
}

/**
 * gda_field_get_integer_value
 * @field: a #GdaField.
 *
 * Returns: the integer value stored in @field.
 */
gint
gda_field_get_integer_value (GdaField *field)
{
	g_return_val_if_fail (field != NULL, -1);
	return gda_value_get_integer (field->value);
}

/**
 * gda_field_set_integer_value
 * @field: a #GdaField.
 * @value: the integer value to store in @field.
 *
 * Sets the value of @field to @value.
 */
void
gda_field_set_integer_value (GdaField *field, gint value)
{
	g_return_if_fail (field != NULL);

	gda_field_set_gdatype (field, GDA_VALUE_TYPE_INTEGER);
	field->actual_size = sizeof (gint);
	gda_value_set_integer (field->value, value);
}

/**
 * gda_field_get_list_value
 * @field: a #GdaField.
 *
 * Returns: a pointer to the #GdaValueList value stored in @field.
 */
const GdaValueList *
gda_field_get_list_value (GdaField *field)
{
	g_return_val_if_fail (field != NULL, NULL);
	return gda_value_get_list (field->value);
}

/**
 * gda_field_set_list_value
 * @field: a #GdaField.
 * @value: the #GdaValueList value to store in @field.
 *
 * Sets the value of @field to @value.
 */
void
gda_field_set_list_value (GdaField *field, GdaValueList *value)
{
	g_return_if_fail (field != NULL);
	g_return_if_fail (value != NULL);

	gda_field_set_gdatype (field, GDA_VALUE_TYPE_LIST);
	field->actual_size = sizeof (GdaValue) * g_list_length (value);
	gda_value_set_list (field->value, value);
}

/**
 * gda_field_set_null_value
 * @field: a #GdaField.
 *
 * Sets the value of @field to be of type #GDA_TYPE_VALUE_NULL.
 */
void
gda_field_set_null_value (GdaField *field)
{
	g_return_if_fail (field != NULL);

	field->actual_size = 0;
	gda_value_set_null (field->value);
}

/**
 * gda_field_get_numeric_value
 * @field: a #GdaField.
 *
 * Returns: a pointer to the #GdaNumeric value stored in @field.
 */
const GdaNumeric *
gda_field_get_numeric_value (GdaField *field)
{
	g_return_val_if_fail (field != NULL, NULL);
	return gda_value_get_numeric (field->value);
}

/**
 * gda_field_set_numeric_value
 * @field: a #GdaField.
 * @value: the #GdaNumeric value to store in @field.
 *
 * Sets the value of @field to @value.
 */
void
gda_field_set_numeric_value (GdaField *field, GdaNumeric *value)
{
	g_return_if_fail (field != NULL);
	g_return_if_fail (value != NULL);

	gda_field_set_gdatype (field, GDA_VALUE_TYPE_NUMERIC);
	field->actual_size = sizeof (GdaNumeric);
	gda_value_set_numeric (field->value, value);
}

/**
 * gda_field_get_single_value
 * @field: a #GdaField.
 *
 * Returns: the single precision value stored in @field.
 */
gfloat
gda_field_get_single_value (GdaField *field)
{
	g_return_val_if_fail (field != NULL, -1);
	return gda_value_get_single (field->value);
}

/**
 * gda_field_set_single_value
 * @field: a #GdaField.
 * @value: the single precision value to store in @field.
 *
 * Sets the value of @field to @value.
 */
void
gda_field_set_single_value (GdaField *field, gfloat value)
{
	g_return_if_fail (field != NULL);

	gda_field_set_gdatype (field, GDA_VALUE_TYPE_SINGLE);
	field->actual_size = sizeof (gfloat);
	gda_value_set_single (field->value, value);
}

/**
 * gda_field_get_smallint_value
 * @field: a #GdaField.
 *
 * Returns: the smallint value (16-bit signed integer) stored in @field.
 */
gshort
gda_field_get_smallint_value (GdaField *field)
{
	g_return_val_if_fail (field != NULL, -1);
	return gda_value_get_smallint (field->value);
}

/**
 * gda_field_set_smallint_value
 * @field: a #GdaField.
 * @value: the smallint value (16-bit signed integer) to store in @field.
 *
 * Sets the value of @field to @value.
 */
void
gda_field_set_smallint_value (GdaField *field, gshort value)
{
	g_return_if_fail (field != NULL);

	gda_field_set_gdatype (field, GDA_VALUE_TYPE_SMALLINT);
	field->actual_size = sizeof (gshort);
	gda_value_set_smallint (field->value, value);
}

/**
 * gda_field_get_string_value
 * @field: a #GdaField.
 *
 * Returns: a pointer to the string value stored in @field.
 */
const gchar *
gda_field_get_string_value (GdaField *field)
{
	g_return_val_if_fail (field != NULL, NULL);
	return gda_value_get_string (field->value);
}

/**
 * gda_field_set_string_value
 * @field: a #GdaField.
 * @value: the string value to store in @field.
 *
 * Sets the value of @field to @value.
 */
void
gda_field_set_string_value (GdaField *field, const gchar *value)
{
	g_return_if_fail (field != NULL);

	gda_field_set_gdatype (field, GDA_VALUE_TYPE_STRING);
	field->actual_size = value ? strlen (value) : 0;
	gda_value_set_string (field->value, value);
}

/**
 * gda_field_get_time_value
 * @field: a #GdaField.
 *
 * Returns: a pointer to the #GdaTime value stored in @field.
 */
const GdaTime *
gda_field_get_time_value (GdaField *field)
{
	g_return_val_if_fail (field != NULL, NULL);
	return gda_value_get_time (field->value);
}

/**
 * gda_field_set_time_value
 * @field: a #GdaField.
 * @value: the #GdaTime value to store in @field.
 *
 * Sets the value of @field to the contents of @value.
 */
void
gda_field_set_time_value (GdaField *field, GdaTime *value)
{
	g_return_if_fail (field != NULL);
	g_return_if_fail (value != NULL);
	
	gda_field_set_gdatype (field, GDA_VALUE_TYPE_TIME);
	field->actual_size = sizeof (GdaTime);
	gda_value_set_time (field->value, value);
}

/**
 * gda_field_get_timestamp_value
 * @field: a #GdaField.
 *
 * Returns: a pointer to the #GdaGeometricData stored in @field.
 */
const GdaTimestamp *
gda_field_get_timestamp_value (GdaField *field)
{
	g_return_val_if_fail (field != NULL, NULL);
	return gda_value_get_timestamp (field->value);
}

/**
 * gda_field_set_timestamp_value
 * @field: a #GdaField.
 * @value: the #GdaTimestamp value to store in @field.
 *
 * Sets the value of @field to the contents of @value.
 */
void
gda_field_set_timestamp_value (GdaField *field, GdaTimestamp *value)
{
	g_return_if_fail (field != NULL);

        gda_field_set_gdatype (field, GDA_VALUE_TYPE_TIMESTAMP);
	field->actual_size = sizeof (GdaTimestamp);
	gda_value_set_timestamp (field->value, value);
}

/**
 * gda_field_get_tinyint_value
 * @field: a #GdaField.
 *
 * Returns: the tinyint value (8-bit signed integer) stored in @field.
 */
gchar
gda_field_get_tinyint_value (GdaField *field)
{
	g_return_val_if_fail (field != NULL, -1);
	return gda_value_get_tinyint (field->value);
}

/**
 * gda_field_set_tinyint_value
 * @field: a #GdaField.
 * @value: the tinyint value (8-bit signed integer) to store in @field.
 *
 * Sets the value of @field to the contents of @value.
 */
void
gda_field_set_tinyint_value (GdaField *field, gchar value)
{
	g_return_if_fail (field != NULL);

	gda_field_set_gdatype (field, GDA_VALUE_TYPE_TINYINT);
	field->actual_size = sizeof (gchar);
	gda_value_set_tinyint (field->value, value);
}

/**
 * gda_field_stringify
 * @field: a #GdaField.
 *
 * See also #gda_value_stringify for a list of types and their corresponding
 * string representation format.
 * 
 * Returns: the string representation of the value stored in @field.
 */
gchar *
gda_field_stringify (GdaField *field)
{
	g_return_val_if_fail (field != NULL, NULL);
	return gda_value_stringify ((GdaValue *) field->value);
}
