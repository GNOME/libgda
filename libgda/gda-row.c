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
	g_return_val_if_fail (attrs != NULL, 0);
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
	gint length;

	g_return_val_if_fail (attrs != NULL, NULL);
	g_return_val_if_fail (num >= 0, NULL);

	length = g_list_length (attrs);
	if (num < 0 || num >= length)
		return NULL;

	l = g_list_nth (attrs, num);
	return l ? (GdaFieldAttributes *) l->data : NULL;
}
