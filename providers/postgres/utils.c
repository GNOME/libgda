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

#include <stdlib.h>
#include <string.h>
#include "gda-postgres.h"

GdaError *
gda_postgres_make_error (PGconn *handle)
{
	GdaError *error;

	error = gda_error_new ();
	if (handle != NULL) {
		gda_error_set_description (error, PQerrorMessage (handle));
		gda_error_set_number (error, -1);
	} else {
		gda_error_set_description (error, _("NO DESCRIPTION"));
		gda_error_set_number (error, -1);
	}

	gda_error_set_source (error, "gda-postgres");
	gda_error_set_sqlstate (error, _("Not available"));

	return error;
}

GdaType
gda_postgres_type_name_to_gda (const gchar *name)
{
	if (!strcmp (name, "bool"))
		return GDA_TYPE_BOOLEAN;
	if (!strcmp (name, "int8"))
		return GDA_TYPE_BIGINT;
	if (!strcmp (name, "int4") || !strcmp (name, "abstime") || !strcmp (name, "oid"))
		return GDA_TYPE_INTEGER;
	if (!strcmp (name, "int2"))
		return GDA_TYPE_SMALLINT;
	if (!strcmp (name, "float4"))
		return GDA_TYPE_SINGLE;
	//TODO: numeric handling
	if (!strcmp (name, "float8") || !strcmp (name, "numeric"))
		return GDA_TYPE_DOUBLE;
	if (!strncmp (name, "timestamp", 9))
		return GDA_TYPE_TIMESTAMP;
	if (!strcmp (name, "date"))
		return GDA_TYPE_DATE;
	if (!strncmp (name, "time", 4))
		return GDA_TYPE_TIME;
	/*TODO: by now, this one not supported
	if (!strncmp (name, "bit", 3))
		return GDA_TYPE_BINARY;
	*/
	if (!strcmp (name, "point"))
		return GDA_TYPE_GEOMETRIC_POINT;

	return GDA_TYPE_STRING;
}

GdaType
gda_postgres_type_oid_to_gda (GdaPostgresTypeOid *type_data, gint ntypes, Oid postgres_type)
{
	gint i;

	for (i = 0; i < ntypes; i++)
		if (type_data[i].oid == postgres_type) 
			break;

	if (type_data[i].oid != postgres_type)
		return GDA_TYPE_STRING;

	return gda_postgres_type_name_to_gda (type_data[i].name);
}

/* Makes a point from a string like "(3.2,5.6)" */
static GdaGeometricPoint *
make_point (const gchar *value)
{
	GdaGeometricPoint *point;

	g_return_val_if_fail (value != NULL, NULL);
	
	point = g_new (GdaGeometricPoint, 1);
	value++;
	point->x = atof (value);
	value = strchr (value, ',');
	value++;
	point->y = atof (value);

	return point;
}

static void
delete_point (GdaGeometricPoint *point)
{
	g_return_if_fail (point != NULL);
	
	g_free (point);
}

/* Makes a GdaTime from a string like "12:30:15+01" */
static GdaTime *
make_time (const gchar *value)
{
	GdaTime *time;

	g_return_val_if_fail (value != NULL, NULL);
	
	time = g_new (GdaTime, 1);
	time->hour = atoi (value);
	value += 3;
	time->minute = atoi (value);
	value += 3;
	time->second = atoi (value);
	value += 2;
	if (*value)
		time->timezone = atoi (value);
	else
		time->timezone = TIMEZONE_INVALID;

	return time;
}

static void
delete_time (GdaTime *time)
{
	g_return_if_fail (time != NULL);
	
	g_free (time);
}

/* Makes a GdaTimestamp from a string like "2003-12-13 13:12:01.12+01" */
static GdaTimestamp *
make_timestamp (const gchar *value)
{
	GdaTimestamp *timestamp;

	g_return_val_if_fail (value != NULL, NULL);
	
	timestamp = g_new (GdaTimestamp, 1);
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

	return timestamp;
}

static void
delete_timestamp (GdaTimestamp *timestamp)
{
	g_return_if_fail (timestamp != NULL);
	
	g_free (timestamp);
}

void 
gda_postgres_set_field_data (GdaField *field, const gchar *fname,
				GdaType type, const gchar *value, 
				gint dbsize, gboolean isNull)
{
	GDate *gdate;
	GdaDate *date;
	GdaTime *time;
	GdaTimestamp *timestamp;
	GdaGeometricPoint *point;
	gint scale;

	g_return_if_fail (field != NULL);
	g_return_if_fail (fname != NULL);
	g_return_if_fail (value != NULL);

	//TODO: What do I do with BLOBs?

	gda_field_set_name (field, fname);
	// dbsize == -1 => variable length
	gda_field_set_defined_size (field, dbsize);
	scale = (type == GDA_TYPE_DOUBLE) ? DBL_DIG :
		(type == GDA_TYPE_SINGLE) ? FLT_DIG : 0;
	gda_field_set_scale (field, scale);

	if (isNull) 
		type = GDA_TYPE_NULL;

	switch (type) {
	case GDA_TYPE_BOOLEAN :
		gda_field_set_gdatype (field, type);
		gda_field_set_boolean_value (field, 
				(*value == 't') ? TRUE : FALSE);
		gda_field_set_actual_size (field, sizeof (gboolean));
		break;
	case GDA_TYPE_STRING :
		gda_field_set_gdatype (field, type);
		gda_field_set_string_value (field, value);
		gda_field_set_actual_size (field, strlen (value));
		break;
	case GDA_TYPE_BIGINT :
		gda_field_set_gdatype (field, type);
		gda_field_set_bigint_value (field, atoll (value));
		gda_field_set_actual_size (field, sizeof (gint64));
		break;
	case GDA_TYPE_INTEGER :
		gda_field_set_gdatype (field, type);
		gda_field_set_integer_value (field, atol (value));
		gda_field_set_actual_size (field, sizeof (gint32));
		break;
	case GDA_TYPE_SMALLINT :
		gda_field_set_gdatype (field, type);
		gda_field_set_smallint_value (field, atoi (value));
		gda_field_set_actual_size (field, sizeof (gint16));
		break;
	case GDA_TYPE_SINGLE :
		gda_field_set_gdatype (field, type);
		gda_field_set_single_value (field, atof (value));
		gda_field_set_actual_size (field, sizeof (gfloat));
		break;
	case GDA_TYPE_DOUBLE :
		gda_field_set_gdatype (field, type);
		gda_field_set_double_value (field, atof (value));
		gda_field_set_actual_size (field, sizeof (gdouble));
		break;
	case GDA_TYPE_DATE :
		gda_field_set_gdatype (field, type);
		gdate = g_date_new ();
		g_date_set_parse (gdate, value);
		if (!g_date_valid (gdate)) {
			g_warning ("Could not parse '%s' "
				"Setting date to 01/01/0001!\n", value);
			g_date_clear (gdate, 1);
			g_date_set_dmy (gdate, 1, 1, 1);
		}
		date = g_new (GdaDate, 1);
		date->day = g_date_get_day (gdate);
		date->month = g_date_get_month (gdate);
		date->year = g_date_get_year (gdate);
		gda_field_set_date_value (field, date);
		g_free (date);
		g_date_free (gdate);
		gda_field_set_actual_size (field, sizeof (GdaDate));
		break;
	case GDA_TYPE_GEOMETRIC_POINT :
		point = make_point (value);
		gda_field_set_geometric_point_value (field, point);
		gda_field_set_gdatype (field, type);
		delete_point (point);
		gda_field_set_actual_size (field, sizeof (GdaGeometricPoint));
		break;
	case GDA_TYPE_NULL :
		gda_field_set_gdatype (field, type);
		gda_field_set_null_value (field);
		gda_field_set_actual_size (field, 0);
		break;
	case GDA_TYPE_TIMESTAMP :
		timestamp = make_timestamp (value);
		gda_field_set_timestamp_value (field, timestamp);
		gda_field_set_gdatype (field, type);
		delete_timestamp (timestamp);
		gda_field_set_actual_size (field, sizeof (GdaTimestamp));
		break;
	case GDA_TYPE_TIME :
		time = make_time (value);
		gda_field_set_time_value (field, time);
		gda_field_set_gdatype (field, type);
		delete_time (time);
		gda_field_set_actual_size (field, sizeof (GdaTime));
		break;
	case GDA_TYPE_BINARY : //FIXME
	default :
		gda_field_set_string_value (field, value);
		gda_field_set_gdatype (field, GDA_TYPE_STRING);
		gda_field_set_actual_size (field, strlen (value));
	}
}

