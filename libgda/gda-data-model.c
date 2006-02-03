/* 
 * GDA common library
 * Copyright (C) 1998 - 2006 The GNOME Foundation.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *	Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
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

#include <glib/gi18n-lib.h>
#include <glib/gprintf.h>
#include <libgda/gda-data-model.h>
#include <libgda/gda-data-model-extra.h>
#include <libgda/gda-data-model-iter.h>
#include <libgda/gda-log.h>
#include <libgda/gda-util.h>
#include <libgda/gda-row.h>
#include <libgda/gda-object.h>
#include <string.h>

#define PARENT_TYPE G_TYPE_OBJECT
#define CLASS(model) (GDA_DATA_MODEL_CLASS (G_OBJECT_GET_CLASS (model)))


static void gda_data_model_class_init (gpointer g_class);

/* signals */
enum {
	ROW_INSERTED,
	ROW_UPDATED,
	ROW_REMOVED,
	LAST_SIGNAL
};

static guint gda_data_model_signals[LAST_SIGNAL];

GType
gda_data_model_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaDataModelClass),
			(GBaseInitFunc) gda_data_model_class_init,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) NULL,
			NULL,
			NULL,
			0,
			0,
			(GInstanceInitFunc) NULL
		};
		
		type = g_type_register_static (G_TYPE_INTERFACE, "GdaDataModel", &info, 0);
		g_type_interface_add_prerequisite (type, GDA_TYPE_OBJECT);
	}
	return type;
}

static void
gda_data_model_class_init (gpointer g_class)
{
	static gboolean initialized = FALSE;

	if (! initialized) {
		gda_data_model_signals[ROW_INSERTED] =
			g_signal_new ("row_inserted",
				      GDA_TYPE_DATA_MODEL,
				      G_SIGNAL_RUN_LAST,
				      G_STRUCT_OFFSET (GdaDataModelClass, row_inserted),
				      NULL, NULL,
				      g_cclosure_marshal_VOID__INT,
				      G_TYPE_NONE, 1, G_TYPE_INT);
		gda_data_model_signals[ROW_UPDATED] =
			g_signal_new ("row_updated",
				      GDA_TYPE_DATA_MODEL,
				      G_SIGNAL_RUN_LAST,
				      G_STRUCT_OFFSET (GdaDataModelClass, row_updated),
				      NULL, NULL,
				      g_cclosure_marshal_VOID__INT,
				      G_TYPE_NONE, 1, G_TYPE_INT);
		gda_data_model_signals[ROW_REMOVED] =
			g_signal_new ("row_removed",
				      GDA_TYPE_DATA_MODEL,
				      G_SIGNAL_RUN_LAST,
				      G_STRUCT_OFFSET (GdaDataModelClass, row_removed),
				      NULL, NULL,
				      g_cclosure_marshal_VOID__INT,
				      G_TYPE_NONE, 1, G_TYPE_INT);

		initialized = TRUE;
	}
}

static gboolean
do_notify_changes (GdaDataModel *model)
{
	gboolean notify_changes = TRUE;
	if (GDA_DATA_MODEL_GET_CLASS (model)->i_get_notify)
		notify_changes = (GDA_DATA_MODEL_GET_CLASS (model)->i_get_notify) (model);
	return notify_changes;
}

/**
 * gda_data_model_changed
 * @model: a #GdaDataModel object.
 *
 * Notifies listeners of the given data model object of changes
 * in the underlying data. Listeners usually will connect
 * themselves to the "changed" signal in the #GdaDataModel
 * class, thus being notified of any new data being appended
 * or removed from the data model.
 */
void
gda_data_model_changed (GdaDataModel *model)
{
	g_return_if_fail (GDA_IS_DATA_MODEL (model));
	
	if (do_notify_changes (model)) 
		gda_object_changed (GDA_OBJECT (model));
}

/**
 * gda_data_model_row_inserted
 * @model: a #GdaDataModel object.
 * @row: row number.
 *
 * Emits the 'row_inserted' and 'changed' signals on @model.
 */
void
gda_data_model_row_inserted (GdaDataModel *model, gint row)
{
	g_return_if_fail (GDA_IS_DATA_MODEL (model));

	/* update column's data types if they are not yet defined */
	if (gda_data_model_get_n_rows (model) == 1) {
		GdaColumn *column;
		gint i, nbcols;
		const GdaValue *value;

		nbcols = gda_data_model_get_n_columns (model);
		for (i = 0; i < nbcols; i++) {
			column = gda_data_model_describe_column (model, i);
			value = gda_data_model_get_value_at (model, i, 0);
			if ((gda_column_get_gda_type (column) == GDA_VALUE_TYPE_UNKNOWN))
				gda_column_set_gda_type (column, 
							 value ? gda_value_get_type ((GdaValue *)value) : GDA_VALUE_TYPE_NULL);
		}
	}

	/* notify changes */
	if (do_notify_changes (model)) {
		g_signal_emit (G_OBJECT (model),
			       gda_data_model_signals[ROW_INSERTED],
			       0, row);

		gda_data_model_changed (model);
	}
}

/**
 * gda_data_model_row_updated
 * @model: a #GdaDataModel object.
 * @row: row number.
 *
 * Emits the 'row_updated' and 'changed' signals on @model.
 */
void
gda_data_model_row_updated (GdaDataModel *model, gint row)
{
	g_return_if_fail (GDA_IS_DATA_MODEL (model));

	if (do_notify_changes (model)) {
		g_signal_emit (G_OBJECT (model),
			       gda_data_model_signals[ROW_UPDATED],
			       0, row);

		gda_data_model_changed (model);
	}
}

/**
 * gda_data_model_row_removed
 * @model: a #GdaDataModel object.
 * @row: row number.
 *
 * Emits the 'row_removed' and 'changed' signal on @model.
 */
void
gda_data_model_row_removed (GdaDataModel *model, gint row)
{
	g_return_if_fail (GDA_IS_DATA_MODEL (model));

	if (do_notify_changes (model)) {
		g_signal_emit (G_OBJECT (model),
			       gda_data_model_signals[ROW_REMOVED],
			       0, row);

		gda_data_model_changed (model);
	}
}

/**
 * gda_data_model_freeze
 * @model: a #GdaDataModel object.
 *
 * Disables notifications of changes on the given data model. To
 * re-enable notifications again, you should call the
 * #gda_data_model_thaw function.
 */
void
gda_data_model_freeze (GdaDataModel *model)
{
	g_return_if_fail (GDA_IS_DATA_MODEL (model));
	
	if (GDA_DATA_MODEL_GET_CLASS (model)->i_set_notify)
		(GDA_DATA_MODEL_GET_CLASS (model)->i_set_notify) (model, FALSE);
	else
		g_warning ("%s() method not supported\n", __FUNCTION__);
}

/**
 * gda_data_model_thaw
 * @model: a #GdaDataModel object.
 *
 * Re-enables notifications of changes on the given data model.
 */
void
gda_data_model_thaw (GdaDataModel *model)
{
	g_return_if_fail (GDA_IS_DATA_MODEL (model));

	if (GDA_DATA_MODEL_GET_CLASS (model)->i_set_notify)
		(GDA_DATA_MODEL_GET_CLASS (model)->i_set_notify) (model, TRUE);
	else
		g_warning ("%s() method not supported\n", __FUNCTION__);
}

/**
 * gda_data_model_is_updatable
 * @model: a #GdaDataModel object.
 *
 * Tells if @model can be modified
 *
 * Returns: TRUE if @model can be modified
 */
gboolean
gda_data_model_is_updatable (GdaDataModel *model)
{
	guint flags;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);
	flags = gda_data_model_get_access_flags (model);
	return (flags & GDA_DATA_MODEL_ACCESS_WRITE);
}

/**
 * gda_data_model_get_access_flags
 * @model: a #GdaDataModel object.
 *
 * Get the attributes of @model such as how to access the data it contains if it's modifiable, etc.
 *
 * Returns: an ORed value of #GdaDataModelAccessFlags flags
 */
guint
gda_data_model_get_access_flags (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), 0);
	if (GDA_DATA_MODEL_GET_CLASS (model)->i_get_access_flags)
		return (GDA_DATA_MODEL_GET_CLASS (model)->i_get_access_flags) (model);
	else
		return 0;
}

/**
 * gda_data_model_get_n_rows
 * @model: a #GdaDataModel object.
 *
 * Returns: the number of rows in the given data model.
 */
gint
gda_data_model_get_n_rows (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), -1);

	if (GDA_DATA_MODEL_GET_CLASS (model)->i_get_n_rows)
		return (GDA_DATA_MODEL_GET_CLASS (model)->i_get_n_rows) (model);
	else {
		g_warning ("%s() method not supported\n", __FUNCTION__);
		return -1;
	}
}

/**
 * gda_data_model_get_n_columns
 * @model: a #GdaDataModel object.
 *
 * Returns: the number of columns in the given data model.
 */
gint
gda_data_model_get_n_columns (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), -1);
	
	if (GDA_DATA_MODEL_GET_CLASS (model)->i_get_n_columns)
		return (GDA_DATA_MODEL_GET_CLASS (model)->i_get_n_columns) (model);
	else {
		g_warning ("%s() method not supported\n", __FUNCTION__);
		return -1;
	}
}

/**
 * gda_data_model_describe_column
 * @model: a #GdaDataModel object.
 * @col: column number.
 *
 * Queries the underlying data model implementation for a description
 * of a given column. That description is returned in the form of
 * a #GdaColumn structure, which contains all the information
 * about the given column in the data model.
 *
 * WARNING: the returned #GdaColumn object belongs to the @model model and
 * and should not be destroyed; any modification will impact the whole data model.
 *
 * Returns: the description of the column.
 */
GdaColumn *
gda_data_model_describe_column (GdaDataModel *model, gint col)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

	if (GDA_DATA_MODEL_GET_CLASS (model)->i_describe_column)
		return (GDA_DATA_MODEL_GET_CLASS (model)->i_describe_column) (model, col);
	else {
		g_warning ("%s() method not supported\n", __FUNCTION__);
		return NULL;
	}
}

/**
 * gda_data_model_get_column_title
 * @model: a #GdaDataModel object.
 * @col: column number.
 *
 * Returns: the title for the given column in a data model object.
 */
const gchar *
gda_data_model_get_column_title (GdaDataModel *model, gint col)
{
	GdaColumn *column;
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

	column = gda_data_model_describe_column (model, col);
	if (column)
		return gda_column_get_title (column);
	else {
		g_warning ("%s() method not supported\n", __FUNCTION__);
		return NULL;
	}
}

/**
 * gda_data_model_set_column_title
 * @model: a #GdaDataModel object.
 * @col: column number
 * @title: title for the given column.
 *
 * Sets the @title of the given @col in @model.
 */
void
gda_data_model_set_column_title (GdaDataModel *model, gint col, const gchar *title)
{
	GdaColumn *column;
	g_return_if_fail (GDA_IS_DATA_MODEL (model));

	column = gda_data_model_describe_column (model, col);
	if (column)
		gda_column_set_title (column, title);
	else 
		g_warning ("%s() method not supported\n", __FUNCTION__);
}

/**
 * gda_data_model_get_value_at
 * @model: a #GdaDataModel object.
 * @col: a valid column number.
 * @row: a valid row number.
 *
 * Retrieves the data stored in the given position (identified by
 * the @col and @row parameters) on a data model.
 *
 * This is the main function for accessing data in a model.
 *
 * Note that the returned #GdaValue must not be modified directly (unexpected behaviours may
 * occur if you do so). If you want to
 * modify a value stored in a #GdaDataModel, use the gda_data_model_set_value() method.
 *
 * Returns: a #GdaValue containing the value stored in the given
 * position, or %NULL on error (out-of-bound position, etc).
 */
const GdaValue *
gda_data_model_get_value_at (GdaDataModel *model, gint col, gint row)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

	if (GDA_DATA_MODEL_GET_CLASS (model)->i_get_value_at)
		return (GDA_DATA_MODEL_GET_CLASS (model)->i_get_value_at) (model, col, row);
	else {
		g_warning ("%s() method not supported\n", __FUNCTION__);
		return NULL;
	}
}

/**
 * gda_data_model_get_attributes_at
 * @model: a #GdaDataModel object
 * @col: a valid column number
 * @row: a valid row number, or -1
 *
 * Get the attributes of the value stored at (row, col) in @proxy, which
 * is an ORed value of #GdaValueAttribute flags. As a special case, if
 * @row is -1, then the attributes returned correspond to a "would be" value
 * if a row was added to @model.
 *
 * Returns: the attributes
 */
guint
gda_data_model_get_attributes_at (GdaDataModel *model, gint col, gint row)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), 0);

	if (GDA_DATA_MODEL_GET_CLASS (model)->i_get_attributes_at)
		return (GDA_DATA_MODEL_GET_CLASS (model)->i_get_attributes_at) (model, col, row);
	else 
		return 0;
}

/**
 * gda_data_model_set_value_at
 * @model: a #GdaDataModel object.
 * @col: column number.
 * @row: row number.
 * @value: a #GdaValue, or %NULL
 * @error: a place to store errors, or %NULL
 *
 * Returns: TRUE if the value in the data model has been updated and no error occured
 */
gboolean
gda_data_model_set_value_at (GdaDataModel *model, gint col, gint row, const GdaValue *value, GError **error)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);

	if (GDA_DATA_MODEL_GET_CLASS (model)->i_set_value_at)
		return (GDA_DATA_MODEL_GET_CLASS (model)->i_set_value_at) (model, col, row, value, error);
	else {
		g_warning ("%s() method not supported\n", __FUNCTION__);
		return FALSE;
	}
}

/**
 * gda_data_model_set_values
 * @model: a #GdaDataModel object.
 * @row: row number.
 * @values: a list of #GdaValue, one for each n (<nb_cols) columns of @model
 * @error: a place to store errors, or %NULL
 *
 * Returns: TRUE if the value in the data model has been updated and no error occured
 */
gboolean
gda_data_model_set_values (GdaDataModel *model, gint row, GList *values, GError **error)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);

	if (GDA_DATA_MODEL_GET_CLASS (model)->i_set_values)
		return (GDA_DATA_MODEL_GET_CLASS (model)->i_set_values) (model, row, values, error);
	else {
		g_warning ("%s() method not supported\n", __FUNCTION__);
		return FALSE;
	}
}

/**
 * gda_data_model_create_iter
 * @model: a #GdaDataModel object.
 *
 * Creates a new iterator object #GdaDataModelIter object which can be used to iterate through
 * rows in @model.
 *
 * The row the returned #GdaDataModelIter represents is undefined. For models which can be accessed 
 * randomly the correspoding row can be set using gda_data_model_iter_at_row(), 
 * and for models which are accessible sequentially only then the first row will be
 * fetched using gda_data_model_iter_next().
 *
 * Returns: a new #GdaDataModelIter object, or %NULL if an error occured
 */
GdaDataModelIter *
gda_data_model_create_iter (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);
	if (GDA_DATA_MODEL_GET_CLASS (model)->i_create_iter)
		return (GDA_DATA_MODEL_GET_CLASS (model)->i_create_iter) (model);
	else 
		/* default method */
		return gda_data_model_iter_new (model);
}

/**
 * gda_data_model_iter_at_row
 * @model: a #GdaDataModel object.
 * @iter: a #GdaDataModelIter object
 * @row: row number.
 *
 * Sets @iter to represent the @row row. @iter must be a valid iterator object obtained
 * using the gda_data_model_create_iter() method.
 *
 * Returns: TRUE if no error occured
 */
gboolean
gda_data_model_iter_at_row (GdaDataModel *model, GdaDataModelIter *iter, gint row)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);
	if (GDA_DATA_MODEL_GET_CLASS (model)->i_iter_at_row)
		return (GDA_DATA_MODEL_GET_CLASS (model)->i_iter_at_row) (model, iter, row);
	else 
		/* default method */
		return gda_data_model_iter_at_row_default (model, iter, row);
}

gboolean
gda_data_model_iter_at_row_default (GdaDataModel *model, GdaDataModelIter *iter, gint row)
{
	/* default method */
	GSList *list;
	gint col;
	GdaDataModel *test;
	gboolean update_model;
	
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);

	/* validity tests */
	if (row >= gda_data_model_get_n_rows (model)) {
		gda_data_model_iter_set_invalid (iter);
		g_object_set (G_OBJECT (iter), "current_row", -1, NULL);
		return FALSE;
	}
	
	if (! (gda_data_model_get_access_flags (model) & GDA_DATA_MODEL_ACCESS_RANDOM))
		return FALSE;
	
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ITER (iter), FALSE);
	
	g_object_get (G_OBJECT (iter), "data_model", &test, NULL);
	g_return_val_if_fail (test == model, FALSE);
	
	/* actual sync. */
	g_object_get (G_OBJECT (iter), "update_model", &update_model, NULL);
	g_object_set (G_OBJECT (iter), "update_model", FALSE, NULL);
	col = 0;
	list = ((GdaParameterList *) iter)->parameters;
	while (list) {
		gda_parameter_set_value (GDA_PARAMETER (list->data), 
					 gda_data_model_get_value_at (model, col, row));
		list = g_slist_next (list);
		col ++;
	}
	g_object_set (G_OBJECT (iter), "current_row", row, "update_model", update_model, NULL);
	return TRUE;
}

/**
 * gda_data_model_iter_next
 * @model: a #GdaDataModel object.
 * @iter: a #GdaDataModelIter object
 *
 * Sets @iter to the next available row in @model. @iter must be a valid iterator object obtained
 * using the gda_data_model_create_iter() method.
 *
 * Returns: TRUE if no error occured
 */
gboolean
gda_data_model_iter_next (GdaDataModel *model, GdaDataModelIter *iter)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);
	if (GDA_DATA_MODEL_GET_CLASS (model)->i_iter_next)
		return (GDA_DATA_MODEL_GET_CLASS (model)->i_iter_next) (model, iter);
	else {
		/* default method */
		GSList *list;
		gint col;
		gint row;
		GdaDataModel *test;

		/* validity tests */
		if (! (gda_data_model_get_access_flags (model) & GDA_DATA_MODEL_ACCESS_RANDOM))
			return FALSE;
		
		g_return_val_if_fail (GDA_IS_DATA_MODEL_ITER (iter), FALSE);

		g_object_get (G_OBJECT (iter), "data_model", &test, NULL);
		g_return_val_if_fail (test == model, FALSE);

		g_object_get (G_OBJECT (iter), "current_row", &row, NULL);
		row++;
		if (row >= gda_data_model_get_n_rows (model))
			return FALSE;

		/* actual sync. */
		col = 0;
		list = ((GdaParameterList *) iter)->parameters;
		while (list) {
			gda_parameter_set_value (GDA_PARAMETER (list->data), 
						 gda_data_model_get_value_at (model, col, row));
			list = g_slist_next (list);
			col ++;
		}
		g_object_set (G_OBJECT (iter), "current_row", row, NULL);
		return TRUE;
	}
}

/**
 * gda_data_model_iter_prev
 * @model: a #GdaDataModel object.
 * @iter: a #GdaDataModelIter object
 *
 * Sets @iter to the previous available row in @model. @iter must be a valid iterator object obtained
 * using the gda_data_model_create_iter() method.
 *
 * Returns: TRUE if no error occured
 */
gboolean
gda_data_model_iter_prev (GdaDataModel *model, GdaDataModelIter *iter)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);
	if (GDA_DATA_MODEL_GET_CLASS (model)->i_iter_prev)
		return (GDA_DATA_MODEL_GET_CLASS (model)->i_iter_prev) (model, iter);
	else {
		/* default method */
		GSList *list;
		gint col;
		gint row;
		GdaDataModel *test;

		/* validity tests */
		if (! (gda_data_model_get_access_flags (model) & GDA_DATA_MODEL_ACCESS_RANDOM))
			return FALSE;
		
		g_return_val_if_fail (GDA_IS_DATA_MODEL_ITER (iter), FALSE);

		g_object_get (G_OBJECT (iter), "data_model", &test, NULL);
		g_return_val_if_fail (test == model, FALSE);

		g_object_get (G_OBJECT (iter), "current_row", &row, NULL);
		row--;
		if (row < 0)
			return FALSE;

		/* actual sync. */
		col = 0;
		list = ((GdaParameterList *) iter)->parameters;
		while (list) {
			gda_parameter_set_value (GDA_PARAMETER (list->data), 
						 gda_data_model_get_value_at (model, col, row));
			list = g_slist_next (list);
			col ++;
		}
		g_object_set (G_OBJECT (iter), "current_row", row, NULL);
		return TRUE;
	}
}


/**
 * gda_data_model_append_values
 * @model: a #GdaDataModel object.
 * @values: #GList of #GdaValue* representing the row to add.  The
 *          length must match model's column count.  These #GdaValue
 *	    are value-copied.  The user is still responsible for freeing them.
 * @error: a place to store errors, or %NULL
 *
 * Appends a row to the given data model.
 *
 * Returns: the number of the added row, or -1 if an error occured
 */
gint
gda_data_model_append_values (GdaDataModel *model, const GList *values, GError **error)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), -1);

	if (GDA_DATA_MODEL_GET_CLASS (model)->i_append_values) 
		return (GDA_DATA_MODEL_GET_CLASS (model)->i_append_values) (model, values, error);
	else {
		g_warning ("%s() method not supported\n", __FUNCTION__);
		return -1;
	}
}

/**
 * gda_data_model_append_row
 * @model: a #GdaDataModel object.
 * @error: a place to store errors, or %NULL
 * 
 * Appends a row to the data model. 
 *
 * Returns: the number of the added row, or -1 if an error occured
 */
gint
gda_data_model_append_row (GdaDataModel *model, GError **error)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);

	if (GDA_DATA_MODEL_GET_CLASS (model)->i_append_row) 
		return (GDA_DATA_MODEL_GET_CLASS (model)->i_append_row) (model, error);
	else {
		g_warning ("%s() method not supported\n", __FUNCTION__);
		return -1;
	}
}


/**
 * gda_data_model_remove_row
 * @model: a #GdaDataModel object.
 * @row: the row number to be removed.
 * @error: a place to store errors, or %NULL
 *
 * Removes a row from the data model.
 *
 * Returns: %TRUE if successful, %FALSE otherwise.
 */
gboolean
gda_data_model_remove_row (GdaDataModel *model, gint row, GError **error)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);

	if (GDA_DATA_MODEL_GET_CLASS (model)->i_remove_row)
		return (GDA_DATA_MODEL_GET_CLASS (model)->i_remove_row) (model, row, error);
	else {
		g_warning ("%s() method not supported\n", __FUNCTION__);
		return FALSE;
	}
}

/**
 * gda_data_model_get_row_from_values
 * @model: a #GdaDataModel object.
 * @values: a list of #GdaValue values
 * @cols_index: an array of #gint containing the column number to match each value of @values
 *
 * Returns the first row where all the values in @values at the columns identified at
 * @cols_index match. If the row can't be identified, then returns -1;
 *
 * NOTE: the @cols_index array MUST contain a column index for each value in @values
 *
 * Returns: the requested row number, of -1 if not found
 */
gint
gda_data_model_get_row_from_values (GdaDataModel *model, GSList *values, gint *cols_index)
{
	gint row = -1;
        gint current_row, n_rows, n_cols;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), -1);
	g_return_val_if_fail (values, -1);

	if (GDA_DATA_MODEL_GET_CLASS (model)->i_find_row)
		return (GDA_DATA_MODEL_GET_CLASS (model)->i_find_row) (model, values, cols_index);

        n_rows = gda_data_model_get_n_rows (model);
        n_cols = gda_data_model_get_n_columns (model);
        current_row = 0;

#ifdef GDA_DEBUG_NO
        {
                GSList *list = values;

                g_print ("%s() find row for values: ", __FUNCTION__);
                while (list) {
                        g_print ("#%s# ", gda_value_stringify ((GdaValue *)(list->data)));
                        list = g_slist_next (list);
                }
                g_print ("In %d rows\n", n_rows);
        }
#endif

	while ((current_row < n_rows) && (row == -1)) {
                GSList *list;
                gboolean allequal = TRUE;
                const GdaValue *value;
                gint index;

                list = values;
                index = 0;
                while (list && allequal) {
                        if (cols_index)
                                g_return_val_if_fail (cols_index [index] < n_cols, FALSE);
                        value = gda_data_model_get_value_at (model, cols_index [index], current_row);

                        if (gda_value_compare_ext ((GdaValue *) (list->data), (GdaValue *) value))
                                allequal = FALSE;

                        list = g_slist_next (list);
                        index++;
                }

                if (allequal)
                        row = current_row;

                current_row++;
        }

        return row;
}

/**
 * gda_data_model_send_hint
 * @model: a #GdaDataModel
 * @hint: a hint to send to the model
 * @hint_value: an optional value to specify the hint, or %NULL
 *
 * Sends a hint to the data model. The hint may or may not be handled by the data
 * model, depending on its implementation
 */
void
gda_data_model_send_hint (GdaDataModel *model, GdaDataModelHint hint, const GdaValue *hint_value)
{
	g_return_if_fail (GDA_IS_DATA_MODEL (model));

	if (GDA_DATA_MODEL_GET_CLASS (model)->i_send_hint)
		(GDA_DATA_MODEL_GET_CLASS (model)->i_send_hint) (model, hint, hint_value);
}

static gchar *
export_to_text_separated (GdaDataModel *model, const gint *cols, gint nb_cols, gchar sep)
{
	GString *str;
	gchar *retval;
	gint c, rows, r;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

	str = g_string_new ("");
	rows = gda_data_model_get_n_rows (model);

	for (r = 0; r < rows; r++) {
		if (r > 0)
			str = g_string_append_c (str, '\n');

		for (c = 0; c < nb_cols; c++) {
			GdaValue *value;
			gchar *txt;

			value = (GdaValue *) gda_data_model_get_value_at (model, cols[c], r);
			if (gda_value_get_type (value) == GDA_VALUE_TYPE_BOOLEAN)
				txt = g_strdup (gda_value_get_boolean (value) ? "TRUE" : "FALSE");
			else
				txt = gda_value_stringify (value);
			if (c > 0)
				str = g_string_append_c (str, sep);
			str = g_string_append_c (str, '"');
			str = g_string_append (str, txt);
			str = g_string_append_c (str, '"');

			g_free (txt);
		}
	}

	retval = str->str;
	g_string_free (str, FALSE);

	return retval;
}

/**
 * gda_data_model_to_text_separated
 * @model: a #GdaDataModel object.
 * @cols: an array containing which columns of @model will be exported, or %NULL for all columns
 * @nb_cols: the number of columns in @cols
 * @sep: a char separator
 *
 * Converts the given model into a char-separated series of rows.
 * Examples of @sep are ',' for comma separated export and '\t' for tab
 * separated export
 *
 * Returns: the representation of the model. You should free this
 * string when you no longer need it.
 */
gchar *
gda_data_model_to_text_separated (GdaDataModel *model, const gint *cols, gint nb_cols, gchar sep)
{
	if (cols)
		return export_to_text_separated (model, cols, nb_cols, sep);
	else {
		gchar *retval;
		gint *rcols, rnb_cols, i;

		g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

		rnb_cols = gda_data_model_get_n_columns (model);
		rcols = g_new (gint, rnb_cols);
		for (i = 0; i < rnb_cols; i++)
			rcols[i] = i;
		retval = export_to_text_separated (model, rcols, rnb_cols, sep);
		g_free (rcols);

		return retval;
	}
}


/**
 * gda_data_model_to_xml
 * @model: a #GdaDataModel object.
 * @cols: an array containing which columns of @model will be exported, or %NULL for all columns
 * @nb_cols: the number of columns in @cols
 *
 * Converts the given model into a XML representation.
 *
 * Returns: the representation of the model. You should free this
 * string when you no longer need it.
 */
gchar *
gda_data_model_to_xml (GdaDataModel *model, const gint *cols, gint nb_cols, const gchar *name)
{
	xmlChar *xml_contents;
	xmlNodePtr xml_node;
	gchar *xml;
	gint size;
	xmlDocPtr xml_doc;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

	xml_node = gda_data_model_to_xml_node (model, cols, nb_cols, name);
	xml_doc = xmlNewDoc ("1.0");
	xmlDocSetRootElement (xml_doc, xml_node);
	
	xmlDocDumpMemory (xml_doc, &xml_contents, &size);
	xmlFreeDoc (xml_doc);
	
	xml = g_strdup (xml_contents);
	xmlFree (xml_contents);

	return xml;
}

static void
xml_set_boolean (xmlNodePtr node, const gchar *name, gboolean value)
{
	xmlSetProp (node, name, value ? "TRUE" : "FALSE");
}

static void
xml_set_int (xmlNodePtr node, const gchar *name, gint value)
{
	char s[80];

	sprintf (s, "%d", value);
	xmlSetProp (node, name, s);
}

/**
 * gda_data_model_to_xml_node
 * @model: a #GdaDataModel object.
 * @cols: an array containing which columns of @model will be exported, or %NULL for all columns
 * @nb_cols: the number of columns in @cols
 * @name: name to use for the XML resulting table.
 *
 * Converts a #GdaDataModel into a xmlNodePtr (as used in libxml).
 *
 * Returns: a xmlNodePtr representing the whole data model.
 */
xmlNodePtr
gda_data_model_to_xml_node (GdaDataModel *model, const gint *cols, gint nb_cols, const gchar *name)
{
	xmlNodePtr node;
	gint rows, i;
	gint *rcols, rnb_cols;
	gchar *arrayid;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

	node = xmlNewNode (NULL, "gda_array");
	
	arrayid = g_strdup_printf ("DA%s", gda_object_get_id (GDA_OBJECT (model)));
	xmlSetProp (node, "id", arrayid);

	if (name)
		xmlSetProp (node, "name", name);
	else
		xmlSetProp (node, "name", _("Exported Data"));

	/* compute columns if not provided */
	if (!cols) {
		rnb_cols = gda_data_model_get_n_columns (model);
		rcols = g_new (gint, rnb_cols);
		for (i = 0; i < rnb_cols; i++)
			rcols [i] = i;
	}
	else {
		rcols = (gint *) cols;
		rnb_cols = nb_cols;
	}

	/* set the table structure */
	rows = gda_data_model_get_n_rows (model);
	for (i = 0; i < rnb_cols; i++) {
		GdaColumn *column;
		xmlNodePtr field;
		const gchar *cstr;
		gchar *str;

		column = gda_data_model_describe_column (model, rcols [i]);
		if (!column) {
			xmlFreeNode (node);
			return NULL;
		}

		field = xmlNewChild (node, NULL, "gda_array_field", NULL);
		str = g_strdup_printf ("%s:FI%d", arrayid, i);
		xmlSetProp (field, "id", str);
		g_free (str);
		xmlSetProp (field, "name", gda_column_get_name (column));
		cstr = gda_column_get_title (column);
		if (cstr && *cstr)
			xmlSetProp (field, "title", cstr);
		cstr = gda_column_get_caption (column);
		if (cstr && *cstr)
			xmlSetProp (field, "caption", cstr);
		cstr = gda_column_get_dbms_type (column);
		if (cstr && *cstr)
			xmlSetProp (field, "dbms_type", cstr);
		xmlSetProp (field, "gdatype", gda_type_to_string (gda_column_get_gda_type (column)));
		if (gda_column_get_defined_size (column) != 0)
			xml_set_int (field, "size", gda_column_get_defined_size (column));
		if (gda_column_get_scale (column) != 0)
			xml_set_int (field, "scale", gda_column_get_scale (column));
		if (gda_column_get_primary_key (column))
			xml_set_boolean (field, "pkey", gda_column_get_primary_key (column));
		if (gda_column_get_unique_key (column))
			xml_set_boolean (field, "unique", gda_column_get_unique_key (column));
		if (gda_column_get_allow_null (column))
			xml_set_boolean (field, "nullok", gda_column_get_allow_null (column));
		if (gda_column_get_auto_increment (column))
			xml_set_boolean (field, "auto_increment", gda_column_get_auto_increment (column));
		cstr = gda_column_get_references (column);
		if (cstr && *cstr)
			xmlSetProp (field, "ref", cstr);
		cstr = gda_column_get_table (column);
		if (cstr && *cstr)
			xmlSetProp (field, "table", cstr);
	}
	
	/* add the model data to the XML output */
	if (rows > 0) {
		xmlNodePtr row, data, field;
		gint r, c;

		data = xmlNewChild (node, NULL, "gda_array_data", NULL);
		for (r = 0; r < rows; r++) {
			row = xmlNewChild (data, NULL, "gda_array_row", NULL);
			for (c = 0; c < rnb_cols; c++) {
				GdaValue *value;
				gchar *str;

				value = (GdaValue *) gda_data_model_get_value_at (model, rcols [c], r);
				if (!value || gda_value_is_null ((GdaValue *) value))
					str = NULL;
				else {
					if (gda_value_get_type (value) == GDA_VALUE_TYPE_BOOLEAN)
						str = g_strdup (gda_value_get_boolean (value) ? "TRUE" : "FALSE");
					else
						str = gda_value_stringify (value);
				}
				field = xmlNewChild (row, NULL, "gda_value", str);
				if (!str)
					xmlSetProp (field, "isnull", "t");

				g_free (str);
			}
		}
	}

	if (!cols)
		g_free (rcols);

	g_free (arrayid);
	return node;
}

static gboolean
add_xml_row (GdaDataModel *model, xmlNodePtr xml_row, GError **error)
{
	xmlNodePtr xml_field;
	GList *value_list = NULL;
	GPtrArray *values;
	gint i;
	gboolean retval = TRUE;
	gint pos = 0;

	values = g_ptr_array_new ();
	g_ptr_array_set_size (values, gda_data_model_get_n_columns (model));
	for (xml_field = xml_row->xmlChildrenNode; xml_field != NULL; xml_field = xml_field->next) {
		GdaValue *value = NULL;
		GdaColumn *column;
		GdaValueType gdatype;
		gchar *isnull;

		if (xmlNodeIsText (xml_field))
			continue;

		if (strcmp (xml_field->name, "gda_value")) {
			g_set_error (error, 0, 0, _("Expected tag <gda_value>, got <%s>, ignoring"), xml_field->name);
			continue;
		}

		/* create the value for this field */
		column = gda_data_model_describe_column (model, pos);
		gdatype = gda_column_get_gda_type (column);
		if ((gdatype == GDA_VALUE_TYPE_UNKNOWN) ||
		    (gdatype == GDA_VALUE_TYPE_NULL)) {
			g_set_error (error, 0, 0,
				     _("Cannot retrieve column data type (type is UNKNOWN or not specified)"));
			retval = FALSE;
			break;
		}

		isnull = xmlGetProp (xml_field, "isnull");
		if (isnull) {
			if ((*isnull == 'f') || (*isnull == 'F')) {
				g_free (isnull);
				isnull = NULL;
			}
		}

		if (!isnull) {
			value = g_new0 (GdaValue, 1);
			if (!gda_value_set_from_string (value, xmlNodeGetContent (xml_field), gdatype)) {
				g_free (value);
				g_set_error (error, 0, 0, _("Cannot interpret string as a valid %s value"), 
					     gda_type_to_string (gdatype));
				retval = FALSE;
				break;
			}
		}
		else
			g_free (isnull);

		g_ptr_array_index (values, pos) = value;
		pos ++;
	}

	if (retval) {
		for (i = 0; i < values->len; i++) {
			GdaValue *value = (GdaValue *) g_ptr_array_index (values, i);

			if (!value) {
				value = gda_value_new_null ();
				g_ptr_array_index (values, i) = value;
			}

			value_list = g_list_append (value_list, value);
		}

		if (retval)
			gda_data_model_append_values (model, value_list, NULL);

		g_list_free (value_list);
	}

	for (i = 0; i < values->len; i++)
		if (g_ptr_array_index (values, i))
			gda_value_free ((GdaValue *) g_ptr_array_index (values, i));

	return retval;
}

/**
 * gda_data_model_add_data_from_xml_node
 * @model: a #GdaDataModel.
 * @node: a XML node representing a &lt;data&gt; XML node.
 *
 * Adds the data from a XML node to the given data model.
 *
 * Returns: %TRUE if successful, %FALSE otherwise.
 */
gboolean
gda_data_model_add_data_from_xml_node (GdaDataModel *model, xmlNodePtr node, GError **error)
{
	xmlNodePtr children;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);
	g_return_val_if_fail (node != NULL, FALSE);

	while (xmlNodeIsText (node))
		node = node->next;

	if (strcmp (node->name, "gda_array_data")) {
		g_set_error (error, 0, 0,
			     _("Expected tag <gda_array_data>, got <%s>"), node->name);
		return FALSE;
	}

	for (children = node->xmlChildrenNode; children != NULL; children = children->next) {
		if (!strcmp (children->name, "gda_array_row")) {
			if (!add_xml_row (model, children, error))
				return FALSE;
		}
	}

	return TRUE;
}

/**
 * gda_data_model_dump
 * @model: a #GdaDataModel.
 * @to_stream: where to dump the data model
 *
 * Dumps a textual representation of the @model to the @to_stream stream
 */
void
gda_data_model_dump (GdaDataModel *model, FILE *to_stream)
{
	gchar *str;

	g_return_if_fail (GDA_IS_DATA_MODEL (model));
	g_return_if_fail (to_stream);

	str = gda_data_model_dump_as_string (model);
	g_fprintf (to_stream, "%s", str);
	g_free (str);
}

/**
 * gda_data_model_dump_as_string
 * @model: a #GdaDataModel.
 *
 * Dumps a textual representation of the @model into a new string
 *
 * Returns: a new string.
 */
gchar *
gda_data_model_dump_as_string (GdaDataModel *model)
{
	GString *string;
	gchar *offstr, *str;
	gint n_cols, n_rows;
	gint *cols_size;
	gboolean *cols_is_num;
	gchar *sep_col  = " | ";
	gchar *sep_row  = "-+-";
	gchar sep_fill = '-';
	gint i, j;
	const GdaValue *value;

	gint offset = 0;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

	string = g_string_new ("");

        /* string for the offset */
        offstr = g_new0 (gchar, offset+1);
	memset (offstr, ' ', offset);

	/* compute the columns widths: using column titles... */
	n_cols = gda_data_model_get_n_columns (model);
	n_rows = gda_data_model_get_n_rows (model);
	cols_size = g_new0 (gint, n_cols);
	cols_is_num = g_new0 (gboolean, n_cols);
	
	for (i = 0; i < n_cols; i++) {
		GdaColumn *gdacol;
		GdaValueType coltype;

		/* { */
/* 			GdaColumn *col = gda_data_model_describe_column (model, i); */
/* 			g_print ("Model col %d has type %s\n", i, gda_type_to_string (gda_column_get_gda_type (col))); */
/* 		} */

		str = (gchar *) gda_data_model_get_column_title (model, i);
		if (str)
			cols_size [i] = g_utf8_strlen (str, -1);
		else
			cols_size [i] = 6; /* for "<none>" */

		gdacol = gda_data_model_describe_column (model, i);
		coltype = gda_column_get_gda_type (gdacol);
		/*g_string_append_printf (string, "COL %d is a %s\n", i+1, gda_type_to_string (coltype));*/
		if ((coltype == GDA_VALUE_TYPE_BIGINT) ||
		    (coltype == GDA_VALUE_TYPE_BIGUINT) ||
		    (coltype == GDA_VALUE_TYPE_INTEGER) ||
		    (coltype == GDA_VALUE_TYPE_NUMERIC) ||
		    (coltype == GDA_VALUE_TYPE_SINGLE) ||
		    (coltype == GDA_VALUE_TYPE_SMALLINT) ||
		    (coltype == GDA_VALUE_TYPE_SMALLUINT) ||
		    (coltype == GDA_VALUE_TYPE_TINYINT) ||
		    (coltype == GDA_VALUE_TYPE_TINYUINT) ||
		    (coltype == GDA_VALUE_TYPE_UINTEGER))
			cols_is_num [i] = TRUE;
		else
			cols_is_num [i] = FALSE;
	}

	/* ... and using column data */
	for (j = 0; j < n_rows; j++) {
		for (i = 0; i < n_cols; i++) {
			value = gda_data_model_get_value_at (model, i, j);
			str = value ? gda_value_stringify ((GdaValue*)value) : g_strdup ("_null_");
			if (str) {
				cols_size [i] = MAX (cols_size [i], g_utf8_strlen (str, -1));
				g_free (str);
			}
		}
	}
	
	/* actual dumping of the contents: column titles...*/
	for (i = 0; i < n_cols; i++) {
		gint j, max;
		str = (gchar *) gda_data_model_get_column_title (model, i);
		if (i != 0)
			g_string_append_printf (string, "%s", sep_col);

		if (str) {
			g_string_append_printf (string, "%s", str);
			max = cols_size [i] - g_utf8_strlen (str, -1);
		}
		else {
			g_string_append (string, "<none>");
			max = cols_size [i] - 6;
		}
		for (j = 0; j < max; j++)
			g_string_append_c (string, ' ');
	}
	g_string_append_c (string, '\n');
		
	/* ... separation line ... */
	for (i = 0; i < n_cols; i++) {
		if (i != 0)
			g_string_append_printf (string, "%s", sep_row);
		for (j = 0; j < cols_size [i]; j++)
			g_string_append_c (string, sep_fill);
	}
	g_string_append_c (string, '\n');

	/* ... and data */
	for (j = 0; j < n_rows; j++) {
		for (i = 0; i < n_cols; i++) {
			value = gda_data_model_get_value_at (model, i, j);
			str = value ? gda_value_stringify ((GdaValue *)value) : g_strdup ("_null_");
			if (i != 0)
				g_string_append_printf (string, "%s", sep_col);
			if (cols_is_num [i])
				g_string_append_printf (string, "%*s", cols_size [i], str);
			else {
				gint j, max;
				if (str) {
					g_string_append_printf (string, "%s", str);
					max = cols_size [i] - g_utf8_strlen (str, -1);
				}
				else
					max = cols_size [i];
				for (j = 0; j < max; j++)
					g_string_append_c (string, ' ');
			}
			g_free (str);
		}
		g_string_append_c (string, '\n');
	}
	g_free (cols_size);
	g_free (cols_is_num);

	g_free (offstr);
	
	str = string->str;
	g_string_free (string, FALSE);
	return str;
}
