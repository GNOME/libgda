/* GDA library
 * Copyright (C) 1998 - 2006 The GNOME Foundation.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *	Álvaro Peña <alvaropg@telefonica.net>
 *      Vivien Malerba <malerba@gnome-db.org>
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

#include "gda-row.h"
#include <string.h>
#include "gda-marshal.h"
#include "gda-data-model.h"

#define PARENT_TYPE G_TYPE_OBJECT

struct _GdaRowPrivate {
	GdaDataModel *model; /* can be NULL */
        gint          number;
        gchar        *id;

        GValue     *fields;        /* GValue for each column */
        gboolean     *is_default;    /* one gboolean for each column */
        gint          nfields;
};

/* signals */
enum {
	VALUE_TO_CHANGE,
	VALUE_CHANGED,
	LAST_SIGNAL
};

/* properties */
enum
{
        PROP_0,
        PROP_MODEL,
        PROP_VALUES,
        PROP_NB_VALUES
};

static void gda_row_class_init (GdaRowClass *klass);
static void gda_row_init       (GdaRow *row, GdaRowClass *klass);
static void gda_row_finalize   (GObject *object);
static void gda_row_dispose    (GObject *object);

static void gda_row_set_property (GObject *object,
				  guint param_id,
				  const GValue *value,
				  GParamSpec *pspec);
static void gda_row_get_property (GObject *object,
				  guint param_id,
				  GValue *value,
				  GParamSpec *pspec);

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
			      gda_marshal_VOID__INT_BOXED_BOXED,
			      G_TYPE_NONE,
			      3, G_TYPE_INT, G_TYPE_VALUE, G_TYPE_VALUE);
	gda_row_signals[VALUE_TO_CHANGE] =
		g_signal_new ("value_to_change",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaRowClass, value_to_change),
			      NULL, NULL,
			      gda_marshal_VOID__INT_BOXED_BOXED,
			      G_TYPE_BOOLEAN,
			      3, G_TYPE_INT, G_TYPE_VALUE, G_TYPE_VALUE);

	object_class->finalize = gda_row_finalize;
	object_class->dispose = gda_row_dispose;

	/* Properties */
        object_class->set_property = gda_row_set_property;
        object_class->get_property = gda_row_get_property;

	g_object_class_install_property (object_class, PROP_MODEL,
                                         g_param_spec_object ("model", NULL, NULL, 
                                                               GDA_TYPE_DATA_MODEL,
							       G_PARAM_READABLE | G_PARAM_WRITABLE));

        /* Note that this is actually a GList*, 
         * but there does not seem to be a way to define that in the properties system:
         */
	g_object_class_install_property (object_class, PROP_VALUES,
                                         g_param_spec_pointer ("values", NULL, NULL, 
							       G_PARAM_WRITABLE));
	g_object_class_install_property (object_class, PROP_NB_VALUES,
                                         g_param_spec_int ("nb_values", NULL, NULL,
							   1, G_MAXINT, 1, 
							   G_PARAM_WRITABLE));
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
	row->priv->is_default = NULL;
	row->priv->nfields = 0;
}

static void
gda_row_dispose (GObject *object)
{
	GdaRow *row = (GdaRow *) object;
	
	g_return_if_fail (GDA_IS_ROW (row));
	
	if (row->priv->model)
		gda_row_set_model (row, NULL);
	
	parent_class->finalize (object);
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

static void
gda_row_set_property (GObject *object,
		      guint param_id,
		      const GValue *value,
		      GParamSpec *pspec)
{
        GdaRow *row;

        row = GDA_ROW (object);
        if (row->priv) {
                switch (param_id) {
                case PROP_MODEL:
			gda_row_set_model (row, GDA_DATA_MODEL (g_value_get_object(value)));
                        break;
                case PROP_VALUES: {
			const GList *l;
			GList *values;
			gint i;
			
			g_return_if_fail (!row->priv->fields);

			values = (GList *) g_value_get_pointer (value);
			i = g_list_length (values);

			row->priv->nfields = i;
			row->priv->fields = g_new0 (GValue, row->priv->nfields);
			
			for (i = 0, l = values; l != NULL; l = l->next, i++) {
				const GValue *value = (const GValue *) l->data;
				
				if (value) {
					GValue *dest;
					dest = gda_row_get_value (row, i);
					gda_value_reset_with_type (dest, G_VALUE_TYPE ((GValue *) value));
					gda_value_set_from_value (dest, value);
				}
				else
					gda_value_set_null (gda_row_get_value (row, i));
			}
                        break;
		}
		case PROP_NB_VALUES:
			g_return_if_fail (!row->priv->fields);

			row->priv->nfields = g_value_get_int (value);
			row->priv->fields = g_new0 (GValue, row->priv->nfields);			
			break;
		default:
			g_assert_not_reached ();
			break;
                }
        }
}

static void
gda_row_get_property (GObject *object,
		      guint param_id,
		      GValue *value,
		      GParamSpec *pspec)
{
        GdaRow *row;

        row = GDA_ROW (object);
        if (row->priv) {
                switch (param_id) {
                case PROP_MODEL:
			g_value_set_object (value, G_OBJECT (gda_row_get_model (row)));
                        break;
                case PROP_VALUES:
		case PROP_NB_VALUES:
			g_assert_not_reached ();
			break;
                }
        }
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
 * @model: the #GdaDataModel this row belongs to, or %NULL if the row is outside any data model
 * @count: number of #GValue in the new #GdaRow.
 *
 * Creates a #GdaRow which can hold @count #GValue values.
 *
 * The caller of this function is the only owner of a reference to the newly created #GdaRow
 * object, even if @model is not %NULL (it is recommended to pass %NULL as the @model argument
 * if this function is not called from within a #GdaDataModel implementation).
 *
 * Returns: a newly allocated #GdaRow object.
 */
GdaRow *
gda_row_new (GdaDataModel *model, gint count)
{
	GdaRow *row = NULL;

	if (model)
		g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);
        g_return_val_if_fail (count > 0, NULL);

	row = g_object_new (GDA_TYPE_ROW, "model", model, "nb_values", count, NULL);

	return row;
}

/**
 * gda_row_copy
 * @row: the #GdaRow to copy
 *
 * Copy constructor.
 *
 * Returns: a new #GdaRow
 */
GdaRow *
gda_row_copy (GdaRow *row)
{
	GdaRow *newrow;
	gint i;

	g_return_val_if_fail (GDA_IS_ROW (row), NULL);
	g_return_val_if_fail (row->priv, NULL);

	newrow = g_object_new (GDA_TYPE_ROW, "model", row->priv->model, "nb_values", row->priv->nfields, NULL);

	newrow->priv->number = row->priv->number;
	if (row->priv->id)
		newrow->priv->id = g_strdup (row->priv->id);
	
	/* copy values */
	newrow->priv->fields = g_new0 (GValue, row->priv->nfields);
	for (i = 0; i < row->priv->nfields; i++) {
		GValue *origval = gda_row_get_value (row, i);
		g_value_init (&(newrow->priv->fields[i]), G_VALUE_TYPE (origval));
		gda_value_set_from_value (&(newrow->priv->fields[i]), origval);
	}

	/* copy values' attributes */
	if (row->priv->is_default) {
		newrow->priv->is_default = g_new0 (gboolean, row->priv->nfields);
		memcpy (newrow->priv->is_default, row->priv->is_default, sizeof (gboolean) * row->priv->nfields);
	}

	return newrow;
}

/**
 * gda_row_new_from_list
 * @model: a #GdaDataModel this row belongs to, or %NULL if the row is outside any data model
 * @values: a list of #GValue's.
 *
 * Creates a #GdaRow from a list of #GValue's.  These GValue's are
 * value-copied and the user are still responsible for freeing them.
 *
 * See the gda_row_new() function's documentation for more information about the @model attribute
 *
 * Returns: the newly created row.
 */
GdaRow *
gda_row_new_from_list (GdaDataModel *model, const GList *values)
{
        GdaRow *row;
	
	if (model)
		g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);
        row = g_object_new (GDA_TYPE_ROW, "model", model, "values", values, NULL);

        return row;
}

/**
 * gda_row_set_model
 * @row: a #GdaRow.
 * @model: a #GdaDataModel this row belongs to, or %NULL if the row is outside any data model
 *
 * Set the #GdaDataModel the given #GdaRow belongs to. Note that calling
 * this method should be reserved to GdaDataModel implementations and should
 * therefore not be called by the user.
 */
void
gda_row_set_model (GdaRow *row, GdaDataModel *model)
{
        g_return_if_fail (GDA_IS_ROW (row));
	g_return_if_fail (row->priv);

	if (row->priv->model) {
		g_object_remove_weak_pointer (G_OBJECT (row->priv->model), (gpointer *) &(row->priv->model));
		row->priv->model = NULL;
	}

	if (model) {
		g_return_if_fail (GDA_IS_DATA_MODEL (model));
		row->priv->model = model;
		g_object_add_weak_pointer (G_OBJECT (model), (gpointer *) &(row->priv->model));
	}
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
        g_return_val_if_fail (GDA_IS_ROW (row), NULL);
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
        g_return_val_if_fail (GDA_IS_ROW (row), -1);
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
	g_return_if_fail (GDA_IS_ROW (row));
	g_return_if_fail (row->priv);

	row->priv->number = number;
}

/**
 * gda_row_get_id
 * @row: a #GdaRow (which contains #GValue).
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
        g_return_val_if_fail (GDA_IS_ROW (row), NULL);
	g_return_val_if_fail (row->priv, NULL);

        return (const gchar *) row->priv->id;
}

/**
 * gda_row_set_id
 * @row: a #GdaRow (which contains #GValue).
 * @id: new identifier for the row.
 *
 * Assigns a new identifier to the given row. This function is
 * usually called by providers.
 */
void
gda_row_set_id (GdaRow *row, const gchar *id)
{
	g_return_if_fail (GDA_IS_ROW (row));
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
 * Gets a pointer to a #GValue stored in a #GdaRow.
 *
 * This is a pointer to the internal array of values. Don't try to free
 * or modify it!
 *
 * Returns: a pointer to the #GValue in the position @num of @row.
 */
GValue *
gda_row_get_value (GdaRow *row, gint num)
{
        g_return_val_if_fail (GDA_IS_ROW (row), NULL);
	g_return_val_if_fail (row->priv, NULL);
        g_return_val_if_fail (num >= 0 && num < row->priv->nfields, NULL);

        return & (row->priv->fields[num]);
}

/**
 * gda_row_set_value
 * @row: a #GdaRow
 * @num: field index.
 * @value: a #GValue to insert into @row at the @num position, or %NULL
 *
 * Sets the value stored at position @num in @row to be a copy of
 * @value.
 *
 * Returns: TRUE if no error occurred.
 */
gboolean
gda_row_set_value (GdaRow *row, gint num, const GValue *value)
{
	GValue *current, *newval;
	gboolean retval;

        g_return_val_if_fail (GDA_IS_ROW (row), FALSE);
	g_return_val_if_fail (row->priv, FALSE);
        g_return_val_if_fail (num >= 0 && num < row->priv->nfields, FALSE);

	if (!value) 
		newval = gda_value_new_null ();
	else
		newval = (GValue *) value;

	current = gda_row_get_value (row, num);
	g_signal_emit (G_OBJECT (row),
		       gda_row_signals [VALUE_TO_CHANGE],
		       0, num, current, newval, &retval);
	
	/* FIXME: it seems the return value is always FALSE */
	retval = TRUE;

	if (retval) {
		current = gda_value_copy (gda_row_get_value (row, num));
		if (value) {
			if (gda_value_is_null (&(row->priv->fields[num])))
				gda_value_reset_with_type (&(row->priv->fields[num]),
							   G_VALUE_TYPE (newval));
			retval = gda_value_set_from_value (&(row->priv->fields[num]), newval);
		}
		else
			gda_value_set_null (&(row->priv->fields[num]));
		
		if (retval) {
			const GValue *realval;
			realval = gda_row_get_value (row, num);
			g_signal_emit (G_OBJECT (row),
				       gda_row_signals [VALUE_CHANGED],
				       0, num, current, realval);
		}
		gda_value_free (current);
	}

	if (!value)
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
        g_return_val_if_fail (GDA_IS_ROW (row), 0);
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
        g_return_if_fail (GDA_IS_ROW (row));
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
        g_return_val_if_fail (GDA_IS_ROW (row), FALSE);
	g_return_val_if_fail (row->priv, FALSE);
        g_return_val_if_fail (num >= 0 && num < row->priv->nfields, FALSE);

        if (row->priv->is_default)
                return row->priv->is_default [num];
        else
                return FALSE;
}
