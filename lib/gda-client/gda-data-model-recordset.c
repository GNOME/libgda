/* GDA client library
 * Copyright (C) 1998,1999 Michael Lausch
 * Copyright (C) 1999,2000,2001 Rodrigo Moya
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

#include "gda-data-model-recordset.h"

struct _GdaDataModelRecordsetPrivate {
	GdaRecordset *recset;
	guint idle_id;
	gulong last_move_result;
	gulong current_row;
};

static void gda_data_model_recordset_class_init (GdaDataModelRecordsetClass *klass);
static void gda_data_model_recordset_init       (GdaDataModelRecordset *model,
						 GdaDataModelRecordsetClass *klass);
static void gda_data_model_recordset_finalize   (GObject *object);

/*
 * Callbacks
 */

static gboolean
fill_data_model_cb (gpointer user_data)
{
	gint i;
	GdaDataModelRecordset *model = (GdaDataModelRecordset *) user_data;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_RECORDSET (model), FALSE);

	if (model->priv->last_move_result == GDA_RECORDSET_INVALID_POSITION
	    || gda_recordset_eof (model->priv->recset)) {
		model->priv->idle_id = -1;
		return FALSE;
	}

	/* add this row to the data model */
	gda_data_model_append_row (GDA_DATA_MODEL (model));
	for (i = 0; i < gda_recordset_rowsize (model->priv->recset); i++) {
		GdaField *field;

		field = gda_recordset_field_idx (model->priv->recset, i);
		if (GDA_IS_FIELD (field)) {
			gda_data_model_set_value_at (GDA_DATA_MODEL (model),
						     model->priv->current_row,
						     i,
						     gda_field_get_value (field));
		}
	}
	model->priv->current_row++;

	return TRUE;
}

/*
 * GdaDataModelRecordset class implementation
 */

static void
gda_data_model_recordset_class_init (GdaDataModelRecordsetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gda_data_model_recordset_finalize;
}

static void
gda_data_model_recordset_init (GdaDataModelRecordset *model,
			       GdaDataModelRecordsetClass *klass)
{
	/* allocate internal structure */
	model->priv = g_new0 (GdaDataModelRecordsetPrivate, 1);
}

static void
gda_data_model_recordset_finalize (GObject *object)
{
	GObjectClass *parent_class;
	GdaDataModelRecordset *model = (GdaDataModelRecordset *) object;

	g_return_if_fail (GDA_IS_DATA_MODEL_RECORDSET (model));

	/* free memory */
	g_idle_remove_by_data (model);
	if (GDA_IS_RECORDSET (model->priv->recset))
		gda_recordset_free (model->priv->recset);

	g_free (model->priv);
	model->priv = NULL;

	parent_class = g_type_class_peek (GDA_TYPE_DATA_MODEL);
	if (parent_class && parent_class->finalize)
		parent_class->finalize (object);
}

GType
gda_data_model_recordset_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaDataModelRecordsetClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_data_model_recordset_class_init,
			NULL,
			NULL,
			sizeof (GdaDataModelRecordset),
			0,
			(GInstanceInitFunc) gda_data_model_recordset_init
		};
		type = g_type_register_static (G_TYPE_OBJECT,
					       "GdaDataModelRecordset",
					       &info, 0);
	}
	return type;
}

/**
 * gda_data_model_recordset_new
 */
GdaDataModel *
gda_data_model_recordset_new (void)
{
	GdaDataModel *model;

	model = g_object_new (GDA_TYPE_DATA_MODEL_RECORDSET, NULL);
	return model;
}

/**
 * gda_data_model_recordset_new_with_recordset
 */
GdaDataModel *
gda_data_model_recordset_new_with_recordset (GdaRecordset *recset)
{
	GdaDataModel *model;

	model = g_object_new (GDA_TYPE_DATA_MODEL_RECORDSET, NULL);
	gda_data_model_recordset_set_recordset (GDA_DATA_MODEL_RECORDSET (model), recset);

	return model;
}

/**
 * gda_data_model_recordset_get_recordset
 */
GdaRecordset *
gda_data_model_recordset_get_recordset (GdaDataModelRecordset *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_RECORDSET (model), NULL);
	return model->priv->recset;
}

/**
 * gda_data_model_recordset_set_recordset
 */
void
gda_data_model_recordset_set_recordset (GdaDataModelRecordset *model,
					GdaRecordset *recset)
{
	gint i;

	g_return_if_fail (GDA_IS_DATA_MODEL_RECORDSET (model));
	g_return_if_fail (GDA_IS_RECORDSET (recset));

	/* free all data from before */
	g_idle_remove_by_data (model);
	if (GDA_IS_RECORDSET (model->priv->recset))
		gda_recordset_free (model->priv->recset);

	g_object_ref (G_OBJECT (recset));
	model->priv->recset = recset;

	/* add column definition */
	for (i = 0; i < gda_recordset_rowsize (model->priv->recset); i++) {
		GdaField *field;

		field = gda_recordset_field_idx (model->priv->recset, i);
		gda_data_model_append_column (GDA_DATA_MODEL (model),
					      gda_field_get_name (field));
	}

	/* prepare idle callback to fill GdaDataModel */
	model->priv->last_move_result = gda_recordset_move_first (model->priv->recset);
	model->priv->current_row = 0;
	model->priv->idle_id = g_idle_add ((GSourceFunc) fill_data_model_cb, model);
}
