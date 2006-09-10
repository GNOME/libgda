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
#include <libgda/gda-data-model-private.h>
#include <libgda/gda-data-model-extra.h>
#include <libgda/gda-data-model-iter.h>
#include <libgda/gda-data-model-import.h>
#include <libgda/gda-log.h>
#include <libgda/gda-util.h>
#include <libgda/gda-row.h>
#include <libgda/gda-object.h>
#include <libgda/gda-enums.h>
#include <string.h>
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

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
 * gda_data_model_signal_emit_changed
 * @model: a #GdaDataModel object.
 *
 * Notifies listeners of the given data model object of changes
 * in the underlying data. Listeners usually will connect
 * themselves to the "changed" signal in the #GdaDataModel
 * class, thus being notified of any new data being appended
 * or removed from the data model.
 */
void
gda_data_model_signal_emit_changed (GdaDataModel *model)
{
	g_return_if_fail (GDA_IS_DATA_MODEL (model));
	
	if (do_notify_changes (model)) 
		gda_object_signal_emit_changed (GDA_OBJECT (model));
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
		const GValue *value;

		nbcols = gda_data_model_get_n_columns (model);
		for (i = 0; i < nbcols; i++) {
			column = gda_data_model_describe_column (model, i);
			value = gda_data_model_get_value_at (model, i, 0);
			if ((gda_column_get_gda_type (column) == G_TYPE_INVALID))
				gda_column_set_gda_type (column, 
							 value ? G_VALUE_TYPE ((GValue *)value) : GDA_TYPE_NULL);
		}
	}

	/* notify changes */
	if (do_notify_changes (model)) {
		g_signal_emit (G_OBJECT (model),
			       gda_data_model_signals[ROW_INSERTED],
			       0, row);

		gda_data_model_signal_emit_changed (model);
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

		gda_data_model_signal_emit_changed (model);
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

		gda_data_model_signal_emit_changed (model);
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
	if (GDA_DATA_MODEL_GET_CLASS (model)->i_get_access_flags) {
		guint flags = (GDA_DATA_MODEL_GET_CLASS (model)->i_get_access_flags) (model);
		if (flags & GDA_DATA_MODEL_ACCESS_RANDOM)
			flags |= GDA_DATA_MODEL_ACCESS_CURSOR_FORWARD | GDA_DATA_MODEL_ACCESS_CURSOR_BACKWARD;
		return flags;
	}
	else
		return 0;
}

/**
 * gda_data_model_get_n_rows
 * @model: a #GdaDataModel object.
 *
 * Returns: the number of rows in the given data model, or -1 if the number of rows is not known
 */
gint
gda_data_model_get_n_rows (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), -1);

	if (GDA_DATA_MODEL_GET_CLASS (model)->i_get_n_rows)
		return (GDA_DATA_MODEL_GET_CLASS (model)->i_get_n_rows) (model);
	else 
		return -1;
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
 * Note that the returned #GValue must not be modified directly (unexpected behaviours may
 * occur if you do so). If you want to
 * modify a value stored in a #GdaDataModel, use the gda_data_model_set_value() method.
 *
 * Returns: a #GValue containing the value stored in the given
 * position, or %NULL on error (out-of-bound position, etc).
 */
const GValue *
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
 * is an ORed value of #GValueAttribute flags. As a special case, if
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
 * @value: a #GValue, or %NULL
 * @error: a place to store errors, or %NULL
 *
 * Returns: TRUE if the value in the data model has been updated and no error occurred
 */
gboolean
gda_data_model_set_value_at (GdaDataModel *model, gint col, gint row, const GValue *value, GError **error)
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
 * @values: a list of #GValue, one for each n (&lt;nb_cols) columns of @model
 * @error: a place to store errors, or %NULL
 *
 * If any value in @values is actually %NULL, then 
 * it is considered as a default value.
 *
 * Returns: TRUE if the value in the data model has been updated and no error occurred
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
 * randomly the correspoding row can be set using gda_data_model_move_iter_at_row(), 
 * and for models which are accessible sequentially only then the first row will be
 * fetched using gda_data_model_move_iter_next().
 *
 * Returns: a new #GdaDataModelIter object, or %NULL if an error occurred
 */
GdaDataModelIter *
gda_data_model_create_iter (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);
	if (GDA_DATA_MODEL_GET_CLASS (model)->i_create_iter)
		return (GDA_DATA_MODEL_GET_CLASS (model)->i_create_iter) (model);
	else 
		/* default method */
		return  g_object_new (GDA_TYPE_DATA_MODEL_ITER, 
				      "dict", gda_object_get_dict (GDA_OBJECT (model)), 
				      "data_model", model, NULL);
}

/**
 * gda_data_model_move_iter_at_row
 * @model: a #GdaDataModel object.
 * @iter: a #GdaDataModelIter object
 * @row: row number.
 *
 * Sets @iter to represent the @row row. @iter must be a valid iterator object obtained
 * using the gda_data_model_create_iter() method.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_data_model_move_iter_at_row (GdaDataModel *model, GdaDataModelIter *iter, gint row)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);
	if (GDA_DATA_MODEL_GET_CLASS (model)->i_iter_at_row)
		return (GDA_DATA_MODEL_GET_CLASS (model)->i_iter_at_row) (model, iter, row);
	else 
		/* default method */
		return gda_data_model_move_iter_at_row_default (model, iter, row);
}

gboolean
gda_data_model_move_iter_at_row_default (GdaDataModel *model, GdaDataModelIter *iter, gint row)
{
	/* default method */
	GSList *list;
	gint col;
	GdaDataModel *test;
	gboolean update_model;
	guint flags;
	
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);

	if (! (gda_data_model_get_access_flags (model) & GDA_DATA_MODEL_ACCESS_RANDOM))
		return FALSE;

	/* validity tests */
	if (row >= gda_data_model_get_n_rows (model)) {
		gda_data_model_iter_invalidate_contents (iter);
		g_object_set (G_OBJECT (iter), "current_row", -1, NULL);
		return FALSE;
	}
		
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

		flags = gda_data_model_get_attributes_at (model, col, row);
		if (flags & GDA_VALUE_ATTR_IS_DEFAULT)
			g_object_set (G_OBJECT (list->data), "use-default-value", TRUE, NULL);
		gda_parameter_set_exists_default_value (GDA_PARAMETER (list->data), 
							flags & GDA_VALUE_ATTR_CAN_BE_DEFAULT);
		list = g_slist_next (list);
		col ++;
	}
	g_object_set (G_OBJECT (iter), "current_row", row, "update_model", update_model, NULL);
	return TRUE;
}

/**
 * gda_data_model_move_iter_next
 * @model: a #GdaDataModel object.
 * @iter: a #GdaDataModelIter object
 *
 * Sets @iter to the next available row in @model. @iter must be a valid iterator object obtained
 * using the gda_data_model_create_iter() method.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_data_model_move_iter_next (GdaDataModel *model, GdaDataModelIter *iter)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);
	if (GDA_DATA_MODEL_GET_CLASS (model)->i_iter_next)
		return (GDA_DATA_MODEL_GET_CLASS (model)->i_iter_next) (model, iter);
	else 
		/* default method */
		return gda_data_model_move_iter_next_default (model, iter);
}

gboolean
gda_data_model_move_iter_next_default (GdaDataModel *model, GdaDataModelIter *iter)
{
	GSList *list;
	gint col;
	gint row;
	GdaDataModel *test;
	gboolean update_model;
	guint flags;
	
	/* validity tests */
	if (! (gda_data_model_get_access_flags (model) & GDA_DATA_MODEL_ACCESS_RANDOM))
		return FALSE;
	
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ITER (iter), FALSE);
	
	g_object_get (G_OBJECT (iter), "data_model", &test, NULL);
	g_return_val_if_fail (test == model, FALSE);
	
	g_object_get (G_OBJECT (iter), "current_row", &row, NULL);
	row++;
	if (row >= gda_data_model_get_n_rows (model)) {
		g_object_set (G_OBJECT (iter), "current_row", -1, NULL);
		return FALSE;
	}
	
	/* actual sync. */
	g_object_get (G_OBJECT (iter), "update_model", &update_model, NULL);
	g_object_set (G_OBJECT (iter), "update_model", FALSE, NULL);
	col = 0;
	list = ((GdaParameterList *) iter)->parameters;
	while (list) {
		gda_parameter_set_value (GDA_PARAMETER (list->data), 
					 gda_data_model_get_value_at (model, col, row));
		flags = gda_data_model_get_attributes_at (model, col, row);
		if (flags & GDA_VALUE_ATTR_IS_DEFAULT)
			g_object_set (G_OBJECT (list->data), "use-default-value", TRUE, NULL);
		gda_parameter_set_exists_default_value (GDA_PARAMETER (list->data), 
							flags & GDA_VALUE_ATTR_CAN_BE_DEFAULT);
		list = g_slist_next (list);
		col ++;
	}
	g_object_set (G_OBJECT (iter), "current_row", row, 
		      "update_model", update_model, NULL);
	return TRUE;
}

/**
 * gda_data_model_move_iter_prev
 * @model: a #GdaDataModel object.
 * @iter: a #GdaDataModelIter object
 *
 * Sets @iter to the previous available row in @model. @iter must be a valid iterator object obtained
 * using the gda_data_model_create_iter() method.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_data_model_move_iter_prev (GdaDataModel *model, GdaDataModelIter *iter)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);
	if (GDA_DATA_MODEL_GET_CLASS (model)->i_iter_prev)
		return (GDA_DATA_MODEL_GET_CLASS (model)->i_iter_prev) (model, iter);
	else 
		/* default method */
		return gda_data_model_move_iter_prev_default (model, iter);
}

gboolean
gda_data_model_move_iter_prev_default (GdaDataModel *model, GdaDataModelIter *iter)
{
	GSList *list;
	gint col;
	gint row;
	GdaDataModel *test;
	gboolean update_model;
	guint flags;
	
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
	g_object_get (G_OBJECT (iter), "update_model", &update_model, NULL);
	g_object_set (G_OBJECT (iter), "update_model", FALSE, NULL);
	col = 0;
	list = ((GdaParameterList *) iter)->parameters;
	while (list) {
		gda_parameter_set_value (GDA_PARAMETER (list->data), 
					 gda_data_model_get_value_at (model, col, row));
		flags = gda_data_model_get_attributes_at (model, col, row);
		if (flags & GDA_VALUE_ATTR_IS_DEFAULT)
			g_object_set (G_OBJECT (list->data), "use-default-value", TRUE, NULL);
		gda_parameter_set_exists_default_value (GDA_PARAMETER (list->data), 
							flags & GDA_VALUE_ATTR_CAN_BE_DEFAULT);
		list = g_slist_next (list);
		col ++;
	}
	g_object_set (G_OBJECT (iter), "current_row", row, 
		      "update_model", update_model, NULL);
	return TRUE;	
}


/**
 * gda_data_model_append_values
 * @model: a #GdaDataModel object.
 * @values: #GList of #GValue* representing the row to add.  The
 *          length must match model's column count.  These #GValue
 *	    are value-copied (the user is still responsible for freeing them).
 * @error: a place to store errors, or %NULL
 *
 * Appends a row to the given data model. If any value in @values is actually %NULL, then 
 * it is considered as a default value.
 *
 * Returns: the number of the added row, or -1 if an error occurred
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
 * Returns: the number of the added row, or -1 if an error occurred
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
 * @values: a list of #GValue values
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
                        g_print ("#%s# ", gda_value_stringify ((GValue *)(list->data)));
                        list = g_slist_next (list);
                }
                g_print ("In %d rows\n", n_rows);
        }
#endif

	while ((current_row < n_rows) && (row == -1)) {
                GSList *list;
                gboolean allequal = TRUE;
                const GValue *value;
                gint index;

                list = values;
                index = 0;
                while (list && allequal) {
                        if (cols_index)
                                g_return_val_if_fail (cols_index [index] < n_cols, FALSE);
                        value = gda_data_model_get_value_at (model, cols_index [index], current_row);

                        if (gda_value_compare_ext ((GValue *) (list->data), (GValue *) value))
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
gda_data_model_send_hint (GdaDataModel *model, GdaDataModelHint hint, const GValue *hint_value)
{
	g_return_if_fail (GDA_IS_DATA_MODEL (model));

	if (GDA_DATA_MODEL_GET_CLASS (model)->i_send_hint)
		(GDA_DATA_MODEL_GET_CLASS (model)->i_send_hint) (model, hint, hint_value);
}

static gchar *export_to_text_separated (GdaDataModel *model, const gint *cols, gint nb_cols, gchar sep);


/**
 * gda_data_model_export_to_string
 * @model: a #GdaDataModel
 * @format: the format in which to export data
 * @cols: an array containing which columns of @model will be exported, or %NULL for all columns
 * @nb_cols: the number of columns in @cols
 * @options: list of options for the export
 *
 * Exports data contained in @model to a string; the format is specified using the @format argument.
 *
 * Specifically, the parameters in the @options list can be:
 * <itemizedlist>
 *   <listitem><para>"SEPARATOR": a string value of which the first character is used as a separator in case of CSV export
 *             </para></listitem>
 *   <listitem><para>"NAME": a string value used to name the exported data if the export format is XML</para></listitem>
 * </itemizedlist>
 *
 * Returns: a new string.
 */
gchar *
gda_data_model_export_to_string (GdaDataModel *model, GdaDataModelIOFormat format, 
				 const gint *cols, gint nb_cols, GdaParameterList *options)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);
	g_return_val_if_fail (!options || GDA_IS_PARAMETER_LIST (options), NULL);

	switch (format) {
	case GDA_DATA_MODEL_IO_DATA_ARRAY_XML: {
		const gchar *name = NULL;
		xmlChar *xml_contents;
		xmlNodePtr xml_node;
		gchar *xml;
		gint size;
		xmlDocPtr xml_doc;

		if (options) {
			GdaParameter *param;

			param = gda_parameter_list_find_param (options, "NAME");
			if (param) {
				const GValue *value;
				value = gda_parameter_get_value (param);
				if (value && gda_value_isa ((GValue *) value, G_TYPE_STRING))
					name = g_value_get_string ((GValue *) value);
				else
					g_warning (_("The 'NAME' parameter must hold a string value, ignored."));
			}
		}

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

	case GDA_DATA_MODEL_IO_TEXT_SEPARATED: {
		gchar sep = ',';
		if (options) {
			GdaParameter *param;

			param = gda_parameter_list_find_param (options, "SEPARATOR");
			if (param) {
				const GValue *value;
				value = gda_parameter_get_value (param);
				if (value && gda_value_isa ((GValue *) value, G_TYPE_STRING)) {
					const gchar *str;

					str = g_value_get_string ((GValue *) value);
					if (str && *str)
						sep = *str;
				}
				else
					g_warning (_("The 'SEPARATOR' parameter must hold a string value, ignored."));
			}
		}

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

	default:
		g_assert_not_reached ();
	}
	return NULL;
}

/**
 * gda_data_model_export_to_file
 * @model: a #GdaDataModel
 * @format: the format in which to export data
 * @file: the filename to export to
 * @cols: an array containing which columns of @model will be exported, or %NULL for all columns
 * @nb_cols: the number of columns in @cols
 * @options: list of options for the export
 * @error: a place to store errors, or %NULL
 *
 * Exports data contained in @model to the @file file; the format is specified using the @format argument.
 *
 * Specifically, the parameters in the @options list can be:
 * <itemizedlist>
 *   <listitem><para>"SEPARATOR": a string value of which the first character is used as a separator in case of CSV export
 *             </para></listitem>
 *   <listitem><para>"NAME": a string value used to name the exported data if the export format is XML</para></listitem>
 *   <listitem><para>"OVERWRITE": a boolean value which tells if the file must be over-written if it already exists.
 *             </para></listitem>
 * </itemizedlist>
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_data_model_export_to_file (GdaDataModel *model, GdaDataModelIOFormat format, 
			       const gchar *file,
			       const gint *cols, gint nb_cols, 
			       GdaParameterList *options, GError **error)
{
	gchar *body;
	gboolean overwrite = FALSE;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);
	g_return_val_if_fail (!options || GDA_IS_PARAMETER_LIST (options), FALSE);
	g_return_val_if_fail (file, FALSE);

	body = gda_data_model_export_to_string (model, format, cols, nb_cols, options);
	if (options) {
		GdaParameter *param;
		
		param = gda_parameter_list_find_param (options, "OVERWRITE");
		if (param) {
			const GValue *value;
			value = gda_parameter_get_value (param);
			if (value && gda_value_isa ((GValue *) value, G_TYPE_BOOLEAN))
				overwrite = g_value_get_boolean ((GValue *) value);
			else
				g_warning (_("The 'OVERWRITE' parameter must hold a boolean value, ignored."));
		}
	}

	if (g_file_test (file, G_FILE_TEST_EXISTS)) {
		if (! overwrite) {
			g_free (body);
			g_set_error (error, 0, 0,
				     _("File '%s' already exists"), file);
			return FALSE;
		}
	}
	
	if (! gda_file_save (file, body, strlen (body))) {
		g_set_error (error, 0, 0, _("Could not save file %s"), file);
		g_free (body);
		return FALSE;
	}

	g_free (body);
	return TRUE;
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
			GValue *value;
			gchar *txt;

			value = (GValue *) gda_data_model_get_value_at (model, cols[c], r);
			if (G_VALUE_TYPE (value) == G_TYPE_BOOLEAN)
				txt = g_strdup (g_value_get_boolean (value) ? "TRUE" : "FALSE");
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
	gchar *str;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

	node = xmlNewNode (NULL, "gda_array");
	
	str = (gchar *) gda_object_get_id (GDA_OBJECT (model));
	if (str)
		arrayid = g_strdup_printf ("DA%s", str);
	else
		arrayid = g_strdup ("EXPORT");
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
	utility_data_model_dump_data_to_xml (model, node, cols, nb_cols, FALSE);

	if (!cols)
		g_free (rcols);

	g_free (arrayid);
	return node;
}

static GdaColumn *
find_column_from_id (GdaDataModel *model, const gchar *colid, gint *pos)
{
	GdaColumn *column = NULL;
	gint c, nbcols;
	
	/* assume @colid is the ID of a column */
	nbcols = gda_data_model_get_n_columns (model);
	for (c = 0; !column && (c < nbcols); c++) {
		const gchar *id;
		column = gda_data_model_describe_column (model, c);
		g_object_get (column, "id", &id, NULL);
		if (!id || strcmp (id, colid)) 
			column = NULL;
		else
			*pos = c;
	}

	/* if no column has been found, assumr @colid is like "_%d" where %d is a column number */
	if (!column && (*colid == '_')) {
		column = gda_data_model_describe_column (model, atoi (colid + 1));
		if (column)
			*pos = atoi (colid + 1);
	}

	return column;
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

	const gchar *lang = NULL;
#ifdef HAVE_LC_MESSAGES
	lang = setlocale (LC_MESSAGES, NULL);
#else
	lang = setlocale (LC_CTYPE, NULL);
#endif

	values = g_ptr_array_new ();
	g_ptr_array_set_size (values, gda_data_model_get_n_columns (model));
	for (xml_field = xml_row->xmlChildrenNode; xml_field != NULL; xml_field = xml_field->next) {
		GValue *value = NULL;
		GdaColumn *column;
		GType gdatype;
		gchar *isnull;
		xmlChar *this_lang;
		xmlChar *colid;

		if (xmlNodeIsText (xml_field))
			continue;

		if (strcmp (xml_field->name, "gda_value") && strcmp (xml_field->name, "gda_array_value")) {
			g_set_error (error, 0, 0, _("Expected tag <gda_value> or <gda_array_value>, "
						    "got <%s>, ignoring"), xml_field->name);
			continue;
		}
		
		this_lang = xmlGetProp (xml_field, "lang");
		if (this_lang && strncmp (this_lang, lang, strlen (this_lang))) {
			xmlFree (this_lang);
			continue;
		}

		colid = xmlGetProp (xml_field, "colid");

		/* create the value for this field */
		if (!colid) {
			if (this_lang)
				column = gda_data_model_describe_column (model, pos - 1);
			else
				column = gda_data_model_describe_column (model, pos);
		}
		else {
			column = find_column_from_id (model, colid, &pos);
			xmlFree (colid);
			if (!column)
				continue;
		}

		gdatype = gda_column_get_gda_type (column);
		if ((gdatype == G_TYPE_INVALID) ||
		    (gdatype == GDA_TYPE_NULL)) {
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
			value = g_new0 (GValue, 1);
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
		if (this_lang)
			xmlFree (this_lang);
		else
			pos ++;
	}

	if (retval) {
		for (i = 0; i < values->len; i++) {
			GValue *value = (GValue *) g_ptr_array_index (values, i);

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
			gda_value_free ((GValue *) g_ptr_array_index (values, i));

	return retval;
}

/**
 * gda_data_model_add_data_from_xml_node
 * @model: a #GdaDataModel.
 * @node: a XML node representing a &lt;gda_array_data&gt; XML node.
 *
 * Adds the data from a XML node to the given data model (see the DTD for that node
 * in the $prefix/share/libgda/dtd/libgda-array.dtd file).
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
 * gda_data_model_import_from_model
 * @to: the destination #GdaDataModel
 * @from: the source #GdaDataModel
 * @cols_trans: a #GHashTable for columns translating, or %NULL
 * @error: a place to store errors, or %NULL
 *
 * Copy the contents of the @from data model to the @to data model. The copy stops as soon as an error
 * orrurs.
 *
 * The @cols_trans is a hash table for which keys are @to columns numbers and the values are
 * the corresponding column numbers in the @from data model. To set the values of a column in @to to NULL,
 * create an entry in the hash table with a negative value.
 *
 * Returns: TRUE if no error occurred.
 */
gboolean
gda_data_model_import_from_model (GdaDataModel *to, GdaDataModel *from, GHashTable *cols_trans, GError **error)
{
	GdaDataModelIter *from_iter;
	gboolean retval = TRUE;
	gint to_nb_cols;
	gint from_nb_cols;
	GSList *copy_params = NULL;
	gint i;
	GList *append_values = NULL; /* list of #GValue values to add to the @to model */
	GType *append_types = NULL; /* array of the Glib type of the values to append */
	GSList *plist;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (to), FALSE);
	g_return_val_if_fail (GDA_IS_DATA_MODEL (from), FALSE);

	/* return if @to does not have any column */
	to_nb_cols = gda_data_model_get_n_columns (to);
	if (to_nb_cols == 0)
		return TRUE;
	from_nb_cols = gda_data_model_get_n_columns (from);
	if (from_nb_cols == 0)
		return TRUE;

	/* obtain an iterator */
	from_iter = gda_data_model_create_iter (from);
	if (!from_iter) {
		g_set_error (error, 0, 0,
			     _("Could not get an iterator for soure data model"));
		return FALSE;
	}

	/* make a list of the parameters which will be used during copy (stored in reverse order!) ,
	 * an some tests */
	for (i = 0; i < to_nb_cols; i++) {
		GdaParameter *param = NULL;
		gint col; /* source column number */
		GdaColumn *column;

		if (cols_trans) {
			col = GPOINTER_TO_INT (g_hash_table_lookup (cols_trans, GINT_TO_POINTER (i)));
			if (col >= from_nb_cols) {
				g_slist_free (copy_params);
				g_set_error (error, 0, 0,
					     _("Inexistant column in source data model: %d"), col);
				return FALSE;
			}
		}
		else
			col = i;
		if (col >= 0)
			param = gda_data_model_iter_get_param_for_column (from_iter, col);

		/* tests */
		column = gda_data_model_describe_column (to, i);
		if (! gda_column_get_allow_null (column) && !param) {
			g_slist_free (copy_params);
			g_set_error (error, 0, 0,
				     _("Destination column %d can't be NULL but has no correspondance in the "
				       "source data model"), i);
			return FALSE;
		}
		if (param) {
			if (! g_value_type_transformable (gda_parameter_get_gda_type (param), 
							  gda_column_get_gda_type (column))) {
				g_set_error (error, 0, 0,
					     _("Destination column %d has a gda type (%s) incompatible with "
					       "source column %d type (%s)"), i,
					     gda_type_to_string (gda_column_get_gda_type (column)),
					     col,
					     gda_type_to_string (gda_parameter_get_gda_type (param)));
				return FALSE;
			}
		}
		copy_params = g_slist_prepend (copy_params, param);
	}

	/* build the append_values list (stored in reverse order!)
	 * node->data is:
	 * - NULL if the value must be replaced by the value of the copied model
	 * - a GValue of type GDA_VALYE_TYPE_NULL if a null value must be inserted in the dest data model
	 * - a GValue of a different type is the value must be converted from the ser data model
	 */
	append_types = g_new0 (GType, to_nb_cols);
	plist = copy_params;
	i = 0;
	while (plist) {
		GdaColumn *column;

		column = gda_data_model_describe_column (to, i);
		if (plist->data) {
			if (gda_parameter_get_gda_type (GDA_PARAMETER (plist->data)) != 
			    gda_column_get_gda_type (column)) {
				GValue *newval;
				
				newval = g_new0 (GValue, 1);
				append_types [i] = gda_column_get_gda_type (column);
				g_value_init (newval, append_types [i]);
				append_values = g_list_prepend (append_values, newval);
			}
			else
				append_values = g_list_prepend (append_values, NULL);
		}
		else
			append_values = g_list_prepend (append_values, gda_value_new_null ());

		plist = g_slist_next (plist);
		i++;
	}
	
	/* actual data copy (no memory allocation is needed here) */
	gda_data_model_send_hint (to, GDA_DATA_MODEL_HINT_START_BATCH_UPDATE, NULL);
	
	gda_data_model_move_iter_next (from, from_iter); /* move to first row */
	while (retval && gda_data_model_iter_is_valid (from_iter)) {
		GList *values = NULL;
		GList *avlist = append_values;
		plist = copy_params;
		i = to_nb_cols - 1;

		while (plist && avlist && retval) {
			GValue *value;

			value = (GValue *) (avlist->data);
			if (plist->data) {
				value = (GValue *) gda_parameter_get_value (GDA_PARAMETER (plist->data));
				if (avlist->data) {
					if (append_types [i] && gda_value_is_null ((GValue *) (avlist->data))) 
						g_value_init ((GValue *) (avlist->data), append_types [i]);
					if (!gda_value_is_null (value) && !g_value_transform (value, (GValue *) (avlist->data))) {
						gchar *str;

						str = gda_value_stringify (value);
						g_set_error (error, 0, 0,
							     _("Can't transform '%s' from GDA type %s to GDA type %s"),
							     str, 
							     gda_type_to_string (G_VALUE_TYPE (value)),
							     gda_type_to_string (G_VALUE_TYPE ((GValue *) (avlist->data))));
						g_free (str);
						retval = FALSE;
					}
					value = (GValue *) (avlist->data);
				}
			}
			g_assert (value);

			values = g_list_prepend (values, value);

			plist = g_slist_next (plist);
			avlist = g_list_next (avlist);
			i--;
		}

		if (retval) {
			if (gda_data_model_append_values (to, values, error) < 0) 
				retval = FALSE;
		}
		
		g_list_free (values);
		gda_data_model_iter_move_next (from_iter);
	}
	
	/* free memory */
	{
		GList *vlist;

		vlist = append_values;
		while (vlist) {
			if (vlist->data)
				gda_value_free ((GValue *) vlist->data);
			vlist = g_list_next (vlist);
		}
		g_free (append_types);
	}

	gda_data_model_send_hint (to, GDA_DATA_MODEL_HINT_END_BATCH_UPDATE, NULL);
	return retval;
}

/**
 * gda_data_model_import_from_string
 * @model: a #GdaDataModel
 * @string: the string to import data from
 * @cols_trans: a hash table containing which columns of @model will be imported, or %NULL for all columns
 * @options: list of options for the export
 * @error: a place to store errors, or %NULL
 *
 * Loads the data from @string into @model.
 *
 * Returns: TRUE if no error occurred.
 */
gboolean
gda_data_model_import_from_string (GdaDataModel *model,
				   const gchar *string, GHashTable *cols_trans,
				   GdaParameterList *options, GError **error)
{
	GdaDataModel *import;
	gboolean retval;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);
	g_return_val_if_fail (!options || GDA_IS_PARAMETER_LIST (options), FALSE);
	if (!string)
		return TRUE;

	import = gda_data_model_import_new_mem (string, FALSE, options);
	retval = gda_data_model_import_from_model (model, import, cols_trans, error);
	g_object_unref (import);

	return retval;
}

/**
 * gda_data_model_import_from_file
 * @model: a #GdaDataModel
 * @file: the filename to export to
 * @cols: an array containing which columns of @model will be exported, or %NULL for all columns
 * @nb_cols: the number of columns in @cols
 * @options: list of options for the export
 * @error: a place to store errors, or %NULL
 *
 * Imports data contained in the @file file into @model; the format is detected.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_data_model_import_from_file (GdaDataModel *model, 
				 const gchar *file, GHashTable *cols_trans,
				 GdaParameterList *options, GError **error)
{
	GdaDataModel *import;
	gboolean retval;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);
	g_return_val_if_fail (!options || GDA_IS_PARAMETER_LIST (options), FALSE);
	if (!file)
		return TRUE;

	import = gda_data_model_import_new_file (file, FALSE, options);
	retval = gda_data_model_import_from_model (model, import, cols_trans, error);
	g_object_unref (import);

	return retval;
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
	const GValue *value;

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
		GType coltype;

#ifdef GDA_DEBUG_NO
		{
			GdaColumn *col = gda_data_model_describe_column (model, i);
			g_print ("Model col %d has type %s\n", i, gda_type_to_string (gda_column_get_gda_type (col)));
		}
#endif

		str = (gchar *) gda_data_model_get_column_title (model, i);
		if (str)
			cols_size [i] = g_utf8_strlen (str, -1);
		else
			cols_size [i] = 6; /* for "<none>" */

		gdacol = gda_data_model_describe_column (model, i);
		coltype = gda_column_get_gda_type (gdacol);
		/*g_string_append_printf (string, "COL %d is a %s\n", i+1, gda_type_to_string (coltype));*/
		if ((coltype == G_TYPE_INT64) ||
		    (coltype == G_TYPE_UINT64) ||
		    (coltype == G_TYPE_INT) ||
		    (coltype == GDA_TYPE_NUMERIC) ||
		    (coltype == G_TYPE_FLOAT) ||
		    (coltype == GDA_TYPE_SHORT) ||
		    (coltype == GDA_TYPE_USHORT) ||
		    (coltype == G_TYPE_CHAR) ||
		    (coltype == G_TYPE_UCHAR) ||
		    (coltype == G_TYPE_UINT))
			cols_is_num [i] = TRUE;
		else
			cols_is_num [i] = FALSE;
	}

	/* ... and using column data */
	for (j = 0; j < n_rows; j++) {
		for (i = 0; i < n_cols; i++) {
			value = gda_data_model_get_value_at (model, i, j);
			str = value ? gda_value_stringify ((GValue*)value) : g_strdup ("_null_");
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
	if (gda_data_model_get_access_flags (model) & GDA_DATA_MODEL_ACCESS_RANDOM) {
		for (j = 0; j < n_rows; j++) {
			for (i = 0; i < n_cols; i++) {
				value = gda_data_model_get_value_at (model, i, j);
				str = value ? gda_value_stringify ((GValue *)value) : g_strdup ("_null_");
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
	}
	else 
		g_string_append (string, _("Model does not support random access, not showing data\n"));

	g_free (cols_size);
	g_free (cols_is_num);

	g_free (offstr);
	
	str = string->str;
	g_string_free (string, FALSE);
	return str;
}
