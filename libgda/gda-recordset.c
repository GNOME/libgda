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
#include <glib-object.h>
#include <bonobo/bonobo-exception.h>

#define PARENT_TYPE GDA_TYPE_DATA_MODEL_ARRAY

struct _GdaRecordsetPrivate {
	GdaConnection *cnc;
	GNOME_Database_Recordset corba_recset;
	GNOME_Database_RowAttributes *attributes;
	guint timeout_id;

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

	if (recset->priv->row_count == -1) {
		CORBA_Environment ev;

		CORBA_exception_init (&ev);
		recset->priv->row_count = GNOME_Database_Recordset_getRowCount (
			recset->priv->corba_recset, &ev);
		if (BONOBO_EX (&ev)) {
			gda_connection_add_error_list (
				recset->priv->cnc, gda_error_list_from_exception (&ev));
			recset->priv->row_count = -1;
		}

		CORBA_exception_free (&ev);
	}

	return recset->priv->row_count;
}

static gint
gda_recordset_get_n_columns (GdaDataModel *model)
{
	GdaRecordset *recset = (GdaRecordset *) model;

	g_return_val_if_fail (GDA_IS_RECORDSET (recset), -1);
	g_return_val_if_fail (recset->priv->attributes != NULL, -1);

	return recset->priv->attributes->_length;
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
		CORBA_Environment ev;
		gint n;

		CORBA_exception_init (&ev);

		row_data = GNOME_Database_Recordset_fetch (recset->priv->corba_recset, &ev);
		if (BONOBO_EX (&ev)) {
			gda_connection_add_error_list (
				recset->priv->cnc, gda_error_list_from_exception (&ev));
			CORBA_exception_free (&ev);
			return NULL;
		}

		CORBA_exception_free (&ev);

		if (row_data) {
			GList *value_list = NULL;

			for (n = 0; n < row_data->_length; n++) {
				GdaValue *value;
				GdaField *field;

				field = gda_row_get_field (row_data, n);
				value = gda_field_get_value (field);
				value_list = g_list_append (value_list, value);
			}

			gda_data_model_array_append_row (GDA_DATA_MODEL_ARRAY (recset),
							 (const GList *) value_list);

			gda_row_free (row_data);
			g_list_free (value_list);

			/* move to next row */
			CORBA_exception_init (&ev);
			GNOME_Database_Recordset_moveNext (recset->priv->corba_recset, &ev);
			CORBA_exception_free (&ev);
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
	model_class->get_value_at = gda_recordset_get_value_at;
}

static void
gda_recordset_init (GdaRecordset *recset, GdaRecordsetClass *klass)
{
	g_return_if_fail (GDA_IS_RECORDSET (recset));

	recset->priv = g_new0 (GdaRecordsetPrivate, 1);

	recset->priv->cnc = NULL;
	recset->priv->corba_recset = CORBA_OBJECT_NIL;
	recset->priv->attributes = NULL;
	recset->priv->timeout_id = -1;
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

	/* remove timeout handler */
	if (recset->priv->timeout_id != -1) {
		g_source_remove (recset->priv->timeout_id);
		recset->priv->timeout_id = -1;
	}

	/* free memory */
	if (recset->priv->corba_recset != CORBA_OBJECT_NIL) {
		CORBA_Environment ev;

		CORBA_exception_init (&ev);
		CORBA_Object_release (recset->priv->corba_recset, &ev);
		CORBA_exception_free (&ev);
	}
	if (recset->priv->attributes != NULL)
		CORBA_free (recset->priv->attributes);

	if (recset->priv->cmd_text != NULL)
		CORBA_free (recset->priv->cmd_text);

	g_free (recset->priv);
	recset->priv = NULL;

	parent_class->finalize (object);
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
gda_recordset_get_command_text (GdaRecordset *recset) {
	g_return_val_if_fail (GDA_IS_RECORDSET (recset), NULL);
	return recset->priv->cmd_text;
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
gda_recordset_get_command_type (GdaRecordset *recset) {
	g_return_val_if_fail (GDA_IS_RECORDSET (recset), GDA_COMMAND_TYPE_INVALID);
	return recset->priv->cmd_type;

}

/**
 * gda_recordset_new
 * @cnc: a #GdaConnection to be associated with the new recordset.
 * @corba_recset: a GNOME_Database_Recordset object
 *
 * Allocates space for a new recordset.
 *
 * Returns: the allocated recordset object
 */
GdaRecordset *
gda_recordset_new (GdaConnection *cnc, GNOME_Database_Recordset corba_recset)
{
	GdaRecordset *recset;
	CORBA_Environment ev;
	gint i;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (corba_recset != CORBA_OBJECT_NIL, NULL);

	recset = g_object_new (GDA_TYPE_RECORDSET, NULL);

	recset->priv->cnc = cnc;
	recset->priv->corba_recset = corba_recset;

	/* get recordset description */
	CORBA_exception_init (&ev);

	recset->priv->attributes = GNOME_Database_Recordset_describe (
		recset->priv->corba_recset, &ev);
	if (BONOBO_EX (&ev)) {
		gda_connection_add_error_list (
			cnc, gda_error_list_from_exception (&ev));
		CORBA_exception_free (&ev);
		g_object_unref (G_OBJECT (recset));
		return NULL;
	}

	CORBA_exception_free (&ev);

	gda_data_model_array_set_n_columns (GDA_DATA_MODEL_ARRAY (recset),
					    recset->priv->attributes->_length);

	for (i = 0; i < recset->priv->attributes->_length; i++) {
		gda_data_model_set_column_title (
			GDA_DATA_MODEL (recset), i,
			gda_field_attributes_get_name (
				&recset->priv->attributes->_buffer[i]));
	}

	/* retrieve all data from the underlying recordset */
	CORBA_exception_init (&ev);
	if (!GNOME_Database_Recordset_moveFirst (recset->priv->corba_recset, &ev)) {
		if (BONOBO_EX (&ev)) {
			gda_connection_add_error_list (
				cnc, gda_error_list_from_exception (&ev));
			CORBA_exception_free (&ev);
			g_object_unref (G_OBJECT (recset));
			return NULL;
		}

		recset->priv->load_completed = TRUE;
	}

	recset->priv->cmd_text = GNOME_Database_Recordset_getCommandText (
					recset->priv->corba_recset, &ev);

	recset->priv->cmd_type = GNOME_Database_Recordset_getCommandType (
					recset->priv->corba_recset, &ev);

	return recset;
}

