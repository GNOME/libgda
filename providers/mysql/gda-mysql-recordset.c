/* GDA MySQL provider
 * Copyright (C) 1998 - 2005 The GNOME Foundation.
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

#include <libgda/gda-intl.h>
#include <stdlib.h>
#include <string.h>
#include "gda-mysql.h"
#include "gda-mysql-recordset.h"

#define PARENT_TYPE GDA_TYPE_DATA_MODEL_BASE

static void gda_mysql_recordset_class_init (GdaMysqlRecordsetClass *klass);
static void gda_mysql_recordset_init       (GdaMysqlRecordset *recset,
					    GdaMysqlRecordsetClass *klass);
static void gda_mysql_recordset_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

/*
 * Private functions
 */

static void
fill_gda_value (GdaValue *gda_value, enum enum_field_types type, gchar *value,
		unsigned long length, gboolean is_unsigned)
{
	if (!value) {
		gda_value_set_null (gda_value);
		return;
	}

	switch (type) {
	case FIELD_TYPE_DECIMAL :
	case FIELD_TYPE_DOUBLE :
		gda_value_set_double (gda_value, atof (value));
		break;
	case FIELD_TYPE_FLOAT :
		gda_value_set_single (gda_value, atof (value));
		break;
	case FIELD_TYPE_LONG :
		if (is_unsigned)
			gda_value_set_uinteger (gda_value, atol (value));
	case FIELD_TYPE_YEAR :
		gda_value_set_integer (gda_value, atol (value));
		break;
	case FIELD_TYPE_LONGLONG :
	case FIELD_TYPE_INT24 :
		if (is_unsigned)
			gda_value_set_biguint (gda_value, atoll (value));
		gda_value_set_bigint (gda_value, atoll (value));
		break;
	case FIELD_TYPE_SHORT :
		if (is_unsigned)
			gda_value_set_smalluint (gda_value, atoi (value));
		gda_value_set_smallint (gda_value, atoi (value));
		break;
	case FIELD_TYPE_TINY :
		if (is_unsigned)
			gda_value_set_tinyuint (gda_value, atoi (value));
		gda_value_set_tinyint (gda_value, atoi (value));
		break;
	case FIELD_TYPE_TINY_BLOB :
	case FIELD_TYPE_MEDIUM_BLOB :
	case FIELD_TYPE_LONG_BLOB :
	case FIELD_TYPE_BLOB :
		gda_value_set_binary (gda_value, value, length);
		break;
	case FIELD_TYPE_VAR_STRING :
	case FIELD_TYPE_STRING :
		/* FIXME: we might get "[VAR]CHAR(20) BINARY" type with \0 inside
		   We should check for BINARY flag and treat it like a BLOB
		 */
		gda_value_set_string (gda_value, value);
		break;
	case FIELD_TYPE_DATE :
		gda_value_set_from_string (gda_value, value, GDA_VALUE_TYPE_DATE);
		break;
	case FIELD_TYPE_TIME :
		gda_value_set_from_string (gda_value, value, GDA_VALUE_TYPE_TIME);
		break;
	case FIELD_TYPE_TIMESTAMP :
	case FIELD_TYPE_DATETIME :
		gda_value_set_from_string (gda_value, value, GDA_VALUE_TYPE_TIMESTAMP);
		break;
	case FIELD_TYPE_NULL :
	case FIELD_TYPE_NEWDATE :
	case FIELD_TYPE_ENUM :
	case FIELD_TYPE_SET : /* FIXME */
		gda_value_set_string (gda_value, value);
		break;
	default :
		g_printerr ("Unknown MySQL datatype.  This is fishy, but continuing anyway.\n");
		gda_value_set_string (gda_value, value);
	}
}


static GdaRow *
fetch_row (GdaMysqlRecordset *recset, gulong rownum)
{
	GdaRow *row;
	gint field_count;
	gint row_count;
	gint i;
	MYSQL_FIELD *mysql_fields;
	MYSQL_ROW mysql_row;
	unsigned long *mysql_lengths;


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
	mysql_fields = mysql_fetch_fields (recset->mysql_res);
	mysql_row = mysql_fetch_row (recset->mysql_res);
	mysql_lengths = mysql_fetch_lengths (recset->mysql_res);
	if (!mysql_row || !mysql_lengths)
		return NULL;
	
	row = gda_row_new (GDA_DATA_MODEL (recset), field_count);

	for (i = 0; i < field_count; i++) {
		fill_gda_value ((GdaValue*) gda_row_get_value (row, i),
				mysql_fields[i].type,
				mysql_row[i],
				mysql_lengths[i],
				mysql_fields[i].flags & UNSIGNED_FLAG);
	}

	return row;
}

/*
 * GdaMysqlRecordset class implementation
 */

static gint
gda_mysql_recordset_get_n_rows (GdaDataModelBase *model)
{
	GdaMysqlRecordset *recset = (GdaMysqlRecordset *) model;

	g_return_val_if_fail (GDA_IS_MYSQL_RECORDSET (recset), -1);
	if (recset->mysql_res == NULL)
		return recset->affected_rows;

	return mysql_num_rows (recset->mysql_res);
}

static gint
gda_mysql_recordset_get_n_columns (GdaDataModelBase *model)
{
	GdaMysqlRecordset *recset = (GdaMysqlRecordset *) model;

	g_return_val_if_fail (GDA_IS_MYSQL_RECORDSET (recset), -1);
	if (recset->mysql_res == NULL)
		return 0;

	return mysql_num_fields (recset->mysql_res);
}

static GdaDataModelColumnAttributes *
gda_mysql_recordset_describe_column (GdaDataModelBase *model, gint col)
{
	gint field_count;
	GdaDataModelColumnAttributes *attrs;
	MYSQL_FIELD *mysql_field;
	GdaMysqlRecordset *recset = (GdaMysqlRecordset *) model;

	g_return_val_if_fail (GDA_IS_MYSQL_RECORDSET (recset), NULL);

	if (!recset->mysql_res) {
		gda_connection_add_error_string (recset->cnc, _("Invalid MySQL handle"));
		return NULL;
	}

	/* create the GdaDataModelColumnAttributes to be returned */
	field_count = mysql_num_fields (recset->mysql_res);
	if (col >= field_count)
		return NULL;

	mysql_field = mysql_fetch_field_direct (recset->mysql_res, col);
	if (!mysql_field)
		return NULL;

	attrs = gda_data_model_column_attributes_new ();

	if (mysql_field->name)
		gda_data_model_column_attributes_set_name (attrs, mysql_field->name);
	gda_data_model_column_attributes_set_defined_size (attrs, mysql_field->length);
	gda_data_model_column_attributes_set_table (attrs, mysql_field->table);
	/* gda_data_model_column_attributes_set_caption(attrs, ); */
	gda_data_model_column_attributes_set_scale (attrs, mysql_field->decimals);
	gda_data_model_column_attributes_set_gdatype (attrs, gda_mysql_type_to_gda (mysql_field->type,
									mysql_field->flags & UNSIGNED_FLAG));
	gda_data_model_column_attributes_set_allow_null (attrs, !IS_NOT_NULL (mysql_field->flags));
	gda_data_model_column_attributes_set_primary_key (attrs, IS_PRI_KEY (mysql_field->flags));
	gda_data_model_column_attributes_set_unique_key (attrs, mysql_field->flags & UNIQUE_KEY_FLAG);
	/* gda_data_model_column_attributes_set_references(attrs, ); */
	gda_data_model_column_attributes_set_auto_increment (attrs, mysql_field->flags & AUTO_INCREMENT_FLAG);
	/* attrs->auto_increment_start */
	/* attrs->auto_increment_step  */
	gda_data_model_column_attributes_set_position (attrs, col);

	return attrs;
}

static const GdaRow *
gda_mysql_recordset_get_row (GdaDataModelBase *model, gint row)
{
	gint rows;
	gint fetched_rows;
	gint i;
	GdaRow *fields = NULL;
	GdaMysqlRecordset *recset = (GdaMysqlRecordset *) model;

	g_return_val_if_fail (GDA_IS_MYSQL_RECORDSET (recset), NULL);

	rows = mysql_num_rows (recset->mysql_res);
	fetched_rows = recset->rows->len;

	if (row >= rows)
		return NULL;
	if (row < fetched_rows)
		return (const GdaRow *) g_ptr_array_index (recset->rows, row);

	gda_data_model_freeze (GDA_DATA_MODEL (recset));

	for (i = fetched_rows; i <= row; i++) {
		fields = fetch_row (recset, i);
		if (!fields)
			return NULL;

		g_ptr_array_add (recset->rows, fields);
	}

	gda_data_model_thaw (GDA_DATA_MODEL (recset));

	return (const GdaRow *) fields;
}

static const GdaValue *
gda_mysql_recordset_get_value_at (GdaDataModelBase *model, gint col, gint row)
{
	gint cols;
	const GdaRow *fields;
	GdaMysqlRecordset *recset = (GdaMysqlRecordset *) model;

	g_return_val_if_fail (GDA_IS_MYSQL_RECORDSET (recset), NULL);
	
	cols = mysql_num_fields (recset->mysql_res);
	if (col >= cols)
		return NULL;

	fields = gda_mysql_recordset_get_row (model, row);
	return fields != NULL ? gda_row_get_value ((GdaRow *) fields, col) : NULL;
}

static gboolean
gda_mysql_recordset_is_updatable (GdaDataModelBase *model)
{
	GdaCommandType cmd_type;
	GdaMysqlRecordset *recset = (GdaMysqlRecordset *) model;

	g_return_val_if_fail (GDA_IS_MYSQL_RECORDSET (recset), FALSE);
	
	cmd_type = gda_data_model_get_command_type (model);
	return cmd_type == GDA_COMMAND_TYPE_TABLE ? TRUE : FALSE;
}

static const GdaRow *
gda_mysql_recordset_append_values (GdaDataModelBase *model, const GList *values)
{
	GString *sql;
	GdaRow *row;
	gint i;
	gint rc;
	gint cols;
	GList *l;
	GdaMysqlRecordset *recset = (GdaMysqlRecordset *) model;

	g_return_val_if_fail (GDA_IS_MYSQL_RECORDSET (recset), NULL);
	g_return_val_if_fail (values != NULL, NULL);
	g_return_val_if_fail (gda_data_model_is_updatable (model), NULL);
	g_return_val_if_fail (gda_data_model_has_changed (model), NULL);

	cols = mysql_num_fields (recset->mysql_res);
	if (cols != g_list_length ((GList *) values)) {
		gda_connection_add_error_string (
			recset->cnc,
			_("Attempt to insert a row with an invalid number of columns"));
		return NULL;
	}

	/* prepare the SQL command */
	sql = g_string_new ("INSERT INTO ");
	sql = g_string_append (sql, gda_data_model_get_command_text (model));
	sql = g_string_append (sql, "(");
	for (i = 0; i < cols; i++) {
		GdaDataModelColumnAttributes *fa;

		fa = gda_data_model_describe_column (model, i);
		if (!fa) {
			gda_connection_add_error_string (
				recset->cnc,
				_("Could not retrieve column's information"));
			g_string_free (sql, TRUE);
			return NULL;
		}

		if (i != 0)
			sql = g_string_append (sql, ", ");
		sql = g_string_append (sql, gda_data_model_column_attributes_get_name (fa));
	}
	sql = g_string_append (sql, ") VALUES (");

	for (l = (GList *) values, i = 0; i < cols; i++, l = l->next) {
		gchar *val_str;
		const GdaValue *val = (const GdaValue *) l->data;

		if (!val) {
			gda_connection_add_error_string (
				recset->cnc,
				_("Could not retrieve column's value"));
			g_string_free (sql, TRUE);
			return NULL;
		}

		if (i != 0)
			sql = g_string_append (sql, ", ");
		val_str = gda_mysql_value_to_sql_string ((GdaValue*)val);
		sql = g_string_append (sql, val_str);

		g_free (val_str);
	}
	sql = g_string_append (sql, ")");

	/* execute the UPDATE command */
	rc = mysql_real_query (recset->mysql_res->handle, sql->str, strlen (sql->str));
	g_string_free (sql, TRUE);
	if (rc != 0) {
		gda_connection_add_error (
			recset->cnc, gda_mysql_make_error (recset->mysql_res->handle));
		return NULL;
	}

	/* append the row to the data model */
	row = gda_row_new_from_list (model, values);
	g_ptr_array_add (recset->rows, row);

	return (const GdaRow *) row;
}

static gboolean
gda_mysql_recordset_remove_row (GdaDataModelBase *model, const GdaRow *row)
{
	return FALSE;
}

static gboolean
gda_mysql_recordset_update_row (GdaDataModelBase *model, const GdaRow *row)
{
	return FALSE;
}

static void
gda_mysql_recordset_class_init (GdaMysqlRecordsetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaDataModelBaseClass *model_class = GDA_DATA_MODEL_BASE_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_mysql_recordset_finalize;
	model_class->get_n_rows = gda_mysql_recordset_get_n_rows;
	model_class->get_n_columns = gda_mysql_recordset_get_n_columns;
	model_class->describe_column = gda_mysql_recordset_describe_column;
	model_class->get_row = gda_mysql_recordset_get_row;
	model_class->get_value_at = gda_mysql_recordset_get_value_at;
	model_class->is_updatable = gda_mysql_recordset_is_updatable;
	model_class->append_values = gda_mysql_recordset_append_values;
	model_class->remove_row = gda_mysql_recordset_remove_row;
	model_class->update_row = gda_mysql_recordset_update_row;
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
gda_mysql_recordset_new (GdaConnection *cnc, MYSQL_RES *mysql_res, MYSQL *mysql)
{
	GdaMysqlRecordset *recset;
	MYSQL_FIELD *mysql_fields;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (mysql_res != NULL || mysql != NULL, NULL);

	recset = g_object_new (GDA_TYPE_MYSQL_RECORDSET, NULL);
	recset->cnc = cnc;
	recset->mysql_res = mysql_res;
	if (mysql_res == NULL) {
		recset->affected_rows = mysql_affected_rows (mysql);
		return recset;
	}

	mysql_fields = mysql_fetch_fields (recset->mysql_res);
	if (mysql_fields != NULL) {
		gint i;

		for (i = 0; i < mysql_num_fields (recset->mysql_res); i++) {
			gda_data_model_set_column_title (GDA_DATA_MODEL (recset),
							 i, mysql_fields[i].name);
		}
	}

	return recset;
}
