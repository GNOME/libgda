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
#include <libgda/gda-field.h>

/**
 * gda_field_attributes_new
 *
 * Returns: a newly allocated #GdaFieldAttributes object.
 */
GdaFieldAttributes *
gda_field_attributes_new (void)
{
	GdaFieldAttributes *fa;

	fa = g_new0 (GdaFieldAttributes, 1);
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

	return fa;
}

/**
 * gda_field_attributes_copy
 * @fa: attributes to get a copy from.
 *
 * Creates a new #GdaFieldAttributes object from an existing one.
 * Returns: a newly allocated #GdaFieldAttributes with a copy of the data
 * in @fa.
 */
GdaFieldAttributes *
gda_field_attributes_copy (GdaFieldAttributes *fa)
{
	GdaFieldAttributes *fa_copy;

	g_return_val_if_fail (fa != NULL, NULL);

	fa_copy = gda_field_attributes_new ();
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

	return fa_copy;
}

/**
 * gda_field_attributes_free
 * @fa: the resource to free.
 *
 * Deallocates all memory associated to the given #GdaFieldAttributes object.
 */
void
gda_field_attributes_free (GdaFieldAttributes *fa)
{
	g_return_if_fail (fa != NULL);

	g_free (fa->name);
	g_free (fa->table);
	g_free (fa->caption);
	g_free (fa->references);
	g_free (fa);
}

/**
 * gda_field_attributes_get_defined_size
 * @fa: a @GdaFieldAttributes.
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
 * gda_field_attributes_get_table
 * @fa: a #GdaFieldAttributes.
 *
 * Returns: the name of the table to which this field belongs.
 */
const gchar *
gda_field_attributes_get_table (GdaFieldAttributes *fa)
{
	g_return_val_if_fail (fa != NULL, NULL);
	return fa->table;;
}

/**
 * gda_field_attributes_set_table
 * @fa: a #GdaFieldAttributes.
 * @table: table name.
 *
 * Sets the name of the table to which the given field belongs.
 */
void
gda_field_attributes_set_table (GdaFieldAttributes *fa, const gchar *table)
{
	g_return_if_fail (fa != NULL);

	if (fa->table != NULL)
		g_free (fa->table);
	fa->table = g_strdup (table);
}

/**
 * gda_field_attributes_get_caption
 * @fa: a #GdaFieldAttributes.
 *
 * Returns: @fa's caption.
 */
const gchar *
gda_field_attributes_get_caption (GdaFieldAttributes *fa)
{
	g_return_val_if_fail (fa != NULL, NULL);
	return (const gchar *) fa->caption;
}

/**
 * gda_field_attributes_set_caption
 * @fa: a #GdaFieldAttributes.
 * @caption: caption.
 *
 * Sets @fa's @caption.
 */
void
gda_field_attributes_set_caption (GdaFieldAttributes *fa, const gchar *caption)
{
	g_return_if_fail (fa != NULL);

	if (fa->caption)
		g_free (fa->caption);
	fa->caption = g_strdup (caption);
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
 * Gets the 'allow null' flag of the given field attributes.
 *
 * Returns: whether the given field allows null values or not (%TRUE or %FALSE).
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
 * @allow: whether the given field should allows null values or not.
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
 *
 * Returns: whether if the given field is a primary key (%TRUE or %FALSE).
 */
gboolean
gda_field_attributes_get_primary_key (GdaFieldAttributes *fa)
{
	g_return_val_if_fail (fa != NULL, FALSE);
	return fa->primary_key;
}

/**
 * gda_field_attributes_set_primary_key
 * @fa: a #GdaFieldAttributes.
 * @pk: whether if the given field should be a primary key.
 *
 * Sets the 'primary key' flag of the given field attributes.
 */
void
gda_field_attributes_set_primary_key (GdaFieldAttributes *fa, gboolean pk)
{
	g_return_if_fail (fa != NULL);
	fa->primary_key = pk;
}

/**
 * gda_field_attributes_get_unique_key
 * @fa: a #GdaFieldAttributes.
 *
 * Returns: whether if the given field is an unique key (%TRUE or %FALSE).
 */
gboolean
gda_field_attributes_get_unique_key (GdaFieldAttributes *fa)
{
	g_return_val_if_fail (fa != NULL, FALSE);
	return fa->unique_key;
}

/**
 * gda_field_attributes_set_unique_key
 * @fa: a #GdaFieldAttributes.
 * @uk: whether if the given field should be an unique key.
 *
 * Sets the 'unique key' flag of the given field attributes.
 */
void
gda_field_attributes_set_unique_key (GdaFieldAttributes *fa, gboolean uk)
{
	g_return_if_fail (fa != NULL);
	fa->unique_key = uk;
}

/**
 * gda_field_attributes_get_references
 * @fa: a #GdaFieldAttributes.
 *
 * Returns: @fa's references.
 */
const gchar *
gda_field_attributes_get_references (GdaFieldAttributes *fa)
{
	g_return_val_if_fail (fa != NULL, NULL);
	return (const gchar *) fa->references;
}

/**
 * gda_field_attributes_set_references
 * @fa: a #GdaFieldAttributes.
 * @ref: references.
 *
 * Sets @fa's @references.
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
 * gda_field_attributes_get_auto_increment
 * @fa: a #GdaFieldAttributes.
 *
 * Returns: whether the given field is an auto incremented one (%TRUE or %FALSE).
 */
gboolean
gda_field_attributes_get_auto_increment (GdaFieldAttributes *fa)
{
	g_return_val_if_fail (fa != NULL, FALSE);
	return fa->auto_increment;
}

/**
 * gda_field_attributes_set_auto_increment
 * @fa: a #GdaFieldAttributes.
 * @is_auto: auto increment status.
 *
 * Sets the auto increment flag for the given field.
 */
void
gda_field_attributes_set_auto_increment (GdaFieldAttributes *fa,
					 gboolean is_auto)
{
	g_return_if_fail (fa != NULL);
	fa->auto_increment = is_auto;
}

/**
 * gda_field_attributes_get_position
 * @fa: a #GdaFieldAttributes.
 *
 * Returns: the position of the field the attributes refer to in the
 * containing data model.
 */
gint
gda_field_attributes_get_position (GdaFieldAttributes *fa)
{
	g_return_val_if_fail (fa != NULL, -1);
	return fa->position;
}

/**
 * gda_field_attributes_set_position
 * @fa: a #GdaFieldAttributes.
 * @position: the wanted position of the field in the containing data model.
 *
 * Sets the position of the field the attributes refer to in the containing
 * data model.
 */
void
gda_field_attributes_set_position (GdaFieldAttributes *fa, gint position)
{
	g_return_if_fail (fa != NULL);
	fa->position = position;
}
