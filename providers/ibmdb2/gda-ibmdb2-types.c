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
gda_freetds_get_value_type (GdaIBMDB2Field *col)
{
	g_return_val_if_fail (col != NULL, GDA_VALUE_TYPE_UNKNOWN);

	switch (col->column_type) {
		case SQL_VARCHAR:
		case SQL_CHAR:
		case SQL_DATALINK:
			return GDA_VALUE_TYPE_STRING;
		case SQL_NUMERIC:
		case SQL_DECIMAL:
			return GDA_VALUE_TYPE_NUMERIC;
		case SQL_INTEGER:
			return GDA_VALUE_TYPE_INTEGER;
		case SQL_SMALLINT:
			return GDA_VALUE_TYPE_SMALLINT;
		case SQL_REAL:
			return GDA_VALUE_TYPE_SINGLE;
		case SQL_DOUBLE:
		case SQL_FLOAT:
			return GDA_VALUE_TYPE_DOUBLE;
		case SQL_TYPE_DATE:
			return GDA_VALUE_TYPE_DATE;
		case SQL_TYPE_TIME:
			return GDA_VALUE_TYPE_TIME;
		case SQL_TYPE_TIMESTAMP:
		case SQL_DATETIME:
			return GDA_VALUE_TYPE_TIMESTAMP;
		/* FIXME SQL extended data types */
		case SQL_GRAPHIC:
		case SQL_VARGRAPHIC:
		case SQL_LONGVARGRAPHIC:
		case SQL_BLOB:
		case SQL_CLOB:
		case SQL_DBCLOB:
		case SQL_USER_DEFINED_TYPE:
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

/* FIXME Other types
SQL_TIME 
SQL_TIMESTAMP
SQL_LONGVARCHAR
SQL_BINARY    
SQL_VARBINARY
SQL_LONGVARBINARY
SQL_BIGINT      
SQL_TINYINT    
SQL_BIT
*/
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
		switch (field->column_type) {
			case SQL_INTEGER:
				gda_value_set_integer(value, (gint)field->column_data);
				break;
			case SQL_VARCHAR:
			case SQL_DATALINK:
			case SQL_CHAR:
				gda_value_set_string(value, (gchar*)field->column_data);
				break;
			default:
				gda_value_set_string(value, field->column_data);
		}
	}
}

