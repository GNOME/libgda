/* GDA library
 * Copyright (C) 1998-2005 The GNOME Foundation.
 *
 * AUTHORS:
 *	Bas Driessen <bas.driessen@xobas.com>
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

#include <libgda/gda-data-model-column-index-attributes.h>

GType
gda_data_model_column_index_attributes_get_type (void)
{
	static GType our_type = 0;

	if (our_type == 0) 
		our_type = g_boxed_type_register_static ("GdaDataModelColumnIndexAttributes",
							 (GBoxedCopyFunc) gda_data_model_column_index_attributes_copy,
							 (GBoxedFreeFunc) gda_data_model_column_index_attributes_free);
	return our_type;
}

/**
 * gda_data_model_column_index_attributes_new
 *
 * Returns: a newly allocated #GdaDataModelColumnIndexAttributes object.
 */
GdaDataModelColumnIndexAttributes *
gda_data_model_column_index_attributes_new (void)
{
	GdaDataModelColumnIndexAttributes *dmcia;

	dmcia = g_new0 (GdaDataModelColumnIndexAttributes, 1);
	dmcia->column_name = NULL;
	dmcia->defined_size = 0;
	dmcia->sorting = GDA_SORTING_ASCENDING;
	dmcia->references = NULL;

	return dmcia;
}

/**
 * gda_data_model_column_index_attributes_copy
 * @dmcia: attributes to get a copy from.
 *
 * Creates a new #GdaDataModelColumnIndexAttributes object from an existing one.
 * Returns: a newly allocated #GdaDataModelColumnIndexAttributes with a copy of the data
 * in @dmcia.
 */
GdaDataModelColumnIndexAttributes *
gda_data_model_column_index_attributes_copy (GdaDataModelColumnIndexAttributes *dmcia)
{
	GdaDataModelColumnIndexAttributes *dmcia_copy;

	g_return_val_if_fail (dmcia != NULL, NULL);

	dmcia_copy = gda_data_model_column_index_attributes_new ();
	dmcia_copy->column_name = g_strdup (dmcia->column_name);
	dmcia_copy->defined_size = dmcia->defined_size;
	dmcia_copy->sorting = dmcia->sorting;
	dmcia_copy->references = g_strdup (dmcia->references);

	return dmcia_copy;
}

/**
 * gda_data_model_column_index_attributes_free
 * @dmcia: the resource to free.
 *
 * Deallocates all memory associated to the given #GdaDataModelColumnIndexAttributes object.
 */
void
gda_data_model_column_index_attributes_free (GdaDataModelColumnIndexAttributes *dmcia)
{
	g_return_if_fail (dmcia != NULL);

	g_free (dmcia->column_name);
	g_free (dmcia->references);
	g_free (dmcia);
}

/**
 * gda_data_model_column_index_attributes_equal:
 * @lhs: a #GdaDataModelColumnIndexAttributes
 * @rhs: another #GdaDataModelColumnIndexAttributes
 *
 * Tests whether two field attributes are equal.
 *
 * Return value: %TRUE if the field attributes contain the same information.
 **/
gboolean
gda_data_model_column_index_attributes_equal (const GdaDataModelColumnIndexAttributes *lhs,
                            const GdaDataModelColumnIndexAttributes *rhs)
{
	g_return_val_if_fail (lhs != NULL, FALSE);
	g_return_val_if_fail (rhs != NULL, FALSE);

	/* Compare every struct field: */
	if ((lhs->defined_size != rhs->defined_size) ||
	    (lhs->sorting != rhs->sorting))
		return FALSE;

	/* Check the strings if they have are not null.
	   Then check whether one is null while the other is not, because strcmp can not do that. */
	if ((lhs->column_name && rhs->column_name) && (strcmp (lhs->column_name, rhs->column_name) != 0))
		return FALSE;
    
	if ((lhs->column_name == 0) != (rhs->column_name == 0))
		return FALSE;
    
	if ((lhs->references && rhs->references) && (strcmp (lhs->references, rhs->references) != 0))
		return FALSE;

	if ((lhs->references == 0) != (rhs->references == 0))
		return FALSE;
    
	return TRUE;
}

/**
 * gda_data_model_column_index_attributes_get_column_name
 * @dmcia: a #GdaDataModelColumnIndexAttributes.
 *
 * Returns: the column name of @dmcia.
 */
const gchar *
gda_data_model_column_index_attributes_get_column_name (GdaDataModelColumnIndexAttributes *dmcia)
{
	g_return_val_if_fail (dmcia != NULL, NULL);
	return (const gchar *) dmcia->column_name;
}

/**
 * gda_data_model_column_index_attributes_set_column_name
 * @dmcia: a #GdaDataModelColumnIndexAttributes.
 * @column_name: the new name of @dmcia.
 *
 * Sets the name of @dmcia to @column_name.
 */
void
gda_data_model_column_index_attributes_set_column_name (GdaDataModelColumnIndexAttributes *dmcia, const gchar *column_name)
{
	g_return_if_fail (dmcia != NULL);
	g_return_if_fail (column_name != NULL);

	if (dmcia->column_name != NULL)
		g_free (dmcia->column_name);
	dmcia->column_name = g_strdup (column_name);
}

/**
 * gda_data_model_column_index_attributes_get_defined_size
 * @dmcia: a @GdaDataModelColumnIndexAttributes.
 *
 * Returns: the defined size of @dmcia.
 */
glong
gda_data_model_column_index_attributes_get_defined_size (GdaDataModelColumnIndexAttributes *dmcia)
{
	g_return_val_if_fail (dmcia != NULL, 0);
	return dmcia->defined_size;
}

/**
 * gda_data_model_column_index_attributes_set_defined_size
 * @dmcia: a #GdaDataModelColumnIndexAttributes.
 * @size: the defined size we want to set.
 *
 * Sets the defined size of a #GdaDataModelColumnIndexAttributes.
 */
void
gda_data_model_column_index_attributes_set_defined_size (GdaDataModelColumnIndexAttributes *dmcia, glong size)
{
	g_return_if_fail (dmcia != NULL);
	dmcia->defined_size = size;
}

/**
 * gda_data_model_column_index_attributes_get_sorting
 * @dmcia: a @GdaDataModelColumnIndexAttributes.
 *
 * Returns: the sorting of @dmcia.
 */
GdaSorting
gda_data_model_column_index_attributes_get_sorting (GdaDataModelColumnIndexAttributes *dmcia)
{
	g_return_val_if_fail (dmcia != NULL, 0);
	return dmcia->sorting;
}

/**
 * gda_data_model_column_index_attributes_set_sorting
 * @dmcia: a #GdaDataModelColumnIndexAttributes.
 * @sorting: the new sorting of @dmcia.
 *
 * Sets the sorting of a #GdaDataModelColumnIndexAttributes.
 */
void
gda_data_model_column_index_attributes_set_sorting (GdaDataModelColumnIndexAttributes *dmcia, GdaSorting sorting)
{
	g_return_if_fail (dmcia != NULL);
	dmcia->sorting = sorting;
}

/**
 * gda_data_model_column_index_attributes_get_references
 * @dmcia: a #GdaDataModelColumnIndexAttributes.
 *
 * Returns: @dmcia's references.
 */
const gchar *
gda_data_model_column_index_attributes_get_references (GdaDataModelColumnIndexAttributes *dmcia)
{
	g_return_val_if_fail (dmcia != NULL, NULL);
	return (const gchar *) dmcia->references;
}

/**
 * gda_data_model_column_index_attributes_set_references
 * @dmcia: a #GdaDataModelColumnIndexAttributes.
 * @ref: references.
 *
 * Sets @dmcia's @references.
 */
void
gda_data_model_column_index_attributes_set_references (GdaDataModelColumnIndexAttributes *dmcia, const gchar *ref)
{
	g_return_if_fail (dmcia != NULL);

	if (dmcia->references != NULL)
		g_free (dmcia->references);

	if (ref)
		dmcia->references = g_strdup (ref);
}

