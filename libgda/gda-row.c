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

struct _GdaRow {
	GdaDataModel *model;
	gint number;
	gchar *id;
	GdaValue *fields;
	gint nfields;
};

/**
 * gda_row_new
 * model: the #GdaDataModel this row belongs to.
 * @count: number of #GdaValue in the new #GdaRow.
 *
 * Creates a #GdaRow which can hold @count #GdaValue.
 *
 * Returns: the newly allocated #GdaRow.
 */
GdaRow *
gda_row_new (GdaDataModel *model, gint count)
{
	GdaRow *row = NULL;

	g_return_val_if_fail (count >= 0, NULL);

	row = g_new0 (GdaRow, 1);
	row->model = model;
	row->number = -1;
	row->id = NULL;
	row->nfields = count;
	row->fields = g_new0 (GdaValue, count);

	return row;
}

/**
 * gda_row_new_from_list
 * @values: a list of #GdaValue's.
 *
 * Create a #GdaRow from a list of #GdaValue's.
 *
 * Returns: the newly created row.
 */
GdaRow *
gda_row_new_from_list (GdaDataModel *model, const GList *values)
{
	GdaRow *row;
	const GList *l;
	gint i;

	row = gda_row_new (model, g_list_length ((GList *) values));
	for (i = 0, l = values; l != NULL; l = l->next, i++) {
		const GdaValue *value = (const GdaValue *) l->data;

		if (value)
			gda_value_set_from_value (gda_row_get_value (row, i), value);
		else
			gda_value_set_null (gda_row_get_value (row, i));
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
	gint i;
	
	g_return_if_fail (row != NULL);

	g_free (row->id);
	for (i = 0; i < row->nfields; i++)
		gda_value_set_null (&row->fields [i]);
	g_free (row->fields);
	g_free (row);
}

/**
 * gda_row_get_model
 * @row: a #GdaRow.
 *
 * Get the #GdaDataModel the given #GdaRow belongs to.
 *
 * Returns: a #GdaDataModel.
 */
GdaDataModel *
gda_row_get_model (GdaRow *row)
{
	g_return_val_if_fail (row != NULL, NULL);
	return row->model;
}

/**
 * gda_row_get_number
 * @row: a #GdaRow.
 *
 * Get the number of the given row, that is, its position in its containing
 * data model.
 *
 * Returns: the row number, or -1 if there was an error.
 */
gint
gda_row_get_number (GdaRow *row)
{
	g_return_val_if_fail (row != NULL, -1);
	return row->number;
}

/**
 * gda_row_set_number
 * @row: a #GdaRow.
 * @number: the new row number.
 *
 * Set the row number for the given row.
 */
void
gda_row_set_number (GdaRow *row, gint number)
{
	g_return_if_fail (row != NULL);
	row->number = 0;
}

/**
 * gda_row_get_id
 * @row: a #GdaRow (which contains #GdaValue).
 *
 * Return the unique identifier for this row. This identifier is
 * assigned by the different providers, to uniquely identify
 * rows returned to clients. If there is no ID, this means that
 * the row has not been created by a provider, or that it the
 * provider cannot identify it (ie, modifications to it won't
 * take place into the database).
 *
 * Returns: the unique identifier for this row.
 */
const gchar *
gda_row_get_id (GdaRow *row)
{
	g_return_val_if_fail (row != NULL, NULL);
	return (const gchar *) row->id;
}

/**
 * gda_row_set_id
 * @row: A #GdaRow (which contains #GdaValue).
 * @id: New identifier for the row.
 *
 * Assign a new identifier to the given row. This function is
 * usually called by providers.
 */
void
gda_row_set_id (GdaRow *row, const gchar *id)
{
	g_return_if_fail (row != NULL);

	if (row->id)
		g_free (row->id);
	row->id = g_strdup (id);
}

/**
 * gda_row_get_value
 * @row: a #GdaRow (which contains #GdaValue).
 * @num: field index.
 *
 * Gets a pointer to a #GdaValue stored in a #GdaRow.
 *
 * This is a pointer to the internal array of values. Don't try to free
 * or modify it!
 * 
 * Returns: a pointer to the #GdaValue in the position @num of @row.
 */
G_CONST_RETURN GdaValue *
gda_row_get_value (GdaRow *row, gint num)
{
	g_return_val_if_fail (row != NULL, NULL);
	g_return_val_if_fail (num >= 0 && num < row->nfields, NULL);

	return &row->fields[num];
}

/**
 * gda_row_get_length
 * @row: a #GdaRow
 *
 * Gets the number of columns that the row has.
 */
gint
gda_row_get_length (GdaRow *row)
{
	g_return_val_if_fail (row != NULL, 0);
	return row->nfields;
}

