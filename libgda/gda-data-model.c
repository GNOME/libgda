/* 
 * GDA common library
 * Copyright (C) 1998 - 2008 The GNOME Foundation.
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
#include <libgda/gda-enums.h>
#include <string.h>
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#include "csv.h"

extern gchar *gda_lang_locale;

static GStaticRecMutex init_mutex = G_STATIC_REC_MUTEX_INIT;
static void gda_data_model_class_init (gpointer g_class);

/* signals */
enum {
	CHANGED,
	ROW_INSERTED,
	ROW_UPDATED,
	ROW_REMOVED,
	RESET,
	LAST_SIGNAL
};

static guint gda_data_model_signals[LAST_SIGNAL];

/* module error */
GQuark gda_data_model_error_quark (void)
{
        static GQuark quark;
        if (!quark)
                quark = g_quark_from_static_string ("gda_data_model_error");
        return quark;
}

GType
gda_data_model_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
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
		
		g_static_rec_mutex_lock (&init_mutex);
		if (type == 0) {
			type = g_type_register_static (G_TYPE_INTERFACE, "GdaDataModel", &info, 0);
			g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
		}
		g_static_rec_mutex_unlock (&init_mutex);
	}
	return type;
}

static void
gda_data_model_class_init (gpointer g_class)
{
	static gboolean initialized = FALSE;

	g_static_rec_mutex_lock (&init_mutex);
	if (! initialized) {
		gda_data_model_signals[CHANGED] =
			g_signal_new ("changed",
				      GDA_TYPE_DATA_MODEL,
				      G_SIGNAL_RUN_LAST,
				      G_STRUCT_OFFSET (GdaDataModelClass, changed),
				      NULL, NULL,
				      g_cclosure_marshal_VOID__VOID,
				      G_TYPE_NONE, 0);
		gda_data_model_signals[ROW_INSERTED] =
			g_signal_new ("row-inserted",
				      GDA_TYPE_DATA_MODEL,
				      G_SIGNAL_RUN_LAST,
				      G_STRUCT_OFFSET (GdaDataModelClass, row_inserted),
				      NULL, NULL,
				      g_cclosure_marshal_VOID__INT,
				      G_TYPE_NONE, 1, G_TYPE_INT);
		gda_data_model_signals[ROW_UPDATED] =
			g_signal_new ("row-updated",
				      GDA_TYPE_DATA_MODEL,
				      G_SIGNAL_RUN_LAST,
				      G_STRUCT_OFFSET (GdaDataModelClass, row_updated),
				      NULL, NULL,
				      g_cclosure_marshal_VOID__INT,
				      G_TYPE_NONE, 1, G_TYPE_INT);
		gda_data_model_signals[ROW_REMOVED] =
			g_signal_new ("row-removed",
				      GDA_TYPE_DATA_MODEL,
				      G_SIGNAL_RUN_LAST,
				      G_STRUCT_OFFSET (GdaDataModelClass, row_removed),
				      NULL, NULL,
				      g_cclosure_marshal_VOID__INT,
				      G_TYPE_NONE, 1, G_TYPE_INT);
		gda_data_model_signals[RESET] =
			g_signal_new ("reset",
				      GDA_TYPE_DATA_MODEL,
				      G_SIGNAL_RUN_LAST,
				      G_STRUCT_OFFSET (GdaDataModelClass, reset),
				      NULL, NULL,
				      g_cclosure_marshal_VOID__VOID,
				      G_TYPE_NONE, 0);

		initialized = TRUE;
	}
	g_static_rec_mutex_unlock (&init_mutex);
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
		g_signal_emit (model, gda_data_model_signals[CHANGED], 0);
}

/**
 * gda_data_model_row_inserted
 * @model: a #GdaDataModel object.
 * @row: row number.
 *
 * Emits the 'row_inserted' and 'changed' signals on @model. 
 *
 * This method should only be used by #GdaDataModel implementations to 
 * signal that a row has been inserted.
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
			value = gda_data_model_get_value_at (model, i, 0, NULL);
			if (value && (gda_column_get_g_type (column) == G_TYPE_INVALID))
				gda_column_set_g_type (column, G_VALUE_TYPE ((GValue *)value));
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
 *
 * This method should only be used by #GdaDataModel implementations to 
 * signal that a row has been updated.
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
 *
 * This method should only be used by #GdaDataModel implementations to 
 * signal that a row has been removed
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
 * gda_data_model_reset
 * @model: a #GdaDataModel object.
 *
 * Emits the 'reset' and 'changed' signal on @model.
 */
void
gda_data_model_reset (GdaDataModel *model)
{
	g_return_if_fail (GDA_IS_DATA_MODEL (model));

	g_signal_emit (G_OBJECT (model),
		       gda_data_model_signals[RESET], 0);

	gda_data_model_signal_emit_changed (model);
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
	GdaDataModelAccessFlags flags;

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
GdaDataModelAccessFlags
gda_data_model_get_access_flags (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), 0);
	if (GDA_DATA_MODEL_GET_CLASS (model)->i_get_access_flags) {
		GdaDataModelAccessFlags flags = (GDA_DATA_MODEL_GET_CLASS (model)->i_get_access_flags) (model);
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
		/* method not supported */
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
 * and should not be destroyed; any modification will affect the whole data model.
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
		/* method not supported */
		return NULL;
	}
}

/**
 * gda_data_model_get_column_index
 * @model: a #GdaDataModel object.
 * @name: a column name
 *
 * Get the index of the first column named @name in @model.
 *
 * Returns: the column index, or -1 if no column named @name was found
 */
gint
gda_data_model_get_column_index (GdaDataModel *model, const gchar *name)
{
	gint nbcols, ncol, ret;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), -1);
	g_return_val_if_fail (name, -1);
	
	ret = -1;
	nbcols = gda_data_model_get_n_columns (model);
	for (ncol = 0; ncol < nbcols; ncol++) {
		if (g_str_equal (name, gda_data_model_get_column_title (model, ncol))) {
			
			ret = ncol;
			break;
		}
	}
	
	return ret;
}

/**
 * gda_data_model_get_column_name
 * @model: a #GdaDataModel object.
 * @col: column number.
 *
 * Since: 3.2
 *
 * Returns: the name for the given column in a data model object.
 */
const gchar *
gda_data_model_get_column_name (GdaDataModel *model, gint col)
{
	GdaColumn *column;
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

	column = gda_data_model_describe_column (model, col);
	if (column)
		return gda_column_get_name (column);
	else {
		g_warning ("%s(): can't get GdaColumn object for column %d\n", __FUNCTION__, col);
		return NULL;
	}
}

/**
 * gda_data_model_set_column_name
 * @model: a #GdaDataModel object.
 * @col: column number
 * @name: name for the given column.
 *
 * Sets the @name of the given @col in @model, and if its title is not set, also sets the
 * title to @name.
 *
 * Since: 3.2
 */
void
gda_data_model_set_column_name (GdaDataModel *model, gint col, const gchar *name)
{
	GdaColumn *column;
	g_return_if_fail (GDA_IS_DATA_MODEL (model));

	column = gda_data_model_describe_column (model, col);
	if (column) {
		gda_column_set_name (column, name);
		if (!gda_column_get_title (column))
			gda_column_set_title (column, name);
	}
	else 
		g_warning ("%s(): can't get GdaColumn object for column %d\n", __FUNCTION__, col);
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
		g_warning ("%s(): can't get GdaColumn object for column %d\n", __FUNCTION__, col);
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
		g_warning ("%s(): can't get GdaColumn object for column %d\n", __FUNCTION__, col);
}

/**
 * gda_data_model_get_value_at
 * @model: a #GdaDataModel object.
 * @col: a valid column number.
 * @row: a valid row number.
 * @error: a place to store errors, or %NULL
 *
 * Retrieves the data stored in the given position (identified by
 * the @col and @row parameters) on a data model.
 *
 * This is the main function for accessing data in a model which allow random access to its data.
 * To access data in a data model using a cursor, use a #GdaDataModelIter object, obtained using
 * gda_data_model_create_iter().
 *
 * Note1: the returned #GValue must not be modified directly (unexpected behaviours may
 * occur if you do so).
 *
 * Note2: the returned value may become invalid as soon as any Libgda part is executed again,
 * which means if you want to keep the value, a copy must be made, however it will remain valid
 * as long as the only Libgda usage is calling gda_data_model_get_value_at() for different values
 * of the same row.
 *
 * If you want to modify a value stored in a #GdaDataModel, use the gda_data_model_set_value_at() or 
 * gda_data_model_set_values() methods.
 *
 * Returns: a #GValue containing the value stored in the given
 * position, or %NULL on error (out-of-bound position, etc).
 */
const GValue *
gda_data_model_get_value_at (GdaDataModel *model, gint col, gint row, GError **error)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

	if (GDA_DATA_MODEL_GET_CLASS (model)->i_get_value_at)
		return (GDA_DATA_MODEL_GET_CLASS (model)->i_get_value_at) (model, col, row, error);
	else {
		/* method not supported */
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_FEATURE_NON_SUPPORTED_ERROR,
			     _("Data model does not support getting individual value"));
		return NULL;
	}
}

/**
 * gda_data_model_get_typed_value_at
 * @model: a #GdaDataModel object.
 * @col: a valid column number.
 * @row: a valid row number.
 * @expected_type: the expected data type of the returned value
 * @nullok: if TRUE, then NULL values (value of type %GDA_TYPE_NULL) will not generate any error
 * @error: a place to store errors, or %NULL
 *
 * This method is similar to gda_data_model_get_value_at(), except that it also allows one to specify the expected
 * #GType of the value to get: if the data model returned a #GValue of a type different than the expected one, then
 * this method returns %NULL and an error code.
 *
 * Note: the same limitations and usage instructions apply as for gda_data_model_get_value_at().
 *
 * Returns: a #GValue containing the value stored in the given
 * position, or %NULL on error (out-of-bound position, wrong data type, etc).
 */
const GValue *
gda_data_model_get_typed_value_at (GdaDataModel *model, gint col, gint row, GType expected_type, gboolean nullok, GError **error)
{
	const GValue *cvalue = NULL;
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

	if (GDA_DATA_MODEL_GET_CLASS (model)->i_get_value_at)
		cvalue = (GDA_DATA_MODEL_GET_CLASS (model)->i_get_value_at) (model, col, row, error);

	if (cvalue) {
		if (nullok && (G_VALUE_TYPE (cvalue) != GDA_TYPE_NULL) && (G_VALUE_TYPE (cvalue) != expected_type)) {
			cvalue = NULL;
			g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_VALUE_TYPE_ERROR,
				     _("Data model returned value of invalid '%s' type"), gda_g_type_to_string (G_VALUE_TYPE (cvalue)));
		}
		else if (!nullok && (G_VALUE_TYPE (cvalue) != expected_type)) {
			cvalue = NULL;
			if (G_VALUE_TYPE (cvalue) != GDA_TYPE_NULL)
				g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_VALUE_TYPE_ERROR,
					     _("Data model returned invalid NULL value"));
			else
				g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_VALUE_TYPE_ERROR,
					     _("Data model returned value of invalid '%s' type"), 
					     gda_g_type_to_string (G_VALUE_TYPE (cvalue)));
		}
	}
	return cvalue;
}

/**
 * gda_data_model_get_value_at_column
 * @model: a #GdaDataModel object.
 * @column_name: a valid column name.
 * @row: a valid row number.
 * @error: a place to store errors, or %NULL
 *
 * Retrieves the data stored in the given position (identified by
 * the first column named @column_name and the @row row) in a data model.
 *
 * See also gda_data_model_get_value_at().
 *
 * Returns: a #GValue containing the value stored in the given
 * position, or %NULL on error (out-of-bound position, etc).
 */
const GValue *
gda_data_model_get_value_at_column (GdaDataModel *model, const gchar *column_name, gint row, GError **error)
{
	gint ncol;
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);
	g_return_val_if_fail (column_name, NULL);

	ncol = gda_data_model_get_column_index (model, column_name);
	if (ncol == -1)
		return NULL;
	else
		return gda_data_model_get_value_at (model, ncol, row, error);
}

/**
 * gda_data_model_get_attributes_at
 * @model: a #GdaDataModel object
 * @col: a valid column number
 * @row: a valid row number, or -1
 *
 * Get the attributes of the value stored at (row, col) in @model, which
 * is an ORed value of #GdaValueAttribute flags. As a special case, if
 * @row is -1, then the attributes returned correspond to a "would be" value
 * if a row was added to @model.
 *
 * Returns: the attributes as an ORed value of #GdaValueAttribute
 */
GdaValueAttribute
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
 * @value: a #GValue (not %NULL)
 * @error: a place to store errors, or %NULL
 *
 * Modifies a value in @model, at (@col, @row).
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
		/* method not supported */
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_FEATURE_NON_SUPPORTED_ERROR,
			     _("Data model does not support setting individual value"));
		return FALSE;
	}
}

/**
 * gda_data_model_set_values
 * @model: a #GdaDataModel object.
 * @row: row number.
 * @values: a list of #GValue, one for at most the number of columns of @model
 * @error: a place to store errors, or %NULL
 *
 * In a similar way to gda_data_model_set_value_at(), this method modifies a data model's contents
 * by setting several values at once.
 *
 * If any value in @values is actually %NULL, then the value in the corresponding column is left
 * unchanged.
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
		/* method not supported */
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
 * randomly the correspoding row can be set using gda_data_model_iter_move_at_row(), 
 * and for models which are accessible sequentially only then the first row will be
 * fetched using gda_data_model_iter_move_next().
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
				      "data-model", model, NULL);
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
		/* method not supported */
		return -1;
	}
}

/**
 * gda_data_model_append_row
 * @model: a #GdaDataModel object.
 * @error: a place to store errors, or %NULL
 * 
 * Appends a row to the data model (the new row will possibliy have NULL values for all columns,
 * or some other values depending on the data model implementation)
 *
 * Returns: the number of the added row, or -1 if an error occurred
 */
gint
gda_data_model_append_row (GdaDataModel *model, GError **error)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);

	if (! (gda_data_model_get_access_flags (model) & GDA_DATA_MODEL_ACCESS_INSERT)) {
		g_set_error (error, 0, 0,
			     _("Model does not allow row insertion"));
		return -1;
	}

	if (GDA_DATA_MODEL_GET_CLASS (model)->i_append_row) 
		return (GDA_DATA_MODEL_GET_CLASS (model)->i_append_row) (model, error);
	else {
		/* method not supported */
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

	if (! (gda_data_model_get_access_flags (model) & GDA_DATA_MODEL_ACCESS_DELETE)) {
		g_set_error (error, 0, 0,
			     _("Model does not allow row deletion"));
		return FALSE;
	}

	if (GDA_DATA_MODEL_GET_CLASS (model)->i_remove_row)
		return (GDA_DATA_MODEL_GET_CLASS (model)->i_remove_row) (model, row, error);
	else {
		/* method not supported */
		return FALSE;
	}
}

/**
 * gda_data_model_get_row_from_values
 * @model: a #GdaDataModel object.
 * @values: a list of #GValue values (no %NULL is allowed)
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
                        value = gda_data_model_get_value_at (model, cols_index [index], current_row, NULL);

                        if (!value || !(list->data) || gda_value_compare ((GValue *) (list->data), (GValue *) value))
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

static gchar *export_to_text_separated (GdaDataModel *model, const gint *cols, gint nb_cols, 
					const gint *rows, gint nb_rows, gchar sep, gchar quote, gboolean field_quotes);


/**
 * gda_data_model_export_to_string
 * @model: a #GdaDataModel
 * @format: the format in which to export data
 * @cols: an array containing which columns of @model will be exported, or %NULL for all columns
 * @nb_cols: the number of columns in @cols
 * @rows: an array containing which rows of @model will be exported, or %NULL for all rows
 * @nb_rows: the number of rows in @rows
 * @options: list of options for the export
 *
 * Exports data contained in @model to a string; the format is specified using the @format argument, see the
 * gda_data_model_export_to_file() documentation for more information about the @options argument (except for the
 * "OVERWRITE" option).
 *
 * Returns: a new string.
 */
gchar *
gda_data_model_export_to_string (GdaDataModel *model, GdaDataModelIOFormat format, 
				 const gint *cols, gint nb_cols, 
				 const gint *rows, gint nb_rows, GdaSet *options)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);
	g_return_val_if_fail (!options || GDA_IS_SET (options), NULL);

	switch (format) {
	case GDA_DATA_MODEL_IO_DATA_ARRAY_XML: {
		const gchar *name = NULL;
		xmlChar *xml_contents;
		xmlNodePtr xml_node;
		gchar *xml;
		gint size;
		xmlDocPtr xml_doc;

		if (options) {
			GdaHolder *holder;

			holder = gda_set_get_holder (options, "NAME");
			if (holder) {
				const GValue *value;
				value = gda_holder_get_value (holder);
				if (value && (G_VALUE_TYPE (value) == G_TYPE_STRING))
					name = g_value_get_string ((GValue *) value);
				else
					g_warning (_("The '%s' parameter must hold a string value, ignored."), "NAME");
			}
		}

		g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);
		
		xml_node = gda_data_model_to_xml_node (model, cols, nb_cols, rows, nb_rows, name);
		xml_doc = xmlNewDoc ((xmlChar*)"1.0");
		xmlDocSetRootElement (xml_doc, xml_node);
		
		xmlDocDumpFormatMemory (xml_doc, &xml_contents, &size, 1);
		xmlFreeDoc (xml_doc);
		
		xml = g_strdup ((gchar*)xml_contents);
		xmlFree (xml_contents);
		
		return xml;
	}

	case GDA_DATA_MODEL_IO_TEXT_SEPARATED: {
		gchar sep = ',';
		gchar quote = '"';
		gboolean field_quote = TRUE;

		if (options) {
			GdaHolder *holder;

			holder = gda_set_get_holder (options, "SEPARATOR");
			if (holder) {
				const GValue *value;
				value = gda_holder_get_value (holder);
				if (value && (G_VALUE_TYPE (value) == G_TYPE_STRING)) {
					const gchar *str;

					str = g_value_get_string ((GValue *) value);
					if (str && *str)
						sep = *str;
				}
				else
					g_warning (_("The '%s' parameter must hold a string value, ignored."), "SEPARATOR");
			}
			holder = gda_set_get_holder (options, "QUOTE");
			if (holder) {
				const GValue *value;
				value = gda_holder_get_value (holder);
				if (value && (G_VALUE_TYPE (value) == G_TYPE_STRING)) {
					const gchar *str;

					str = g_value_get_string ((GValue *) value);
					if (str && *str)
						quote = *str;
				}
				else 
					g_warning (_("The '%s' parameter must hold a string value, ignored."), "QUOTE");
			}
			holder = gda_set_get_holder (options, "FIELD_QUOTE");
			if (holder) {
				const GValue *value;
				value = gda_holder_get_value (holder);
				if (value && (G_VALUE_TYPE (value) == G_TYPE_BOOLEAN))
					field_quote = g_value_get_boolean ((GValue *) value);
				else 
					g_warning (_("The '%s' parameter must hold a boolean value, ignored."), "FIELD_QUOTE");
			}
		}

		if (cols)
			return export_to_text_separated (model, cols, nb_cols, rows, nb_rows, sep, quote, field_quote);
		else {
			gchar *retval;
			gint *rcols, rnb_cols, i;
			
			g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);
			
			rnb_cols = gda_data_model_get_n_columns (model);
			rcols = g_new (gint, rnb_cols);
			for (i = 0; i < rnb_cols; i++)
				rcols[i] = i;
			retval = export_to_text_separated (model, rcols, rnb_cols, rows, nb_rows, sep, quote, field_quote);
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
 * @rows: an array containing which rows of @model will be exported, or %NULL for all rows
 * @nb_rows: the number of rows in @rows
 * @options: list of options for the export
 * @error: a place to store errors, or %NULL
 *
 * Exports data contained in @model to the @file file; the format is specified using the @format argument.
 *
 * Specifically, the parameters in the @options list can be:
 * <itemizedlist>
 *   <listitem><para>"SEPARATOR": a string value of which the first character is used as a separator in case of CSV export
 *             </para></listitem>
 *   <listitem><para>"QUOTE": a string value of which the first character is used as a quote character in case of CSV export
 *             </para></listitem>
 *   <listitem><para>"FIELD_QUOTE": a boolean value which can be set to FALSE if no quote around the individual fields 
 *             is requeted, in case of CSV export</para></listitem>
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
			       const gint *rows, gint nb_rows, 
			       GdaSet *options, GError **error)
{
	gchar *body;
	gboolean overwrite = FALSE;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);
	g_return_val_if_fail (!options || GDA_IS_SET (options), FALSE);
	g_return_val_if_fail (file, FALSE);

	body = gda_data_model_export_to_string (model, format, cols, nb_cols, rows, nb_cols, options);
	if (options) {
		GdaHolder *holder;
		
		holder = gda_set_get_holder (options, "OVERWRITE");
		if (holder) {
			const GValue *value;
			value = gda_holder_get_value (holder);
			if (value && (G_VALUE_TYPE (value) == G_TYPE_BOOLEAN))
				overwrite = g_value_get_boolean ((GValue *) value);
			else
				g_warning (_("The '%s' parameter must hold a boolean value, ignored."), "OVERWRITE");
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
	
	if (! g_file_set_contents (file, body, -1, error)) {
		g_free (body);
		return FALSE;
	}
	g_free (body);
	return TRUE;
}

static gchar *
export_to_text_separated (GdaDataModel *model, const gint *cols, gint nb_cols, const gint *rows, gint nb_rows, 
			  gchar sep, gchar quote, gboolean field_quotes)
{
	GString *str;
	gchar *retval;
	gint c, nbrows, r;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

	str = g_string_new ("");
	if (!rows)
		nbrows = gda_data_model_get_n_rows (model);
	else
		nbrows = nb_rows;

	for (r = 0; r < nbrows; r++) {
		if (r > 0)
			str = g_string_append_c (str, '\n');

		for (c = 0; c < nb_cols; c++) {
			GValue *value;
			gchar *txt;

			value = (GValue *) gda_data_model_get_value_at (model, cols[c], rows ? rows[r] : r, NULL);
			if (G_VALUE_TYPE (value) == G_TYPE_BOOLEAN)
				txt = g_strdup (g_value_get_boolean (value) ? "TRUE" : "FALSE");
			else {
				gchar *tmp;
				gsize len, size;
				
				tmp = gda_value_stringify (value);
				len = strlen (tmp);
				size = 2 * len + 3;
				txt = g_new (gchar, size);

				len = csv_write2 (txt, size, tmp, len, quote);
				txt [len] = 0;
				if (!field_quotes) {
					txt [len - 1] = 0;
					memmove (txt, txt+1, len);
				}
			}
			if (c > 0)
				str = g_string_append_c (str, sep);

			str = g_string_append (str, txt);
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
	xmlSetProp (node, (xmlChar*)name, value ? (xmlChar*)"TRUE" : (xmlChar*)"FALSE");
}

/**
 * gda_data_model_to_xml_node
 * @model: a #GdaDataModel object.
 * @cols: an array containing which columns of @model will be exported, or %NULL for all columns
 * @nb_cols: the number of columns in @cols
 * @rows: an array containing which rows of @model will be exported, or %NULL for all rows
 * @nb_rows: the number of rows in @rows
 * @name: name to use for the XML resulting table.
 *
 * Converts a #GdaDataModel into a xmlNodePtr (as used in libxml).
 *
 * Returns: a xmlNodePtr representing the whole data model, or %NULL if an error occurred
 */
xmlNodePtr
gda_data_model_to_xml_node (GdaDataModel *model, const gint *cols, gint nb_cols, 
			    const gint *rows, gint nb_rows, const gchar *name)
{
	xmlNodePtr node;
	gint i;
	gint *rcols, rnb_cols;
	const gchar *cstr;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

	node = xmlNewNode (NULL, BAD_CAST "gda_array");
	cstr = g_object_get_data (G_OBJECT (model), "id");
	if (cstr)
		xmlSetProp (node, BAD_CAST "id", BAD_CAST cstr);
	else
		xmlSetProp (node, BAD_CAST "id", BAD_CAST "EXPORT");

	if (name)
		xmlSetProp (node, BAD_CAST "name", BAD_CAST name);
	else {
		cstr = g_object_get_data (G_OBJECT (model), "name");
		if (cstr)
			xmlSetProp (node, BAD_CAST "name", BAD_CAST cstr);
		else
			xmlSetProp (node, BAD_CAST "name", BAD_CAST _("Exported Data"));
	}

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

		field = xmlNewChild (node, NULL, BAD_CAST "gda_array_field", NULL);
		g_object_get (G_OBJECT (column), "id", &str, NULL);
		if (!str)
			str = g_strdup_printf ("FI%d", i);
		xmlSetProp (field, BAD_CAST "id", BAD_CAST str);
		g_free (str);
		xmlSetProp (field, BAD_CAST "name", BAD_CAST gda_column_get_name (column));
		cstr = gda_column_get_title (column);
		if (cstr && *cstr)
			xmlSetProp (field, BAD_CAST "title", BAD_CAST cstr);
		cstr = gda_column_get_dbms_type (column);
		if (cstr && *cstr)
			xmlSetProp (field, BAD_CAST "dbms_type", BAD_CAST cstr);
		xmlSetProp (field, BAD_CAST "gdatype", BAD_CAST gda_g_type_to_string (gda_column_get_g_type (column)));
		if (gda_column_get_allow_null (column))
			xml_set_boolean (field, "nullok", gda_column_get_allow_null (column));
		if (gda_column_get_auto_increment (column))
			xml_set_boolean (field, "auto_increment", gda_column_get_auto_increment (column));
	}
	
	/* add the model data to the XML output */
	if (!gda_utility_data_model_dump_data_to_xml (model, node, cols, nb_cols, rows, nb_rows, FALSE)) {
		xmlFreeNode (node);
		node = NULL;
	}

	if (!cols)
		g_free (rcols);

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
		gchar *id;
		column = gda_data_model_describe_column (model, c);
		g_object_get (column, "id", &id, NULL);
		if (!id || strcmp (id, colid)) 
			column = NULL;
		else
			*pos = c;
		g_free (id);
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

	const gchar *lang = gda_lang_locale;

	values = g_ptr_array_new ();
	g_ptr_array_set_size (values, gda_data_model_get_n_columns (model));
	for (xml_field = xml_row->xmlChildrenNode; xml_field != NULL; xml_field = xml_field->next) {
		GValue *value = NULL;
		GdaColumn *column = NULL;
		GType gdatype;
		gchar *isnull = NULL;
		xmlChar *this_lang = NULL;
		xmlChar *colid = NULL;

		if (xmlNodeIsText (xml_field))
			continue;

		if (strcmp ((gchar*)xml_field->name, "gda_value") && strcmp ((gchar*)xml_field->name, "gda_array_value")) {
			g_set_error (error, 0, 0, _("Expected tag <gda_value> or <gda_array_value>, "
						    "got <%s>, ignoring"), (gchar*)xml_field->name);
			continue;
		}
		
		this_lang = xmlGetProp (xml_field, BAD_CAST "lang");
		if (this_lang && strncmp ((gchar*)this_lang, lang, strlen ((gchar*)this_lang))) {
			xmlFree (this_lang);
			continue;
		}

		colid = xmlGetProp (xml_field, BAD_CAST "colid");

		/* create the value for this field */
		if (!colid) {
			if (this_lang)
				column = gda_data_model_describe_column (model, pos - 1);
			else
				column = gda_data_model_describe_column (model, pos);
		}
		else {
			column = find_column_from_id (model, (gchar*)colid, &pos);
			xmlFree (colid);
			if (!column)
				continue;
		}

		gdatype = gda_column_get_g_type (column);
		if ((gdatype == G_TYPE_INVALID) ||
		    (gdatype == GDA_TYPE_NULL)) {
			g_set_error (error, 0, 0,
				     _("Cannot retrieve column data type (type is UNKNOWN or not specified)"));
			retval = FALSE;
			break;
		}

		gboolean nullforced = FALSE;
		isnull = (gchar*)xmlGetProp (xml_field, BAD_CAST "isnull");
		if (isnull) {
			if ((*isnull == 't') || (*isnull == 'T'))
				nullforced = TRUE;
			g_free (isnull);
		}

		if (!nullforced) {
			value = g_new0 (GValue, 1);
			gchar* nodeval = (gchar*)xmlNodeGetContent (xml_field);
			if (nodeval) {
				if (!gda_value_set_from_string (value, nodeval, gdatype)) {
					g_free (value);
					value = gda_value_new_null ();
				}
				xmlFree(nodeval);
			}
			else
				value = gda_value_new_null ();	
		}

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
 * @node: an XML node representing a &lt;gda_array_data&gt; XML node.
 *
 * Adds the data from an XML node to the given data model (see the DTD for that node
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

	if (strcmp ((gchar*)node->name, "gda_array_data")) {
		g_set_error (error, 0, 0,
			     _("Expected tag <gda_array_data>, got <%s>"), node->name);
		return FALSE;
	}

	for (children = node->xmlChildrenNode; children != NULL; children = children->next) {
		if (!strcmp ((gchar*)children->name, "gda_array_row")) {
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
 * @overwrite: TRUE if @to is completely overwritten by @from's data, and FALSE if @from's data is appended to @to
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
gda_data_model_import_from_model (GdaDataModel *to, GdaDataModel *from, 
				  gboolean overwrite, GHashTable *cols_trans, GError **error)
{
	GdaDataModelIter *from_iter;
	gboolean retval = TRUE;
	gint to_nb_cols, to_nb_rows=-1, to_row = -1;
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
			     _("Could not get an iterator for source data model"));
		return FALSE;
	}

	/* make a list of the parameters which will be used during copy (stored in reverse order: last column of data model
	 * represented by the 1st GdaHolder) ,
	 * an some tests */
	for (i = 0; i < to_nb_cols; i++) {
		GdaHolder *param = NULL;
		gint col; /* source column number */
		GdaColumn *column;

		if (cols_trans) {
			col = GPOINTER_TO_INT (g_hash_table_lookup (cols_trans, GINT_TO_POINTER (i)));
			if (col >= from_nb_cols) {
				g_slist_free (copy_params);
				g_set_error (error, 0, 0,
					     _("Inexistent column in source data model: %d"), col);
				return FALSE;
			}
		}
		else
			col = i;
		if (col >= 0)
			param = gda_data_model_iter_get_holder_for_field (from_iter, col);

		/* tests */
		column = gda_data_model_describe_column (to, i);
		if (! gda_column_get_allow_null (column) && !param) {
			g_slist_free (copy_params);
			g_set_error (error, 0, 0,
				     _("Destination column %d can't be NULL but has no correspondence in the "
				       "source data model"), i);
			return FALSE;
		}
		if (param) {
			if ((gda_column_get_g_type (column) != G_TYPE_INVALID) &&
			    (gda_holder_get_g_type (param) != G_TYPE_INVALID) &&
			    !g_value_type_transformable (gda_holder_get_g_type (param), 
							 gda_column_get_g_type (column))) {
				g_set_error (error, 0, 0,
					     _("Destination column %d has a gda type (%s) incompatible with "
					       "source column %d type (%s)"), i,
					     gda_g_type_to_string (gda_column_get_g_type (column)),
					     col,
					     gda_g_type_to_string (gda_holder_get_g_type (param)));
				return FALSE;
			}
		}
		
		copy_params = g_slist_prepend (copy_params, param);
	}

#ifdef GDA_DEBUG_NO
	{
		GSList *list;
		for (list = copy_params; list; list = list->next) {
			GdaHolder *param = GDA_HOLDER (list->data);
			g_print ("Copy Param: %s (%s)\n", gda_holder_get_id (param), 
				 gda_g_type_to_string (gda_holder_get_g_type (param)));
		}
	}
#endif

	/* build the append_values list (stored in reverse order!)
	 * node->data is:
	 * - NULL if the value must be replaced by the value of the copied model
	 * - a GValue of type GDA_VALYE_TYPE_NULL if a null value must be inserted in the dest data model
	 * - a GValue of a different type if the value must be converted from the src data model
	 */
	append_types = g_new0 (GType, to_nb_cols);
	for (plist = copy_params, i = to_nb_cols - 1; plist; plist = plist->next, i--) {
		GdaColumn *column;

		column = gda_data_model_describe_column (to, i);
		if (plist->data) {
			if ((gda_holder_get_g_type (GDA_HOLDER (plist->data)) != 
			     gda_column_get_g_type (column)) &&
			    (gda_column_get_g_type (column) != G_TYPE_INVALID)) {
				GValue *newval;
				
				newval = g_new0 (GValue, 1);
				append_types [i] = gda_column_get_g_type (column);
				g_value_init (newval, append_types [i]);
				append_values = g_list_prepend (append_values, newval);
			}
			else
				append_values = g_list_prepend (append_values, NULL);
		}
		else
			append_values = g_list_prepend (append_values, gda_value_new_null ());
	}
	append_values = g_list_reverse (append_values);
#ifdef GDA_DEBUG_NO
	{
		GList *list;
		for (list = append_values; list; list = list->next) 
			g_print ("Append Value: %s\n", list->data ? gda_g_type_to_string (G_VALUE_TYPE ((GValue *) (list->data))) : "NULL");
	}
#endif
	
	/* actual data copy (no memory allocation is needed here) */
	gda_data_model_send_hint (to, GDA_DATA_MODEL_HINT_START_BATCH_UPDATE, NULL);
	
	if (overwrite) {
		to_row = 0;
		to_nb_rows = gda_data_model_get_n_rows (to);
	}

	gboolean mstatus;
	mstatus = gda_data_model_iter_move_next (from_iter); /* move to first row */
	if (!mstatus) {
		gint crow;
		g_object_get (from_iter, "current-row", &crow, NULL);
		if (crow >= 0)
			retval = FALSE;
	}
	while (retval && gda_data_model_iter_is_valid (from_iter)) {
		GList *values = NULL;
		GList *avlist = append_values;
		plist = copy_params;
		i = to_nb_cols - 1;

		while (plist && avlist && retval) {
			GValue *value;

			value = (GValue *) (avlist->data);
			if (plist->data) {
				value = (GValue *) gda_holder_get_value (GDA_HOLDER (plist->data));
				if (avlist->data) {
					if (append_types [i] && gda_value_is_null ((GValue *) (avlist->data))) 
						g_value_init ((GValue *) (avlist->data), append_types [i]);
					if (!gda_value_is_null (value) && 
					    !g_value_transform (value, (GValue *) (avlist->data))) {
						gchar *str;

						str = gda_value_stringify (value);
						g_set_error (error, 0, 0,
							     _("Can't transform '%s' from GDA type %s to GDA type %s"),
							     str, 
							     gda_g_type_to_string (G_VALUE_TYPE (value)),
							     gda_g_type_to_string (G_VALUE_TYPE ((GValue *) (avlist->data))));
						g_free (str);
						retval = FALSE;
					}
					value = (GValue *) (avlist->data);
				}
			}
			g_assert (value);

			values = g_list_prepend (values, value);

			plist = plist->next;
			avlist = avlist->next;
			i--;
		}

		if (retval) {
			if (to_row >= to_nb_rows)
				/* we have finished modifying the existing rows */
				to_row = -1;

			if (to_row >= 0) {
				if (!gda_data_model_set_values (to, to_row, values, error))
					retval = FALSE;
				else 
					to_row ++;
			}
			else {
				if (gda_data_model_append_values (to, values, error) < 0) 
					retval = FALSE;
			}
		}
		
		g_list_free (values);
		mstatus = gda_data_model_iter_move_next (from_iter);
		if (!mstatus) {
			gint crow;
			g_object_get (from_iter, "current-row", &crow, NULL);
			if (crow >= 0)
				retval = FALSE;
		}
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

	if (retval && (to_row >= 0)) {
		/* remove extra rows */
		for (; retval && (to_row < to_nb_rows); to_row++) {
			if (!gda_data_model_remove_row (to, to_row, error))
				retval = FALSE;
		}
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
				   GdaSet *options, GError **error)
{
	GdaDataModel *import;
	gboolean retval;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);
	g_return_val_if_fail (!options || GDA_IS_SET (options), FALSE);
	if (!string)
		return TRUE;

	import = gda_data_model_import_new_mem (string, FALSE, options);
	retval = gda_data_model_import_from_model (model, import, FALSE, cols_trans, error);
	g_object_unref (import);

	return retval;
}

/**
 * gda_data_model_import_from_file
 * @model: a #GdaDataModel
 * @file: the filename to import from
 * @cols_trans: a #GHashTable for columns translating, or %NULL
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
				 GdaSet *options, GError **error)
{
	GdaDataModel *import;
	gboolean retval;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), FALSE);
	g_return_val_if_fail (!options || GDA_IS_SET (options), FALSE);
	if (!file)
		return TRUE;

	import = gda_data_model_import_new_file (file, FALSE, options);
	retval = gda_data_model_import_from_model (model, import, FALSE, cols_trans, error);
	g_object_unref (import);

	return retval;
}

static gchar *real_gda_data_model_dump_as_string (GdaDataModel *model, gboolean dump_attributes, 
						  gboolean dump_rows, gboolean dump_title, gboolean null_as_empty,
						  GError **error);

/**
 * gda_data_model_dump
 * @model: a #GdaDataModel.
 * @to_stream: where to dump the data model
 *
 * Dumps a textual representation of the @model to the @to_stream stream
 *
 * The following environment variables can affect the resulting output:
 * <itemizedlist>
 *   <listitem><para>GDA_DATA_MODEL_DUMP_ROW_NUMBERS: if set, the first coulumn of the output will contain row numbers</para></listitem>
 *   <listitem><para>GDA_DATA_MODEL_DUMP_ATTRIBUTES: if set, also dump the data model's columns' types and value's attributes</para></listitem>
*   <listitem><para>GDA_DATA_MODEL_DUMP_TITLE: if set, also dump the data model's title</para></listitem>
*   <listitem><para>GDA_DATA_MODEL_DUMP_NULL_AS_EMPTY: if set, replace the 'NULL' string with an empty string for NULL values </para></listitem>
 * </itemizedlist>
 */
void
gda_data_model_dump (GdaDataModel *model, FILE *to_stream)
{
	gchar *str;
	gboolean dump_attrs = FALSE;
	gboolean dump_rows = FALSE;
	gboolean dump_title = FALSE;
	gboolean null_as_empty = FALSE;
	GError *error = NULL;

	g_return_if_fail (GDA_IS_DATA_MODEL (model));
	g_return_if_fail (to_stream);

	if (getenv ("GDA_DATA_MODEL_DUMP_ATTRIBUTES")) 
		dump_attrs = TRUE;
	if (getenv ("GDA_DATA_MODEL_DUMP_ROW_NUMBERS"))
		dump_rows = TRUE;
	if (getenv ("GDA_DATA_MODEL_DUMP_TITLE")) 
		dump_title = TRUE;
	if (getenv ("GDA_DATA_MODEL_NULL_AS_EMPTY")) 
		null_as_empty = TRUE;

	str = real_gda_data_model_dump_as_string (model, FALSE, dump_rows, dump_title, null_as_empty, &error);
	if (str) {
		g_fprintf (to_stream, "%s", str);
		g_free (str);
		if (dump_attrs) {
			str = real_gda_data_model_dump_as_string (model, TRUE, dump_rows, dump_title, null_as_empty, &error);
			if (str) {
				g_fprintf (to_stream, "%s", str);
				g_free (str);
			}
			else {
				g_warning (_("Could not dump data model's attributes: %s"),
					   error && error->message ? error->message : _("No detail"));
				if (error)
					g_error_free (error);
			}
		}
	}
	else {
		g_warning (_("Could not dump data model's contents: %s"),
			   error && error->message ? error->message : _("No detail"));
		if (error)
			g_error_free (error);
	}
}

/**
 * gda_data_model_dump_as_string
 * @model: a #GdaDataModel.
 *
 * Dumps a textual representation of the @model into a new string
 *
 * The following environment variables can affect the resulting output:
 * <itemizedlist>
 *   <listitem><para>GDA_DATA_MODEL_DUMP_ROW_NUMBERS: if set, the first coulumn of the output will contain row numbers</para></listitem>
*   <listitem><para>GDA_DATA_MODEL_DUMP_TITLE: if set, also dump the data model's title</para></listitem>
*   <listitem><para>GDA_DATA_MODEL_DUMP_NULL_AS_EMPTY: if set, replace the 'NULL' string with an empty string for NULL values </para></listitem>
 * </itemizedlist>
 * Returns: a new string.
 */
gchar *
gda_data_model_dump_as_string (GdaDataModel *model)
{
	gboolean dump_rows = FALSE;
	gboolean dump_title = FALSE;
	gboolean null_as_empty = FALSE;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

	if (getenv ("GDA_DATA_MODEL_DUMP_ROW_NUMBERS"))
		dump_rows = TRUE;
	if (getenv ("GDA_DATA_MODEL_DUMP_TITLE")) 
		dump_title = TRUE;
	if (getenv ("GDA_DATA_MODEL_NULL_AS_EMPTY")) 
		null_as_empty = TRUE;

	return real_gda_data_model_dump_as_string (model, FALSE, dump_rows, dump_title, null_as_empty, NULL);
}

static void
string_get_dimensions (const gchar *string, gint *width, gint *rows)
{
	const gchar *ptr;
	gint w = 0, h = 0, maxw = 0;
	for (ptr = string; *ptr; ptr = g_utf8_next_char (ptr)) {
		if (*ptr == '\n') {
			h++;
			maxw = MAX (maxw, w);
			w = 0;
		}
		else
			w++;
	}
	maxw = MAX (maxw, w);

	if (width)
		*width = maxw;
	if (rows)
		*rows = h;
}

static gchar *
real_gda_data_model_dump_as_string (GdaDataModel *model, gboolean dump_attributes, 
				    gboolean dump_rows, gboolean dump_title, gboolean null_as_empty, GError **error)
{
#define MULTI_LINE_NO_SEPARATOR

	gboolean allok = TRUE;
	GString *string;
	gchar *offstr, *str;
	gint n_cols, n_rows;
	gint *cols_size;
	gboolean *cols_is_num;
	gchar *sep_col  = " | ";
#ifdef MULTI_LINE_NO_SEPARATOR
	gchar *sep_col_e  = "   ";
#endif
	gchar *sep_row  = "-+-";
	gchar sep_fill = '-';
	gint i, j;
	const GValue *value;

	gint offset = 0;
	gint col_offset = dump_rows ? 1 : 0;

	string = g_string_new ("");

        /* string for the offset */
        offstr = g_new0 (gchar, offset+1);
	memset (offstr, ' ', offset);

	/* compute the columns widths: using column titles... */
	n_cols = gda_data_model_get_n_columns (model);
	n_rows = gda_data_model_get_n_rows (model);
	cols_size = g_new0 (gint, n_cols + col_offset);
	cols_is_num = g_new0 (gboolean, n_cols + col_offset);
	
	if (dump_rows) {
		str = g_strdup_printf ("%d", n_rows);
		cols_size [0] = MAX (strlen (str), strlen ("#row"));
		cols_is_num [0] = TRUE;
		g_free (str);
	}

	for (i = 0; i < n_cols; i++) {
		GdaColumn *gdacol;
		GType coltype;

#ifdef GDA_DEBUG_NO
		{
			GdaColumn *col = gda_data_model_describe_column (model, i);
			g_print ("Model col %d has type %s\n", i, gda_g_type_to_string (gda_column_get_g_type (col)));
		}
#endif

		str = (gchar *) gda_data_model_get_column_title (model, i);
		if (str)
			cols_size [i + col_offset] = g_utf8_strlen (str, -1);
		else
			cols_size [i + col_offset] = 6; /* for "<none>" */

		if (! dump_attributes) {
			gdacol = gda_data_model_describe_column (model, i);
			coltype = gda_column_get_g_type (gdacol);
			/*g_string_append_printf (string, "COL %d is a %s\n", i+1, gda_g_type_to_string (coltype));*/
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
				cols_is_num [i + col_offset] = TRUE;
			else
				cols_is_num [i + col_offset] = FALSE;
		}
		else
			cols_is_num [i + col_offset] = TRUE;
	}

	/* ... and using column data */
	for (j = 0; j < n_rows; j++) {
		for (i = 0; i < n_cols; i++) {
			if (! dump_attributes) {
				value = gda_data_model_get_value_at (model, i, j, error);
				if (!value) {
					allok = FALSE;
					break;
				}
				str = NULL;
				if (null_as_empty) {
					if (!value || gda_value_is_null (value))
						str = g_strdup ("");
				}
				if (!str)
					str = value ? gda_value_stringify ((GValue*)value) : g_strdup ("_null_");
				if (str) {
					gint w;
					string_get_dimensions (str, &w, NULL);
					cols_size [i + col_offset] = MAX (cols_size [i + col_offset], w);
					g_free (str);
				}
			}
			else {
				GdaValueAttribute attrs;
				gint w;
				attrs = gda_data_model_get_attributes_at (model, i, j);
				str = g_strdup_printf ("%u", attrs);
				string_get_dimensions (str, &w, NULL);
				cols_size [i + col_offset] = MAX (cols_size [i + col_offset], w);
				g_free (str);
			}
		}
		if (!allok)
			break;
	}
	
	/* actual dumping of the contents: title */
	if (allok && dump_title) {
		const gchar *title;
		title = g_object_get_data (G_OBJECT (model), "name");
		if (title) {
			gint total_width = n_cols -1, i;

			for (i = 0; i < n_cols; i++)
				total_width += cols_size [i];
			if (total_width > strlen (title))
				i = (total_width - strlen (title))/2.;
			else
				i = 0;
			for (; i > 0; i--)
				g_string_append_c (string, ' ');
			g_string_append (string, title);
			g_string_append_c (string, '\n');
		}
	}

	/* ...column titles...*/
	if (allok && dump_rows) 
		g_string_append_printf (string, "%*s", cols_size [0], "#row");

	for (i = 0; (i < n_cols) && allok; i++) {
		gint j, max;
		str = (gchar *) gda_data_model_get_column_title (model, i);
		if (dump_rows || (i != 0))
			g_string_append_printf (string, "%s", sep_col);

		if (str) {
			g_string_append_printf (string, "%s", str);
			max = cols_size [i + col_offset] - g_utf8_strlen (str, -1);
		}
		else {
			g_string_append (string, "<none>");
			max = cols_size [i + col_offset] - 6;
		}
		for (j = 0; j < max; j++)
			g_string_append_c (string, ' ');
	}
	g_string_append_c (string, '\n');
		
	/* ... separation line ... */
	for (i = 0; (i < n_cols + col_offset) && allok; i++) {
		if (i != 0)
			g_string_append_printf (string, "%s", sep_row);
		for (j = 0; j < cols_size [i]; j++)
			g_string_append_c (string, sep_fill);
	}
	g_string_append_c (string, '\n');

	/* ... and data */
	if (allok && gda_data_model_get_access_flags (model) & GDA_DATA_MODEL_ACCESS_RANDOM) {
		for (j = 0; j < n_rows; j++) {
			/* determine height for each column in that row */
			gint *cols_height = g_new (gint, n_cols + col_offset);
			gchar ***cols_str = g_new (gchar **, n_cols + col_offset);
			gint k, kmax = 1;

			if (dump_rows) {
				str = g_strdup_printf ("%d", j);
				cols_str [0] = g_strsplit (str, "\n", -1);
				cols_height [0] = 1;
				kmax = 1;
			}

			for (i = 0; i < n_cols; i++) {
				if (!dump_attributes) {
					value = gda_data_model_get_value_at (model, i, j, error);
					if (!value) {
						allok = FALSE;
						break;
					}
					str = NULL;
					if (null_as_empty) {
						if (!value || gda_value_is_null (value))
							str = g_strdup ("");
					}
					if (!str)
						str = value ? gda_value_stringify ((GValue *)value) : g_strdup ("_null_");
				}
				else {
					GdaValueAttribute attrs;
					attrs = gda_data_model_get_attributes_at (model, i, j);
					str = g_strdup_printf ("%u", attrs);
				}
				if (str) {
					cols_str [i + col_offset] = g_strsplit (str, "\n", -1);
					g_free (str);
					cols_height [i + col_offset] = g_strv_length (cols_str [i + col_offset]);
					kmax = MAX (kmax, cols_height [i + col_offset]);
				}
				else {
					cols_str [i + col_offset] = NULL;
					cols_height [i + col_offset] = 0;
				}
			}
			if (!allok)
				break;

			for (k = 0; k < kmax; k++) {
				for (i = 0; i < n_cols + col_offset; i++) {
					gboolean align_center = FALSE;
					if (i != 0) {
#ifdef MULTI_LINE_NO_SEPARATOR
						if (k != 0)
							g_string_append_printf (string, "%s", sep_col_e);
						else
#endif
							g_string_append_printf (string, "%s", sep_col);
					}
					if (k < cols_height [i]) 
						str = (cols_str[i])[k];
					else {
#ifdef MULTI_LINE_NO_SEPARATOR
						str = "";
#else
						if (cols_height [i] == 0)
							str = "";
						else
							str = ".";
						align_center = TRUE;
#endif
					}
					
					if (cols_is_num [i])
						g_string_append_printf (string, "%*s", cols_size [i], str);
					else {
						gint j, max;
						if (str) 
							max = cols_size [i] - g_utf8_strlen (str, -1);
						else
							max = cols_size [i];

						if (!align_center) {
							g_string_append_printf (string, "%s", str);
							for (j = 0; j < max; j++)
								g_string_append_c (string, ' ');
						}
						else {
							for (j = 0; j < max / 2; j++)
								g_string_append_c (string, ' ');
							g_string_append_printf (string, "%s", str);
							for ( j+= g_utf8_strlen (str, -1); j < cols_size [i]; j++)
								g_string_append_c (string, ' ');
						}
						/*
						gint j, max;
						if (str) {
							g_string_append_printf (string, "%s", str);
							max = cols_size [i] - g_utf8_strlen (str, -1);
						}
						else
							max = cols_size [i];
						for (j = 0; j < max; j++)
							g_string_append_c (string, ' ');
						*/
					}
				}
				g_string_append_c (string, '\n');
			}

			g_free (cols_height);
			for (i = 0; i < n_cols; i++) 
				g_strfreev (cols_str [i]);
			g_free (cols_str);
		}
		g_string_append_printf (string, ngettext("(%d row)\n", "(%d rows)\n", n_rows), n_rows);
	}
	else 
		g_string_append (string, _("Model does not support random access, not showing data\n"));

	g_free (cols_size);
	g_free (cols_is_num);

	g_free (offstr);
	
	if (allok) {
		str = string->str;
		g_string_free (string, FALSE);
	}
	else {
		str = NULL;
		g_string_free (string, TRUE);
	}
	return str;
}
