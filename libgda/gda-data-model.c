/* 
 * GDA common library
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *	Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
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
#include <libgda/gda-xml-database.h>
#include <string.h>

#define PARENT_TYPE G_TYPE_OBJECT
#define CLASS(model) (GDA_DATA_MODEL_CLASS (G_OBJECT_GET_CLASS (model)))

struct _GdaDataModelPrivate {
	gboolean notify_changes;
	GHashTable *column_titles;
	gchar *cmd_text;
	GdaCommandType cmd_type;

	/* update mode */
	gboolean updating;
};

static void gda_data_model_class_init (GdaDataModelClass *klass);
static void gda_data_model_init       (GdaDataModel *model, GdaDataModelClass *klass);
static void gda_data_model_finalize   (GObject *object);

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
	END_UPDATE,
	LAST_SIGNAL
};

static guint gda_data_model_signals[LAST_SIGNAL];
static GObjectClass *parent_class = NULL;

/*
 * GdaDataModel class implementation
 */

static void
gda_data_model_class_init (GdaDataModelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	gda_data_model_signals[CHANGED] =
		g_signal_new ("changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GdaDataModelClass, changed),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
	gda_data_model_signals[ROW_INSERTED] =
		g_signal_new ("row_inserted",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GdaDataModelClass, row_inserted),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__INT,
                              G_TYPE_NONE, 1, G_TYPE_INT);
	gda_data_model_signals[ROW_UPDATED] =
		g_signal_new ("row_updated",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GdaDataModelClass, row_updated),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__INT,
                              G_TYPE_NONE, 1, G_TYPE_INT);
	gda_data_model_signals[ROW_REMOVED] =
		g_signal_new ("row_removed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GdaDataModelClass, row_removed),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__INT,
                              G_TYPE_NONE, 1, G_TYPE_INT);
	gda_data_model_signals[COLUMN_INSERTED] =
		g_signal_new ("column_inserted",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GdaDataModelClass, column_inserted),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__INT,
                              G_TYPE_NONE, 1, G_TYPE_INT);
	gda_data_model_signals[COLUMN_UPDATED] =
		g_signal_new ("column_updated",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GdaDataModelClass, column_updated),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__INT,
                              G_TYPE_NONE, 1, G_TYPE_INT);
	gda_data_model_signals[COLUMN_REMOVED] =
		g_signal_new ("column_removed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GdaDataModelClass, column_removed),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__INT,
                              G_TYPE_NONE, 1, G_TYPE_INT);
	gda_data_model_signals[BEGIN_UPDATE] =
		g_signal_new ("begin_update",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GdaDataModelClass, begin_update),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
	gda_data_model_signals[CANCEL_UPDATE] =
		g_signal_new ("cancel_update",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GdaDataModelClass, cancel_update),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
	gda_data_model_signals[END_UPDATE] =
		g_signal_new ("end_update",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GdaDataModelClass, end_update),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);

	object_class->finalize = gda_data_model_finalize;
	klass->changed = NULL;
	klass->begin_update = NULL;
	klass->cancel_update = NULL;
	klass->end_update = NULL;
	klass->get_n_rows = NULL;
	klass->get_n_columns = NULL;
	klass->describe_column = NULL;
	klass->get_row = NULL;
	klass->get_value_at = NULL;
	klass->is_updatable = NULL;
	klass->append_row = NULL;
	klass->remove_row = NULL;
	klass->update_row = NULL;
	klass->append_column = NULL;
	klass->remove_column = NULL;
	klass->update_column = NULL;
}

static void
gda_data_model_init (GdaDataModel *model, GdaDataModelClass *klass)
{
	g_return_if_fail (GDA_IS_DATA_MODEL (model));

	model->priv = g_new (GdaDataModelPrivate, 1);
	model->priv->notify_changes = TRUE;
	model->priv->column_titles = g_hash_table_new (g_direct_hash,
						       g_direct_equal);
	model->priv->updating = FALSE;
	model->priv->cmd_text = NULL;
	model->priv->cmd_type = GDA_COMMAND_TYPE_INVALID;
}

static void
free_hash_string (gpointer key, gpointer value, gpointer user_data)
{
	g_free (value);
}

static void
gda_data_model_finalize (GObject *object)
{
	GdaDataModel *model = (GdaDataModel *) object;

	g_return_if_fail (GDA_IS_DATA_MODEL (model));

	/* free memory */
	g_hash_table_foreach (model->priv->column_titles, free_hash_string, NULL);
	g_hash_table_destroy (model->priv->column_titles);
	model->priv->column_titles = NULL;

	g_free (model->priv->cmd_text);
	model->priv->cmd_text = NULL;

	g_free (model->priv);
	model->priv = NULL;

	/* chain to parent class */
	parent_class->finalize (object);
}

GType
gda_data_model_get_type (void)
{
	static GType type = 0;

	if (!type) {
		if (type == 0) {
			static GTypeInfo info = {
				sizeof (GdaDataModelClass),
				(GBaseInitFunc) NULL,
				(GBaseFinalizeFunc) NULL,
				(GClassInitFunc) gda_data_model_class_init,
				NULL, NULL,
				sizeof (GdaDataModel),
				0,
				(GInstanceInitFunc) gda_data_model_init
			};
			type = g_type_register_static (PARENT_TYPE, "GdaDataModel", &info, G_TYPE_FLAG_ABSTRACT);
		}
	}

	return type;
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

	if (model->priv->notify_changes) {
		g_signal_emit (G_OBJECT (model),
			       gda_data_model_signals[CHANGED],
			       0);
	}
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

	if (model->priv->notify_changes) {
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

	if (model->priv->notify_changes) {
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

	if (model->priv->notify_changes) {
		g_signal_emit (G_OBJECT (model),
			       gda_data_model_signals[ROW_REMOVED],
			       0, row);

		gda_data_model_changed (model);
	}
}

/**
 * gda_data_model_column_inserted
 * @model: a #GdaDataModel object.
 * @col: column number.
 *
 * Emits the 'column_inserted' and 'changed' signals on @model.
 */
void
gda_data_model_column_inserted (GdaDataModel *model, gint col)
{
	g_return_if_fail (GDA_IS_DATA_MODEL (model));

	if (model->priv->notify_changes) {
		g_signal_emit (G_OBJECT (model),
			       gda_data_model_signals[COLUMN_INSERTED],
			       0, col);

		gda_data_model_changed (model);
	}
}

/**
 * gda_data_model_column_updated
 * @model: a #GdaDataModel object.
 * @col: column number.
 *
 * Emits the 'column_updated' and 'changed' signals on @model.
 */
void
gda_data_model_column_updated (GdaDataModel *model, gint col)
{
	g_return_if_fail (GDA_IS_DATA_MODEL (model));

	if (model->priv->notify_changes) {
		g_signal_emit (G_OBJECT (model),
			       gda_data_model_signals[COLUMN_UPDATED],
			       0, col);

		gda_data_model_changed (model);
	}
}

/**
 * gda_data_model_column_removed
 * @model: a #GdaDataModel object.
 * @col: column number.
 *
 * Emits the 'column_removed' and 'changed' signal on @model.
 */
void
gda_data_model_column_removed (GdaDataModel *model, gint col)
{
	g_return_if_fail (GDA_IS_DATA_MODEL (model));

	if (model->priv->notify_changes) {
		g_signal_emit (G_OBJECT (model),
			       gda_data_model_signals[COLUMN_REMOVED],
			       0, col);

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
	model->priv->notify_changes = FALSE;
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
	model->priv->notify_changes = TRUE;
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
	g_return_val_if_fail (CLASS (model)->get_n_rows != NULL, -1);

	return CLASS (model)->get_n_rows (model);
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
	g_return_val_if_fail (CLASS (model)->get_n_columns != NULL, -1);

	return CLASS (model)->get_n_columns (model);
}

/**
 * gda_data_model_describe_column
 * @model: a #GdaDataModel object.
 * @col: column number.
 *
 * Queries the underlying data model implementation for a description
 * of a given column. That description is returned in the form of
 * a #GdaFieldAttributes structure, which contains all the information
 * about the given column in the data model.
 *
 * Returns: the description of the column.
 */
GdaFieldAttributes *
gda_data_model_describe_column (GdaDataModel *model, gint col)
{
	GdaFieldAttributes *fa;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);
	g_return_val_if_fail (CLASS (model)->describe_column != NULL, NULL);

	fa = CLASS (model)->describe_column (model, col);
	if (!fa) {
		const GdaValue *value;

		/* we generate a basic FieldAttributes structure */
		fa = gda_field_attributes_new ();
		gda_field_attributes_set_defined_size (fa, 0);
		gda_field_attributes_set_name (fa, g_hash_table_lookup (model->priv->column_titles,
									GINT_TO_POINTER (col)));
		gda_field_attributes_set_scale (fa, 0);
		value = gda_data_model_get_value_at (model, col, 0);
		if (value == NULL)
			gda_field_attributes_set_gdatype (fa, GDA_VALUE_TYPE_STRING);
		else
			gda_field_attributes_set_gdatype (fa, gda_value_get_type (value));

		gda_field_attributes_set_allow_null (fa, TRUE);
	}

	return fa;
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
	gint n_cols;
	gchar *title;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

	n_cols = gda_data_model_get_n_columns (model);
	if (col < n_cols && col >= 0){
		title = g_hash_table_lookup (model->priv->column_titles,
					     GINT_TO_POINTER (col));
		if (title == NULL) {
			GdaFieldAttributes *fa;

			fa = gda_data_model_describe_column (model, col);
			if (fa) {
				gda_data_model_set_column_title (model, col, title);
				gda_field_attributes_free (fa);

				return g_hash_table_lookup (model->priv->column_titles,
							    GINT_TO_POINTER (col));
			}
			else
				title = "";
		}
	}
	else
		title = "";

	return (const gchar *) title;
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
	gint n_cols;

	g_return_if_fail (GDA_IS_DATA_MODEL (model));

	n_cols = gda_data_model_get_n_columns (model);
	if (col >= 0 && col < n_cols) {
		gpointer key, value;

		if (g_hash_table_lookup_extended (model->priv->column_titles,
						  GINT_TO_POINTER (col),
						  &key, &value)) {
			g_hash_table_remove (model->priv->column_titles,
					     GINT_TO_POINTER (col));
			g_free (value);
		}

		g_hash_table_insert (model->priv->column_titles, 
				     GINT_TO_POINTER (col), g_strdup (title));
	}
}

/**
 * gda_data_model_get_column_position
 * @model: a #GdaDataModel object.
 * @title: column title.
 *
 * Gets the position of a column on the data model, based on
 * the column's title.
 *
 * Returns: the position of the column in the data model, or -1
 * if the column could not be found.
 */
gint
gda_data_model_get_column_position (GdaDataModel *model, const gchar *title)
{
	gint n_cols;
	gint i;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), -1);
	g_return_val_if_fail (title != NULL, -1);

	n_cols = gda_data_model_get_n_columns (model);
	for (i = 0; i < n_cols; i++) {
		gpointer value;

		value = g_hash_table_lookup (model->priv->column_titles,
					     GINT_TO_POINTER (i));
		if (value && !strcmp (title, (const char *) value))
			return i;
	}

	return -1;
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
const GdaRow *
gda_data_model_get_row (GdaDataModel *model, gint row)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);
	g_return_val_if_fail (CLASS (model)->get_row != NULL, NULL);

	return CLASS (model)->get_row (model, row);
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
 * Returns: a #GdaValue containing the value stored in the given
 * position, or %NULL on error (out-of-bound position, etc).
 */
const GdaValue *
gda_data_model_get_value_at (GdaDataModel *model, gint col, gint row)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);
	g_return_val_if_fail (CLASS (model)->get_value_at != NULL, NULL);

	return CLASS (model)->get_value_at (model, col, row);
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
	g_return_val_if_fail (CLASS (model)->is_updatable != NULL, FALSE);

	return CLASS (model)->is_updatable (model);
}

/**
 * gda_data_model_append_row
 * @model: a #GdaDataModel object.
 * @values: #GList of #GdaValue* representing the row to add.  The
 *          length must match model's column count.  These #GdaValue
 *	    are value-copied.  The user is still responsible for freeing them.
 *
 * Appends a row to the given data model.
 *
 * Returns: the added row.
 */
const GdaRow *
gda_data_model_append_row (GdaDataModel *model, const GList *values)
{
	const GdaRow *row;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);
	g_return_val_if_fail (CLASS (model)->append_row != NULL, NULL);

	row = CLASS (model)->append_row (model, values);
	gda_data_model_row_inserted (model, gda_row_get_number ((GdaRow *) row));
	return row;
}

/**
 * gda_data_model_remove_row
 * @model: a #GdaDataModel object.
 * @row: the #GdaRow to be removed.
 *
 * Removes a row from the data model. This results in the underlying
 * database row being removed in the database.
 *
 * Returns: %TRUE if successful, %FALSE otherwise.
 */
gboolean
gda_data_model_remove_row (GdaDataModel *model, const GdaRow *row)
{
	gboolean result;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);
	g_return_val_if_fail (row != NULL, FALSE);
	g_return_val_if_fail (CLASS (model)->remove_row != NULL, FALSE);

	result = CLASS (model)->remove_row (model, row);
	if (result) {
		gda_data_model_row_removed (model, gda_row_get_number ((GdaRow *) row));
	}

	return result;
}

/**
 * gda_data_model_update_row
 * @model: a #GdaDataModel object.
 * @row: the #GdaRow to be updated.
 *
 * Updates a row data model. This results in the underlying
 * database row's values being changed.
 *
 * Returns: %TRUE if successful, %FALSE otherwise.
 */
gboolean
gda_data_model_update_row (GdaDataModel *model, const GdaRow *row)
{
	gboolean result;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);
	g_return_val_if_fail (row != NULL, FALSE);
	g_return_val_if_fail (CLASS (model)->update_row != NULL, FALSE);

	result = CLASS (model)->update_row (model, row);
	if (result) {
		gda_data_model_row_updated (model, gda_row_get_number ((GdaRow *) row));
	}
	return result;
}

/**
 * gda_data_model_append_column
 * @model: a #GdaDataModel object.
 * @col: a #GdaFieldAttributes describing the column to add.
 *
 * Appends a column to the given data model.  If successful, the position of
 * the new column in the data model is set on @col, and you can grab it using
 * @gda_field_attributes_get_position.
 *
 * Returns: %TRUE if successful, %FALSE otherwise.
 */
gboolean
gda_data_model_append_column (GdaDataModel *model, GdaFieldAttributes *col)
{
	gboolean result;
	
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);
	g_return_val_if_fail (CLASS (model)->append_column != NULL, FALSE);
	g_return_val_if_fail (col != NULL, FALSE);

	result = CLASS (model)->append_column (model, col);
	if (result) {
		gda_data_model_column_inserted (model,
						gda_field_attributes_get_position (col));
	}

	return result;
}

/**
 * gda_data_model_remove_column
 * @model: a #GdaDataModel object.
 * @col: the column to be removed.
 *
 * Removes a column from the data model. This means that all values attached to this
 * column in the data model will be destroyed in the underlying database.
 *
 * Returns: %TRUE if successful, %FALSE otherwise.
 */
gboolean
gda_data_model_remove_column (GdaDataModel *model, const GdaFieldAttributes *col)
{
	gboolean result;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);
	g_return_val_if_fail (CLASS (model)->remove_column != NULL, FALSE);
	g_return_val_if_fail (col != NULL, FALSE);

	result = CLASS (model)->remove_column (model, col);
	if (result) {
		gda_data_model_column_removed (model, gda_field_attributes_get_position (col));
	}

	return result;
}

/**
 * gda_data_model_update_column
 * @model: a #GdaDataModel object.
 * @col: the column to be updated.
 *
 * Updates a column in the given data model. This results in the underlying
 * database row's values being changed.
 *
 * Returns: %TRUE if successful, %FALSE otherwise.
 */
gboolean
gda_data_model_update_column (GdaDataModel *model, const GdaFieldAttributes *col)
{
	gboolean result;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);
	g_return_val_if_fail (CLASS (model)->update_column != NULL, FALSE);
	g_return_val_if_fail (col != NULL, FALSE);

	result = CLASS (model)->update_column (model, col);
	if (result) {
		gda_data_model_column_updated (model, gda_field_attributes_get_position (col));
	}
	return result;
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
 * This callback function can return %FALSE to stop the processing. If it
 * returns %TRUE, processing will continue until no rows remain.
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
 * @gda_data_model_cancel_update or @gda_data_model_end_update have
 * been called.
 *
 * Returns: %TRUE if updating mode, %FALSE otherwise.
 */
gboolean
gda_data_model_has_changed (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);
	return model->priv->updating;
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
	g_return_val_if_fail (model->priv->updating == FALSE, FALSE);

	if (!gda_data_model_is_updatable (model)) {
		gda_log_error (_("Data model %p is not updatable"), model);
		return FALSE;
	}

	model->priv->updating = TRUE;
	g_signal_emit (G_OBJECT (model),
		       gda_data_model_signals[BEGIN_UPDATE], 0);

	return model->priv->updating;
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
	g_return_val_if_fail (model->priv->updating, FALSE);

	g_signal_emit (G_OBJECT (model),
		       gda_data_model_signals[CANCEL_UPDATE], 0);
	model->priv->updating = FALSE;

	return TRUE;
}

/**
 * gda_data_model_end_update
 * @model: a #GdaDataModel object.
 *
 * Approves all modifications and send them to the underlying
 * data source/store.
 *
 * Returns: %TRUE on success, %FALSE if there was an error.
 */
gboolean
gda_data_model_end_update (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);
	g_return_val_if_fail (model->priv->updating, FALSE);

	g_signal_emit (G_OBJECT (model), gda_data_model_signals[END_UPDATE], 0);
	model->priv->updating = FALSE;

	return TRUE;
}

static gchar *
export_to_separated (GdaDataModel *model, gchar sep)
{
	GString *str;
	gchar *retval;
	gint cols, c, rows, r;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

	str = g_string_new ("");
	cols = gda_data_model_get_n_columns (model);
	rows = gda_data_model_get_n_rows (model);

	for (r = 0; r < rows; r++) {
		if (r > 0)
			str = g_string_append_c (str, '\n');

		for (c = 0; c < cols; c++) {
			const GdaValue *value;
			gchar *txt;

			value = gda_data_model_get_value_at (model, c, r);
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
 * gda_data_model_to_comma_separated
 * @model: a #GdaDataModel object.
 *
 * Converts the given model into a comma-separated series of rows.
 *
 * Returns: the representation of the model. You should free this
 * string when you no longer need it.
 */
gchar *
gda_data_model_to_comma_separated (GdaDataModel *model)
{
	return export_to_separated (model, ',');
}

/**
 * gda_data_model_to_tab_separated
 * @model: a #GdaDataModel object.
 *
 * Converts the given model into a tab-separated series of rows.
 *
 * Returns: the representation of the model. You should free this
 * string when you no longer need it.
 */
gchar *
gda_data_model_to_tab_separated (GdaDataModel *model)
{
	return export_to_separated (model, '\t');
}

/**
 * gda_data_model_to_xml
 * @model: a #GdaDataModel object.
 * @standalone: whether ... 
 *
 * Converts the given model into a XML representation.
 *
 * Returns: the representation of the model. You should free this
 * string when you no longer need it.
 */
gchar *
gda_data_model_to_xml (GdaDataModel *model, gboolean standalone)
{
	xmlChar *xml_contents;
	xmlNodePtr xml_node;
	gchar *xml;
	gint size;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

	xml_node = gda_data_model_to_xml_node (model, "exported_model");

	if (standalone) {
		xmlDocPtr xml_doc;
		xmlNodePtr xml_root, xml_tables;
	
		xml_doc = xmlNewDoc ("1.0");
		
		xml_root = xmlNewDocNode (xml_doc, NULL, "database", NULL);
		xmlDocSetRootElement (xml_doc, xml_root);

		xml_tables = xmlNewChild (xml_root, NULL, "tables", NULL);
		xmlAddChild (xml_tables, xml_node);

		xmlDocDumpMemory (xml_doc, &xml_contents, &size);
		xmlFreeDoc (xml_doc);
		
		xml = g_strdup (xml_contents);
		xmlFree (xml_contents);
	}
	else {
		xmlBufferPtr xml_buf = xmlBufferCreate ();
		xmlNodeDump (xml_buf, NULL, xml_node, 0, 0); 
		xml_contents = (xmlChar *) xmlBufferContent (xml_buf);
		xmlBufferFree (xml_buf);
		xml = g_strdup (xml_contents);
	}

	return xml;
}

static void
xml_set_boolean (xmlNodePtr node, const gchar *name, gboolean value)
{
	xmlSetProp (node, name, value ? "1" : "0");
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
 * @name: name to use for the XML resulting table.
 *
 * Converts a #GdaDataModel into a xmlNodePtr (as used in libxml).
 *
 * Returns: a xmlNodePtr representing the whole data model.
 */
xmlNodePtr
gda_data_model_to_xml_node (GdaDataModel *model, const gchar *name)
{
	xmlNodePtr node;
	gint cols, rows, i;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

	node = xmlNewNode (NULL, "table");
	if (name != NULL)
		xmlSetProp (node, "name", name);

	/* set the table structure */
	rows = gda_data_model_get_n_rows (model);
	cols = gda_data_model_get_n_columns (model);
	for (i = 0; i < cols; i++) {
		GdaFieldAttributes *fa;
		xmlNodePtr field;

		fa = gda_data_model_describe_column (model, i);
		if (!fa) {
			xmlFreeNode (node);
			return NULL;
		}

		field = xmlNewChild (node, NULL, "field", NULL);
		xmlSetProp (field, "name", gda_field_attributes_get_name (fa));
		xmlSetProp (field, "caption", gda_field_attributes_get_caption (fa));
		xmlSetProp (field, "gdatype", gda_type_to_string (gda_field_attributes_get_gdatype (fa)));
		xml_set_int (field, "size", gda_field_attributes_get_defined_size (fa));
		xml_set_int (field, "scale", gda_field_attributes_get_scale (fa));
		xml_set_boolean (field, "pkey", gda_field_attributes_get_primary_key (fa));
		xml_set_boolean (field, "unique", gda_field_attributes_get_unique_key (fa));
		xml_set_boolean (field, "isnull", gda_field_attributes_get_allow_null (fa));
		xml_set_boolean (field, "auto_increment", gda_field_attributes_get_auto_increment (fa));
		xmlSetProp (field, "references", gda_field_attributes_get_references (fa));
		xml_set_int (field, "position", i);
	}
	
	/* add the model data to the XML output */
	if (rows > 0) {
		xmlNodePtr row, data, field;
		gint r, c;

		data = xmlNewChild (node, NULL, "data", NULL);
		for (r = 0; r < rows; r++) {
			row = xmlNewChild (data, NULL, "row", NULL);
			xml_set_int (row, "position", r);
			for (c = 0; c < cols; c++) {
				const GdaValue *value;
				gchar *str;

				value = gda_data_model_get_value_at (model, c, r);
				if (gda_value_get_type (value) == GDA_VALUE_TYPE_BOOLEAN)
					str = g_strdup (gda_value_get_boolean (value) ? "TRUE" : "FALSE");
				else
					str = gda_value_stringify (value);
				field = xmlNewChild (row, NULL, "value", str);
				xml_set_int (field, "position", c);
				xmlSetProp (field, "gdatype", gda_type_to_string (gda_value_get_type (value)));

				g_free (str);
			}
		}
	}

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

	values = g_ptr_array_new ();
	g_ptr_array_set_size (values, gda_data_model_get_n_columns (model));
	for (xml_field = xml_row->xmlChildrenNode; xml_field != NULL; xml_field = xml_field->next) {
		gint pos;
		GdaValue *value;

		if (strcmp (xml_field->name, "value"))
			continue;

		pos = atoi (xmlGetProp (xml_field, "position"));
		if (pos < 0 || pos >= gda_data_model_get_n_columns (model)) {
			g_warning ("add_xml_row(): invalid position on 'field' node");
			retval = FALSE;
			break;
		}

		if (g_ptr_array_index (values, pos) != NULL) {
			g_warning ("add_xml_row(): two fields with the same position");
			retval = FALSE;
			break;
		}

		/* create the value for this field */
		value = gda_value_new_from_xml ((const xmlNodePtr) xml_field);
		if (!value) {
			g_warning ("add_xml_row(): cannot retrieve value from XML node");
			retval = FALSE;
			break;
		}

		g_ptr_array_index (values, pos) = value;
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
			gda_data_model_append_row (model, value_list);

		g_list_free (value_list);
	}

	for (i = 0; i < values->len; i++)
		gda_value_free ((GdaValue *) g_ptr_array_index (values, i));

	return retval;
}

/**
 * gda_data_model_add_data_from_xml_node
 * @model: a #GdaDataModel.
 * @node: a XML node representing a <data> XML node.
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
	return model->priv->cmd_text;
}

/**
 * gda_data_model_set_command_text
 * @model: a #GdaDataModel.
 * @txt: the command text.
 *
 * Sets the command text of the given @model.
 */
void
gda_data_model_set_command_text (GdaDataModel *model, const gchar *txt)
{
	g_return_if_fail (GDA_IS_DATA_MODEL (model));
	g_return_if_fail (txt != NULL);

	if (model->priv->cmd_text)
		g_free (model->priv->cmd_text);
	model->priv->cmd_text = g_strdup (txt);
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
	return model->priv->cmd_type;
}

/**
 * gda_data_model_set_command_type
 * @model: a #GdaDataModel.
 * @type: the type of the command (one of #GdaCommandType)
 *
 * Sets the type of command that generated this data model.
 */
void
gda_data_model_set_command_type (GdaDataModel *model, GdaCommandType type)
{
	g_return_if_fail (GDA_IS_DATA_MODEL (model));
	model->priv->cmd_type = type;
}

