/* 
 * GDA common library
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

#define PARENT_TYPE G_TYPE_OBJECT
#define CLASS(model) (GDA_DATA_MODEL_CLASS (G_OBJECT_GET_CLASS (model)))

struct _GdaDataModelPrivate {
	GList *columns;
};

static void gda_data_model_class_init (GdaDataModelClass *klass);
static void gda_data_model_init       (GdaDataModel *model, GdaDataModelClass *klass);
static void gda_data_model_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

/*
 * GdaDataModel class implementation
 */

static void
gda_data_model_class_init (GdaDataModelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_data_model_finalize;
	klass->get_value_at = NULL;
}

static void
gda_data_model_init (GdaDataModel *model, GdaDataModelClass *klass)
{
	g_return_if_fail (GDA_IS_DATA_MODEL (model));

	model->priv = g_new0 (GdaDataModelPrivate, 1);
	model->priv->columns = NULL;
}

static void
gda_data_model_finalize (GObject *object)
{
	GdaDataModel *model = (GdaDataModel *) object;

	/* free memory */
	g_list_foreach (model->priv->columns, , );
	g_list_free (model->priv->columns);

	g_free (model->priv);
	model->priv = NULL;

	/* chain to parent class */
	parent_class->finalize;
}

GType
gda_data_model_get_type (void)
{
	static GType type = 0;

	if (!type) {
		if (type == 0) {
			static GTypeInfo info = {
				sizeof (GdaDataModelClass),
				(GBaseInitFunc) NULL,
				(GBaseFinalizeFunc) NULL,
				(GClassInitFunc) gda_data_model_class_init,
				NULL, NULL,
				sizeof (GdaDataModel),
				0,
				(GInstanceInitFunc) gda_data_model_init
			};
			type = g_type_register_static (PARENT_TYPE, "GdaDataModel", &info, 0);
		}
	}

	return type;
}

/**
 * gda_data_model_get_n_columns
 * @model: a #GdaDataModel object.
 *
 * Return the number of columns in the given data model.
 */
gint
gda_data_model_get_n_columns (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), -1);

	return g_list_length (model->priv->columns);
}

/**
 * gda_data_model_add_column
 */
void
gda_data_model_add_column (GdaDataModel *model, GdaFieldAttributes *attr)
{
	g_return_if_fail (GDA_IS_DATA_MODEL (model));
	g_return_if_fail (attr != NULL);
}

/**
 * gda_data_model_insert_column
 */
void
gda_data_model_insert_column (GdaDataModel *model, GdaFieldAttributes *attr, gint pos)
{
}

/**
 * gda_data_model_remove_column_by_pos
 */
void
gda_data_model_remove_column_by_pos (GdaDataModel *model, gint pos)
{
}

/**
 * gda_data_model_remove_column_by_name
 */
void
gda_data_model_remove_column_by_name (GdaDataModel *model, const gchar *name)
{
}

/**
 * gda_data_model_describe_column_by_pos
 */
GdaFieldAttributes *
gda_data_model_describe_column_by_pos (GdaDataModel *model, gint pos)
{
}

/**
 * gda_data_model_describe_column_by_name
 */
GdaFieldAttributes *
gda_data_model_describe_column_by_name (GdaDataModel *model, const gchar *name)
{
}

/**
 * 
/**
 * gda_data_model_get_value_at
 * @model: a #GdaDataModel object.
 * @col: column number.
 * @row: row number.
 *
 * Retrieve the data stored in the given position (identified by
 * the @col and @row parameters) on a data model.
 *
 * This is the main function for accessing data in a model.
 *
 * Returns: a #GdaValue containing the value stored in the given
 * position, or NULL on error (out-of-bound position, etc).
 */
GdaValue *
gda_data_model_get_value_at (GdaDataModel *model, gint col, gint row)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);
	g_return_val_if_fail (col > 0, NULL);
	g_return_val_if_fail (col < g_list_length (model->priv->columns), NULL);
	g_return_val_if_fail (row > 0, NULL);
	g_return_val_if_fail (CLASS (model)->get_value_at != NULL, NULL);

	return CLASS (model)->get_value_at (model, col, row);
}
