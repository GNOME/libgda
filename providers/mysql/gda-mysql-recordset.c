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

#include <bonobo/bonobo-i18n.h>
#include <stdlib.h>
#include "gda-mysql.h"
#include "gda-mysql-recordset.h"

#define OBJECT_DATA_RECSET_HANDLE "GDA_Mysql_RecsetHandle"

/*
 * Private functions
 */

static void
free_mysql_res (gpointer data)
{
	MYSQL_RES *mysql_res = (MYSQL_RES *) data;

	g_return_if_fail (mysql_res != NULL);
	mysql_free_result (mysql_res);
}

static GdaRow *
fetch_func (GdaServerRecordset *recset, gulong rownum)
{
	GdaRow *row;
	gint field_count;
	gint row_count;
	gint i;
	unsigned long *lengths;
	MYSQL_FIELD *mysql_fields;
	MYSQL_RES *mysql_res;
	MYSQL_ROW mysql_row;

	g_return_val_if_fail (GDA_IS_SERVER_RECORDSET (recset), NULL);

	mysql_res = g_object_get_data (G_OBJECT (recset), OBJECT_DATA_RECSET_HANDLE);
	if (!mysql_res) {
		gda_server_connection_add_error_string (
			gda_server_recordset_get_connection (recset),
			_("Invalid MySQL handle"));
		return NULL;
	}

	/* move to the corresponding row */
	row_count = mysql_num_rows (mysql_res);
	if (row_count == 0)
		return NULL;
	field_count = mysql_num_fields (mysql_res);

	if (rownum < 0 || rownum >= row_count) {
		gda_server_connection_add_error_string (
			gda_server_recordset_get_connection (recset),
			_("Row number out of range"));
		return NULL;
	}

	mysql_data_seek (mysql_res, rownum);
	row = gda_row_new (field_count);

	lengths = mysql_res->lengths;
	mysql_fields = mysql_fetch_fields (mysql_res);

	mysql_row = mysql_fetch_row (mysql_res);
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

static GdaRowAttributes *
describe_func (GdaServerRecordset *recset)
{
	MYSQL_RES *mysql_res;
	gint field_count;
	gint i;
	GdaRowAttributes *attrs;

	g_return_val_if_fail (GDA_IS_SERVER_RECORDSET (recset), NULL);

	mysql_res = g_object_get_data (G_OBJECT (recset), OBJECT_DATA_RECSET_HANDLE);
	if (!mysql_res) {
		gda_server_connection_add_error_string (
			gda_server_recordset_get_connection (recset),
			_("Invalid MySQL handle"));
		return NULL;
	}

	/* create the GdaRowAttributes to be returned */
	field_count = mysql_num_fields (mysql_res);
	attrs = gda_row_attributes_new (field_count);
	
	for (i = 0; i < field_count; i++) {
		GdaFieldAttributes *field_attrs;
		MYSQL_FIELD *mysql_field;

		mysql_field = mysql_fetch_field (mysql_res);

		field_attrs = gda_row_attributes_get_field (attrs, i);
		gda_field_attributes_set_name (field_attrs, mysql_field->name);
		gda_field_attributes_set_defined_size (field_attrs, mysql_field->max_length);
		gda_field_attributes_set_scale (field_attrs, mysql_field->decimals);
		gda_field_attributes_set_gdatype (field_attrs,
						  gda_mysql_type_to_gda (mysql_field->type));
	}

	return attrs;
}

/*
 * Public functions
 */

GdaServerRecordset *
gda_mysql_recordset_new (GdaServerConnection *cnc, MYSQL_RES *mysql_res)
{
	GdaServerRecordset *recset;

	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), NULL);
	g_return_val_if_fail (mysql_res != NULL, NULL);

	recset = gda_server_recordset_new (cnc, fetch_func, describe_func);
	g_object_set_data_full (G_OBJECT (recset), OBJECT_DATA_RECSET_HANDLE,
				mysql_res, (GDestroyNotify) free_mysql_res);

	return recset;
}
