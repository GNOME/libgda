/* 
 * GDA common library
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

#include <libgda/gda-data-model.h>

#define PARENT_TYPE G_TYPE_OBJECT
#define CLASS(model) (GDA_DATA_MODEL_CLASS (G_OBJECT_GET_CLASS (model)))

struct _GdaDataModelPrivate {
	gboolean notify_changes;
	GHashTable *column_titles;

	/* edition mode */
	gboolean editing;
};

static void gda_data_model_class_init (GdaDataModelClass *klass);
static void gda_data_model_init       (GdaDataModel *model, GdaDataModelClass *klass);
static void gda_data_model_finalize   (GObject *object);

enum {
	CHANGED,
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
	klass->get_value_at = NULL;
}

static void
gda_data_model_init (GdaDataModel *model, GdaDataModelClass *klass)
{
	g_return_if_fail (GDA_IS_DATA_MODEL (model));

	model->priv = g_new0 (GdaDataModelPrivate, 1);
	model->priv->notify_changes = TRUE;
	model->priv->column_titles = g_hash_table_new (g_str_hash, g_str_equal);
	model->priv->editing = FALSE;
}

static void
free_hash_string (gpointer key, gpointer value, gpointer user_data)
{
	g_free (key);
	g_free (value);
}

static void
gda_data_model_finalize (GObject *object)
{
	GdaDataModel *model = (GdaDataModel *) object;

	g_return_if_fail (GDA_IS_DATA_MODEL (model));

	/* free memory */
	g_hash_table_foreach (model->priv->column_titles, (GHFunc) free_hash_string, NULL);
	g_hash_table_destroy (model->priv->column_titles);

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
		/* we generate a basic FieldAttributes structure */
		fa = gda_field_attributes_new ();
		gda_field_attributes_set_defined_size (fa, 0);
		gda_field_attributes_set_name (fa, gda_data_model_get_column_title (model, col));
		gda_field_attributes_set_scale (fa, 0);
		gda_field_attributes_set_gdatype (fa, GDA_VALUE_TYPE_STRING);
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
	if (col < n_cols && col >= 0) {
		gchar *col_str = g_strdup_printf ("%d", col);
		title = g_hash_table_lookup (model->priv->column_titles, col_str);
		g_free (col_str);
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
		gpointer orig_key;
		gpointer orig_value;
		gchar *col_str;

		col_str = g_strdup_printf ("%d", col);
		if (g_hash_table_lookup_extended (model->priv->column_titles, col_str,
			&orig_key, &orig_value)) {
			g_hash_table_remove (model->priv->column_titles, &col);
			g_free (orig_key);
			g_free (orig_value);
		}

		g_hash_table_insert (model->priv->column_titles, col_str, g_strdup (title));
	}
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
 * position, or NULL on error (out-of-bound position, etc). The
 * returned value should be freed with #gda_value_free
 */
const GdaValue *
gda_data_model_get_value_at (GdaDataModel *model, gint col, gint row)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);
	g_return_val_if_fail (CLASS (model)->get_value_at != NULL, NULL);

	return CLASS (model)->get_value_at (model, col, row);
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
	return NULL;
}
