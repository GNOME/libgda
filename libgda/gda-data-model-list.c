/* GDA common library
 * Copyright (C) 1998 - 2005 The GNOME Foundation.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
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

#include <libgda/gda-data-model-array.h>
#include <libgda/gda-data-model-list.h>
#include <libgda/gda-data-model-private.h>

#define PARENT_TYPE GDA_TYPE_DATA_MODEL_BASE

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
gda_data_model_list_get_n_rows (GdaDataModelBase *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_LIST (model), -1);
	return gda_data_model_get_n_rows (GDA_DATA_MODEL (GDA_DATA_MODEL_LIST (model)->priv->rows));
}

static gint
gda_data_model_list_get_n_columns (GdaDataModelBase *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_LIST (model), -1);
	return 1;
}

static GdaRow *
gda_data_model_list_get_row (GdaDataModelBase *model, gint row)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_LIST (model), NULL);
	return gda_data_model_get_row (GDA_DATA_MODEL (GDA_DATA_MODEL_LIST (model)->priv->rows), row);
}

static const GdaValue *
gda_data_model_list_get_value_at (GdaDataModelBase *model, gint col, gint row)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_LIST (model), NULL);
	g_return_val_if_fail (col == 0, NULL);

	return gda_data_model_get_value_at (GDA_DATA_MODEL (GDA_DATA_MODEL_LIST (model)->priv->rows), col, row);
}

static gboolean
gda_data_model_list_is_updatable (GdaDataModelBase *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_LIST (model), FALSE);
	return TRUE;
}

static GdaRow *
gda_data_model_list_append_values (GdaDataModelBase *model, const GList *values)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_LIST (model), NULL);
	g_return_val_if_fail (values != NULL, NULL);

	return gda_data_model_list_append_value (GDA_DATA_MODEL_LIST (model),
						 (const GdaValue *) values->data);
}

static gboolean
gda_data_model_list_remove_row (GdaDataModelBase *model, GdaRow *row)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_LIST (model), FALSE);
	g_return_val_if_fail (row != NULL, FALSE);

	return gda_data_model_remove_row (GDA_DATA_MODEL (GDA_DATA_MODEL_LIST (model)->priv->rows), row);
}

static void
gda_data_model_list_class_init (GdaDataModelListClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaDataModelBaseClass *model_class = GDA_DATA_MODEL_BASE_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_data_model_list_finalize;
	model_class->get_n_rows = gda_data_model_list_get_n_rows;
	model_class->get_n_columns = gda_data_model_list_get_n_columns;
	model_class->get_row = gda_data_model_list_get_row;
	model_class->get_value_at = gda_data_model_list_get_value_at;
	model_class->is_updatable = gda_data_model_list_is_updatable;
	model_class->append_values = gda_data_model_list_append_values;
	model_class->remove_row = gda_data_model_list_remove_row;
}

static void
proxy_changed_cb (GdaDataModel *model, gpointer user_data)
{
	GdaDataModelList *list = (GdaDataModelList *) user_data;

	g_return_if_fail (GDA_IS_DATA_MODEL_LIST (list));

	gda_data_model_changed (GDA_DATA_MODEL (list));
}

static void
proxy_row_inserted_cb (GdaDataModel *model, gint row, gpointer user_data)
{
	GdaDataModelList *list = (GdaDataModelList *) user_data;

	g_return_if_fail (GDA_IS_DATA_MODEL_LIST (list));

	gda_data_model_row_inserted (GDA_DATA_MODEL (list), row);
}

static void
proxy_row_updated_cb (GdaDataModel *model, gint row, gpointer user_data)
{
	GdaDataModelList *list = (GdaDataModelList *) user_data;

	g_return_if_fail (GDA_IS_DATA_MODEL_LIST (list));

	gda_data_model_row_updated (GDA_DATA_MODEL (list), row);
}

static void
proxy_row_removed_cb (GdaDataModel *model, gint row, gpointer user_data)
{
	GdaDataModelList *list = (GdaDataModelList *) user_data;

	g_return_if_fail (GDA_IS_DATA_MODEL_LIST (list));

	gda_data_model_row_removed (GDA_DATA_MODEL (list), row);
}

static void
gda_data_model_list_init (GdaDataModelList *list, GdaDataModelListClass *klass)
{
	/* allocate internal structure */
	list->priv = g_new0 (GdaDataModelListPrivate, 1);
	list->priv->rows = (GdaDataModelArray *) gda_data_model_array_new (1);

	g_signal_connect (G_OBJECT (list->priv->rows), "changed",
			  G_CALLBACK (proxy_changed_cb), list);
	g_signal_connect (G_OBJECT (list->priv->rows), "row_inserted",
			  G_CALLBACK (proxy_row_inserted_cb), list);
	g_signal_connect (G_OBJECT (list->priv->rows), "row_updated",
			  G_CALLBACK (proxy_row_updated_cb), list);
	g_signal_connect (G_OBJECT (list->priv->rows), "row_removed",
			  G_CALLBACK (proxy_row_removed_cb), list);
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
 *
 * Returns: a newly allocated #GdaDataModel.
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
 * @list: a list of strings.
 *
 * Returns: a newly allocated #GdaDataModel which contains a set of
 * #GdaValue according to the given @list.
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
 * @model: the #GdaDataModelList which is gonna hold the row.
 * @value: a #GdaValue which will be used to fill the row.
 *
 * Inserts a row in the @model, using @value.
 *
 * Returns: the #GdaRow which has been inserted, or %NULL on failure.
 */
GdaRow *
gda_data_model_list_append_value (GdaDataModelList *model, const GdaValue *value)
{
	GList *values;
	GdaRow *row;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_LIST (model), NULL);
	g_return_val_if_fail (value != NULL, NULL);

	values = g_list_append (NULL, (GdaValue *) value);
	row = gda_data_model_append_values (GDA_DATA_MODEL (model->priv->rows), values);
	if (row)
		gda_data_model_changed (GDA_DATA_MODEL (model));

	return row;
}
