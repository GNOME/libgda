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

#include <libgda/gda-data-model-list.h>

#define PARENT_TYPE GDA_TYPE_DATA_MODEL

struct _GdaDataModelListPrivate {
	/* list of GdaValue's */
	GList *value_list;
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
	return g_list_length (GDA_DATA_MODEL_LIST (model)->priv->value_list);
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
	gint count;
	GdaValue *value;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_LIST (model), NULL);
	g_return_val_if_fail (col == 0, NULL);

	count = g_list_length (GDA_DATA_MODEL_LIST (model)->priv->value_list);
	if (row > count)
		return NULL;

	value = (GdaValue *) g_list_nth (GDA_DATA_MODEL_LIST (model)->priv->value_list, row);
	return value;
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
	model_class->get_value_at = gda_data_model_list_get_value_at;
}

static void
gda_data_model_list_init (GdaDataModelList *list, GdaDataModelListClass *klass)
{
	/* allocate internal structure */
	list->priv = g_new0 (GdaDataModelListPrivate, 1);
	list->priv->value_list = NULL;
}

static void
gda_data_model_list_finalize (GObject *object)
{
	GdaDataModelList *model = (GdaDataModelList *) object;

	g_return_if_fail (GDA_IS_DATA_MODEL_LIST (model));

	/* free memory */
	g_list_foreach (model->priv->value_list, (GFunc) gda_value_free, NULL);
	g_list_free (model->priv->value_list);

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

	model = gda_data_model_list_new ();

	for (l = (GList *) list; l; l = l->next) {
		gchar *str = (gchar *) l->data;
		if (str) {
			GdaValue *value;

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
void
gda_data_model_list_append_value (GdaDataModelList *model, const GdaValue *value)
{
	GdaValue *new_value;

	g_return_if_fail (GDA_IS_DATA_MODEL_LIST (model));
	g_return_if_fail (value != NULL);

	new_value = gda_value_copy ((GdaValue *) value);
	model->priv->value_list = g_list_append (model->priv->value_list, new_value);
	gda_data_model_changed (GDA_DATA_MODEL (model));
}

/**
 * gda_data_model_list_prepend_value
 */
void
gda_data_model_list_prepend_value (GdaDataModelList *model, const GdaValue *value)
{
	GdaValue *new_value;

	g_return_if_fail (GDA_IS_DATA_MODEL_LIST (model));
	g_return_if_fail (value != NULL);

	new_value = gda_value_copy ((GdaValue *) value);
	model->priv->value_list = g_list_prepend (model->priv->value_list, new_value);
	gda_data_model_changed (GDA_DATA_MODEL (model));
}
