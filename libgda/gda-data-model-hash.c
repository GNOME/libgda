/* GDA library
 * Copyright (C) 1998 - 2004 The GNOME Foundation.
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

#include <glib/ghash.h>
#include <libgda/gda-data-model-hash.h>

#define PARENT_TYPE GDA_TYPE_DATA_MODEL_BASE

struct _GdaDataModelHashPrivate {
	/* number of columns in each row */
	gint number_of_columns;

	/* the hash of rows, each item is a GdaRow */
	GHashTable *rows;
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
gda_data_model_hash_get_n_rows (GdaDataModelBase *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_HASH (model), -1);
	return g_hash_table_size (GDA_DATA_MODEL_HASH (model)->priv->rows);
}

static gint
gda_data_model_hash_get_n_columns (GdaDataModelBase *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_HASH (model), -1);
	return GDA_DATA_MODEL_HASH (model)->priv->number_of_columns;
}

static const GdaRow *
gda_data_model_hash_get_row (GdaDataModelBase *model, gint row)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_HASH (model), NULL);

	return (const GdaRow *) g_hash_table_lookup (GDA_DATA_MODEL_HASH (model)->priv->rows,
						     GINT_TO_POINTER (row));
}

static const GdaValue *
gda_data_model_hash_get_value_at (GdaDataModelBase *model, gint col, gint row)
{
	const GdaRow *fields;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_HASH (model), NULL);

	fields = gda_data_model_hash_get_row (model, row);
	if (fields == NULL)
		return NULL;

	return (const GdaValue *) gda_row_get_value ((GdaRow *) fields, col);
}

static GdaDataModelColumnAttributes *
gda_data_model_hash_describe_column (GdaDataModelBase *model, gint col)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_HASH (model), NULL);

	return NULL;
}

static gboolean
gda_data_model_hash_is_updatable (GdaDataModelBase *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_HASH (model), FALSE);
	return TRUE;
}

static const GdaRow *
gda_data_model_hash_append_row (GdaDataModelBase *model, const GList *values)
{
	GdaRow *row;
	gint cols, rownum;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_HASH (model), NULL);
	g_return_val_if_fail (values != NULL, NULL);

	cols = g_list_length ((GList *) values);
	if (cols != GDA_DATA_MODEL_HASH (model)->priv->number_of_columns)
		return NULL;

	/* create the GdaRow to add */
	row = gda_row_new_from_list (GDA_DATA_MODEL (model), values);

	/* get the new row number */
	rownum = gda_data_model_get_n_rows (GDA_DATA_MODEL (model));

	if (row) {
		gda_data_model_hash_insert_row (
			GDA_DATA_MODEL_HASH (model),
			rownum,
			row);

		gda_row_set_number (row, rownum);
		gda_data_model_row_inserted (GDA_DATA_MODEL (model), rownum);
	}

	return row;
}

static gboolean
gda_data_model_hash_remove_row (GdaDataModelBase *model, const GdaRow *row)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_HASH (model), FALSE);
	return FALSE;
}

static void
free_hash (gpointer value)
{
	gda_row_free ((GdaRow *) value);
}

static void
gda_data_model_hash_class_init (GdaDataModelHashClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaDataModelBaseClass *model_class = GDA_DATA_MODEL_BASE_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_data_model_hash_finalize;
	model_class->get_n_rows = gda_data_model_hash_get_n_rows;
	model_class->get_n_columns = gda_data_model_hash_get_n_columns;
	model_class->describe_column = gda_data_model_hash_describe_column;
	model_class->get_row = gda_data_model_hash_get_row;
	model_class->get_value_at = gda_data_model_hash_get_value_at;
	model_class->is_updatable = gda_data_model_hash_is_updatable;
	model_class->append_row = gda_data_model_hash_append_row;
	model_class->remove_row = gda_data_model_hash_remove_row;
}

static void
gda_data_model_hash_init (GdaDataModelHash *model, GdaDataModelHashClass *klass)
{
	g_return_if_fail (GDA_IS_DATA_MODEL_HASH (model));

	/* allocate internal structure */
	model->priv = g_new0 (GdaDataModelHashPrivate, 1);
	model->priv->number_of_columns = 0;
	model->priv->rows = NULL;
}

static void
gda_data_model_hash_finalize (GObject *object)
{
	GdaDataModelHash *model = (GdaDataModelHash *) object;

	g_return_if_fail (GDA_IS_DATA_MODEL_HASH (model));

	/* free memory */
	g_hash_table_destroy (model->priv->rows);
	model->priv->rows = NULL;

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
	gda_data_model_row_inserted (GDA_DATA_MODEL (model), rownum);
	gda_data_model_changed (GDA_DATA_MODEL (model));
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
	g_return_if_fail (GDA_IS_DATA_MODEL_HASH (model));

	if (model->priv->rows != NULL)
		g_hash_table_destroy (model->priv->rows);
	model->priv->rows = g_hash_table_new_full (g_direct_hash,
						   g_direct_equal,
						   NULL, free_hash);
}

