/* GDA common library
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

#include <libgda/gda-data-model-array.h>
#include <libgda/gda-data-model-list.h>

#define PARENT_TYPE GDA_TYPE_DATA_MODEL

struct _GdaDataModelListPrivate {
	GdaDataModelArray *rows;
};

static void gda_data_model_list_class_init (GdaDataModelListClass *klass);
static void gda_data_model_list_init       (GdaDataModelList *model,
					    GdaDataModelListClass *klass);
static void gda_data_model_list_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

/*
 * GdaDataModelList class implementation
 */

static gint
gda_data_model_list_get_n_rows (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_LIST (model), -1);
	return gda_data_model_get_n_rows (GDA_DATA_MODEL (GDA_DATA_MODEL_LIST (model)->priv->rows));
}

static gint
gda_data_model_list_get_n_columns (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_LIST (model), -1);
	return 1;
}

static const GdaValue *
gda_data_model_list_get_value_at (GdaDataModel *model, gint col, gint row)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_LIST (model), NULL);
	g_return_val_if_fail (col == 0, NULL);

	return gda_data_model_get_value_at (GDA_DATA_MODEL (GDA_DATA_MODEL_LIST (model)->priv->rows), col, row);
}

static gboolean
gda_data_model_list_is_editable (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_LIST (model), FALSE);
	return TRUE;
}

static const GdaRow *
gda_data_model_list_append_row (GdaDataModel *model, const GList *values)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_LIST (model), NULL);
	g_return_val_if_fail (values != NULL, NULL);

	return gda_data_model_list_append_value (GDA_DATA_MODEL_LIST (model),
						 (const GdaValue *) values->data);
}

static void
gda_data_model_list_class_init (GdaDataModelListClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaDataModelClass *model_class = GDA_DATA_MODEL_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_data_model_list_finalize;
	model_class->get_n_rows = gda_data_model_list_get_n_rows;
	model_class->get_n_columns = gda_data_model_list_get_n_columns;
	model_class->describe_column = NULL;
	model_class->get_value_at = gda_data_model_list_get_value_at;
	model_class->is_editable = gda_data_model_list_is_editable;
	model_class->append_row = gda_data_model_list_append_row;
}

static void
gda_data_model_list_init (GdaDataModelList *list, GdaDataModelListClass *klass)
{
	/* allocate internal structure */
	list->priv = g_new0 (GdaDataModelListPrivate, 1);
	list->priv->rows = gda_data_model_array_new (1);
}

static void
gda_data_model_list_finalize (GObject *object)
{
	GdaDataModelList *model = (GdaDataModelList *) object;

	g_return_if_fail (GDA_IS_DATA_MODEL_LIST (model));

	/* free memory */
	g_object_unref (G_OBJECT (model->priv->rows));

	g_free (model->priv);
	model->priv = NULL;

	/* chain to parent class */
	parent_class->finalize (object);
}

GType
gda_data_model_list_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaDataModelListClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_data_model_list_class_init,
			NULL,
			NULL,
			sizeof (GdaDataModelList),
			0,
			(GInstanceInitFunc) gda_data_model_list_init
		};
		type = g_type_register_static (PARENT_TYPE,
					       "GdaDataModelList",
					       &info, 0);
	}
	return type;
}

/**
 * gda_data_model_list_new
 */
GdaDataModel *
gda_data_model_list_new (void)
{
	GdaDataModel *model;

	model = g_object_new (GDA_TYPE_DATA_MODEL_LIST, NULL);
	return model;
}

/**
 * gda_data_model_list_new_from_string_list
 */
GdaDataModel *
gda_data_model_list_new_from_string_list (const GList *list)
{
	GdaDataModel *model;
	GList *l;
	GdaValue *value;

	model = gda_data_model_list_new ();

	for (l = (GList *) list; l; l = l->next) {
		gchar *str = (gchar *) l->data;
		if (str) {
			value = gda_value_new_string ((const gchar *) str);
			gda_data_model_list_append_value (GDA_DATA_MODEL_LIST (model), value);
			gda_value_free (value);
		}
	}

	return model;
}

/**
 * gda_data_model_list_append_value
 */
const GdaRow *
gda_data_model_list_append_value (GdaDataModelList *model, const GdaValue *value)
{
	GList *values;
	GdaRow *row;

	g_return_if_fail (GDA_IS_DATA_MODEL_LIST (model));
	g_return_if_fail (value != NULL);

	values = g_list_append (NULL, value);
	row = gda_data_model_append_row (GDA_DATA_MODEL (model->priv->rows), values);
	if (row)
		gda_data_model_changed (GDA_DATA_MODEL (model));

	return row;
}
