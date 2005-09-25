/* GDA library
 * Copyright (C) 1998 - 2005 The GNOME Foundation.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *	Álvaro Peña <alvaropg@telefonica.net>
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
#include <string.h>
#include "gda-marshal.h"

#define PARENT_TYPE G_TYPE_OBJECT

struct _GdaRowPrivate {
	GdaDataModel *model;
        gint          number;
        gchar        *id;

        GdaValue     *fields;        /* GdaValue for each column */
        gboolean     *is_default;    /* one gboolean for each column */
        gint          nfields;
};

enum {
	VALUE_TO_CHANGE,
	VALUE_CHANGED,
	LAST_SIGNAL
};

static void gda_row_class_init (GdaRowClass *klass);
static void gda_row_init       (GdaRow *row, GdaRowClass *klass);
static void gda_row_finalize   (GObject *object);

static guint gda_row_signals[LAST_SIGNAL] = { 0, 0 };
static GObjectClass *parent_class = NULL;

static void
gda_row_class_init (GdaRowClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_peek_parent (klass);
	
	gda_row_signals[VALUE_CHANGED] =
		g_signal_new ("value_changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaRowClass, value_changed),
			      NULL, NULL,
			      gda_marshal_VOID__INT_POINTER_POINTER,
			      G_TYPE_NONE,
			      3, G_TYPE_INT, G_TYPE_POINTER, G_TYPE_POINTER);
	gda_row_signals[VALUE_TO_CHANGE] =
		g_signal_new ("value_to_change",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaRowClass, value_to_change),
			      NULL, NULL,
			      gda_marshal_VOID__INT_POINTER_POINTER,
			      G_TYPE_BOOLEAN,
			      3, G_TYPE_INT, G_TYPE_POINTER, G_TYPE_POINTER);

	object_class->finalize = gda_row_finalize;
}

static void
gda_row_init (GdaRow *row, GdaRowClass *klass)
{
	g_return_if_fail (GDA_IS_ROW (row));
	
	row->priv = g_new0 (GdaRowPrivate, 1);
	row->priv->model = NULL;
	row->priv->number = -1;
	row->priv->id = NULL;
	row->priv->fields = NULL;
	row->priv->is_default = FALSE;
	row->priv->nfields = 0;
}

static void
gda_row_finalize (GObject *object)
{
	GdaRow *row = (GdaRow *) object;
	
	g_return_if_fail (GDA_IS_ROW (row));
	
	if (row->priv) {
		gint i;

		if (row->priv->id)
			g_free (row->priv->id);
		for (i = 0; i < row->priv->nfields; i++)
			gda_value_set_null (&(row->priv->fields [i]));
		g_free (row->priv->fields);
		if (row->priv->is_default)
			g_free (row->priv->is_default);

		g_free (row->priv);
		row->priv = NULL;
	}
	
	parent_class->finalize (object);
}

GType
gda_row_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaRowClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_row_class_init,
			NULL,
			NULL,
			sizeof (GdaRow),
			0,
			(GInstanceInitFunc) gda_row_init
		};
		type = g_type_register_static (PARENT_TYPE, "GdaRow", &info, 0);
	}
	
	return type;
}

/**
 * gda_row_new
 * @model: the #GdaDataModel this row belongs to.
 * @count: number of #GdaValue in the new #GdaRow.
 *
 * Creates a #GdaRow which can hold @count #GdaValue values.
 *
 * Returns: a newly allocated #GdaRow object.
 */
GdaRow *
gda_row_new (GdaDataModel *model, gint count)
{
	GdaRow *row;
	gint i;
        GdaValue *value;

        g_return_val_if_fail (count >= 0, NULL);
	row = g_object_new (GDA_TYPE_ROW, NULL);

        row->priv->model = model;
        row->priv->number = -1;
        row->priv->id = NULL;
        row->priv->nfields = count;
        row->priv->fields = g_new0 (GdaValue, count);
        row->priv->is_default = NULL;

	return row;
}

/**
 * gda_row_copy
 * @row: the #GdaRow to copy
 *
 * Copy constructor
 *
 * Returns: a new #GdaRow
 */
GdaRow *
gda_row_copy (GdaRow *row)
{
	GdaRow *newrow;
	gint i;

	g_return_val_if_fail (row && GDA_IS_ROW (row), NULL);
	g_return_val_if_fail (row->priv, NULL);

	newrow = gda_row_new (row->priv->model, row->priv->nfields);
	newrow->priv->number = row->priv->number;
	if (row->priv->id)
		newrow->priv->id = g_strdup (row->priv->id);
	
	/* copy values */
	newrow->priv->fields = g_new0 (GdaValue, row->priv->nfields);
	for (i = 0; i < row->priv->nfields; i++)
		gda_value_set_from_value (&(newrow->priv->fields[i]), gda_row_get_value (row, i));

	/* copy values' attributes */
	if (row->priv->is_default) {
		newrow->priv->is_default = g_new0 (gboolean, row->priv->nfields);
		memcpy (newrow->priv->is_default, row->priv->is_default, sizeof (gboolean) * row->priv->nfields);
	}
}

/**
 * gda_row_new_from_list
 * @model: a #GdaDataModel.
 * @values: a list of #GdaValue's.
 *
 * Creates a #GdaRow from a list of #GdaValue's.  These GdaValue's are
 * value-copied and the user are still resposible for freeing them.
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

                if (value) {
                        GdaValue *dest;
                        dest = gda_row_get_value (row, i);
                        gda_value_reset_with_type (dest, gda_value_get_type (value));
                        gda_value_set_from_value (dest, value);
                }
                else
                        gda_value_set_null (gda_row_get_value (row, i));
        }

        return row;
}

/**
 * gda_row_get_model
 * @row: a #GdaRow.
 *
 * Gets the #GdaDataModel the given #GdaRow belongs to.
 *
 * Returns: a #GdaDataModel.
 */
GdaDataModel *
gda_row_get_model (GdaRow *row)
{
        g_return_val_if_fail (row && GDA_IS_ROW (row), NULL);
	g_return_val_if_fail (row->priv, NULL);

        return row->priv->model;
}

/**
 * gda_row_get_number
 * @row: a #GdaRow.
 *
 * Gets the number of the given row, that is, its position in its containing
 * data model.
 *
 * Returns: the row number, or -1 if there was an error.
 */
gint
gda_row_get_number (GdaRow *row)
{
        g_return_val_if_fail (row && GDA_IS_ROW (row), -1);
	g_return_val_if_fail (row->priv, -1);

        return row->priv->number;
}

/**
 * gda_row_set_number
 * @row: a #GdaRow.
 * @number: the new row number.
 *
 * Sets the row number for the given row.
 */
void
gda_row_set_number (GdaRow *row, gint number)
{
	g_return_if_fail (row && GDA_IS_ROW (row));
	g_return_if_fail (row->priv);

	row->priv->number = number;
}

/**
 * gda_row_get_id
 * @row: a #GdaRow (which contains #GdaValue).
 *
 * Returns the unique identifier for this row. This identifier is
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
        g_return_val_if_fail (row && GDA_IS_ROW (row), NULL);
	g_return_val_if_fail (row->priv, NULL);

        return (const gchar *) row->priv->id;
}

/**
 * gda_row_set_id
 * @row: a #GdaRow (which contains #GdaValue).
 * @id: new identifier for the row.
 *
 * Assigns a new identifier to the given row. This function is
 * usually called by providers.
 */
void
gda_row_set_id (GdaRow *row, const gchar *id)
{
	g_return_if_fail (row && GDA_IS_ROW (row));
	g_return_if_fail (row->priv); 

        if (row->priv->id)
                g_free (row->priv->id);
        row->priv->id = g_strdup (id);
}

/**
 * gda_row_get_value
 * @row: a #GdaRow
 * @num: field index.
 *
 * Gets a pointer to a #GdaValue stored in a #GdaRow.
 *
 * This is a pointer to the internal array of values. Don't try to free
 * or modify it!
 *
 * Returns: a pointer to the #GdaValue in the position @num of @row.
 */
GdaValue *
gda_row_get_value (GdaRow *row, gint num)
{
        g_return_val_if_fail (row && GDA_IS_ROW (row), NULL);
	g_return_val_if_fail (row->priv, NULL);
        g_return_val_if_fail (num >= 0 && num < row->priv->nfields, NULL);

        return & (row->priv->fields[num]);
}

/**
 * gda_row_set_value
 * @row: a #GdaRow
 * @num: field index.
 * @value: a #GdaValue to insert into @row at the @num position, or %NULL
 *
 * Sets the value stored at position @num in @row to be a copy of
 * @value.
 *
 * Returns: TRUE if no error occured.
 */
gboolean
gda_row_set_value (GdaRow *row, gint num, const GdaValue *value)
{
	GdaValue *current, *newval;
	gboolean retval;

        g_return_val_if_fail (row && GDA_IS_ROW (row), FALSE);
	g_return_val_if_fail (row->priv, FALSE);
        g_return_val_if_fail (num >= 0 && num < row->priv->nfields, FALSE);

	if (!value) 
		newval = gda_value_new_null ();
	else
		newval = value;

	current = gda_value_copy (gda_row_get_value (row, num));
	g_signal_emit (G_OBJECT (row),
		       gda_row_signals [VALUE_TO_CHANGE],
		       0, num, current, newval, &retval);
	if (retval)
		retval = gda_value_set_from_value (&(row->priv->fields[num]), newval);
	
	if (retval) {
		const GdaValue *realval;
		realval = gda_row_get_value (row, num);
		g_signal_emit (G_OBJECT (row),
			       gda_row_signals [VALUE_CHANGED],
			       0, num, current, realval);
	}
	gda_value_free (current);
	if (value)
		gda_value_free (newval);

	return retval;
}

/**
 * gda_row_get_length
 * @row: a #GdaRow.
 *
 * Returns: the number of columns that the @row has.
 */
gint
gda_row_get_length (GdaRow *row)
{
        g_return_val_if_fail (row && GDA_IS_ROW (row), 0);
	g_return_val_if_fail (row->priv, 0);

        return row->priv->nfields;
}


/**
 * gda_row_set_default
 * @row: a #GdaRow
 * @num: field index
 * @is_default:
 *
 * Instructs the @row that the value at column @num must be considered as a default value
 */
void
gda_row_set_is_default (GdaRow *row, gint num, gboolean is_default)
{
        g_return_if_fail (row && GDA_IS_ROW (row));
	g_return_if_fail (row->priv);
        g_return_if_fail (num >= 0 && num < row->priv->nfields);

        if (! row->priv->is_default)
                row->priv->is_default = g_new0 (gboolean, row->priv->nfields);
        row->priv->is_default [num] = is_default;
}


/**
 * gda_row_get_is_default
 * @row: a #GdaRow
 * @num: field index
 *
 * Tells if the value at column @num in @row must be considered as a default value
 *
 * Returns:
 */
gboolean
gda_row_get_is_default (GdaRow *row, gint num)
{
        g_return_val_if_fail (row && GDA_IS_ROW (row), FALSE);
	g_return_val_if_fail (row->priv, FALSE);
        g_return_val_if_fail (num >= 0 && num < row->priv->nfields, FALSE);

        if (row->priv->is_default)
                return row->priv->is_default [num];
        else
                return FALSE;
}
