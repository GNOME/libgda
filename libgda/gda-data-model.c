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
	gboolean notify_changes;
};

static void gda_data_model_class_init (GdaDataModelClass *klass);
static void gda_data_model_init       (GdaDataModel *model, GdaDataModelClass *klass);
static void gda_data_model_finalize   (GObject *object);

enum {
	CHANGED,
	LAST_SIGNAL
};

static guint gda_data_model_signals[LAST_SIGNAL];
static GObjectClass *parent_class = NULL;

/*
 * GdaDataModel class implementation
 */

static void
gda_data_model_class_init (GdaDataModelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	gda_data_model_signals[CHANGED] =
		g_signal_new ("changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GdaDataModelClass, changed),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);

	object_class->finalize = gda_data_model_finalize;
	klass->get_n_rows = NULL;
	klass->get_n_columns = NULL;
	klass->get_column_title = NULL;
	klass->get_value_at = NULL;
}

static void
gda_data_model_init (GdaDataModel *model, GdaDataModelClass *klass)
{
	g_return_if_fail (GDA_IS_DATA_MODEL (model));

	model->priv = g_new0 (GdaDataModelPrivate, 1);
	model->priv->notify_changes = TRUE;
}

static void
gda_data_model_finalize (GObject *object)
{
	GdaDataModel *model = (GdaDataModel *) object;

	g_return_if_fail (GDA_IS_DATA_MODEL (model));

	/* free memory */
	g_free (model->priv);
	model->priv = NULL;

	/* chain to parent class */
	parent_class->finalize (object);
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
 * gda_data_model_changed
 * @model: a #GdaDataModel object.
 *
 * Notify listeners of the given data model object of changes
 * in the underlying data. Listeners usually will connect
 * themselves to the "changed" signal in the #GdaDataModel
 * class, thus being notified of any new data being appended
 * or removed from the data model.
 */
void
gda_data_model_changed (GdaDataModel *model)
{
	g_return_if_fail (GDA_IS_DATA_MODEL (model));

	if (model->priv->notify_changes) {
		g_signal_emit (G_OBJECT (model),
			       gda_data_model_signals[CHANGED],
			       0);
	}
}

/**
 * gda_data_model_freeze
 * @model: a #GdaDataModel object.
 *
 * Disable notifications of changes on the given data model. To
 * re-enable notifications again, you should call the
 * #gda_data_model_thaw function.
 */
void
gda_data_model_freeze (GdaDataModel *model)
{
	g_return_if_fail (GDA_IS_DATA_MODEL (model));
	model->priv->notify_changes = FALSE;
}

/**
 * gda_data_model_thaw
 * @model: a #GdaDataModel object.
 *
 * Re-enable notifications of changes on the given data model.
 */
void
gda_data_model_thaw (GdaDataModel *model)
{
}

/**
 * gda_data_model_get_n_rows
 * @model: a #GdaDataModel object.
 *
 * Return the number of rows in the given data model.
 */
gint
gda_data_model_get_n_rows (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), -1);
	g_return_val_if_fail (CLASS (model)->get_n_rows != NULL, -1);

	return CLASS (model)->get_n_rows (model);
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
	g_return_val_if_fail (CLASS (model)->get_n_columns != NULL, -1);

	return CLASS (model)->get_n_columns (model);
}

/**
 * gda_data_model_get_column_title
 * @model: a #GdaDataModel object.
 * @col: column number.
 *
 * Return the title for the given column in a data model object.
 */
const gchar *
gda_data_model_get_column_title (GdaDataModel *model, gint col)
{
	return "FIXME";
}

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
 * position, or NULL on error (out-of-bound position, etc). The
 * returned value should be freed with #gda_value_free
 */
const GdaValue *
gda_data_model_get_value_at (GdaDataModel *model, gint col, gint row)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);
	g_return_val_if_fail (CLASS (model)->get_value_at != NULL, NULL);

	return CLASS (model)->get_value_at (model, col, row);
}
