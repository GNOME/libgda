/* GDA library
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

#include <glib/garray.h>
#include <libgda/gda-data-model-array.h>

#define PARENT_TYPE GDA_TYPE_DATA_MODEL

struct _GdaDataModelArrayPrivate {
	/* number of columns in each row */
	gint number_of_columns;

	/* the array of rows, each item is a GdaRow */
	GPtrArray *rows;
};

static void gda_data_model_array_class_init (GdaDataModelArrayClass *klass);
static void gda_data_model_array_init       (GdaDataModelArray *model,
					     GdaDataModelArrayClass *klass);
static void gda_data_model_array_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

/*
 * GdaDataModelArray class implementation
 */

static gint
gda_data_model_array_get_n_rows (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ARRAY (model), -1);
	return GDA_DATA_MODEL_ARRAY (model)->priv->rows->len;
}

static gint
gda_data_model_array_get_n_columns (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ARRAY (model), -1);
	return GDA_DATA_MODEL_ARRAY (model)->priv->number_of_columns;
}

static GdaFieldAttributes *
gda_data_model_array_describe_column (GdaDataModel *model, gint col)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ARRAY (model), NULL);

	return NULL;
}

static const GdaRow *
gda_data_model_array_get_row (GdaDataModel *model, gint row)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ARRAY (model), NULL);

	if (row >= GDA_DATA_MODEL_ARRAY (model)->priv->rows->len)
		return NULL;

	return (const GdaRow *) g_ptr_array_index (GDA_DATA_MODEL_ARRAY (model)->priv->rows, row);
}

static const GdaValue *
gda_data_model_array_get_value_at (GdaDataModel *model, gint col, gint row)
{
	GdaRow *fields;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_ARRAY (model), NULL);

	if (row >= GDA_DATA_MODEL_ARRAY (model)->priv->rows->len)
		return NULL;

	fields = g_ptr_array_index (GDA_DATA_MODEL_ARRAY (model)->priv->rows, row);
	if (fields != NULL) {
		GdaValue *field;

		field = gda_row_get_value (fields, col);
		return (const GdaValue *) field;
	}

	return NULL;
}

static gboolean
gda_data_model_array_is_editable (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ARRAY (model), FALSE);
	return TRUE;
}

static const GdaRow *
gda_data_model_array_append_row (GdaDataModel *model, const GList *values)
{
	gint len;
	gint i;
	const GList *l;
	GdaRow *row = NULL;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_ARRAY (model), NULL);
	g_return_val_if_fail (values != NULL, NULL);

	len = g_list_length ((GList *) values);
	if (len != GDA_DATA_MODEL_ARRAY (model)->priv->number_of_columns)
		return NULL;

	l = values;
	row = gda_row_new (len);
	for (i = 0; i < len; i++) {
		GdaValue *field;

		if (!l)
			return NULL;

		field = gda_value_copy ((GdaValue *) l->data);
		memcpy (gda_row_get_value (row, i), field, sizeof (GdaValue));
		l = l->next;
	}

	g_ptr_array_add (GDA_DATA_MODEL_ARRAY (model)->priv->rows, row);
	gda_data_model_changed (model);

	return (const GdaRow *) row;
}

static void
gda_data_model_array_class_init (GdaDataModelArrayClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaDataModelClass *model_class = GDA_DATA_MODEL_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_data_model_array_finalize;
	model_class->get_n_rows = gda_data_model_array_get_n_rows;
	model_class->get_n_columns = gda_data_model_array_get_n_columns;
	model_class->describe_column = gda_data_model_array_describe_column;
	model_class->get_row = gda_data_model_array_get_row;
	model_class->get_value_at = gda_data_model_array_get_value_at;
	model_class->is_editable = gda_data_model_array_is_editable;
	model_class->append_row = gda_data_model_array_append_row;
}

static void
gda_data_model_array_init (GdaDataModelArray *model, GdaDataModelArrayClass *klass)
{
	g_return_if_fail (GDA_IS_DATA_MODEL_ARRAY (model));

	/* allocate internal structure */
	model->priv = g_new0 (GdaDataModelArrayPrivate, 1);
	model->priv->number_of_columns = 0;
	model->priv->rows = g_ptr_array_new ();
}

static void
gda_data_model_array_finalize (GObject *object)
{
	GdaDataModelArray *model = (GdaDataModelArray *) object;

	g_return_if_fail (GDA_IS_DATA_MODEL_ARRAY (model));

	/* free memory */
	gda_data_model_array_clear (model);
	g_ptr_array_free (model->priv->rows, TRUE);

	g_free (model->priv);
	model->priv = NULL;

	/* chain to parent class */
	parent_class->finalize (object);
}

GType
gda_data_model_array_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaDataModelArrayClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_data_model_array_class_init,
			NULL,
			NULL,
			sizeof (GdaDataModelArray),
			0,
			(GInstanceInitFunc) gda_data_model_array_init
		};
		type = g_type_register_static (PARENT_TYPE,
					       "GdaDataModelArray",
					       &info, 0);
	}
	return type;
}

/**
 * gda_data_model_array_new
 */
GdaDataModel *
gda_data_model_array_new (gint cols)
{
	GdaDataModel *model;

	model = g_object_new (GDA_TYPE_DATA_MODEL_ARRAY, NULL);
	gda_data_model_array_set_n_columns (GDA_DATA_MODEL_ARRAY (model), cols);
	return model;
}

/**
 * gda_data_model_array_set_n_columns
 */
void
gda_data_model_array_set_n_columns (GdaDataModelArray *model, gint cols)
{
	g_return_if_fail (GDA_IS_DATA_MODEL_ARRAY (model));
	model->priv->number_of_columns = cols;
	/* FIXME: should rebuild the internal array, removing/adding empty cols */
}

/**
 * gda_data_model_array_clear
 */
void
gda_data_model_array_clear (GdaDataModelArray *model)
{
	g_return_if_fail (GDA_IS_DATA_MODEL_ARRAY (model));

	while (model->priv->rows->len > 0) {
		GdaRow *row = (GdaRow *) g_ptr_array_index (model->priv->rows, 0);

		if (row != NULL)
			gda_row_free (row);
		g_ptr_array_remove_index (model->priv->rows, 0);
	}
}
