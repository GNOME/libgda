/* GDA common library
 * Copyright (C) 1998-2001 The Free Software Foundation
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

#include "gda-data-model.h"

typedef struct {
	gchar *title;
	GPtrArray *values;
} GdaDataModelColumn;

struct _GdaDataModelPrivate {
	GPtrArray *columns;
	gint row_count;
};

static void gda_data_model_class_init (GdaDataModelClass *klass);
static void gda_data_model_init       (GdaDataModel *model, GdaDataModelClass *klass);
static void gda_data_model_finalize   (GObject *object);

/*
 * GdaDataModelColumn util functions
 */

static GdaDataModelColumn *
gdmc_new (const gchar *title)
{
	GdaDataModelColumn *col;

	col = g_new0 (GdaDataModelColumn, 1);
	col->title = g_strdup (title);
	col->values = g_ptr_array_new ();
}

static void
gdmc_free (GdaDataModelColumn *col)
{
	g_return_if_fail (col != NULL);

	g_free (col->title);
	/* FIXME: free list of values */
	g_free (col);
}

/*
 * GdaDataModel class implementation
 */

static void
gda_data_model_class_init (GdaDataModelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gda_data_model_finalize;
}

static void
gda_data_model_init (GdaDataModel *model, GdaDataModelClass *klass)
{
	/* allocate internal structure */
	model->priv = g_new0 (GdaDataModelPrivate, 1);
	model->priv->columns = g_ptr_array_new ();
}

static void
gda_data_model_finalize (GObject *object)
{
	GObjectClass *parent_class;
	gint i;
	GdaDataModel *model = (GdaDataModel *) object;

	g_return_if_fail (GDA_IS_DATA_MODEL (model));

	/* free memory */
	for (i = 0; i < model->priv->columns->len; i++)
		gdmc_free (g_ptr_array_index (model->priv->columns, i));
	g_ptr_array_free (model->priv->columns, TRUE);

	g_free (model->priv);
	model->priv = NULL;

	parent_class = g_type_class_peek (G_TYPE_OBJECT);
	if (parent_class && parent_class->finalize)
		parent_class->finalize (object);
}

GType
gda_data_model_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaDataModelClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_data_model_class_init,
			NULL,
			NULL,
			sizeof (GdaDataModel),
			0,
			(GInstanceInitFunc) gda_data_model_init
		};
		type = g_type_register_static (G_TYPE_OBJECT, "GdaDataModel", &info, 0);
	}
	return type;
}

/**
 * gda_data_model_append_column
 */
void
gda_data_model_append_column (GdaDataModel *model, const gchar *title)
{
	GdaDataModelColumn *model_col;

	g_return_if_fail (GDA_IS_DATA_MODEL (model));

	model_col = gdmc_new (title);
	g_ptr_array_add (model->priv->columns, model_col);
}

/**
 * gda_data_model_remove_column
 */
void
gda_data_model_remove_column (GdaDataModel *model, gint col)
{
	GdaDataModelColumn *model_col;

	g_return_if_fail (GDA_IS_DATA_MODEL (model));
	g_return_if_fail (col >= 0);
	g_return_if_fail (col < model->priv->columns->len);

	model_col = g_ptr_array_remove_index (model->priv->columns, col);
	gdmc_free (model_col);
}

/**
 * gda_data_model_append_row
 */
void
gda_data_model_append_row (GdaDataModel *model)
{
	gint cnt;

	g_return_if_fail (GDA_IS_DATA_MODEL (model));

	model->priv->row_count++;
	for (cnt = 0; cnt < model->priv->columns->len; cnt++) {
		GdaDataModelColumn *col;

		col = g_ptr_array_index (model->priv->columns, cnt);
		if (col)
			g_ptr_array_set_size (col->values, model->priv->row_count);
	}
}

/**
 * gda_data_model_remove_row
 */
void
gda_data_model_remove_row (GdaDataModel *model, gint row)
{
	gint i;

	g_return_if_fail (GDA_IS_DATA_MODEL (model));
	g_return_if_fail (row >= 0);
	g_return_if_fail (row < model->priv->row_count);

	for (i = 0; i < model->priv->columns->len; i++) {
		GdaDataModelColumn *model_col;

		model_col = g_ptr_array_index (model->priv->columns, i);
		if (model_col) {
			GdaValue *value;

			value = g_ptr_array_remove_index (model_col, row);
			gda_value_free (value);
		}
	}
	if (model->priv->row_count > 0)
		model->priv->row_count--;
}

/**
 * gda_data_model_get_n_columns
 */
gint
gda_data_model_get_n_columns (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), -1);
	return model->priv->columns->len;
}

/**
 * gda_data_model_get_column_title
 */
const gchar *
gda_data_model_get_column_title (GdaDataModel *model, gint col)
{
	GdaDataModelColumn *model_col;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);
	g_return_val_if_fail (col >= 0, NULL);
	g_return_val_if_fail (col < model->priv->columns->len, NULL);

	model_col = g_ptr_array_index (model->priv->columns, col);
	return model_col ? model_col->title : NULL;
}

/**
 * gda_data_model_get_row_count
 */
gint
gda_data_model_get_row_count (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), -1);
	return model->priv->row_count;
}

/**
 * gda_data_model_get_value_at
 */
GdaValue *
gda_data_model_get_value_at (GdaDataModel *model, gint row, gint col)
{
	GdaDataModelColumn *model_col;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);
	g_return_val_if_fail (row >= 0, NULL);
	g_return_val_if_fail (row < model->priv->row_count, NULL);
	g_return_val_if_fail (col >= 0, NULL);
	g_return_val_if_fail (col < model->priv->columns->len, NULL);

	model_col = g_ptr_array_index (model->priv->columns, col);
	if (model_col)
		return g_ptr_array_index (model_col->values, row);

	return NULL;
}

/**
 * gda_data_model_set_value_at
 */
void
gda_data_model_set_value_at (GdaDataModel *model, gint row, gint col, GdaValue *value)
{
	GdaDataModelColumn *model_col;

	g_return_if_fail (GDA_IS_DATA_MODEL (model));
	g_return_if_fail (row >= 0);
	g_return_if_fail (row < model->priv->row_count);
	g_return_if_fail (col >= 0);
	g_return_if_fail (col < model->priv->columns->len);

	model_col = g_ptr_array_index (model->priv->columns, col);
	if (model_col)
		(model_col->values->pdata)[row] = value;
}
