/* GNOME DB IBM DB2 Provider
 * Copyright (C) 2003 The GNOME Foundation
 *
 * AUTHORS:
 *         Sergey N. Belinsky <sergey_be@mail.ru>
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
#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <sqlcli1.h>
#include "gda-ibmdb2.h"
#include "gda-ibmdb2-types.h"

/////////////////////////////////////////////////////////////////////////////
// Public functions
/////////////////////////////////////////////////////////////////////////////

const GdaValueType
gda_ibmdb2_get_value_type (GdaIBMDB2Field *col)
{
	g_return_val_if_fail (col != NULL, GDA_VALUE_TYPE_UNKNOWN);

	switch (col->column_type) {
		case SQL_VARCHAR:
		case SQL_LONGVARCHAR:
		case SQL_CHAR:
		case SQL_DATALINK:
			return GDA_VALUE_TYPE_STRING;
		case SQL_NUMERIC:
		case SQL_DECIMAL:
			return GDA_VALUE_TYPE_NUMERIC;
		case SQL_BIGINT:
			return GDA_VALUE_TYPE_BIGINT;
		case SQL_INTEGER:
			return GDA_VALUE_TYPE_INTEGER;
		case SQL_SMALLINT:
		case SQL_TINYINT:
			return GDA_VALUE_TYPE_SMALLINT;
		case SQL_REAL:
			return GDA_VALUE_TYPE_SINGLE;
		case SQL_DOUBLE:
		case SQL_FLOAT:
			return GDA_VALUE_TYPE_DOUBLE;
		case SQL_TYPE_DATE:
		case SQL_DATE:
			return GDA_VALUE_TYPE_DATE;
		case SQL_TYPE_TIME:
		case SQL_TIME:
			return GDA_VALUE_TYPE_TIME;
		case SQL_TYPE_TIMESTAMP:
		case SQL_TIMESTAMP:
			return GDA_VALUE_TYPE_TIMESTAMP;
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
			return GDA_VALUE_TYPE_BINARY;
		/* FIXME Unicode */
		case SQL_WCHAR:
		case SQL_WVARCHAR:
		case SQL_WLONGVARCHAR:
			return GDA_VALUE_TYPE_UNKNOWN;
		case SQL_UNKNOWN_TYPE:
			return GDA_VALUE_TYPE_UNKNOWN;
		default:
			return GDA_VALUE_TYPE_UNKNOWN;
	}
	
	return GDA_VALUE_TYPE_UNKNOWN;

}

void
gda_ibmdb2_set_gdavalue (GdaValue *value, GdaIBMDB2Field *field)
{
	g_return_if_fail (value != NULL);
	g_return_if_fail (field != NULL);

	if (field->column_data == NULL) {
		gda_value_set_null (value);
	} else {
		/* FIXME */
		/* printf("Type = %d\n", field->column_type); */
		switch (field->column_type) {
			case SQL_SMALLINT:
				gda_value_set_smallint (value, *((gshort*)field->column_data));
				break;
			case SQL_INTEGER:
				gda_value_set_integer (value, *((gint*)field->column_data));
				break;
			case SQL_BIGINT:
				gda_value_set_bigint (value, (gint64)atoll(field->column_data));
				break;
			case SQL_REAL:
				gda_value_set_single (value, *((gfloat*)field->column_data));
				break;
			case SQL_DOUBLE:
			case SQL_FLOAT:
				gda_value_set_double (value, *((gdouble*)field->column_data));
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
				gda_ibmdb2_set_gdavalue_by_numeric (value, (SQL_NUMERIC_STRUCT*)field->column_data);
				break;
			case SQL_VARCHAR:
			case SQL_LONGVARCHAR:
			case SQL_DATALINK:
			case SQL_CHAR:
				gda_value_set_string (value, (gchar*)field->column_data);
				break;
			default:
				gda_value_set_string (value, field->column_data);
		}
	}
}

void gda_ibmdb2_set_gdavalue_by_date (GdaValue *value, DATE_STRUCT *date)
{
	GdaDate tmp;
	
	tmp.year = date->year;
	tmp.month = date->month;
	tmp.day = date->day;
	
	gda_value_set_date (value, &tmp);
}

void gda_ibmdb2_set_gdavalue_by_time (GdaValue *value, TIME_STRUCT *time)
{
	GdaTime tmp;
	
	tmp.hour = time->hour;
	tmp.minute = time->minute;
	tmp.second = time->second;
	tmp.timezone = 0;

	gda_value_set_time (value, &tmp);
}

void gda_ibmdb2_set_gdavalue_by_timestamp (GdaValue *value, TIMESTAMP_STRUCT *timestamp)
{
	GdaTimestamp tmp;
	
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

void gda_ibmdb2_set_gdavalue_by_numeric (GdaValue *value, SQL_NUMERIC_STRUCT *numeric)
{
	GdaNumeric tmp;
	/* FIXME */
	tmp.precision = 0;
	tmp.width = 0;
	tmp.number = NULL;
}
