/* GNOME DB Postgres Provider
 * Copyright (C) 1998-2002 The GNOME Foundation
 *
 * AUTHORS:
 *         Vivien Malerba <malerba@gnome-db.org>
 *         Rodrigo Moya <rodrigo@gnome-db.org>
 *         Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
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
#include "gda-postgres.h"

GdaError *
gda_postgres_make_error (PGconn *pconn, PGresult *pg_res)
{
	GdaError *error;

	error = gda_error_new ();
	if (pconn != NULL) {
		gchar *message;
		
		if (pg_res != NULL)
			message = PQresultErrorMessage (pg_res);
		else
			message = PQerrorMessage (pconn);

		gda_error_set_description (error, message);
	} else {
		gda_error_set_description (error, _("NO DESCRIPTION"));
	}

	gda_error_set_number (error, -1);
	gda_error_set_source (error, "gda-postgres");
	gda_error_set_sqlstate (error, _("Not available"));

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
	value += 3;
	timestamp->fraction = atol (value) * 10; // I have only hundredths of second
	value += 3;
	timestamp->timezone = atol (value) * 60 * 60;
}

void
gda_postgres_set_value (GdaValue *value,
			GdaValueType type,
			const gchar *thevalue,
			gboolean isNull)
{
	GDate *gdate;
	GdaDate date;
	GdaTime timegda;
	GdaTimestamp timestamp;
	GdaGeometricPoint point;
	GdaNumeric numeric;

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
		gda_value_set_single (value, atof (thevalue));
		break;
	case GDA_VALUE_TYPE_DOUBLE :
		gda_value_set_double (value, atof (thevalue));
		break;
	case GDA_VALUE_TYPE_NUMERIC :
		numeric.number = g_strdup (thevalue);
		numeric.precision = 0; //FIXME
		numeric.width = 0; //FIXME
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
	case GDA_VALUE_TYPE_BINARY : //FIXME
	default :
		gda_value_set_string (value, thevalue);
	}
}

