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

#include "gda-data-model-list.h"

struct _GdaDataModelListPrivate {
	GList *value_list;
};

static void gda_data_model_list_class_init (GdaDataModelListClass *klass);
static void gda_data_model_list_init       (GdaDataModelList *model,
					    GdaDataModelListClass *klass);
static void gda_data_model_list_finalize   (GObject *object);

/*
 * GdaDataModelList class implementation
 */

static void
gda_data_model_list_class_init (GdaDataModelListClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gda_data_model_list_finalize;
}

static void
gda_data_model_list_init (GdaDataModelList *list, GdaDataModelListClass *klass)
{
	/* allocate internal structure */
	list->priv = g_new0 (GdaDataModelListPrivate, 1);
}

static void
gda_data_model_list_finalize (GObject *object)
{
	GObjectClass *parent_class;
	GdaDataModelList *model = (GdaDataModelList *) object;

	g_return_if_fail (GDA_IS_DATA_MODEL_LIST (model));

	/* free memory */
	g_list_foreach (model->priv->value_list, gda_value_free, NULL);
	g_list_free (model->priv->value_list);

	g_free (model->priv);
	model->priv = NULL;

	parent_class = g_type_class_peek (GDA_TYPE_DATA_MODEL);
	if (parent_class && parent_class->finalize)
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
		type = g_type_register_static (G_TYPE_OBJECT,
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
 * gda_data_model_list_new_with_list
 */
GdaDataModel *
gda_data_model_list_new_with_list (GList *list)
{
}
