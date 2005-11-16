/* GNOME DB Postgres Provider
 * Copyright (C) 1998 - 2005 The GNOME Foundation
 *
 * AUTHORS:
 *         Vivien Malerba <malerba@gnome-db.org>
 *         Rodrigo Moya <rodrigo@gnome-db.org>
 *         Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 *         Juan-Mariano de Goyeneche <jmseyas@dit.upm.es>
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
#include "gda-postgres.h"
#include "gda-postgres-blob.h"

GdaConnectionEventCode
gda_postgres_sqlsate_to_gda_code (const gchar *sqlstate)
{
	guint64 gda_code = g_ascii_strtoull (sqlstate, NULL, 0);

	switch (gda_code)
	{
		case 42501:
		       	return GDA_CONNECTION_EVENT_CODE_INSUFFICIENT_PRIVILEGES;
		case 23505:
		       	return GDA_CONNECTION_EVENT_CODE_UNIQUE_VIOLATION;
		case 23502:
		       	return GDA_CONNECTION_EVENT_CODE_NOT_NULL_VIOLATION;
		default:
		       	return GDA_CONNECTION_EVENT_CODE_UNKNOWN;
	}
}

GdaConnectionEvent *
gda_postgres_make_error (PGconn *pconn, PGresult *pg_res)
{
	GdaConnectionEvent *error;
	GdaConnectionEventCode gda_code;
	gchar *sqlstate;

	error = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);
	if (pconn != NULL) {
		gchar *message;
		
		if (pg_res != NULL) {
			message = PQresultErrorMessage (pg_res);
			sqlstate = PQresultErrorField (pg_res, PG_DIAG_SQLSTATE);
			gda_code = gda_postgres_sqlsate_to_gda_code (sqlstate);
		}
		else {
			message = PQerrorMessage (pconn);
			sqlstate = _("Not available");
			gda_code = GDA_CONNECTION_EVENT_CODE_UNKNOWN;
		}

		gda_connection_event_set_description (error, message);
		gda_connection_event_set_sqlstate (error, sqlstate);
		gda_connection_event_set_gda_code (error, gda_code);
	} 
	else {
		gda_connection_event_set_description (error, _("NO DESCRIPTION"));
		gda_connection_event_set_sqlstate (error, _("Not available"));
		gda_connection_event_set_gda_code (error, gda_code);
	}

	gda_connection_event_set_code (error, -1);
	gda_connection_event_set_source (error, "gda-postgres");

	return error;
}

GdaValueType
gda_postgres_type_name_to_gda (GHashTable *h_table, const gchar *name)
{
	GdaValueType *type;

	type = g_hash_table_lookup (h_table, name);
	if (type)
		return *type;

	return GDA_VALUE_TYPE_STRING;
}

GdaValueType
gda_postgres_type_oid_to_gda (GdaPostgresTypeOid *type_data, gint ntypes, Oid postgres_type)
{
	gint i;

	for (i = 0; i < ntypes; i++)
		if (type_data[i].oid == postgres_type) 
			break;

  	if (type_data[i].oid != postgres_type)
		return GDA_VALUE_TYPE_STRING;

	return type_data[i].type;
}

/* Makes a point from a string like "(3.2,5.6)" */
static void
make_point (GdaGeometricPoint *point, const gchar *value)
{
	value++;
	point->x = atof (value);
	value = strchr (value, ',');
	value++;
	point->y = atof (value);
}

/* Makes a GdaTime from a string like "12:30:15+01" */
static void
make_time (GdaTime *timegda, const gchar *value)
{
	timegda->hour = atoi (value);
	value += 3;
	timegda->minute = atoi (value);
	value += 3;
	timegda->second = atoi (value);
	value += 2;
	if (*value)
		timegda->timezone = atoi (value);
	else
		timegda->timezone = TIMEZONE_INVALID;
}

/* Makes a GdaTimestamp from a string like "2003-12-13 13:12:01.12+01" */
static void
make_timestamp (GdaTimestamp *timestamp, const gchar *value)
{
	timestamp->year = atoi (value);
	value += 5;
	timestamp->month = atoi (value);
	value += 3;
	timestamp->day = atoi (value);
	value += 3;
	timestamp->hour = atoi (value);
	value += 3;
	timestamp->minute = atoi (value);
	value += 3;
	timestamp->second = atoi (value);
	value += 2;
	if (*value != '.') {
		timestamp->fraction = 0;
	} else {
		gint ndigits = 0;
		gint64 fraction;

		value++;
		fraction = atol (value);
		while (*value && *value != '+') {
			value++;
			ndigits++;
		}

		while (ndigits < 3) {
			fraction *= 10;
			ndigits++;
		}

		while (fraction > 0 && ndigits > 3) {
			fraction /= 10;
			ndigits--;
		}
		
		timestamp->fraction = fraction;
	}

	if (*value != '+') {
		timestamp->timezone = 0;
	} else {
		value++;
		timestamp->timezone = atol (value) * 60 * 60;
	}
}

void
gda_postgres_set_value (GdaConnection *cnc,
			GdaValue *value,
			GdaValueType type,
			const gchar *thevalue,
			gboolean isNull,
			gint length)
{
	GDate *gdate;
	GdaDate date;
	GdaTime timegda;
	GdaTimestamp timestamp;
	GdaGeometricPoint point;
	GdaNumeric numeric;
	GdaBlob *blob;
	GdaBinary bin;
	guchar *unescaped;

	if (isNull){
		gda_value_set_null (value);
		return;
	}

	switch (type) {
	case GDA_VALUE_TYPE_BOOLEAN :
		gda_value_set_boolean (value, (*thevalue == 't') ? TRUE : FALSE);
		break;
	case GDA_VALUE_TYPE_STRING :
		gda_value_set_string (value, thevalue);
		break;
	case GDA_VALUE_TYPE_BIGINT :
		gda_value_set_bigint (value, atoll (thevalue));
		break;
	case GDA_VALUE_TYPE_INTEGER :
		gda_value_set_integer (value, atol (thevalue));
		break;
	case GDA_VALUE_TYPE_SMALLINT :
		gda_value_set_smallint (value, atoi (thevalue));
		break;
	case GDA_VALUE_TYPE_SINGLE :
		setlocale (LC_NUMERIC, "C");
		gda_value_set_single (value, atof (thevalue));
		setlocale (LC_NUMERIC, "");
		break;
	case GDA_VALUE_TYPE_DOUBLE :
		setlocale (LC_NUMERIC, "C");
		gda_value_set_double (value, atof (thevalue));
		setlocale (LC_NUMERIC, "");
		break;
	case GDA_VALUE_TYPE_NUMERIC :
		numeric.number = g_strdup (thevalue);
		numeric.precision = 0; /* FIXME */
		numeric.width = 0; /* FIXME */
		gda_value_set_numeric (value, &numeric);
		g_free (numeric.number);
		break;
	case GDA_VALUE_TYPE_DATE :
		gdate = g_date_new ();
		g_date_set_parse (gdate, thevalue);
		if (!g_date_valid (gdate)) {
			g_warning ("Could not parse '%s' "
				"Setting date to 01/01/0001!\n", thevalue);
			g_date_clear (gdate, 1);
			g_date_set_dmy (gdate, 1, 1, 1);
		}
		date.day = g_date_get_day (gdate);
		date.month = g_date_get_month (gdate);
		date.year = g_date_get_year (gdate);
		gda_value_set_date (value, &date);
		g_date_free (gdate);
		break;
	case GDA_VALUE_TYPE_GEOMETRIC_POINT :
		make_point (&point, thevalue);
		gda_value_set_geometric_point (value, &point);
		break;
	case GDA_VALUE_TYPE_TIMESTAMP :
		make_timestamp (&timestamp, thevalue);
		gda_value_set_timestamp (value, &timestamp);
		break;
	case GDA_VALUE_TYPE_TIME :
		make_time (&timegda, thevalue);
		gda_value_set_time (value, &timegda);
		break;
	case GDA_VALUE_TYPE_BINARY :
		/*
		 * Requires PQunescapeBytea in libpq (present since 7.3.x)
		 */
		unescaped = PQunescapeBytea (thevalue, &(bin.binary_length));
		if (unescaped != NULL) {
			bin.data = unescaped;
			gda_value_set_binary (value, &bin);
			PQfreemem (unescaped);
		} 
		else {
			g_warning ("Error unescaping string: %s\n", thevalue);
			gda_value_set_string (value, thevalue);
		}
		break;
	case GDA_VALUE_TYPE_BLOB :
		blob = gda_postgres_blob_new (cnc);
		gda_postgres_blob_set_id (GDA_POSTGRES_BLOB (blob), atoi (thevalue));
		gda_value_set_blob (value, blob);
 		break;
	default :
		gda_value_set_string (value, thevalue);
	}
}

gchar *
gda_postgres_value_to_sql_string (GdaValue *value)
{
	gchar *val_str;
	gchar *ret;

	g_return_val_if_fail (value != NULL, NULL);

	val_str = gda_value_stringify (value);
	if (!val_str)
		return NULL;

	switch (GDA_VALUE_TYPE(value)) {
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
		ret = g_strdup_printf ("\'%s\'", val_str);
	}

	g_free (val_str);

	return ret;
}
