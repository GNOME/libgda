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
 */
GdaFieldAttributes *
gda_field_attributes_new (void)
{
	GdaFieldAttributes *fa;

	fa = g_new0 (GdaFieldAttributes, 1);
	fa->defined_size = 0;
	fa->name = NULL;
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
	g_free (fa->caption);
	g_free (fa->references);
	g_free (fa);
}

/**
 * gda_field_attributes_get_defined_size
 * @fa: a @GdaFieldAttributes
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
 * gda_field_attributes_get_caption
 */
const gchar *
gda_field_attributes_get_caption (GdaFieldAttributes *fa)
{
	g_return_val_if_fail (fa != NULL, NULL);
	return (const gchar *) fa->caption;
}

/**
 * gda_field_attributes_set_caption
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

