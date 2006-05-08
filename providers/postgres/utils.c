/* GNOME DB Postgres Provider
 * Copyright (C) 1998 - 2006 The GNOME Foundation
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

#include <glib/gi18n-lib.h>
#include <stdlib.h>
#include <string.h>
#include "gda-postgres.h"
#include "gda-postgres-blob.h"
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

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

GType
gda_postgres_type_name_to_gda (GHashTable *h_table, const gchar *name)
{
	GType *type;

	type = g_hash_table_lookup (h_table, name);
	if (type)
		return *type;

	return G_TYPE_STRING;
}

GType
gda_postgres_type_oid_to_gda (GdaPostgresTypeOid *type_data, gint ntypes, Oid postgres_type)
{
	gint i;

	for (i = 0; i < ntypes; i++)
		if (type_data[i].oid == postgres_type) 
			break;

  	if (type_data[i].oid != postgres_type)
		return G_TYPE_STRING;

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
		timegda->timezone = GDA_TIMEZONE_INVALID;
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
			GValue *value,
			GType type,
			const gchar *thevalue,
			gboolean isNull,
			gint length)
{
	if (isNull){
		gda_value_set_null (value);
		return;
	}

	gda_value_reset_with_type (value, type);

	if (type == G_TYPE_BOOLEAN)
		g_value_set_boolean (value, (*thevalue == 't') ? TRUE : FALSE);
	else if (type == G_TYPE_STRING)
		g_value_set_string (value, thevalue);
	else if (type == G_TYPE_INT64)
		g_value_set_int64 (value, atoll (thevalue));
	else if (type == G_TYPE_INT)
		g_value_set_int (value, atol (thevalue));
	else if (type == GDA_TYPE_SHORT)
		gda_value_set_short (value, atoi (thevalue));
	else if (type == G_TYPE_FLOAT) {
		setlocale (LC_NUMERIC, "C");
		g_value_set_float (value, atof (thevalue));
		setlocale (LC_NUMERIC, "");
	}
	else if (type == G_TYPE_DOUBLE) {
		setlocale (LC_NUMERIC, "C");
		g_value_set_double (value, atof (thevalue));
		setlocale (LC_NUMERIC, "");
	}
	else if (type == GDA_TYPE_NUMERIC) {
		GdaNumeric numeric;
		numeric.number = g_strdup (thevalue);
		numeric.precision = 0; /* FIXME */
		numeric.width = 0; /* FIXME */
		gda_value_set_numeric (value, &numeric);
		g_free (numeric.number);
	}
	else if (type == G_TYPE_DATE) {
		GDate *gdate;
		gdate = g_date_new ();
		g_date_set_parse (gdate, thevalue);
		if (!g_date_valid (gdate)) {
			g_warning ("Could not parse '%s' "
				"Setting date to 01/01/0001!\n", thevalue);
			g_date_clear (gdate, 1);
			g_date_set_dmy (gdate, 1, 1, 1);
		}
		g_value_take_boxed (value, gdate);
	}
	else if (type == GDA_TYPE_GEOMETRIC_POINT) {
		GdaGeometricPoint point;
		make_point (&point, thevalue);
		gda_value_set_geometric_point (value, &point);
	}
	else if (type == GDA_TYPE_TIMESTAMP) {
		GdaTimestamp timestamp;
		make_timestamp (&timestamp, thevalue);
		gda_value_set_timestamp (value, &timestamp);
	}
	else if (type == GDA_TYPE_TIME) {
		GdaTime timegda;
		make_time (&timegda, thevalue);
		gda_value_set_time (value, &timegda);
	}
	else if (type == GDA_TYPE_BINARY) {
		/*
		 * Requires PQunescapeBytea in libpq (present since 7.3.x)
		 */
		GdaBinary bin;
		guchar *unescaped;
		unescaped = PQunescapeBytea (thevalue, &(bin.binary_length));
		if (unescaped != NULL) {
			bin.data = unescaped;
			gda_value_set_binary (value, &bin);
			PQfreemem (unescaped);
		} 
		else {
			g_warning ("Error unescaping string: %s\n", thevalue);
			g_value_set_string (value, thevalue);
		}
	}
	else if (type == GDA_TYPE_BLOB) {
		GdaBlob *blob;
		blob = gda_postgres_blob_new (cnc);
		gda_postgres_blob_set_id (GDA_POSTGRES_BLOB (blob), atoi (thevalue));
		gda_value_set_blob (value, blob);
	}
	else {
		gda_value_reset_with_type (value, G_TYPE_STRING);
		g_value_set_string (value, thevalue);
	}
}

gchar *
gda_postgres_value_to_sql_string (GValue *value)
{
	gchar *val_str;
	gchar *ret;
	GType type;

	g_return_val_if_fail (value != NULL, NULL);

	val_str = gda_value_stringify (value);
	if (!val_str)
		return NULL;

	type = G_VALUE_TYPE (value);

	if ((type == G_TYPE_INT64) ||
	    (type == G_TYPE_DOUBLE) ||
	    (type == G_TYPE_INT) ||
	    (type == GDA_TYPE_NUMERIC) ||
	    (type == G_TYPE_FLOAT) ||
	    (type == GDA_TYPE_SHORT) ||
	    (type == G_TYPE_CHAR))
		ret = g_strdup (val_str);
	else
		ret = g_strdup_printf ("\'%s\'", val_str);

	g_free (val_str);

	return ret;
}
