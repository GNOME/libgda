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

	/* the array of rows, each item is a GList of GdaValue's */
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

static const GdaValue *
gda_data_model_array_get_value_at (GdaDataModel *model, gint col, gint row)
{
	GList *fields;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_ARRAY (model), NULL);

	if (row >= GDA_DATA_MODEL_ARRAY (model)->priv->rows->len)
		return NULL;

	fields = g_ptr_array_index (GDA_DATA_MODEL_ARRAY (model)->priv->rows, row);
	if (fields != NULL) {
		gint len = g_list_length (fields);

		if (col < len) {
			GList *node = g_list_nth (fields, col);
			if (node != NULL)
				return (GdaValue *) node->data;
		}
	}

	return NULL;
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
	model_class->get_value_at = gda_data_model_array_get_value_at;
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
		GList *cols = (GList *) g_ptr_array_index (model->priv->rows, 0);

		if (cols != NULL)
			g_list_foreach (cols, (GFunc) gda_value_free, NULL);
		g_ptr_array_remove_index (model->priv->rows, 0);
		g_list_free (cols);
	}
}

/**
 * gda_data_model_array_append_row
 */
void
gda_data_model_array_append_row (GdaDataModelArray *model, const GList *values)
{
	gint len;
	gint i;
	GList *list = NULL;

	g_return_if_fail (GDA_IS_DATA_MODEL_ARRAY (model));
	g_return_if_fail (values != NULL);

	len = g_list_length (values);
	if (len != model->priv->number_of_columns)
		return;

	for (i = 0; i < len; i++) {
		GList *l;
		GdaValue *new_value;

		l = g_list_nth (values, i);
		if (!l)
			return;

		new_value = gda_value_copy ((GdaValue *) l->data);
		list = g_list_append (list, new_value);
	}

	g_ptr_array_add (model->priv->rows, list);
	gda_data_model_changed (GDA_DATA_MODEL (model));
}
