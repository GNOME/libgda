/* GDA library
 * Copyright (C) 1998 - 2006 The GNOME Foundation.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *	Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 *      Vivien Malerba <malerba@gnome-db.org>
 *	Bas Driessen <bas.driessen@xobas.com>
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

#include <glib/ghash.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-data-model-hash.h>
#include <libgda/gda-data-model-private.h>

#define PARENT_TYPE GDA_TYPE_DATA_MODEL_ROW

struct _GdaDataModelHashPrivate {
	/* number of columns in each row */
	gint number_of_columns;

	/* the hash of rows, each item is a GdaRow */
	GHashTable *rows;
	gint number_of_hash_table_rows;

	/* the mapping of a row to hash table entry */
	GArray *row_map;
};

static void gda_data_model_hash_class_init (GdaDataModelHashClass *klass);
static void gda_data_model_hash_init       (GdaDataModelHash *model,
					     GdaDataModelHashClass *klass);
static void gda_data_model_hash_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

/*
 * GdaDataModelHash class implementation
 */

static gint
gda_data_model_hash_get_n_rows (GdaDataModelRow *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_HASH (model), -1);

	if (GDA_DATA_MODEL_HASH (model)->priv->row_map == NULL)
		return -1;
	else
		return GDA_DATA_MODEL_HASH (model)->priv->row_map->len;
}

static gint
gda_data_model_hash_get_n_columns (GdaDataModelRow *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_HASH (model), -1);
	return GDA_DATA_MODEL_HASH (model)->priv->number_of_columns;
}

static GdaRow *
gda_data_model_hash_get_row (GdaDataModelRow *model, gint row, GError **error)
{
	gint hash_entry;
	GdaRow *gdarow;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_HASH (model), NULL);

	/* get row according mapping */
	hash_entry = g_array_index (GDA_DATA_MODEL_HASH (model)->priv->row_map, gint, row);

	gdarow = (GdaRow *) g_hash_table_lookup (GDA_DATA_MODEL_HASH (model)->priv->rows,
					      GINT_TO_POINTER (hash_entry));
	if (!gdarow)
		g_set_error (error, 0, 0, _("Row %d not found in data model"), row);
	return gdarow;
}

static const GdaValue *
gda_data_model_hash_get_value_at (GdaDataModelRow *model, gint col, gint row)
{
	const GdaRow *fields;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_HASH (model), NULL);

	fields = gda_data_model_hash_get_row (model, row, NULL);
	if (fields == NULL)
		return NULL;

	if (col >= GDA_DATA_MODEL_HASH (model)->priv->number_of_columns) {
		g_warning (_("Column out %d of range 0 - %d"), col, 
			   GDA_DATA_MODEL_HASH (model)->priv->number_of_columns);
		return NULL;
	}

	return (const GdaValue *) gda_row_get_value ((GdaRow *) fields, col);
}

static gboolean
gda_data_model_hash_is_updatable (GdaDataModelRow *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_HASH (model), FALSE);
	return TRUE;
}

static gboolean
gda_data_model_hash_append_row (GdaDataModelRow *model, GdaRow *row, GError **error)
{
	gint cols, rownum_hash_new, rownum_array_new;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_HASH (model), FALSE);
	g_return_val_if_fail (row != NULL, FALSE);

	cols = gda_row_get_length (row);
	if (cols != GDA_DATA_MODEL_HASH (model)->priv->number_of_columns) {
		g_set_error (error, 0, GDA_DATA_MODEL_VALUES_LIST_ERROR,
			     _("Wrong number of values in values list"));
		return FALSE;
	}

	/* get the new hash and array row number */
	rownum_hash_new = GDA_DATA_MODEL_HASH (model)->priv->number_of_hash_table_rows;
	rownum_array_new = GDA_DATA_MODEL_HASH (model)->priv->row_map->len;

	gda_data_model_hash_insert_row (
		GDA_DATA_MODEL_HASH (model),
		rownum_hash_new,
		row);

	gda_row_set_number (row, rownum_array_new);

	/* Append row number in row mapping array */
	g_array_append_val (GDA_DATA_MODEL_HASH (model)->priv->row_map, rownum_hash_new);

	/* emit update signals */
	gda_data_model_row_inserted (GDA_DATA_MODEL (model), rownum_array_new);

	/* increment the number of entries in the hash table */
	GDA_DATA_MODEL_HASH (model)->priv->number_of_hash_table_rows++;

	return TRUE;
}

static gboolean
gda_data_model_hash_remove_row (GdaDataModelRow *model, GdaRow *row, GError **error)
{
	gint i, cols, rownum;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_HASH (model), FALSE);
	g_return_val_if_fail (row != NULL, FALSE);

	cols = GDA_DATA_MODEL_HASH (model)->priv->number_of_columns;

	for (i=0; i<cols; i++)
		gda_row_set_value (row, i, NULL);

	rownum = gda_row_get_number ((GdaRow *) row);

	/* renumber following rows */
	for (i=(rownum+1); i<GDA_DATA_MODEL_HASH (model)->priv->row_map->len; i++) 
		gda_row_set_number ((GdaRow *) gda_data_model_hash_get_row (model, i, error), (i-1));

	/* tag the row as being removed */	
	gda_row_set_id ((GdaRow *) row, "R");
	gda_row_set_number ((GdaRow *) row, -1);

	/* remove entry from array */
	g_array_remove_index (GDA_DATA_MODEL_HASH (model)->priv->row_map, rownum);

	/* emit updated signals */
	gda_data_model_row_removed (GDA_DATA_MODEL (model), gda_row_get_number ((GdaRow *) row));
	
	return TRUE;
}

static GdaRow *
gda_data_model_hash_append_values (GdaDataModelRow *model, const GList *values, GError **error)
{
	gint len;
	GdaRow *row = NULL;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_HASH (model), NULL);
	g_return_val_if_fail (values != NULL, NULL);

	len = g_list_length ((GList *) values);
	if (len != GDA_DATA_MODEL_HASH (model)->priv->number_of_columns) {
		g_set_error (error, 0, GDA_DATA_MODEL_VALUES_LIST_ERROR,
			     _("Wrong number of values in values list"));
		return NULL;
	}

	row = gda_row_new_from_list (GDA_DATA_MODEL (model), values);
	if (!gda_data_model_hash_append_row (model, row, error)) {
		g_object_unref (row);
		return NULL;
	}

	return (GdaRow *) row;
}

static void
free_hash (gpointer value)
{
	g_object_unref (value);
}

static void
gda_data_model_hash_class_init (GdaDataModelHashClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaDataModelRowClass *model_class = GDA_DATA_MODEL_ROW_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_data_model_hash_finalize;
	model_class->get_n_rows = gda_data_model_hash_get_n_rows;
	model_class->get_n_columns = gda_data_model_hash_get_n_columns;
	model_class->get_row = gda_data_model_hash_get_row;
	model_class->get_value_at = gda_data_model_hash_get_value_at;
	model_class->is_updatable = gda_data_model_hash_is_updatable;
	model_class->append_values = gda_data_model_hash_append_values;
	model_class->append_row = gda_data_model_hash_append_row;
	model_class->update_row = NULL;
	model_class->remove_row = gda_data_model_hash_remove_row;
}

static void
gda_data_model_hash_init (GdaDataModelHash *model, GdaDataModelHashClass *klass)
{
	g_return_if_fail (GDA_IS_DATA_MODEL_HASH (model));

	/* allocate internal structure */
	model->priv = g_new0 (GdaDataModelHashPrivate, 1);
	model->priv->number_of_columns = 0;
	model->priv->number_of_hash_table_rows = 0;
	model->priv->rows = NULL;
	model->priv->row_map = NULL;
}

static void
gda_data_model_hash_finalize (GObject *object)
{
	GdaDataModelHash *model = (GdaDataModelHash *) object;

	g_return_if_fail (GDA_IS_DATA_MODEL_HASH (model));

	/* free memory */
	g_hash_table_destroy (model->priv->rows);
	model->priv->rows = NULL;

	g_array_free (model->priv->row_map, TRUE);

	g_free (model->priv);
	model->priv = NULL;

	/* chain to parent class */
	parent_class->finalize (object);
}

/*
 * Public functions
 */

/**
 * gda_data_model_hash_get_type
 *
 * Returns: the #GType of GdaDataModelHash.
 */
GType
gda_data_model_hash_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaDataModelHashClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_data_model_hash_class_init,
			NULL,
			NULL,
			sizeof (GdaDataModelHash),
			0,
			(GInstanceInitFunc) gda_data_model_hash_init
		};
		type = g_type_register_static (PARENT_TYPE,
					       "GdaDataModelHash",
					       &info, 0);
	}
	return type;
}

/**
 * gda_data_model_hash_new
 * @cols: number of columns for rows in this data model.
 *
 * Returns: a pointer to the newly created #GdaDataModel.
 */
GdaDataModel *
gda_data_model_hash_new (gint cols)
{
	GdaDataModel *model;

	model = g_object_new (GDA_TYPE_DATA_MODEL_HASH, NULL);
	gda_data_model_hash_set_n_columns (GDA_DATA_MODEL_HASH (model), cols);
	return model;
}

/**
 * gda_data_model_hash_insert_row
 * @model: the #GdaDataModelHash which is gonna hold the row.
 * @rownum: the number of the row.
 * @row: the row to insert. The model is responsible of freeing it!
 *
 * Inserts a @row in the @model.
 */
void
gda_data_model_hash_insert_row (GdaDataModelHash *model,
				gint rownum, 
				GdaRow *row)
{
	g_return_if_fail (GDA_IS_DATA_MODEL_HASH (model));
	g_return_if_fail (rownum >= 0);
	g_return_if_fail (row != NULL);

	if (gda_row_get_length (row) != model->priv->number_of_columns)
		return;

	if (g_hash_table_lookup (model->priv->rows,
				 GINT_TO_POINTER (rownum)) != NULL){
		g_warning ("Inserting an already existing row!");
		g_hash_table_remove (model->priv->rows,
				     GINT_TO_POINTER (rownum));
	}

	g_hash_table_insert (model->priv->rows,
			     GINT_TO_POINTER (rownum), row);
	g_object_ref (row);
	gda_data_model_row_inserted (GDA_DATA_MODEL (model), rownum);
}

/**
 * gda_data_model_hash_set_n_columns
 * @model: the #GdaDataModelHash.
 * @cols: the number of columns for rows inserted in @model.
 *
 * Sets the number of columns for rows inserted in this model.
 * @cols must be greater than or equal to 0.
 *
 * This function calls #gda_data_model_hash_clear to free the
 * existing rows if any.
 */
void
gda_data_model_hash_set_n_columns (GdaDataModelHash *model, gint cols)
{
	g_return_if_fail (GDA_IS_DATA_MODEL_HASH (model));
	g_return_if_fail (cols >= 0);

	model->priv->number_of_columns = cols;
	gda_data_model_hash_clear (model);
}

/**
 * gda_data_model_hash_clear
 * @model: the model to clear.
 *
 * Frees all the rows inserted in @model.
 */
void
gda_data_model_hash_clear (GdaDataModelHash *model)
{
	gint i;

	g_return_if_fail (GDA_IS_DATA_MODEL_HASH (model));

	if (model->priv->rows != NULL)
		g_hash_table_destroy (model->priv->rows);
	model->priv->rows = g_hash_table_new_full (g_direct_hash,
						   g_direct_equal,
						   NULL, free_hash);

	/* free array if exists */
	if (model->priv->row_map != NULL)
	{
		g_array_free (model->priv->row_map, TRUE);
		model->priv->row_map = NULL;
	}

	/* get number of entries in data model */
	model->priv->number_of_hash_table_rows = gda_data_model_get_n_rows (GDA_DATA_MODEL (model));

	/* create row mapping array */
	model->priv->row_map = g_array_new (FALSE, FALSE, sizeof (gint));

        /* Initialize row mapping array */
        for (i = 0; i < model->priv->number_of_hash_table_rows; i++)
                g_array_append_val (model->priv->row_map, i);

}

