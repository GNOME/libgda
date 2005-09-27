/* 
 * GDA common library
 * Copyright (C) 1998 - 2005 The GNOME Foundation.
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

#include <libgda/gda-intl.h>
#include <libgda/gda-data-model.h>
#include <libgda/gda-log.h>
#include <libgda/gda-util.h>
#include <libgda/gda-row.h>
#include <string.h>

#define PARENT_TYPE G_TYPE_OBJECT
#define CLASS(model) (GDA_DATA_MODEL_CLASS (G_OBJECT_GET_CLASS (model)))


static void gda_data_model_iface_init (gpointer g_class);

/* signals */
enum {
	CHANGED,
	ROW_INSERTED,
	ROW_UPDATED,
	ROW_REMOVED,
	COLUMN_INSERTED,
	COLUMN_UPDATED,
	COLUMN_REMOVED,
	BEGIN_UPDATE,
	CANCEL_UPDATE,
	COMMIT_UPDATE,
	LAST_SIGNAL
};

static guint gda_data_model_signals[LAST_SIGNAL];

GType
gda_data_model_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaDataModelIface),
			(GBaseInitFunc) gda_data_model_iface_init,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) NULL,
			NULL,
			NULL,
			0,
			0,
			(GInstanceInitFunc) NULL
		};
		
		type = g_type_register_static (G_TYPE_INTERFACE, "GdaDataModel", &info, 0);
		g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
	}
	return type;
}

static void
gda_data_model_iface_init (gpointer g_class)
{
	static gboolean initialized = FALSE;

	if (! initialized) {
		gda_data_model_signals[CHANGED] =
			g_signal_new ("changed",
				      GDA_TYPE_DATA_MODEL,
				      G_SIGNAL_RUN_LAST,
				      G_STRUCT_OFFSET (GdaDataModelIface, changed),
				      NULL, NULL,
				      g_cclosure_marshal_VOID__VOID,
				      G_TYPE_NONE, 0);
		gda_data_model_signals[ROW_INSERTED] =
			g_signal_new ("row_inserted",
				      GDA_TYPE_DATA_MODEL,
				      G_SIGNAL_RUN_LAST,
				      G_STRUCT_OFFSET (GdaDataModelIface, row_inserted),
				      NULL, NULL,
				      g_cclosure_marshal_VOID__INT,
				      G_TYPE_NONE, 1, G_TYPE_INT);
		gda_data_model_signals[ROW_UPDATED] =
			g_signal_new ("row_updated",
				      GDA_TYPE_DATA_MODEL,
				      G_SIGNAL_RUN_LAST,
				      G_STRUCT_OFFSET (GdaDataModelIface, row_updated),
				      NULL, NULL,
				      g_cclosure_marshal_VOID__INT,
				      G_TYPE_NONE, 1, G_TYPE_INT);
		gda_data_model_signals[ROW_REMOVED] =
			g_signal_new ("row_removed",
				      GDA_TYPE_DATA_MODEL,
				      G_SIGNAL_RUN_LAST,
				      G_STRUCT_OFFSET (GdaDataModelIface, row_removed),
				      NULL, NULL,
				      g_cclosure_marshal_VOID__INT,
				      G_TYPE_NONE, 1, G_TYPE_INT);
		gda_data_model_signals[COLUMN_INSERTED] =
			g_signal_new ("column_inserted",
				      GDA_TYPE_DATA_MODEL,
				      G_SIGNAL_RUN_LAST,
				      G_STRUCT_OFFSET (GdaDataModelIface, column_inserted),
				      NULL, NULL,
				      g_cclosure_marshal_VOID__INT,
				      G_TYPE_NONE, 1, G_TYPE_INT);
		gda_data_model_signals[COLUMN_UPDATED] =
			g_signal_new ("column_updated",
				      GDA_TYPE_DATA_MODEL,
				      G_SIGNAL_RUN_LAST,
				      G_STRUCT_OFFSET (GdaDataModelIface, column_updated),
				      NULL, NULL,
				      g_cclosure_marshal_VOID__INT,
				      G_TYPE_NONE, 1, G_TYPE_INT);
		gda_data_model_signals[COLUMN_REMOVED] =
			g_signal_new ("column_removed",
				      GDA_TYPE_DATA_MODEL,
				      G_SIGNAL_RUN_LAST,
				      G_STRUCT_OFFSET (GdaDataModelIface, column_removed),
				      NULL, NULL,
				      g_cclosure_marshal_VOID__INT,
				      G_TYPE_NONE, 1, G_TYPE_INT);
		gda_data_model_signals[BEGIN_UPDATE] =
			g_signal_new ("begin_update",
				      GDA_TYPE_DATA_MODEL,
				      G_SIGNAL_RUN_LAST,
				      G_STRUCT_OFFSET (GdaDataModelIface, begin_update),
				      NULL, NULL,
				      g_cclosure_marshal_VOID__VOID,
				      G_TYPE_NONE, 0);
		gda_data_model_signals[CANCEL_UPDATE] =
			g_signal_new ("cancel_update",
				      GDA_TYPE_DATA_MODEL,
				      G_SIGNAL_RUN_LAST,
				      G_STRUCT_OFFSET (GdaDataModelIface, cancel_update),
				      NULL, NULL,
				      g_cclosure_marshal_VOID__VOID,
				      G_TYPE_NONE, 0);
		gda_data_model_signals[COMMIT_UPDATE] =
			g_signal_new ("commit_update",
				      GDA_TYPE_DATA_MODEL,
				      G_SIGNAL_RUN_LAST,
				      G_STRUCT_OFFSET (GdaDataModelIface, commit_update),
				      NULL, NULL,
				      g_cclosure_marshal_VOID__VOID,
				      G_TYPE_NONE, 0);

		initialized = TRUE;
	}
}

static gboolean
do_notify_changes (GdaDataModel *model)
{
	gboolean notify_changes = TRUE;
	if (GDA_DATA_MODEL_GET_IFACE (model)->i_get_notify)
		notify_changes = (GDA_DATA_MODEL_GET_IFACE (model)->i_get_notify) (model);
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
		g_signal_emit (G_OBJECT (model),
			       gda_data_model_signals[CHANGED],
			       0);
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
			if ((gda_column_get_gdatype (column) == GDA_VALUE_TYPE_UNKNOWN))
				gda_column_set_gdatype (column, value ? gda_value_get_type (value) : GDA_VALUE_TYPE_NULL);
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
	
	if (GDA_DATA_MODEL_GET_IFACE (model)->i_set_notify)
		(GDA_DATA_MODEL_GET_IFACE (model)->i_set_notify) (model, FALSE);
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

	if (GDA_DATA_MODEL_GET_IFACE (model)->i_set_notify)
		(GDA_DATA_MODEL_GET_IFACE (model)->i_set_notify) (model, TRUE);
	else
		g_warning ("%s() method not supported\n", __FUNCTION__);
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

	if (GDA_DATA_MODEL_GET_IFACE (model)->i_get_n_rows)
		return (GDA_DATA_MODEL_GET_IFACE (model)->i_get_n_rows) (model);
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
	
	if (GDA_DATA_MODEL_GET_IFACE (model)->i_get_n_columns)
		return (GDA_DATA_MODEL_GET_IFACE (model)->i_get_n_columns) (model);
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
 * and should not be destroyed.
 *
 * Returns: the description of the column.
 */
GdaColumn *
gda_data_model_describe_column (GdaDataModel *model, gint col)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

	if (GDA_DATA_MODEL_GET_IFACE (model)->i_describe_column)
		return (GDA_DATA_MODEL_GET_IFACE (model)->i_describe_column) (model, col);
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
 * gda_data_model_get_row
 * @model: a #GdaDataModel object.
 * @row: row number.
 *
 * Retrieves a given row from a data model.
 *
 * Returns: a #GdaRow object.
 */
GdaRow *
gda_data_model_get_row (GdaDataModel *model, gint row)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

	if (GDA_DATA_MODEL_GET_IFACE (model)->i_get_row)
		return (GDA_DATA_MODEL_GET_IFACE (model)->i_get_row) (model, row);
	else {
		g_warning ("%s() method not supported\n", __FUNCTION__);
		return NULL;
	}
}

/**
 * gda_data_model_get_value_at
 * @model: a #GdaDataModel object.
 * @col: column number.
 * @row: row number.
 *
 * Retrieves the data stored in the given position (identified by
 * the @col and @row parameters) on a data model.
 *
 * This is the main function for accessing data in a model.
 *
 * Note that the returned #GdaValue must not be modified directly (unexpected behavoiurs may
 * occur if you do so). If you want to
 * modify a value stored in a #GdaDataModel, you must first obtain a #GdaRow object using
 * gda_data_model_get_row() and then call gda_row_set_value() on that row object.
 *
 * Returns: a #GdaValue containing the value stored in the given
 * position, or %NULL on error (out-of-bound position, etc).
 */
const GdaValue *
gda_data_model_get_value_at (GdaDataModel *model, gint col, gint row)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

	if (GDA_DATA_MODEL_GET_IFACE (model)->i_get_value_at)
		return (GDA_DATA_MODEL_GET_IFACE (model)->i_get_value_at) (model, col, row);
	else {
		g_warning ("%s() method not supported\n", __FUNCTION__);
		return NULL;
	}
}

/**
 * gda_data_model_is_updatable
 * @model: a #GdaDataModel object.
 *
 * Checks whether the given data model can be updated or not.
 *
 * Returns: %TRUE if it can be updated, %FALSE if not.
 */
gboolean
gda_data_model_is_updatable (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);

	if (GDA_DATA_MODEL_GET_IFACE (model)->i_is_updatable)
		return (GDA_DATA_MODEL_GET_IFACE (model)->i_is_updatable) (model);
	else 
		return FALSE;
}

/**
 * gda_data_model_append_values
 * @model: a #GdaDataModel object.
 * @values: #GList of #GdaValue* representing the row to add.  The
 *          length must match model's column count.  These #GdaValue
 *	    are value-copied.  The user is still responsible for freeing them.
 *
 * Appends a row to the given data model.
 *
 * Returns: the added row.
 */
GdaRow *
gda_data_model_append_values (GdaDataModel *model, const GList *values)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

	if (GDA_DATA_MODEL_GET_IFACE (model)->i_append_values) {
		GdaRow *row;
		row = (GDA_DATA_MODEL_GET_IFACE (model)->i_append_values) (model, values);
		gda_data_model_row_inserted (model, gda_row_get_number ((GdaRow *) row));
		return row;
	}
	else {
		g_warning ("%s() method not supported\n", __FUNCTION__);
		return NULL;
	}
}

/**
 * gda_data_model_append_row
 * @model: a #GdaDataModel object.
 * @row: the #GdaRow to be appended
 * 
 * Appends a row to the data model. The 'number' attribute of @row may be
 * modified by the data model to represent the actual row position.
 *
 * The @row object is then referenced by the @model model and can be safely unref'ed by the
 * caller. A pointer to that row object can be obtained at any time using the 
 * gda_data_model_get_row() method.
 *
 * The @row can be created using gda_row_new() or gda_row_new_from_list() (passing %NULL
 * for the @model argument).
 *
 * Returns: %TRUE if successful, %FALSE otherwise.
 */
gboolean
gda_data_model_append_row (GdaDataModel *model, GdaRow *row)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);
	g_return_val_if_fail (row != NULL, FALSE);

	if (GDA_DATA_MODEL_GET_IFACE (model)->i_append_row) {
		return (GDA_DATA_MODEL_GET_IFACE (model)->i_append_row) (model, row);
	}
	else {
		g_warning ("%s() method not supported\n", __FUNCTION__);
		return FALSE;
	}
}

/**
 * gda_data_model_update_row
 * @model: a #GdaDataModel object.
 * @row: the #GdaRow to be updated.
 *
 * Updates a row data model. This results in the underlying
 * database row's values being changed.
 *
 * NOTE: the @row GdaRow must not be used again because it may or may not
 * be used by the @model model depending on @model's implementation.
 *
 * The @row can be created using gda_row_new() or gda_row_new_from_list() (passing %NULL
 * for the @model argument).
 *
 * Returns: %TRUE if successful, %FALSE otherwise.
 */
gboolean
gda_data_model_update_row (GdaDataModel *model, GdaRow *row)
{
        gboolean result;

        g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);
        g_return_val_if_fail (row != NULL, FALSE);

        if (GDA_DATA_MODEL_GET_IFACE (model)->i_update_row) {
                return (GDA_DATA_MODEL_GET_IFACE (model)->i_update_row) (model, row);
        }
        else {
                g_warning ("%s() method not supported\n", __FUNCTION__);
                return FALSE;
        }
}

/**
 * gda_data_model_remove_row
 * @model: a #GdaDataModel object.
 * @row: the #GdaRow to be removed.
 *
 * Removes a row from the data model. This results in the underlying
 * database row being removed in the database.
 *
 * The @row must have been obtained using the gda_data_model_get_row() function.
 *
 * Returns: %TRUE if successful, %FALSE otherwise.
 */
gboolean
gda_data_model_remove_row (GdaDataModel *model, GdaRow *row)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);
	g_return_val_if_fail (row != NULL, FALSE);

	if (GDA_DATA_MODEL_GET_IFACE (model)->i_remove_row) {
		return (GDA_DATA_MODEL_GET_IFACE (model)->i_remove_row) (model, row);
	}
	else {
		g_warning ("%s() method not supported\n", __FUNCTION__);
		return FALSE;
	}
}

/**
 * gda_data_model_append_column
 * @model: a #GdaDataModel object.
 * @attrs: a #GdaColumn describing the column to add.
 *
 * Appends a column to the given data model.  If successful, the position of
 * a pointer to a #GdaColumn is returned (and its attributes can be updated)
 *
 * Returns: a #GdaColumn if successful, and %NULL if the data model does not suppport this method.
 */
GdaColumn *
gda_data_model_append_column (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);
	
	if (GDA_DATA_MODEL_GET_IFACE (model)->i_append_column) 
		return (GDA_DATA_MODEL_GET_IFACE (model)->i_append_column) (model);
	else {
		g_warning ("%s() method not supported\n", __FUNCTION__);
		return NULL;
	}
}

/**
 * gda_data_model_remove_column
 * @model: a #GdaDataModel object.
 * @col: the column to be removed.
 *
 * Removes a column from the data model. This means that all values attached to this
 * column in the data model will be destroyed in the underlying database.
 *
 * Returns: %TRUE if successful, %FALSE if the data model cannot be modified of if it does not
 * support that method.
 */
gboolean
gda_data_model_remove_column (GdaDataModel *model, gint col)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);

	if (GDA_DATA_MODEL_GET_IFACE (model)->i_remove_column) 
		return (GDA_DATA_MODEL_GET_IFACE (model)->i_remove_column) (model, col);
	else {
		g_warning ("%s() method not supported\n", __FUNCTION__);
		return FALSE;
	}
}

/**
 * gda_data_model_foreach
 * @model: a #GdaDataModel object.
 * @func: callback function.
 * @user_data: context data for the callback function.
 *
 * Calls the specified callback function for each row in the data model.
 * This will just traverse all rows, and call the given callback
 * function for each of them.
 *
 * The callback function must have the following form:
 *
 *      gboolean foreach_func (GdaDataModel *model, GdaRow *row, gpointer user_data)
 *
 * where "row" would be the row being read, and "user_data" the parameter
 * specified in @user_data in the call to gda_data_model_foreach.
 * This callback function can return %FALSE to stop the processing. If
 * it returns %TRUE, processing will continue until no rows remain.
 */
void
gda_data_model_foreach (GdaDataModel *model,
			GdaDataModelForeachFunc func,
			gpointer user_data)
{
	gint rows;
	gint r;
	const GdaRow *row;
	gboolean more;

	g_return_if_fail (GDA_IS_DATA_MODEL (model));
	g_return_if_fail (func != NULL);

	rows = gda_data_model_get_n_rows (model);
	more = TRUE;
	for (r = 0; more && r < rows ; r++) {
		row = gda_data_model_get_row (model, r);
		/* call the callback function */
		more = func (model, (GdaRow *) row, user_data);
	}
}

/**
 * gda_data_model_has_changed
 * @model: a #GdaDataModel object.
 *
 * Checks whether this data model is in updating mode or not. Updating
 * mode is set to %TRUE when @gda_data_model_begin_update has been
 * called successfully, and is not set back to %FALSE until either
 * @gda_data_model_cancel_update or @gda_data_model_commit_update have
 * been called.
 *
 * Returns: %TRUE if updating mode, %FALSE otherwise.
 */
gboolean
gda_data_model_has_changed (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);

	if (GDA_DATA_MODEL_GET_IFACE (model)->i_has_changed) 
		return (GDA_DATA_MODEL_GET_IFACE (model)->i_has_changed) (model);
	else {
		g_warning ("%s() method not supported\n", __FUNCTION__);
		return FALSE;
	}
}

/**
 * gda_data_model_begin_update
 * @model: a #GdaDataModel object.
 *
 * Starts update of this data model. This function should be the
 * first called when modifying the data model.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 */
gboolean
gda_data_model_begin_update (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);

	if (!gda_data_model_is_updatable (model)) {
		gda_log_error (_("Data model %p is not updatable"), model);
		return FALSE;
	}

	if (GDA_DATA_MODEL_GET_IFACE (model)->i_begin_changes) 
		(GDA_DATA_MODEL_GET_IFACE (model)->i_begin_changes) (model);
	
	g_signal_emit (G_OBJECT (model),
		       gda_data_model_signals[BEGIN_UPDATE], 0);
	
	return TRUE;
}

/**
 * gda_data_model_cancel_update
 * @model: a #GdaDataModel object.
 *
 * Cancels update of this data model. This means that all changes
 * will be discarded, and the old data put back in the model.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 */
gboolean
gda_data_model_cancel_update (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);

	if (GDA_DATA_MODEL_GET_IFACE (model)->i_cancel_changes) {
		gboolean status;
		status = (GDA_DATA_MODEL_GET_IFACE (model)->i_cancel_changes) (model);
		if (status)
			g_signal_emit (G_OBJECT (model),
				       gda_data_model_signals[CANCEL_UPDATE], 0);
		return status;
	}
	else {
		g_warning ("%s() method not supported\n", __FUNCTION__);
		return FALSE;
	}
}

/**
 * gda_data_model_commit_update
 * @model: a #GdaDataModel object.
 *
 * Approves all modifications and send them to the underlying
 * data source/store.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 */
gboolean
gda_data_model_commit_update (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);

	if (GDA_DATA_MODEL_GET_IFACE (model)->i_commit_changes) {
		gboolean status;
		status = (GDA_DATA_MODEL_GET_IFACE (model)->i_commit_changes) (model);
		if (status)
			g_signal_emit (G_OBJECT (model),
				       gda_data_model_signals[COMMIT_UPDATE], 0);
		return status;
	}
	else {
		g_warning ("%s() method not supported\n", __FUNCTION__);
		return FALSE;
	}
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

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

	node = xmlNewNode (NULL, "data-array");
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
		rcols = cols;
		rnb_cols = nb_cols;
	}

	/* set the table structure */
	rows = gda_data_model_get_n_rows (model);
	for (i = 0; i < rnb_cols; i++) {
		GdaColumn *column;
		xmlNodePtr field;
		const gchar *cstr;

		column = gda_data_model_describe_column (model, rcols [i]);
		if (!column) {
			xmlFreeNode (node);
			return NULL;
		}

		field = xmlNewChild (node, NULL, "field", NULL);
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
		xmlSetProp (field, "gdatype", gda_type_to_string (gda_column_get_gdatype (column)));
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
			xmlSetProp (field, "references", cstr);
	}
	
	/* add the model data to the XML output */
	if (rows > 0) {
		xmlNodePtr row, data, field;
		gint r, c;

		data = xmlNewChild (node, NULL, "data", NULL);
		for (r = 0; r < rows; r++) {
			row = xmlNewChild (data, NULL, "row", NULL);
			for (c = 0; c < rnb_cols; c++) {
				GdaValue *value;
				gchar *str;

				value = (GdaValue *) gda_data_model_get_value_at (model, rcols [c], r);
				if (gda_value_get_type (value) == GDA_VALUE_TYPE_BOOLEAN)
					str = g_strdup (gda_value_get_boolean (value) ? "TRUE" : "FALSE");
				else
					str = gda_value_stringify (value);
				field = xmlNewChild (row, NULL, "value", str);

				g_free (str);
			}
		}
	}

	if (!cols)
		g_free (rcols);

	return node;
}

static gboolean
add_xml_row (GdaDataModel *model, xmlNodePtr xml_row)
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
		GdaValue *value;
		GdaColumn *column;
		GdaValueType gdatype;

		if (strcmp (xml_field->name, "value"))
			continue;

		/* create the value for this field */
		column = gda_data_model_describe_column (model, pos);
		gdatype = gda_column_get_gdatype (column);
		if ((gdatype == GDA_VALUE_TYPE_UNKNOWN) ||
		    (gdatype == GDA_VALUE_TYPE_NULL)) {
			g_warning ("add_xml_row(): cannot retrieve column data type (type is UNKNOWN or NULL)");
			retval = FALSE;
			break;
		}

		value = g_new0 (GdaValue, 1);
		if (!gda_value_set_from_string (value, xmlNodeGetContent (xml_field), gdatype)) {
			g_free (value);
			g_warning ("add_xml_row(): cannot retrieve value from XML node");
			retval = FALSE;
			break;
		}

		g_ptr_array_index (values, pos) = value;
		pos ++;
	}

	if (retval) {
		for (i = 0; i < values->len; i++) {
			GdaValue *value = (GdaValue *) g_ptr_array_index (values, i);

			if (!value) {
				g_warning ("add_xml_row(): there are missing values on the XML node");
				retval = FALSE;
				break;
			}

			value_list = g_list_append (value_list, value);
		}

		if (retval)
			gda_data_model_append_values (model, value_list);

		g_list_free (value_list);
	}

	for (i = 0; i < values->len; i++)
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
gda_data_model_add_data_from_xml_node (GdaDataModel *model, xmlNodePtr node)
{
	xmlNodePtr children;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);
	g_return_val_if_fail (node != NULL, FALSE);

	if (strcmp (node->name, "data") != 0)
		return FALSE;

	for (children = node->xmlChildrenNode; children != NULL; children = children->next) {
		if (!strcmp (children->name, "row")) {
			if (!add_xml_row (model, children))
				return FALSE;
		}
	}

	return TRUE;
}

/**
 * gda_data_model_get_command_text
 * @model: a #GdaDataModel.
 *
 * Gets the text of command that generated this data model.
 *
 * Returns: a string with the command issued.
 */
const gchar *
gda_data_model_get_command_text (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

	if (GDA_DATA_MODEL_GET_IFACE (model)->i_get_command)
		return (GDA_DATA_MODEL_GET_IFACE (model)->i_get_command) (model, NULL);
	else
		return NULL;
}

/**
 * gda_data_model_set_command_text
 * @model: a #GdaDataModel.
 * @txt: the command text.
 *
 * Sets the command text of the given @model.
 *
 * Returns: %TRUE if successful, %FALSE otherwise.
 */
gboolean
gda_data_model_set_command_text (GdaDataModel *model, const gchar *txt)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);
	g_return_val_if_fail (txt != NULL, FALSE);
	
	if (GDA_DATA_MODEL_GET_IFACE (model)->i_set_command)
		return (GDA_DATA_MODEL_GET_IFACE (model)->i_set_command) (model, txt, -1);
	else
		return FALSE;
}

/**
 * gda_data_model_get_command_type
 * @model: a #GdaDataModel.
 *
 * Gets the type of command that generated this data model.
 *
 * Returns: a #GdaCommandType.
 */
GdaCommandType
gda_data_model_get_command_type (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), GDA_COMMAND_TYPE_INVALID);

	if (GDA_DATA_MODEL_GET_IFACE (model)->i_get_command) {
		GdaCommandType type;
		(GDA_DATA_MODEL_GET_IFACE (model)->i_get_command) (model, &type);
		return type;
	}
	else
		return GDA_COMMAND_TYPE_INVALID;
}

/**
 * gda_data_model_set_command_type
 * @model: a #GdaDataModel.
 * @type: the type of the command (one of #GdaCommandType)
 *
 * Sets the type of command that generated this data model.
 *
 * Returns: %TRUE if successful, %FALSE otherwise.
 */
gboolean
gda_data_model_set_command_type (GdaDataModel *model, GdaCommandType type)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);
	if (GDA_DATA_MODEL_GET_IFACE (model)->i_set_command)
		return (GDA_DATA_MODEL_GET_IFACE (model)->i_set_command) (model, NULL, type);
	else
		return FALSE;
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

		str = gda_data_model_get_column_title (model, i);
		cols_size [i] = g_utf8_strlen (str, -1);

		gdacol = gda_data_model_describe_column (model, i);
		coltype = gda_column_get_gdatype (gdacol);
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
			str = value ? gda_value_stringify (value) : g_strdup ("_null_");
			cols_size [i] = MAX (cols_size [i], g_utf8_strlen (str, -1));
			g_free (str);
		}
	}
	
	/* actual dumping of the contents: column titles...*/
	for (i = 0; i < n_cols; i++) {
		gint j;
		str = gda_data_model_get_column_title (model, i);
		if (i != 0)
			g_string_append_printf (string, "%s", sep_col);

		g_string_append_printf (string, "%s", str);
		for (j = 0; j < (cols_size [i] - g_utf8_strlen (str, -1)); j++)
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
			str = value ? gda_value_stringify (value) : g_strdup ("_null_");
			if (i != 0)
				g_string_append_printf (string, "%s", sep_col);
			if (cols_is_num [i])
				g_string_append_printf (string, "%*s", cols_size [i], str);
			else {
				gint j;
				g_string_append_printf (string, "%s", str);
				for (j = 0; j < (cols_size [i] - g_utf8_strlen (str, -1)); j++)
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
