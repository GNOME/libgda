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
#include <libgda/gda-xml-database.h>
#include <string.h>

#define PARENT_TYPE G_TYPE_OBJECT
#define CLASS(model) (GDA_DATA_MODEL_CLASS (G_OBJECT_GET_CLASS (model)))

struct _GdaDataModelPrivate {
	gboolean notify_changes;
	GHashTable *column_titles;
	gchar *cmd_text;
	GdaCommandType cmd_type;

	/* edition mode */
	gboolean editing;
};

static void gda_data_model_class_init (GdaDataModelClass *klass);
static void gda_data_model_init       (GdaDataModel *model, GdaDataModelClass *klass);
static void gda_data_model_finalize   (GObject *object);

enum {
	CHANGED,
	ROW_INSERTED,
	ROW_UPDATED,
	ROW_REMOVED,
	BEGIN_EDIT,
	CANCEL_EDIT,
	END_EDIT,
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
	gda_data_model_signals[BEGIN_EDIT] =
		g_signal_new ("begin_edit",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GdaDataModelClass, begin_edit),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
	gda_data_model_signals[CANCEL_EDIT] =
		g_signal_new ("cancel_edit",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GdaDataModelClass, cancel_edit),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
	gda_data_model_signals[END_EDIT] =
		g_signal_new ("end_edit",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GdaDataModelClass, end_edit),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);

	object_class->finalize = gda_data_model_finalize;
	klass->changed = NULL;
	klass->begin_edit = NULL;
	klass->cancel_edit = NULL;
	klass->end_edit = NULL;
	klass->get_n_rows = NULL;
	klass->get_n_columns = NULL;
	klass->describe_column = NULL;
	klass->get_row = NULL;
	klass->get_value_at = NULL;
	klass->is_editable = NULL;
	klass->append_row = NULL;
	klass->remove_row = NULL;
	klass->update_row = NULL;
}

static void
gda_data_model_init (GdaDataModel *model, GdaDataModelClass *klass)
{
	g_return_if_fail (GDA_IS_DATA_MODEL (model));

	model->priv = g_new (GdaDataModelPrivate, 1);
	model->priv->notify_changes = TRUE;
	model->priv->column_titles = g_hash_table_new (g_direct_hash,
						       g_direct_equal);
	model->priv->editing = FALSE;
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
			type = g_type_register_static (PARENT_TYPE, "GdaDataModel", &info, 0);
		}
	}

	return type;
}

/**
 * gda_data_model_changed
 * @model: a #GdaDataModel object.
 *
 * Notify listeners of the given data model object of changes
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
 * model: a #GdaDataModel object.
 */
void
gda_data_model_row_inserted (GdaDataModel *model, gint row)
{
	g_return_if_fail (GDA_IS_DATA_MODEL (model));

	if (model->priv->notify_changes) {
		g_signal_emit (G_OBJECT (model),
			       gda_data_model_signals[ROW_INSERTED],
			       0, row);
	}
}

/**
 * gda_data_model_row_updated
 * model: a #GdaDataModel object.
 */
void
gda_data_model_row_updated (GdaDataModel *model, gint row)
{
	g_return_if_fail (GDA_IS_DATA_MODEL (model));

	if (model->priv->notify_changes) {
		g_signal_emit (G_OBJECT (model),
			       gda_data_model_signals[ROW_UPDATED],
			       0, row);
	}
}

/**
 * gda_data_model_row_removed
 * model: a #GdaDataModel object.
 */
void
gda_data_model_row_removed (GdaDataModel *model, gint row)
{
	g_return_if_fail (GDA_IS_DATA_MODEL (model));

	if (model->priv->notify_changes) {
		g_signal_emit (G_OBJECT (model),
			       gda_data_model_signals[ROW_REMOVED],
			       0, row);
	}
}

/**
 * gda_data_model_freeze
 * @model: a #GdaDataModel object.
 *
 * Disable notifications of changes on the given data model. To
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
 * Re-enable notifications of changes on the given data model.
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
 * Return the number of rows in the given data model.
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
 * Return the number of columns in the given data model.
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
 * Query the underlying data model implementation for a description
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
 * Return the title for the given column in a data model object.
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
 * Get the position of a column on the data model, based on
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
 * Retrieve a given row from a data model.
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
 * Retrieve the data stored in the given position (identified by
 * the @col and @row parameters) on a data model.
 *
 * This is the main function for accessing data in a model.
 *
 * Returns: a #GdaValue containing the value stored in the given
 * position, or NULL on error (out-of-bound position, etc).
 */
const GdaValue *
gda_data_model_get_value_at (GdaDataModel *model, gint col, gint row)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);
	g_return_val_if_fail (CLASS (model)->get_value_at != NULL, NULL);

	return CLASS (model)->get_value_at (model, col, row);
}

/**
 * gda_data_model_is_editable
 * @model: a #GdaDataModel object.
 *
 * Check whether the given data model can be edited or not.
 *
 * Returns: TRUE if it can be edited, FALSE if not.
 */
gboolean
gda_data_model_is_editable (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);
	g_return_val_if_fail (CLASS (model)->is_editable != NULL, FALSE);

	return CLASS (model)->is_editable (model);
}

/**
 * gda_data_model_append_row
 * @model: a #GdaDataModel object.
 * @values: the row to add.
 *
 * Append a row to the given data model.
 *
 * Returns: the unique ID of the added row.
 */
const GdaRow *
gda_data_model_append_row (GdaDataModel *model, const GList *values)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);
	g_return_val_if_fail (CLASS (model)->append_row != NULL, NULL);

	return CLASS (model)->append_row (model, values);
}

/**
 * gda_data_model_remove_row
 * @model: a #GdaDataModel object.
 * @row: the #GdaRow to be removed.
 *
 * Remove a row from the data model. This results in the underlying
 * database row being removed in the database.
 *
 * Returns: TRUE if successful, FALSE otherwise.
 */
gboolean
gda_data_model_remove_row (GdaDataModel *model, const GdaRow *row)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);
	g_return_val_if_fail (row != NULL, FALSE);
	g_return_val_if_fail (CLASS (model)->remove_row != NULL, FALSE);

	return CLASS (model)->remove_row (model, row);
}

/**
 * gda_data_model_update_row
 * @model: a #GdaDataModel object.
 * @row: the #GdaRow to be updated.
 *
 * Update a row data model. This results in the underlying
 * database row's values being changed.
 *
 * Returns: TRUE if successful, FALSE otherwise.
 */
gboolean
gda_data_model_update_row (GdaDataModel *model, const GdaRow *row)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);
	g_return_val_if_fail (row != NULL, FALSE);
	g_return_val_if_fail (CLASS (model)->update_row != NULL, FALSE);

	return CLASS (model)->update_row (model, row);
}

/**
 * gda_data_model_foreach
 * @model: a #GdaDataModel object.
 * @func: callback function.
 * @user_data: context data for the callback function.
 *
 * Call the specified callback function for each row in the data model.
 * This will just traverse all rows, and call the given callback
 * function for each of them.
 *
 * The callback function must have the following form:
 *
 *      gboolean foreach_func (GdaDataModel *model, GdaRow *row, gpointer user_data)
 *
 * where "row" would be the row being read, and "user_data" the parameter
 * specified in @user_data in the call to gda_data_model_foreach.
 * This callback function can return FALSE to stop the processing. If it
 * returns TRUE, processing will continue until no rows remain.
 */
void
gda_data_model_foreach (GdaDataModel *model,
			GdaDataModelForeachFunc func,
			gpointer user_data)
{
	gint cols;
	gint rows;
	gint c, r;
	GdaRow *row;

	g_return_if_fail (GDA_IS_DATA_MODEL (model));
	g_return_if_fail (func != NULL);

	rows = gda_data_model_get_n_rows (model);
	cols = gda_data_model_get_n_columns (model);

	for (r = 0; r < rows; r++) {
		row = gda_row_new (model, cols);
		for (c = 0; c < cols; c++) {
			GdaValue *value;
			value = gda_value_copy (gda_data_model_get_value_at (model, c, r));
			memcpy (gda_row_get_value (row, c), value, sizeof (GdaValue));
		}

		/* call the callback function */
		func (model, row, user_data);

		gda_row_free (row);
	}
}

/**
 * gda_data_model_is_editing
 * @model: a #GdaDataModel object.
 *
 * Check whether this data model is in editing mode or not. Editing
 * mode is set to TRUE when @gda_data_model_begin_edit has been
 * called successfully, and is not set back to FALSE until either
 * @gda_data_model_cancel_edit or @gda_data_model_end_edit have
 * been called.
 *
 * Returns: TRUE if editing mode, FALSE otherwise.
 */
gboolean
gda_data_model_is_editing (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);
	return model->priv->editing;
}

/**
 * gda_data_model_begin_edit
 * @model: a #GdaDataModel object.
 *
 * Start edition of this data model. This function should be the
 * first called when modifying the data model.
 *
 * Returns: TRUE on success, FALSE if there was an error.
 */
gboolean
gda_data_model_begin_edit (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);
	g_return_val_if_fail (model->priv->editing == FALSE, FALSE);

	if (!gda_data_model_is_editable (model)) {
		gda_log_error (_("Data model %p is not editable"), model);
		return FALSE;
	}

	model->priv->editing = TRUE;
	g_signal_emit (G_OBJECT (model), gda_data_model_signals[BEGIN_EDIT], 0);

	return model->priv->editing;
}

/**
 * gda_data_model_cancel_edit
 * @model: a #GdaDataModel object.
 *
 * Cancels edition of this data model. This means that all changes
 * will be discarded, and the old data put back in the model.
 *
 * Returns: TRUE on success, FALSE if there was an error.
 */
gboolean
gda_data_model_cancel_edit (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);
	g_return_val_if_fail (model->priv->editing, FALSE);

	g_signal_emit (G_OBJECT (model), gda_data_model_signals[CANCEL_EDIT], 0);
	model->priv->editing = FALSE;

	return TRUE;
}

/**
 * gda_data_model_end_edit
 * @model: a #GdaDataModel object.
 *
 * Approves all modifications and send them to the underlying
 * data source/store.
 *
 * Returns: TRUE on success, FALSE if there was an error.
 */
gboolean
gda_data_model_end_edit (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);
	g_return_val_if_fail (model->priv->editing, FALSE);

	g_signal_emit (G_OBJECT (model), gda_data_model_signals[END_EDIT], 0);
	model->priv->editing = FALSE;

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
			txt = gda_value_stringify ((GdaValue *) value);
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
 * Convert the given model into a comma-separated series of rows.
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
 */
gchar *
gda_data_model_to_tab_separated (GdaDataModel *model)
{
	return export_to_separated (model, '\t');
}

/**
 * gda_data_model_to_xml
 */
gchar *
gda_data_model_to_xml (GdaDataModel *model, gboolean standalone)
{
	xmlDocPtr xml_doc;
	xmlNodePtr xml_root, xml_node, xml_tables;
	gchar *xml = NULL;
	xmlChar *xml_contents;
	gint size;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

	xml_node = gda_data_model_to_xml_node (model, "exported_model");
	if (standalone) {
		xml_doc = xmlNewDoc ("1.0");
		xml_root = xmlNewDocNode (xml_doc, NULL, "database", NULL);
		xmlDocSetRootElement (xml_doc, xml_root);

		xml_tables = xmlNewChild (xml_root, NULL, "tables", NULL);
		xmlAddChild (xml_tables, xml_node);

		xmlDocDumpMemory (xml_doc, &xml_contents, &size);
		xmlFreeDoc (xml_doc);

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
 */
xmlNodePtr
gda_data_model_to_xml_node (GdaDataModel *model, const gchar *name)
{
	xmlNodePtr *node;
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
				str = gda_value_stringify (value);
				field = xmlNewChild (row, NULL, "field", str);
				xml_set_int (field, "position", c);

				g_free (str);
			}
		}
	}

	return node;
}

/**
 * gda_data_model_get_command_text
 * @model: a #GdaDataModel.
 *
 * Get the text of command that generated this data model.
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
 * Get the type of command that generated this data model.
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
 */
void
gda_data_model_set_command_type (GdaDataModel *model, GdaCommandType type)
{
	g_return_if_fail (GDA_IS_DATA_MODEL (model));
	model->priv->cmd_type = type;
}

