/* GDA SQLite provider
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *      Carlos Perelló Marín <carlos@gnome-db.org>
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
#include <libgda/gda-intl.h>
#include "gda-sqlite-recordset.h"

#define PARENT_TYPE GDA_TYPE_DATA_MODEL

static void gda_sqlite_recordset_class_init (GdaSqliteRecordsetClass *klass);
static void gda_sqlite_recordset_init       (GdaSqliteRecordset *recset,
					     GdaSqliteRecordsetClass *klass);
static void gda_sqlite_recordset_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

/*
 * Private functions
 */

static GdaRow *
fetch_row (GdaSqliteRecordset *recset, gulong rownum)
{
	GdaRow *row;
	gint field_count;
	gulong rowpos;
	gint row_count;
	gint i;

	g_return_val_if_fail (GDA_IS_SQLITE_RECORDSET (recset), NULL);

	if (!recset->drecset) {
		gda_connection_add_error_string (recset->cnc,
						 _("Invalid SQLITE handle"));
		return NULL;
	}

	/* move to the corresponding row */
	row_count = recset->drecset->nrows;
	field_count = recset->drecset->ncols;

	if (rownum < 0 || rownum >= row_count) {
		gda_connection_add_error_string (recset->cnc,
						 _("Row number out of range"));
		return NULL;
	}

	/* Jump the field names */
	rowpos = (field_count - 1) + (rownum * field_count);
	
	row = gda_row_new (field_count);

	for (i = 0; i < field_count; i++) {
		GdaValue *value;

		value = gda_row_get_value (row, i);

		/* FIXME: We should look for a way to know the real type data */
		gda_value_set_string (value, recset->drecset->data[rowpos +i]);
	}

	return row;
}

/*
 * GdaSqliteRecordset class implementation
 */

static gint
gda_sqlite_recordset_get_n_rows (GdaDataModel *model)
{
	GdaSqliteRecordset *recset = (GdaSqliteRecordset *) model;

	g_return_val_if_fail (GDA_IS_SQLITE_RECORDSET (recset), -1);
	g_return_val_if_fail (recset->drecset != NULL, -1);

	return recset->drecset->nrows;
}

static gint
gda_sqlite_recordset_get_n_columns (GdaDataModel *model)
{
	GdaSqliteRecordset *recset = (GdaSqliteRecordset *) model;

	g_return_val_if_fail (GDA_IS_SQLITE_RECORDSET (recset), -1);
	g_return_val_if_fail (recset->drecset != NULL, -1);

	return recset->drecset->ncols;
}

static GdaFieldAttributes *
gda_sqlite_recordset_describe_column (GdaDataModel *model, gint col)
{
	gint field_count;
	GdaFieldAttributes *attrs;
	GdaSqliteRecordset *recset = (GdaSqliteRecordset *) model;

	g_return_val_if_fail (GDA_IS_SQLITE_RECORDSET (recset), NULL);
	g_return_val_if_fail (recset->drecset != NULL, NULL);

	/* create the GdaFieldAttributes to be returned */
	field_count = recset->drecset->ncols;
	if (col >= field_count)
		return NULL;

	attrs = gda_field_attributes_new ();
	gda_field_attributes_set_name (attrs, recset->drecset->data[col]);
	gda_field_attributes_set_defined_size (attrs, strlen (recset->drecset->data[col]));
	gda_field_attributes_set_scale (attrs, 0);
	gda_field_attributes_set_gdatype (attrs, GDA_VALUE_TYPE_STRING);

	return attrs;
}

static const GdaValue *
gda_sqlite_recordset_get_value_at (GdaDataModel *model, gint col, gint row)
{
	gint row_count;
	gint field_count;
	gint i;
	GdaRow *fields = NULL;
	GdaValue *f;
	GdaSqliteRecordset *recset = (GdaSqliteRecordset *) model;

	g_return_val_if_fail (GDA_IS_SQLITE_RECORDSET (recset), NULL);
	g_return_val_if_fail (recset->drecset != NULL, NULL);

	row_count = recset->drecset->nrows;
	field_count = recset->drecset->ncols;

	if (col >= field_count)
		return NULL;
	if (row >= row_count)
		return NULL;
	if (row < recset->rows->len) {
		fields = g_ptr_array_index (recset->rows, row);
		if (!fields)
			return NULL;

		f = gda_row_get_value (fields, col);
		return (const GdaValue *) f;
	}

	gda_data_model_freeze (GDA_DATA_MODEL (recset));

	for (i = recset->rows->len; i <= row; i++) {
		fields = fetch_row (recset, i);
		if (!fields)
			return NULL;

		g_ptr_array_add (recset->rows, fields);
	}

	gda_data_model_thaw (GDA_DATA_MODEL (recset));

	f = gda_row_get_value (fields, col);
	return (const GdaValue *) f;
}

static gboolean
gda_sqlite_recordset_is_editable (GdaDataModel *model)
{
	GdaSqliteRecordset *recset = (GdaSqliteRecordset *) model;

	g_return_val_if_fail (GDA_IS_SQLITE_RECORDSET (recset), FALSE);
	return FALSE;
}

static const GdaRow *
gda_sqlite_recordset_append_row (GdaDataModel *model, const GList *values)
{
	return NULL;
}

static void
gda_sqlite_recordset_class_init (GdaSqliteRecordsetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaDataModelClass *model_class = GDA_DATA_MODEL_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_sqlite_recordset_finalize;
	model_class->get_n_rows = gda_sqlite_recordset_get_n_rows;
	model_class->get_n_columns = gda_sqlite_recordset_get_n_columns;
	model_class->describe_column = gda_sqlite_recordset_describe_column;
	model_class->get_value_at = gda_sqlite_recordset_get_value_at;
	model_class->is_editable = gda_sqlite_recordset_is_editable;
	model_class->append_row = gda_sqlite_recordset_append_row;
}

static void
gda_sqlite_recordset_init (GdaSqliteRecordset *recset, GdaSqliteRecordsetClass *klass)
{
	g_return_if_fail (GDA_IS_SQLITE_RECORDSET (recset));

	recset->cnc = NULL;
	recset->drecset = NULL;
	recset->rows = g_ptr_array_new ();
}

static void
gda_sqlite_recordset_finalize (GObject *object)
{
	GdaSqliteRecordset *recset = (GdaSqliteRecordset *) object;

	g_return_if_fail (GDA_IS_SQLITE_RECORDSET (recset));

	if (recset->drecset) {
		sqlite_free_table (recset->drecset->data);
		g_free (recset->drecset);
		recset->drecset = NULL;
	}

	while (recset->rows->len > 0) {
		GdaRow * row = (GdaRow *) g_ptr_array_index (recset->rows, 0);

		if (row != NULL)
			gda_row_free (row);
		g_ptr_array_remove_index (recset->rows, 0);
	}
	g_ptr_array_free (recset->rows, TRUE);
	recset->rows = NULL;

	parent_class->finalize (object);
}

GType
gda_sqlite_recordset_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaSqliteRecordsetClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_sqlite_recordset_class_init,
			NULL,
			NULL,
			sizeof (GdaSqliteRecordset),
			0,
			(GInstanceInitFunc) gda_sqlite_recordset_init
		};
		type = g_type_register_static (PARENT_TYPE, "GdaSqliteRecordset", &info, 0);
	}

	return type;
}

GdaSqliteRecordset *
gda_sqlite_recordset_new (GdaConnection *cnc, SQLITE_Recordset *srecset)
{
	GdaSqliteRecordset *recset;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (srecset != NULL, NULL);

	if (srecset->data) {
		recset = g_object_new (GDA_TYPE_SQLITE_RECORDSET, NULL);
		recset->drecset = srecset;

		return recset;
	}

	return NULL;
}
