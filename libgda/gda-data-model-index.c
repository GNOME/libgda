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

#include <libgda/gda-data-model-index.h>
#include <libgda/gda-data-model-column-index-attributes.h>

GType
gda_data_model_index_get_type (void)
{
	static GType our_type = 0;

	if (our_type == 0) 
		our_type = g_boxed_type_register_static ("GdaDataModelIndex",
							 (GBoxedCopyFunc) gda_data_model_index_copy,
							 (GBoxedFreeFunc) gda_data_model_index_free);
	return our_type;
}

/**
 * gda_data_model_index_new
 *
 * Returns: a newly allocated #GdaDataModelIndex object.
 */
GdaDataModelIndex *
gda_data_model_index_new (void)
{
	GdaDataModelIndex *dmi;

	dmi = g_new0 (GdaDataModelIndex, 1);
	dmi->name = NULL;
	dmi->table_name = NULL;
	dmi->primary_key = FALSE;
	dmi->unique_key = FALSE;
	dmi->references = NULL;
	dmi->col_idx_list = NULL;

	return dmi;
}

/**
 * gda_data_model_index_copy
 * @dmi: attributes to get a copy from.
 *
 * Creates a new #GdaDataModelIndex object from an existing one.
 * Returns: a newly allocated #GdaDataModelIndex with a copy of the data
 * in @dmi.
 */
GdaDataModelIndex *
gda_data_model_index_copy (GdaDataModelIndex *dmi)
{
	GdaDataModelIndex *dmi_copy;
	gint i;

	g_return_val_if_fail (dmi != NULL, NULL);

	dmi_copy = gda_data_model_index_new ();
	dmi_copy->name = g_strdup (dmi->name);
	dmi_copy->table_name = g_strdup (dmi->table_name);
	dmi_copy->primary_key = dmi->primary_key;
	dmi_copy->unique_key = dmi->unique_key;
	dmi_copy->references = g_strdup (dmi->references);

	/* g_list_copy (shallow copy) not good enough */
	for (i = 0; i < g_list_length (dmi->col_idx_list); i++) 
		dmi_copy->col_idx_list = g_list_append (dmi_copy->col_idx_list, 
			(GdaDataModelColumnIndexAttributes *) 
			gda_data_model_column_index_attributes_copy (g_list_nth_data (dmi->col_idx_list, i)));

	return dmi_copy;
}

/**
 * gda_data_model_index__free
 * @dmi: the resource to free.
 *
 * Deallocates all memory associated to the given #GdaDataModelIndex object.
 */
void
gda_data_model_index_free (GdaDataModelIndex *dmi)
{
	gint i;

	g_return_if_fail (dmi != NULL);

	g_free (dmi->name);
	g_free (dmi->table_name);
	g_free (dmi->references);

	/* free column index attributes list */	
	for (i = 0; i < g_list_length (dmi->col_idx_list); i++) 
		gda_data_model_column_index_attributes_free (g_list_nth_data (dmi->col_idx_list, i));

	g_list_free (dmi->col_idx_list);

	g_free (dmi);
}

/**
 * gda_data_model_index_equal:
 * @lhs: a #GdaDataModelIndex
 * @rhs: another #GdaDataModelIndex
 *
 * Tests whether two field attributes are equal.
 *
 * Return value: %TRUE if the field attributes contain the same information.
 **/
gboolean
gda_data_model_index_equal (const GdaDataModelIndex *lhs,
                            const GdaDataModelIndex *rhs)
{
	gint i;

	g_return_val_if_fail (lhs != NULL, FALSE);
	g_return_val_if_fail (rhs != NULL, FALSE);

	/* Compare every struct field: */
	if ((lhs->primary_key != rhs->primary_key) ||
	    (lhs->unique_key != rhs->unique_key))
		return FALSE;

	/* Check the strings if they have are not null.
	   Then check whether one is null while the other is not, because strcmp can not do that. */
	if ((lhs->name && rhs->name) && (strcmp (lhs->name, rhs->name) != 0))
		return FALSE;
    
	if ((lhs->name == 0) != (rhs->name == 0))
		return FALSE;
    
	if ((lhs->table_name && rhs->table_name) && (strcmp (lhs->table_name, rhs->table_name) != 0))
		return FALSE;
    
	if ((lhs->table_name == 0) != (rhs->table_name == 0))
		return FALSE;
    
	if ((lhs->references && rhs->references) && (strcmp (lhs->references, rhs->references) != 0))
		return FALSE;

	if ((lhs->references == 0) != (rhs->references == 0))
		return FALSE;

	for (i = 0; i < g_list_length (lhs->col_idx_list); i++) 
		if (gda_data_model_column_index_attributes_equal (g_list_nth_data (lhs->col_idx_list, i), 
							    g_list_nth_data (rhs->col_idx_list, i)) == FALSE)
			return FALSE;
    
	return TRUE;
}

/**
 * gda_data_model_index_get_name
 * @dmi: a #GdaDataModelIndex.
 *
 * Returns: the name of @dmi.
 */
const gchar *
gda_data_model_index_get_name (GdaDataModelIndex *dmi)
{
	g_return_val_if_fail (dmi != NULL, NULL);
	return (const gchar *) dmi->name;
}

/**
 * gda_data_model_index_set_name
 * @dmi: a #GdaDataModelIndex.
 * @name: the new name of @dmi.
 *
 * Sets the name of @dmi to @name.
 */
void
gda_data_model_index_set_name (GdaDataModelIndex *dmi, const gchar *name)
{
	g_return_if_fail (dmi != NULL);
	g_return_if_fail (name != NULL);

	if (dmi->name != NULL)
		g_free (dmi->name);
	dmi->name = g_strdup (name);
}

/**
 * gda_data_model_index_get_table_name
 * @dmi: a #GdaDataModelIndex.
 *
 * Returns: the table name of @dmi.
 */
const gchar *
gda_data_model_index_get_table_name (GdaDataModelIndex *dmi)
{
	g_return_val_if_fail (dmi != NULL, NULL);
	return (const gchar *) dmi->table_name;
}

/**
 * gda_data_model_index_set_table_name
 * @dmi: a #GdaDataModelIndex.
 * @name: the new name of @dmi.
 *
 * Sets the table name of @dmi to @table_name.
 */
void
gda_data_model_index_set_table_name (GdaDataModelIndex *dmi, const gchar *table_name)
{
	g_return_if_fail (dmi != NULL);
	g_return_if_fail (table_name != NULL);

	if (dmi->table_name != NULL)
		g_free (dmi->table_name);
	dmi->table_name = g_strdup (table_name);
}

/**
 * gda_data_model_index_get_primary_key
 * @dmi: a @GdaDataModelIndex.
 *
 * Returns: TRUE if primary key.
 */
gboolean
gda_data_model_index_get_primary_key (GdaDataModelIndex *dmi)
{
	g_return_val_if_fail (dmi != NULL, 0);
	return dmi->primary_key;
}

/**
 * gda_data_model_index_set_primary_key
 * @dmi: a #GdaDataModelIndex.
 * @pk: the new primary key setting of @dmi.
 *
 * Sets if a #GdaDataModelIndex is a primary key.
 */
void
gda_data_model_index_set_primary_key (GdaDataModelIndex *dmi, gboolean pk)
{
	g_return_if_fail (dmi != NULL);
	dmi->primary_key = pk;
}

/**
 * gda_data_model_index_get_unique_key
 * @dmi: a @GdaDataModelIndex.
 *
 * Returns: TRUE if unique key.
 */
gboolean
gda_data_model_index_get_unique_key (GdaDataModelIndex *dmi)
{
	g_return_val_if_fail (dmi != NULL, 0);
	return dmi->unique_key;
}

/**
 * gda_data_model_index_set_unique_key
 * @dmi: a #GdaDataModelIndex.
 * @uk: the new primary key setting of @dmi.
 *
 * Sets if a #GdaDataModelIndex is a unique key.
 */
void
gda_data_model_index_set_unique_key (GdaDataModelIndex *dmi, gboolean uk)
{
	g_return_if_fail (dmi != NULL);
	dmi->unique_key = uk;
}

/**
 * gda_data_model_index_get_references
 * @dmi: a #GdaDataModelIndex.
 *
 * Returns: @dmi's references.
 */
const gchar *
gda_data_model_index_get_references (GdaDataModelIndex *dmi)
{
	g_return_val_if_fail (dmi != NULL, NULL);
	return (const gchar *) dmi->references;
}

/**
 * gda_data_model_index_set_references
 * @dmi: a #GdaDataModelIndex.
 * @ref: references.
 *
 * Sets @dmi's @references.
 */
void
gda_data_model_index_set_references (GdaDataModelIndex *dmi, const gchar *ref)
{
	g_return_if_fail (dmi != NULL);

	if (dmi->references != NULL)
		g_free (dmi->references);

	if (ref)
		dmi->references = g_strdup (ref);
}

/**
 * gda_data_model_index_get_column_index_list
 * @dmi: a #GdaDataModelIndex.
 *
 * Returns: @dmi's list of #GdaDataModelColumnIndexAttributes.
 */
GList *
gda_data_model_index_get_column_index_list (GdaDataModelIndex *dmi)
{
	g_return_val_if_fail (dmi != NULL, NULL);
	return (GList *) dmi->col_idx_list;
}

/**
 * gda_data_model_index_set_column_index_list
 * @dmi: a #GdaDataModelIndex.
 * @col_idx_list: list of #GdaDataModelColumnIndexAttributes.
 *
 * Sets @dmi's list of column index attributes by
 * copying @col_idx_list to its internal representation.
 */
void
gda_data_model_index_set_column_index_list (GdaDataModelIndex *dmi, GList *col_idx_list)
{
	gint i;

	g_return_if_fail (dmi != NULL);

	if (dmi->col_idx_list != NULL) {

		/* free column index attributes list */	
		for (i = 0; i < g_list_length (dmi->col_idx_list); i++) 
			gda_data_model_column_index_attributes_free (g_list_nth_data (dmi->col_idx_list, i));

		g_list_free (dmi->col_idx_list);
		dmi->col_idx_list = NULL;
	}

	if (col_idx_list) {

		/* g_list_copy (shallow copy) not good enough */
		for (i = 0; i < g_list_length (col_idx_list); i++) 
			dmi->col_idx_list = g_list_append (dmi->col_idx_list, 
				(GdaDataModelColumnIndexAttributes *) 
				gda_data_model_column_index_attributes_copy (g_list_nth_data (col_idx_list, i)));
	}
}

