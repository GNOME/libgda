/* GDA Oracle provider
 * Copyright (C) 2002 The GNOME Foundation.
 *
 * AUTHORS:
 * 	Tim Coleman <tim@timcoleman.com>
 *
 * Borrowed from Mysql utils.c, written by:
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
#include <string.h>
#include "gda-oracle.h"

GdaError *
gda_oracle_make_error (GdaOracleConnectionData *handle)
{
	return NULL;
}

gchar *
gda_oracle_value_to_sql_string (GdaValue *value)
{
	gchar *val_str;
	gchar *ret;

	g_return_val_if_fail (value != NULL, NULL);

	val_str = gda_value_stringify (value);
	if (!val_str)
		return NULL;

	switch (value->type) {
	case GDA_VALUE_TYPE_BIGINT :
	case GDA_VALUE_TYPE_DOUBLE :
	case GDA_VALUE_TYPE_INTEGER :
	case GDA_VALUE_TYPE_NUMERIC :
	case GDA_VALUE_TYPE_SINGLE :
	case GDA_VALUE_TYPE_SMALLINT :
	case GDA_VALUE_TYPE_TINYINT :
		ret = g_strdup (val_str);
		break;
	default :
		ret = g_strdup_printf ("\"%s\"", val_str);
	}

	g_free (val_str);

	return ret;
}

void
gda_oracle_set_value (GdaValue *value, 
			GdaValueType type, 
			gconstpointer thevalue, 
			gboolean isNull,
			gint defined_size)
{
	GDate *gdate;
	GdaDate date;
	GdaTime timegda;
	GdaTimestamp timestamp;
	GdaGeometricPoint point;
	GdaNumeric numeric;
	gchar string_buffer[defined_size+1];

	if (isNull) {
		gda_value_set_null (value);
		return;
	}

	switch (type) {
	case GDA_VALUE_TYPE_BOOLEAN:
		gda_value_set_boolean (value, (atoi (thevalue)) ? TRUE: FALSE);
		break;
	case GDA_VALUE_TYPE_STRING:
		memcpy (string_buffer, thevalue, defined_size);
		string_buffer[defined_size] = '\0';
		gda_value_set_string (value, string_buffer);
		break;
	case GDA_VALUE_TYPE_BIGINT:
		gda_value_set_bigint (value, atoll (thevalue));
		break;
	case GDA_VALUE_TYPE_INTEGER:
		gda_value_set_integer (value, atol (thevalue));
		break;
	case GDA_VALUE_TYPE_SMALLINT:
		gda_value_set_smallint (value, atoi (thevalue));
		break;
	case GDA_VALUE_TYPE_SINGLE:
		gda_value_set_single (value, atof (thevalue));
		break;
	case GDA_VALUE_TYPE_DOUBLE:
		gda_value_set_double (value, atof (thevalue));
		break;
	case GDA_VALUE_TYPE_NUMERIC:
		numeric.number = thevalue;
		numeric.precision = 0; // FIXME
		numeric.width = 0; // FIXME
		gda_value_set_numeric (value, &numeric);
		break;
	case GDA_VALUE_TYPE_GEOMETRIC_POINT:
		break;
	case GDA_VALUE_TYPE_NULL:
		gda_value_set_null (value);
		break;
	case GDA_VALUE_TYPE_TIMESTAMP:
		break;
	case GDA_VALUE_TYPE_TIME:
		break;
	case GDA_VALUE_TYPE_BINARY:
		// FIXME
		break;
	default:
		gda_value_set_string (value, thevalue);
	}
}
