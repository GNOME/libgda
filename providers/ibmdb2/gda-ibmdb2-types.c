/* GNOME DB IBM DB2 Provider
 * Copyright (C) 2003 - 2006 The GNOME Foundation
 *
 * AUTHORS:
 *         Sergey N. Belinsky <sergey_be@mail.ru>
 *         Vivien Malerba <malerba@gnome-db.org>
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

#include <glib/gi18n-lib.h>
#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <sqlcli1.h>
#include "gda-ibmdb2.h"
#include "gda-ibmdb2-types.h"

GType
gda_ibmdb2_get_value_type (GdaIBMDB2Field *col)
{
	g_return_val_if_fail (col != NULL, G_TYPE_INVALID);

	switch (col->column_type) {
		case SQL_VARCHAR:
		case SQL_LONGVARCHAR:
		case SQL_CHAR:
		case SQL_DATALINK:
			return G_TYPE_STRING;
		case SQL_NUMERIC:
		case SQL_DECIMAL:
			return GDA_TYPE_NUMERIC;
		case SQL_BIGINT:
			return G_TYPE_INT64;
		case SQL_INTEGER:
			return G_TYPE_INT;
		case SQL_SMALLINT:
		case SQL_TINYINT:
			return GDA_TYPE_SHORT;
		case SQL_REAL:
			return G_TYPE_FLOAT;
		case SQL_DOUBLE:
		case SQL_FLOAT:
			return G_TYPE_DOUBLE;
		case SQL_TYPE_DATE:
		case SQL_DATE:
			return G_TYPE_DATE;
		case SQL_TYPE_TIME:
		case SQL_TIME:
			return GDA_TYPE_TIME;
		case SQL_TYPE_TIMESTAMP:
		case SQL_TIMESTAMP:
			return GDA_TYPE_TIMESTAMP;
		case SQL_GRAPHIC:
		case SQL_VARGRAPHIC:
		case SQL_LONGVARGRAPHIC:
		case SQL_BLOB:
		case SQL_CLOB:
		case SQL_DBCLOB:
		case SQL_USER_DEFINED_TYPE:
		case SQL_BINARY:
		case SQL_VARBINARY:
		case SQL_LONGVARBINARY:
		case SQL_BIT:
			return GDA_TYPE_BINARY;
		/* FIXME Unicode */
		case SQL_WCHAR:
		case SQL_WVARCHAR:
		case SQL_WLONGVARCHAR:
			return G_TYPE_INVALID;
		case SQL_UNKNOWN_TYPE:
			return G_TYPE_INVALID;
		default:
			return G_TYPE_INVALID;
	}
	
	return G_TYPE_INVALID;

}

void
gda_ibmdb2_set_gdavalue (GValue *value, GdaIBMDB2Field *field)
{
	GdaNumeric numeric;


	g_return_if_fail (value != NULL);
	g_return_if_fail (field != NULL);

	if (field->column_data == NULL) {
		gda_value_set_null (value);
	} else {
		/* FIXME */
		switch (field->column_type) {
			case SQL_SMALLINT:
				gda_value_set_short (value, *((gshort*)field->column_data));
				break;
			case SQL_INTEGER:
				g_value_set_int (value, *((gint*)field->column_data));
				break;
			case SQL_BIGINT:
				g_value_set_int64 (value, (gint64)atoll(field->column_data));
				break;
			case SQL_REAL:
				g_value_set_float (value, *((gfloat*)field->column_data));
				break;
			case SQL_DOUBLE:
			case SQL_FLOAT:
				g_value_set_double (value, *((gdouble*)field->column_data));
				break;
			case SQL_TYPE_DATE:
			case SQL_DATE:
				gda_ibmdb2_set_gdavalue_by_date (value, (DATE_STRUCT*)field->column_data);
				break;
			case SQL_TYPE_TIME:
			case SQL_TIME:
				gda_ibmdb2_set_gdavalue_by_time (value, (TIME_STRUCT*)field->column_data);
				break;
			case SQL_TYPE_TIMESTAMP:
			case SQL_TIMESTAMP:
				gda_ibmdb2_set_gdavalue_by_timestamp (value, (TIMESTAMP_STRUCT*)field->column_data); 
				break;
			case SQL_NUMERIC:
			case SQL_DECIMAL:
				numeric.width = field->column_size;
				numeric.precision = field->column_scale;
				numeric.number = g_strdup ((char*)field->column_data);
				gda_value_set_numeric (value, &numeric);
				g_free (numeric.number);
				break;
			case SQL_VARCHAR:
			case SQL_LONGVARCHAR:
			case SQL_DATALINK:
			case SQL_CHAR:
				g_value_set_string (value, (gchar*)field->column_data);
				break;
			case SQL_BLOB:
				g_value_set_string (value, _("[BLOB unsupported]"));
				break;
			default:
				g_value_set_string (value, _("[Unsupported data]"));
		}
	}
}

void gda_ibmdb2_set_gdavalue_by_date (GValue *value, DATE_STRUCT *date)
{
	GDate tmp;

        g_return_if_fail (value != NULL);
	
	tmp.year = date->year;
	tmp.month = date->month;
	tmp.day = date->day;
	
	g_value_set_boxed (value, &tmp);
}

void gda_ibmdb2_set_gdavalue_by_time (GValue *value, TIME_STRUCT *time)
{
	GdaTime tmp;

        g_return_if_fail (value != NULL);
	
	tmp.hour = time->hour;
	tmp.minute = time->minute;
	tmp.second = time->second;
	tmp.timezone = 0;

	gda_value_set_time (value, &tmp);
}

void gda_ibmdb2_set_gdavalue_by_timestamp (GValue *value, TIMESTAMP_STRUCT *timestamp)
{
	GdaTimestamp tmp;

        g_return_if_fail (value != NULL);
	
	tmp.year = timestamp->year;
	tmp.month = timestamp->month;
	tmp.day = timestamp->day;
	tmp.hour = timestamp->hour;
	tmp.minute = timestamp->minute;
	tmp.second = timestamp->second;
	tmp.fraction = timestamp->fraction;
	tmp.timezone = 0;

	gda_value_set_timestamp (value, &tmp);
}

