/* GDA server library
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
#include <libgda/gda-server-recordset-model.h>

#define PARENT_TYPE GDA_TYPE_SERVER_RECORDSET

struct _GdaServerRecordsetModelPrivate {
	GdaDataModel *model;
	GdaRowAttributes *row_attrs;
};

static void gda_server_recordset_model_class_init (GdaServerRecordsetModelClass *klass);
static void gda_server_recordset_model_init       (GdaServerRecordsetModel *recset,
						   GdaServerRecordsetModelClass *klass);
static void gda_server_recordset_model_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

/*
 * Private functions
 */

static GdaRow *
model_fetch_func (GdaServerRecordset *recset, gulong rownum)
{
	gint row_count;
	gint col_count;
	GdaRow *row;
	gint n;

	g_return_val_if_fail (GDA_IS_SERVER_RECORDSET_MODEL (recset), NULL);

	row_count = gda_data_model_get_n_rows (
		GDA_SERVER_RECORDSET_MODEL (recset)->priv->model);
	col_count = gda_data_model_get_n_columns (
		GDA_SERVER_RECORDSET_MODEL (recset)->priv->model);
	if (rownum < 0 || rownum >= row_count)
		return NULL;

	/* convert the given row in the data model into a GdaRow */
	row = gda_row_new (col_count);
	for (n = 0; n < col_count; n++) {
		GdaField *field;
		GdaFieldAttributes *fa;
		const GdaValue *value;

		value = gda_data_model_get_value_at (
			GDA_SERVER_RECORDSET_MODEL (recset)->priv->model, n, rownum);
		fa = gda_row_attributes_get_field (
			GDA_SERVER_RECORDSET_MODEL (recset)->priv->row_attrs, n);
		field = gda_row_get_field (row, n);

		g_assert (value != NULL && fa != NULL && field != NULL);

		gda_field_set_actual_size (field, gda_field_attributes_get_defined_size (fa));
		gda_field_set_defined_size (field, gda_field_attributes_get_defined_size (fa));
		gda_field_set_name (field, gda_field_attributes_get_name (fa));
		gda_field_set_scale (field, gda_field_attributes_get_scale (fa));
		gda_field_set_gdatype (field, gda_field_attributes_get_gdatype (fa));

		gda_field_set_value (field, (const GdaValue *) value);
	}

	return row;
}

static GdaRowAttributes *
model_desc_func (GdaServerRecordset *recset)
{
	GdaRowAttributes *attrs;
	gint n;

	g_return_val_if_fail (GDA_IS_SERVER_RECORDSET_MODEL (recset), NULL);

	attrs = gda_row_attributes_new (
		gda_data_model_get_n_columns (GDA_SERVER_RECORDSET_MODEL (recset)->priv->model));

	for (n = 0; n < attrs->_length; n++) {
		GdaFieldAttributes *fa_src;
		GdaFieldAttributes *fa_dest;

		fa_src = gda_row_attributes_get_field (
			GDA_SERVER_RECORDSET_MODEL (recset)->priv->row_attrs, n);
		fa_dest = gda_row_attributes_get_field (attrs, n);

		gda_field_attributes_set_defined_size (fa_dest, gda_field_attributes_get_defined_size (fa_src));
		gda_field_attributes_set_name (fa_dest, gda_field_attributes_get_name (fa_src));
		gda_field_attributes_set_scale (fa_dest, gda_field_attributes_get_scale (fa_src));
		gda_field_attributes_set_gdatype (fa_dest, gda_field_attributes_get_gdatype (fa_src));
	}
 
	return attrs;
}

/*
 * GdaServerRecordsetModel class implementation
 */

static void
gda_server_recordset_model_class_init (GdaServerRecordsetModelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_server_recordset_model_finalize;
}

static void
gda_server_recordset_model_init (GdaServerRecordsetModel *recset,
				 GdaServerRecordsetModelClass *klass)
{
	g_return_if_fail (GDA_IS_SERVER_RECORDSET_MODEL (recset));

	/* allocate private structure */
	recset->priv = g_new0 (GdaServerRecordsetModelPrivate, 1);
	recset->priv->model = NULL;

	/* initialize GdaServerRecordset-related stuff */
	gda_server_recordset_set_fetch_func (GDA_SERVER_RECORDSET (recset), model_fetch_func);
	gda_server_recordset_set_describe_func (GDA_SERVER_RECORDSET (recset), model_desc_func);
}

static void
gda_server_recordset_model_finalize (GObject *object)
{
	GdaServerRecordsetModel *recset = (GdaServerRecordsetModel *) object;

	g_return_if_fail (GDA_IS_SERVER_RECORDSET_MODEL (recset));

	/* free memory */
	if (GDA_IS_DATA_MODEL (recset->priv->model)) {
		g_object_unref (G_OBJECT (recset->priv->model));
		recset->priv->model = NULL;
	}

	if (recset->priv->row_attrs != NULL) {
		gda_row_attributes_free (recset->priv->row_attrs);
		recset->priv->row_attrs = NULL;
	}

	g_free (recset->priv);
	recset->priv = NULL;

	/* chain to parent class */
	parent_class->finalize (object);
}

GType
gda_server_recordset_model_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static GTypeInfo info = {
			sizeof (GdaServerRecordsetModelClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_server_recordset_model_class_init,
			NULL, NULL,
			sizeof (GdaServerRecordsetModel),
			0,
			(GInstanceInitFunc) gda_server_recordset_model_init
		};
		type = g_type_register_static (PARENT_TYPE,
					       "GdaServerRecordsetModel",
					       &info, 0);
	}

	return type;
}

/**
 * gda_server_recordset_model_new
 * @n_cols: number of columns in the recordset.
 *
 * Create a new #GdaServerRecordsetModel object, which allows you
 * to create a #GdaServerRecordset in a #GdaDataModel's mode. That
 * is, the #GdaServerRecordsetModel provides all the stuff needed for
 * you to provide it with data, and forget about fetch and describe
 * callbacks. This is specially useful for quick databases, or no-DB
 * data sources, such as plain text files, XML files, etc.
 *
 * Returns: the newly created object.
 */
GdaServerRecordset *
gda_server_recordset_model_new (GdaServerConnection *cnc, gint n_cols)
{
	GdaServerRecordsetModel *recset;

	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), NULL);

	recset = g_object_new (GDA_TYPE_SERVER_RECORDSET_MODEL, NULL);

	/* initialize object */
	gda_server_recordset_set_connection (GDA_SERVER_RECORDSET (recset), cnc);
	recset->priv->model = gda_data_model_array_new (n_cols);
	recset->priv->row_attrs = gda_row_attributes_new (n_cols);

	return GDA_SERVER_RECORDSET (recset);
}

/**
 * gda_server_recordset_model_set_field_defined_size
 */
void
gda_server_recordset_model_set_field_defined_size (GdaServerRecordsetModel *recset,
						   gint col,
						   glong size)
{
	GdaFieldAttributes *fa;

	g_return_if_fail (GDA_IS_SERVER_RECORDSET_MODEL (recset));

	fa = gda_row_attributes_get_field (recset->priv->row_attrs, col);
	if (fa != NULL)
		gda_field_attributes_set_defined_size (fa, size);
}

/**
 * gda_server_recordset_model_set_field_name
 */
void
gda_server_recordset_model_set_field_name (GdaServerRecordsetModel *recset,
					   gint col,
					   const gchar *name)
{
	GdaFieldAttributes *fa;

	g_return_if_fail (GDA_IS_SERVER_RECORDSET_MODEL (recset));

	gda_data_model_set_column_title (recset->priv->model, col, name);
	fa = gda_row_attributes_get_field (recset->priv->row_attrs, col);
	if (fa != NULL)
		gda_field_attributes_set_name (fa, name);
}

/**
 * gda_server_recordset_model_set_field_scale
 */
void
gda_server_recordset_model_set_field_scale (GdaServerRecordsetModel *recset,
					    gint col,
					    glong scale)
{
	GdaFieldAttributes *fa;

	g_return_if_fail (GDA_IS_SERVER_RECORDSET_MODEL (recset));

	fa = gda_row_attributes_get_field (recset->priv->row_attrs, col);
	if (fa != NULL)
		gda_field_attributes_set_scale (fa, scale);
}

/**
 * gda_server_recordset_model_set_field_gdatype
 */
void
gda_server_recordset_model_set_field_gdatype (GdaServerRecordsetModel *recset,
					      gint col,
					      GdaType type)
{
	GdaFieldAttributes *fa;

	g_return_if_fail (GDA_IS_SERVER_RECORDSET_MODEL (recset));

	fa = gda_row_attributes_get_field (recset->priv->row_attrs, col);
	if (fa != NULL)
		gda_field_attributes_set_gdatype (fa, type);
}

/**
 * gda_server_recordset_model_append_row
 */
void
gda_server_recordset_model_append_row (GdaServerRecordsetModel *recset, const GList *values)
{
	g_return_if_fail (GDA_IS_SERVER_RECORDSET_MODEL (recset));
	gda_data_model_array_append_row (recset->priv->model, values);
}
