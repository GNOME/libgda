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

#include "gda-data-model-array.h"

#define PARENT_TYPE GDA_TYPE_DATA_MODEL

struct _GdaDataModelArrayPrivate {
};

static void gda_data_model_array_class_init (GdaDataModelArrayClass *klass);
static void gda_data_model_array_init       (GdaDataModelArray *model,
					     GdaDataModelArrayClass *klass);
static void gda_data_model_array_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

/*
 * GdaDataModelArray class implementation
 */

static void
gda_data_model_array_class_init (GdaDataModelClass *klass)
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
}

static void
gda_data_model_array_finalize (GObject *object)
{
	GdaDataModelArray *model = (GdaDataModelArray *) object;

	g_return_if_fail (GDA_IS_DATA_MODEL_ARRAY (model));

	/* free memory */
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
		type = g_type_register_static (G_TYPE_OBJECT,
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
	return model;
}
