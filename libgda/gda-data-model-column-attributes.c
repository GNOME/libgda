/* GDA library
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *      Michael Lausch <michael@lausch.at>
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
#include <libgda/gda-data-model-column-attributes.h>
#include <string.h>

GType
gda_data_model_column_attributes_get_type (void)
{
	static GType our_type = 0;

	if (our_type == 0) 
		our_type = g_boxed_type_register_static ("GdaDataModelColumnAttributes",
							 (GBoxedCopyFunc) gda_data_model_column_attributes_copy,
							 (GBoxedFreeFunc) gda_data_model_column_attributes_free);
	return our_type;
}

/**
 * gda_data_model_column_attributes_new
 *
 * Returns: a newly allocated #GdaDataModelColumnAttributes object.
 */
GdaDataModelColumnAttributes *
gda_data_model_column_attributes_new (void)
{
	GdaDataModelColumnAttributes *fa;

	fa = g_new0 (GdaDataModelColumnAttributes, 1);
	fa->defined_size = 0;
	fa->name = NULL;
	fa->table = NULL;
	fa->caption = NULL;
	fa->scale = 0;
	fa->gda_type = GDA_VALUE_TYPE_NULL;
	fa->allow_null = TRUE;
	fa->primary_key = FALSE;
	fa->unique_key = FALSE;
	fa->references = NULL;
	fa->auto_increment = FALSE;
	fa->auto_increment_start = 0;
	fa->auto_increment_step = 0;
	fa->position = -1;
	fa->default_value = NULL;

	return fa;
}

/**
 * gda_data_model_column_attributes_copy
 * @fa: attributes to get a copy from.
 *
 * Creates a new #GdaDataModelColumnAttributes object from an existing one.
 * Returns: a newly allocated #GdaDataModelColumnAttributes with a copy of the data
 * in @fa.
 */
GdaDataModelColumnAttributes *
gda_data_model_column_attributes_copy (GdaDataModelColumnAttributes *fa)
{
	GdaDataModelColumnAttributes *fa_copy;

	g_return_val_if_fail (fa != NULL, NULL);

	fa_copy = gda_data_model_column_attributes_new ();
	fa_copy->defined_size = fa->defined_size;
	fa_copy->name = g_strdup (fa->name);
	fa_copy->table = g_strdup (fa->table);
	fa_copy->caption = g_strdup (fa->caption);
	fa_copy->scale = fa->scale;
	fa_copy->gda_type = fa->gda_type;
	fa_copy->allow_null = fa->allow_null;
	fa_copy->primary_key = fa->primary_key;
	fa_copy->unique_key = fa->unique_key;
	fa_copy->references = g_strdup (fa->references);
	fa_copy->auto_increment = fa->auto_increment;
	fa_copy->auto_increment_start = fa->auto_increment_start;
	fa_copy->auto_increment_step = fa->auto_increment_step;
	fa_copy->position = fa->position;
	fa_copy->default_value = (fa->default_value ? gda_value_copy (fa->default_value) : 0);

	return fa_copy;
}

/**
 * gda_data_model_column_attributes_free
 * @fa: the resource to free.
 *
 * Deallocates all memory associated to the given #GdaDataModelColumnAttributes object.
 */
void
gda_data_model_column_attributes_free (GdaDataModelColumnAttributes *fa)
{
	g_return_if_fail (fa != NULL);

	g_free (fa->name);
	g_free (fa->table);
	g_free (fa->caption);
	g_free (fa->references);
	g_free (fa->default_value);
	g_free (fa);
}

/**
 * gda_data_model_column_attributes_equal:
 * @lhs: a #GdaDataModelColumnAttributes
 * @rhs: another #GdaDataModelColumnAttributes
 *
 * Tests whether two field attributes are equal.
 *
 * Return value: %TRUE if the field attributes contain the same information.
 **/
gboolean
gda_data_model_column_attributes_equal (const GdaDataModelColumnAttributes *lhs,
                            const GdaDataModelColumnAttributes *rhs)
{
	g_return_val_if_fail (lhs != NULL, FALSE);
	g_return_val_if_fail (rhs != NULL, FALSE);

	/* Compare every struct field: */
	if ((lhs->defined_size != rhs->defined_size) ||
	    (lhs->scale != rhs->scale) ||
	    (lhs->gda_type != rhs->gda_type) ||
	    (lhs->allow_null != rhs->allow_null) ||
	    (lhs->primary_key != rhs->primary_key) ||
	    (lhs->unique_key != rhs->unique_key) ||
	    (lhs->auto_increment != rhs->auto_increment) ||
	    (lhs->auto_increment_step != rhs->auto_increment_step) ||
	    (lhs->position != rhs->position))  
		return FALSE;

	/* Check the strings if they have are not null.
	   Then check whether one is null while the other is not, because strcmp can not do that. */
	if ((lhs->name && rhs->name) && (strcmp (lhs->name, rhs->name) != 0))
		return FALSE;
    
	if ((lhs->name == 0) != (rhs->name == 0))
		return FALSE;
    
	if ((lhs->table && rhs->table) && (strcmp (lhs->table, rhs->table) != 0))
		return FALSE;

	if ((lhs->table == 0) != (rhs->table == 0))
		return FALSE;

	if ((lhs->caption && rhs->caption) && (strcmp (lhs->caption, rhs->caption) != 0))
		return FALSE;

	if ((lhs->caption == 0) != (rhs->caption == 0))
		return FALSE;
    
	if ((lhs->references && rhs->references) && (strcmp (lhs->references, rhs->references) != 0))
		return FALSE;

	if ((lhs->references == 0) != (rhs->references == 0))
		return FALSE;
    
	if (lhs->default_value && rhs->default_value && gda_value_compare (lhs->default_value, rhs->default_value) != 0)
		return FALSE;

	if ((lhs->default_value == 0) != (rhs->default_value == 0))
		return FALSE;
    
	return TRUE;
}

/**
 * gda_data_model_column_attributes_get_defined_size
 * @fa: a @GdaDataModelColumnAttributes.
 *
 * Returns: the defined size of @fa.
 */
glong
gda_data_model_column_attributes_get_defined_size (GdaDataModelColumnAttributes *fa)
{
	g_return_val_if_fail (fa != NULL, 0);
	return fa->defined_size;
}

/**
 * gda_data_model_column_attributes_set_defined_size
 * @fa: a #GdaDataModelColumnAttributes.
 * @size: the defined size we want to set.
 *
 * Sets the defined size of a #GdaDataModelColumnAttributes.
 */
void
gda_data_model_column_attributes_set_defined_size (GdaDataModelColumnAttributes *fa, glong size)
{
	g_return_if_fail (fa != NULL);
	fa->defined_size = size;
}

/**
 * gda_data_model_column_attributes_get_name
 * @fa: a #GdaDataModelColumnAttributes.
 *
 * Returns: the name of @fa.
 */
const gchar *
gda_data_model_column_attributes_get_name (GdaDataModelColumnAttributes *fa)
{
	g_return_val_if_fail (fa != NULL, NULL);
	return (const gchar *) fa->name;
}

/**
 * gda_data_model_column_attributes_set_name
 * @fa: a #GdaDataModelColumnAttributes.
 * @name: the new name of @fa.
 *
 * Sets the name of @fa to @name.
 */
void
gda_data_model_column_attributes_set_name (GdaDataModelColumnAttributes *fa, const gchar *name)
{
	g_return_if_fail (fa != NULL);
	g_return_if_fail (name != NULL);

	if (fa->name != NULL)
		g_free (fa->name);
	fa->name = g_strdup (name);
}

/**
 * gda_data_model_column_attributes_get_table
 * @fa: a #GdaDataModelColumnAttributes.
 *
 * Returns: the name of the table to which this field belongs.
 */
const gchar *
gda_data_model_column_attributes_get_table (GdaDataModelColumnAttributes *fa)
{
	g_return_val_if_fail (fa != NULL, NULL);
	return fa->table;;
}

/**
 * gda_data_model_column_attributes_set_table
 * @fa: a #GdaDataModelColumnAttributes.
 * @table: table name.
 *
 * Sets the name of the table to which the given field belongs.
 */
void
gda_data_model_column_attributes_set_table (GdaDataModelColumnAttributes *fa, const gchar *table)
{
	g_return_if_fail (fa != NULL);

	if (fa->table != NULL)
		g_free (fa->table);
	fa->table = g_strdup (table);
}

/**
 * gda_data_model_column_attributes_get_caption
 * @fa: a #GdaDataModelColumnAttributes.
 *
 * Returns: @fa's caption.
 */
const gchar *
gda_data_model_column_attributes_get_caption (GdaDataModelColumnAttributes *fa)
{
	g_return_val_if_fail (fa != NULL, NULL);
	return (const gchar *) fa->caption;
}

/**
 * gda_data_model_column_attributes_set_caption
 * @fa: a #GdaDataModelColumnAttributes.
 * @caption: caption.
 *
 * Sets @fa's @caption.
 */
void
gda_data_model_column_attributes_set_caption (GdaDataModelColumnAttributes *fa, const gchar *caption)
{
	g_return_if_fail (fa != NULL);

	if (fa->caption)
		g_free (fa->caption);
	fa->caption = g_strdup (caption);
}

/**
 * gda_data_model_column_attributes_get_scale
 * @fa: a #GdaDataModelColumnAttributes.
 *
 * Returns: the number of decimals of @fa.
 */
glong
gda_data_model_column_attributes_get_scale (GdaDataModelColumnAttributes *fa)
{
	g_return_val_if_fail (fa != NULL, 0);
	return fa->scale;
}

/**
 * gda_data_model_column_attributes_set_scale
 * @fa: a #GdaDataModelColumnAttributes.
 * @scale: number of decimals.
 *
 * Sets the scale of @fa to @scale.
 */
void
gda_data_model_column_attributes_set_scale (GdaDataModelColumnAttributes *fa, glong scale)
{
	g_return_if_fail (fa != NULL);
	fa->scale = scale;
}

/**
 * gda_data_model_column_attributes_get_gdatype
 * @fa: a #GdaDataModelColumnAttributes.
 *
 * Returns: the type of @fa.
 */
GdaValueType
gda_data_model_column_attributes_get_gdatype (GdaDataModelColumnAttributes *fa)
{
	g_return_val_if_fail (fa != NULL, GDA_VALUE_TYPE_NULL);
	return fa->gda_type;
}

/**
 * gda_data_model_column_attributes_set_gdatype
 * @fa: a #GdaDataModelColumnAttributes.
 * @type: the new type of @fa.
 *
 * Sets the type of @fa to @type.
 */
void
gda_data_model_column_attributes_set_gdatype (GdaDataModelColumnAttributes *fa, GdaValueType type)
{
	g_return_if_fail (fa != NULL);
	fa->gda_type = type;
}

/**
 * gda_data_model_column_attributes_get_allow_null
 * @fa: a #GdaDataModelColumnAttributes.
 *
 * Gets the 'allow null' flag of the given field attributes.
 *
 * Returns: whether the given field allows null values or not (%TRUE or %FALSE).
 */
gboolean
gda_data_model_column_attributes_get_allow_null (GdaDataModelColumnAttributes *fa)
{
	g_return_val_if_fail (fa != NULL, FALSE);
	return fa->allow_null;
}

/**
 * gda_data_model_column_attributes_set_allow_null
 * @fa: a #GdaDataModelColumnAttributes.
 * @allow: whether the given field should allows null values or not.
 *
 * Sets the 'allow null' flag of the given field attributes.
 */
void
gda_data_model_column_attributes_set_allow_null (GdaDataModelColumnAttributes *fa, gboolean allow)
{
	g_return_if_fail (fa != NULL);
	fa->allow_null = allow;
}

/**
 * gda_data_model_column_attributes_get_primary_key
 * @fa: a #GdaDataModelColumnAttributes.
 *
 * Returns: whether if the given field is a primary key (%TRUE or %FALSE).
 */
gboolean
gda_data_model_column_attributes_get_primary_key (GdaDataModelColumnAttributes *fa)
{
	g_return_val_if_fail (fa != NULL, FALSE);
	return fa->primary_key;
}

/**
 * gda_data_model_column_attributes_set_primary_key
 * @fa: a #GdaDataModelColumnAttributes.
 * @pk: whether if the given field should be a primary key.
 *
 * Sets the 'primary key' flag of the given field attributes.
 */
void
gda_data_model_column_attributes_set_primary_key (GdaDataModelColumnAttributes *fa, gboolean pk)
{
	g_return_if_fail (fa != NULL);
	fa->primary_key = pk;
}

/**
 * gda_data_model_column_attributes_get_unique_key
 * @fa: a #GdaDataModelColumnAttributes.
 *
 * Returns: whether if the given field is an unique key (%TRUE or %FALSE).
 */
gboolean
gda_data_model_column_attributes_get_unique_key (GdaDataModelColumnAttributes *fa)
{
	g_return_val_if_fail (fa != NULL, FALSE);
	return fa->unique_key;
}

/**
 * gda_data_model_column_attributes_set_unique_key
 * @fa: a #GdaDataModelColumnAttributes.
 * @uk: whether if the given field should be an unique key.
 *
 * Sets the 'unique key' flag of the given field attributes.
 */
void
gda_data_model_column_attributes_set_unique_key (GdaDataModelColumnAttributes *fa, gboolean uk)
{
	g_return_if_fail (fa != NULL);
	fa->unique_key = uk;
}

/**
 * gda_data_model_column_attributes_get_references
 * @fa: a #GdaDataModelColumnAttributes.
 *
 * Returns: @fa's references.
 */
const gchar *
gda_data_model_column_attributes_get_references (GdaDataModelColumnAttributes *fa)
{
	g_return_val_if_fail (fa != NULL, NULL);
	return (const gchar *) fa->references;
}

/**
 * gda_data_model_column_attributes_set_references
 * @fa: a #GdaDataModelColumnAttributes.
 * @ref: references.
 *
 * Sets @fa's @references.
 */
void
gda_data_model_column_attributes_set_references (GdaDataModelColumnAttributes *fa, const gchar *ref)
{
	g_return_if_fail (fa != NULL);

	if (fa->references != NULL)
		g_free (fa->references);

	if (ref)
		fa->references = g_strdup (ref);
}

/**
 * gda_data_model_column_attributes_get_auto_increment
 * @fa: a #GdaDataModelColumnAttributes.
 *
 * Returns: whether the given field is an auto incremented one (%TRUE or %FALSE).
 */
gboolean
gda_data_model_column_attributes_get_auto_increment (GdaDataModelColumnAttributes *fa)
{
	g_return_val_if_fail (fa != NULL, FALSE);
	return fa->auto_increment;
}

/**
 * gda_data_model_column_attributes_set_auto_increment
 * @fa: a #GdaDataModelColumnAttributes.
 * @is_auto: auto increment status.
 *
 * Sets the auto increment flag for the given field.
 */
void
gda_data_model_column_attributes_set_auto_increment (GdaDataModelColumnAttributes *fa,
					 gboolean is_auto)
{
	g_return_if_fail (fa != NULL);
	fa->auto_increment = is_auto;
}

/**
 * gda_data_model_column_attributes_get_position
 * @fa: a #GdaDataModelColumnAttributes.
 *
 * Returns: the position of the field the attributes refer to in the
 * containing data model.
 */
gint
gda_data_model_column_attributes_get_position (GdaDataModelColumnAttributes *fa)
{
	g_return_val_if_fail (fa != NULL, -1);
	return fa->position;
}

/**
 * gda_data_model_column_attributes_set_position
 * @fa: a #GdaDataModelColumnAttributes.
 * @position: the wanted position of the field in the containing data model.
 *
 * Sets the position of the field the attributes refer to in the containing
 * data model.
 */
void
gda_data_model_column_attributes_set_position (GdaDataModelColumnAttributes *fa, gint position)
{
	g_return_if_fail (fa != NULL);
	fa->position = position;
}


/**
 * gda_data_model_column_attributes_get_default_value
 * @fa: a #GdaDataModelColumnAttributes.
 *
 * Returns: @fa's default value, as a #GdaValue object.
 */
const GdaValue *
gda_data_model_column_attributes_get_default_value (GdaDataModelColumnAttributes *fa)
{
	g_return_val_if_fail (fa != NULL, NULL);
	return (const GdaValue *) fa->default_value;
}

/**
 * gda_data_model_column_attributes_set_default_value
 * @fa: a #GdaDataModelColumnAttributes.
 * @default_value: default #GdaValue for the field
 *
 * Sets @fa's default #GdaValue.
 */
void
gda_data_model_column_attributes_set_default_value (GdaDataModelColumnAttributes *fa, const GdaValue *default_value)
{
	g_return_if_fail (fa != NULL);
	g_return_if_fail (default_value != NULL);

	if (fa->default_value)
		g_free (fa->default_value);
	fa->default_value = gda_value_copy ( (GdaValue*)default_value);
}

