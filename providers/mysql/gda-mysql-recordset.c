/* GDA MySQL provider
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *      Michael Lausch <michael@lausch.at>
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

#include <config.h>
#include <libgda/gda-intl.h>
#include <stdlib.h>
#include "gda-mysql.h"
#include "gda-mysql-recordset.h"

#define PARENT_TYPE GDA_TYPE_DATA_MODEL

static void gda_mysql_recordset_class_init (GdaMysqlRecordsetClass *klass);
static void gda_mysql_recordset_init       (GdaMysqlRecordset *recset,
					    GdaMysqlRecordsetClass *klass);
static void gda_mysql_recordset_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

/*
 * Private functions
 */

static GdaRow *
fetch_row (GdaMysqlRecordset *recset, gulong rownum)
{
	GdaRow *row;
	gint field_count;
	gint row_count;
	gint i;
	unsigned long *lengths;
	MYSQL_FIELD *mysql_fields;
	MYSQL_ROW mysql_row;

	g_return_val_if_fail (GDA_IS_MYSQL_RECORDSET (recset), NULL);

	if (!recset->mysql_res) {
		gda_connection_add_error_string (recset->cnc, _("Invalid MySQL handle"));
		return NULL;
	}

	/* move to the corresponding row */
	row_count = mysql_num_rows (recset->mysql_res);
	if (row_count == 0)
		return NULL;
	field_count = mysql_num_fields (recset->mysql_res);

	if (rownum < 0 || rownum >= row_count) {
		gda_connection_add_error_string (recset->cnc, _("Row number out of range"));
		return NULL;
	}

	mysql_data_seek (recset->mysql_res, rownum);
	row = gda_row_new (field_count);

	lengths = recset->mysql_res->lengths;
	mysql_fields = mysql_fetch_fields (recset->mysql_res);

	mysql_row = mysql_fetch_row (recset->mysql_res);
	if (!mysql_row)
		return NULL;

	for (i = 0; i < field_count; i++) {
		GdaField *field;
		gchar *thevalue;

		field = gda_row_get_field (row, i);
		gda_field_set_actual_size (field, lengths[i]);
		gda_field_set_defined_size (field, mysql_fields[i].max_length);
		gda_field_set_name (field, mysql_fields[i].name);
		gda_field_set_scale (field, mysql_fields[i].decimals);
		gda_field_set_gdatype (field, gda_mysql_type_to_gda (mysql_fields[i].type));

		thevalue = mysql_row[i];

		switch (mysql_fields[i].type) {
		case FIELD_TYPE_DECIMAL :
		case FIELD_TYPE_DOUBLE :
			gda_field_set_double_value (field, atof (thevalue));
			break;
		case FIELD_TYPE_FLOAT :
			gda_field_set_single_value (field, atof (thevalue));
			break;
		case FIELD_TYPE_LONG :
		case FIELD_TYPE_YEAR :
			gda_field_set_integer_value (field, atol (thevalue));
			break;
		case FIELD_TYPE_LONGLONG :
		case FIELD_TYPE_INT24 :
			gda_field_set_bigint_value (field, atoll (thevalue));
			break;
		case FIELD_TYPE_SHORT :
			gda_field_set_smallint_value (field, atoi (thevalue));
			break;
		case FIELD_TYPE_TINY :
			gda_field_set_tinyint_value (field, atoi (thevalue));
			break;
		case FIELD_TYPE_TINY_BLOB :
		case FIELD_TYPE_MEDIUM_BLOB :
		case FIELD_TYPE_LONG_BLOB :
		case FIELD_TYPE_BLOB :
			gda_field_set_binary_value (field, thevalue, lengths[i]);
			break;
		case FIELD_TYPE_VAR_STRING :
		case FIELD_TYPE_STRING :
			gda_field_set_string_value (field, thevalue ? thevalue : "");
			break;
		case FIELD_TYPE_DATE :
		case FIELD_TYPE_NULL :
		case FIELD_TYPE_NEWDATE :
		case FIELD_TYPE_ENUM :
		case FIELD_TYPE_TIMESTAMP :
		case FIELD_TYPE_DATETIME :
		case FIELD_TYPE_TIME :
		case FIELD_TYPE_SET : /* FIXME */
			gda_field_set_string_value (field, thevalue ? thevalue : "");
			break;
		default :
			gda_field_set_string_value (field, thevalue ? thevalue : "");
		}
	}

	return row;
}

/*
 * GdaMysqlRecordset class implementation
 */

static gint
gda_mysql_recordset_get_n_rows (GdaDataModel *model)
{
	GdaMysqlRecordset *recset = (GdaMysqlRecordset *) model;

	g_return_val_if_fail (GDA_IS_MYSQL_RECORDSET (recset), -1);
	return mysql_num_rows (recset->mysql_res);
}

static gint
gda_mysql_recordset_get_n_columns (GdaDataModel *model)
{
	GdaMysqlRecordset *recset = (GdaMysqlRecordset *) model;

	g_return_val_if_fail (GDA_IS_MYSQL_RECORDSET (recset), -1);
	return mysql_num_fields (recset->mysql_res);
}

static GdaFieldAttributes *
gda_mysql_recordset_describe_column (GdaDataModel *model, gint col)
{
	gint field_count;
	GdaFieldAttributes *attrs;
	MYSQL_FIELD *mysql_fields;
	GdaMysqlRecordset *recset = (GdaMysqlRecordset *) model;

	g_return_val_if_fail (GDA_IS_MYSQL_RECORDSET (recset), NULL);

	if (!recset->mysql_res) {
		gda_connection_add_error_string (recset->cnc, _("Invalid MySQL handle"));
		return NULL;
	}

	/* create the GdaFieldAttributes to be returned */
	field_count = mysql_num_fields (recset->mysql_res);
	if (col >= field_count)
		return NULL;

	attrs = gda_field_attributes_new ();

	mysql_fields = mysql_fetch_field (recset->mysql_res);

	gda_field_attributes_set_name (attrs, mysql_fields[col].name);
	gda_field_attributes_set_defined_size (attrs, mysql_fields[col].max_length);
	gda_field_attributes_set_scale (attrs, mysql_fields[col].decimals);
	gda_field_attributes_set_gdatype (attrs, gda_mysql_type_to_gda (mysql_fields[col].type));

	return attrs;
}

static const GdaValue *
gda_mysql_recordset_get_value_at (GdaDataModel *model, gint col, gint row)
{
	gint rows;
	gint cols;
	gint fetched_rows;
	gint i;
	GdaRow *fields = NULL;
	GdaField *f;
	GdaMysqlRecordset *recset = (GdaMysqlRecordset *) model;

	g_return_val_if_fail (GDA_IS_MYSQL_RECORDSET (recset), NULL);

	rows = mysql_num_rows (recset->mysql_res);
	cols = mysql_num_fields (recset->mysql_res);
	fetched_rows = recset->rows->len;

	if (col >= cols)
		return NULL;
	if (row >= rows)
		return NULL;
	if (row < fetched_rows) {
		fields = g_ptr_array_index (recset->rows, row);
		if (!fields)
			return NULL;

		f = gda_row_get_field (fields, col);
		return (const GdaValue *) gda_field_get_value (f);
	}

	gda_data_model_freeze (GDA_DATA_MODEL (recset));

	for (i = fetched_rows; i <= row; i++) {
		fields = fetch_row (recset, i);
		if (!fields)
			return NULL;

		g_ptr_array_add (recset->rows, fields);
	}

	gda_data_model_thaw (GDA_DATA_MODEL (recset));

	f = gda_row_get_field (fields, col);

	return (const GdaValue *) gda_field_get_value (f);
}

static gboolean
gda_mysql_recordset_is_editable (GdaDataModel *model)
{
	GdaMysqlRecordset *recset = (GdaMysqlRecordset *) model;

	g_return_val_if_fail (GDA_IS_MYSQL_RECORDSET (recset), FALSE);
	return FALSE;
}

static const GdaRow *
gda_mysql_recordset_append_row (GdaDataModel *model, const GList *values)
{
	return NULL;
}

static void
gda_mysql_recordset_class_init (GdaMysqlRecordsetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaDataModelClass *model_class = GDA_DATA_MODEL_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_mysql_recordset_finalize;
	model_class->get_n_rows = gda_mysql_recordset_get_n_rows;
	model_class->get_n_columns = gda_mysql_recordset_get_n_columns;
	model_class->describe_column = gda_mysql_recordset_describe_column;
	model_class->get_value_at = gda_mysql_recordset_get_value_at;
	model_class->is_editable = gda_mysql_recordset_is_editable;
	model_class->append_row = gda_mysql_recordset_append_row;
}

static void
gda_mysql_recordset_init (GdaMysqlRecordset *recset, GdaMysqlRecordsetClass *klass)
{
	g_return_if_fail (GDA_IS_MYSQL_RECORDSET (recset));

	recset->cnc = NULL;
	recset->mysql_res = NULL;
	recset->rows = g_ptr_array_new ();
}

static void
gda_mysql_recordset_finalize (GObject *object)
{
	GdaMysqlRecordset *recset = (GdaMysqlRecordset *) object;

	g_return_if_fail (GDA_IS_MYSQL_RECORDSET (recset));

	mysql_free_result (recset->mysql_res);
	recset->mysql_res = NULL;

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
gda_mysql_recordset_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaMysqlRecordsetClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_mysql_recordset_class_init,
			NULL,
			NULL,
			sizeof (GdaMysqlRecordset),
			0,
			(GInstanceInitFunc) gda_mysql_recordset_init
		};
		type = g_type_register_static (PARENT_TYPE, "GdaMysqlRecordset", &info, 0);
	}

	return type;
}

GdaMysqlRecordset *
gda_mysql_recordset_new (GdaConnection *cnc, MYSQL_RES *mysql_res)
{
	GdaMysqlRecordset *recset;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (mysql_res != NULL, NULL);

	recset = g_object_new (GDA_TYPE_MYSQL_RECORDSET, NULL);
	recset->cnc = cnc;
	recset->mysql_res = mysql_res;

	return recset;
}
