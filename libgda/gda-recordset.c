/* GDA libary
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *	Michael Lausch <michael@lausch.at>
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

#include <config.h>
#include <libgda/gda-recordset.h>
#include <libgda/gda-row.h>

#define PARENT_TYPE GDA_TYPE_DATA_MODEL_ARRAY

struct _GdaRecordsetPrivate {
	GdaConnection *cnc;
	GdaRecordsetFetchFunc fetch_func;
	GdaRecordsetDescribeFunc describe_func;
	gpointer user_data;
	GdaRowAttributes *attributes;

	gint row_count;
	gboolean load_completed;
	gchar *cmd_text;
	GdaCommandType cmd_type;
};

static void gda_recordset_class_init (GdaRecordsetClass *klass);
static void gda_recordset_init       (GdaRecordset *recset, GdaRecordsetClass *klass);
static void gda_recordset_finalize   (GObject *object);

enum {
	LAST_SIGNAL
};

//static gint gda_recordset_signals[LAST_SIGNAL] = { 0, };
static GObjectClass *parent_class = NULL;

/*
 * GdaRecordset class implementation
 */

static gint
gda_recordset_get_n_rows (GdaDataModel *model)
{
	GdaRecordset *recset = (GdaRecordset *) model;

	g_return_val_if_fail (GDA_IS_RECORDSET (recset), -1);

	/* FIXME: implement */

	return 0;
}

static gint
gda_recordset_get_n_columns (GdaDataModel *model)
{
	GdaRecordset *recset = (GdaRecordset *) model;

	g_return_val_if_fail (GDA_IS_RECORDSET (recset), -1);

	/* FIXME: implement */

	return 0;
}

static GdaFieldAttributes *
gda_recordset_describe_column (GdaDataModel *model, gint col)
{
	GdaRecordset *recset = (GdaRecordset *) model;

	g_return_val_if_fail (GDA_IS_RECORDSET (recset), NULL);
	g_return_val_if_fail (recset->priv->attributes != NULL, NULL);

	return NULL; /* FIXME: */
}

static const GdaValue *
gda_recordset_get_value_at (GdaDataModel *model, gint col, gint row)
{
	gint fetched_count;
	gint total_count;
	gint i;
	GdaRecordset *recset = (GdaRecordset *) model;

	g_return_val_if_fail (GDA_IS_RECORDSET (recset), NULL);
	g_return_val_if_fail (parent_class != NULL, NULL);

	fetched_count = GDA_DATA_MODEL_CLASS (parent_class)->get_n_rows (model);
	if (row < fetched_count)
		return GDA_DATA_MODEL_CLASS (parent_class)->get_value_at (model, col, row);

	/* it's not been fetched, so fetch data */
	total_count = gda_data_model_get_n_rows (model);
	if (row >= total_count)
		return NULL;

	for (i = fetched_count; i <= row; i++) {
		GdaRow *row_data;

		row_data = recset->priv->fetch_func (recset, i, recset->priv->user_data);
		if (row_data) {
			GList *value_list = NULL;
			GList *l;

			for (l = row_data; l != NULL; l = l->next) {
				GdaValue *value;
				GdaField *field;

				field = gda_row_get_field (row_data, i);
				value = gda_field_get_value (field);
				value_list = g_list_append (value_list, value);
			}

			gda_data_model_array_append_row (GDA_DATA_MODEL_ARRAY (recset),
							 (const GList *) value_list);

			gda_row_free (row_data);
			g_list_free (value_list);
		}
	}

	return GDA_DATA_MODEL_CLASS (parent_class)->get_value_at (model, col, row);
}

/**
 * gda_recordset_get_type
 *
 * Registers the GType for the #GdaRecordset class.
 *
 * Returns: the identifier for the #GdaRecordset class. 
 */
GType
gda_recordset_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaRecordsetClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_recordset_class_init,
			NULL,
			NULL,
			sizeof (GdaRecordset),
			0,
			(GInstanceInitFunc) gda_recordset_init
		};
		type = g_type_register_static (PARENT_TYPE, "GdaRecordset", &info, 0);
	}
	return type;
}

static void
gda_recordset_class_init (GdaRecordsetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaDataModelClass *model_class = GDA_DATA_MODEL_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_recordset_finalize;
	model_class->get_n_rows = gda_recordset_get_n_rows;
	model_class->get_n_columns = gda_recordset_get_n_columns;
	model_class->describe_column = gda_recordset_describe_column;
	model_class->get_value_at = gda_recordset_get_value_at;
}

static void
gda_recordset_init (GdaRecordset *recset, GdaRecordsetClass *klass)
{
	g_return_if_fail (GDA_IS_RECORDSET (recset));

	recset->priv = g_new0 (GdaRecordsetPrivate, 1);

	recset->priv->cnc = NULL;
	recset->priv->fetch_func = NULL;
	recset->priv->describe_func = NULL;
	recset->priv->attributes = NULL;
	recset->priv->row_count = -1;
	recset->priv->load_completed = FALSE;
	recset->priv->cmd_text = NULL;
	recset->priv->cmd_type = GDA_COMMAND_TYPE_INVALID;
}

static void
gda_recordset_finalize (GObject * object)
{
	GdaRecordset *recset = (GdaRecordset *) object;

	g_return_if_fail (GDA_IS_RECORDSET (recset));

	/* free memory */
	if (recset->priv->attributes != NULL) {
		gda_row_attributes_free (recset->priv->attributes);
		recset->priv->attributes = NULL;
	}

	if (recset->priv->cmd_text != NULL) {
		g_free (recset->priv->cmd_text);
		recset->priv->cmd_text = NULL;
	}

	g_free (recset->priv);
	recset->priv = NULL;

	parent_class->finalize (object);
}

/**
 * gda_recordset_new
 * @cnc: a #GdaConnection to be associated with the new recordset.
 * @fetch_func: Function to be called for fetching data.
 * @desc_func: Function to be called for describing the data.
 * @user_data: Data to be passed to the above functions.
 *
 * Allocates space for a new recordset.
 *
 * Returns: the allocated recordset object
 */
GdaRecordset *
gda_recordset_new (GdaConnection *cnc,
		   GdaRecordsetFetchFunc fetch_func,
		   GdaRecordsetDescribeFunc desc_func,
		   gpointer user_data)
{
	GdaRecordset *recset;
	gint i;
	gint length;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (fetch_func != NULL, NULL);
	g_return_val_if_fail (desc_func != NULL, NULL);

	recset = g_object_new (GDA_TYPE_RECORDSET, NULL);

	recset->priv->cnc = cnc;
	recset->priv->fetch_func = fetch_func;
	recset->priv->describe_func = desc_func;
	recset->priv->user_data = user_data;

	/* get recordset description */
	recset->priv->attributes = recset->priv->describe_func (recset, recset->priv->user_data);
	length = gda_row_attributes_get_length (recset->priv->attributes);

	gda_data_model_array_set_n_columns (GDA_DATA_MODEL_ARRAY (recset), length);
	for (i = 0; i < length; i++) {
		GdaFieldAttributes *fa;

		fa = gda_row_attributes_get_field (recset->priv->attributes, i);
		gda_data_model_set_column_title (
			GDA_DATA_MODEL (recset), i,
			gda_field_attributes_get_name (fa));
	}

	return recset;
}

/**
 * gda_recordset_get_command_text
 * @recset: a #GdaRecordset.
 *
 * Get the text of command that generated this recordset.
 *
 * Returns: a string with the command issued.
 */
const gchar *
gda_recordset_get_command_text (GdaRecordset *recset)
{
	g_return_val_if_fail (GDA_IS_RECORDSET (recset), NULL);
	return recset->priv->cmd_text;
}

/**
 * gda_recordset_set_command_text
 */
void
gda_recordset_set_command_text (GdaRecordset *recset, const gchar *txt)
{
	g_return_if_fail (GDA_IS_RECORDSET (recset));
	g_return_if_fail (txt != NULL);

	if (recset->priv->cmd_text)
		g_free (recset->priv->cmd_text);
	recset->priv->cmd_text = g_strdup (txt);
}

/**
 * gda_recordset_get_command_type
 * @recset: a #GdaRecordset.
 *
 * Get the type of command that generated this recordset.
 *
 * Returns: a #GdaCommandType.
 */
GdaCommandType
gda_recordset_get_command_type (GdaRecordset *recset)
{
	g_return_val_if_fail (GDA_IS_RECORDSET (recset), GDA_COMMAND_TYPE_INVALID);
	return recset->priv->cmd_type;

}

/**
 * gda_recordset_set_command_type
 */
void
gda_recordset_set_command_type (GdaRecordset *recset, GdaCommandType type)
{
	g_return_if_fail (GDA_IS_RECORDSET (recset));
	recset->priv->cmd_type = type;
}
