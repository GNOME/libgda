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

#include <postgres.h>
#include <catalog/pg_type.h>
#include "gda-postgres.h"

GdaError *
gda_postgres_make_error (PGconn *handle)
{
	GdaError *error;

	error = gda_error_new ();
	if (handle != NULL) {
		gda_error_set_description (error, PQerrorMessage(handle));
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
gda_postgres_type_to_gda (Oid postgres_type)
{
	switch (postgres_type) {
	case BOOLOID :
		return GDA_TYPE_BOOLEAN;
	case BYTEAOID :
	case CHAROID :
	case TEXTOID :
	case BPCHAROID :
	case VARCHAROID :
		return GDA_TYPE_STRING;
	case INT8OID :
		return GDA_TYPE_BIGINT;
	case INT4OID :
	case RELTIMEOID :
		return GDA_TYPE_INTEGER;
	case INT2OID :
		return GDA_TYPE_SMALLINT;
	case FLOAT4OID :
		return GDA_TYPE_SINGLE;
	case FLOAT8OID :
	case NUMERICOID :
		return GDA_TYPE_DOUBLE;
	case ABSTIMEOID :
	case TIMESTAMPOID :
		return GDA_TYPE_TIMESTAMP;
	case DATEOID :
		return GDA_TYPE_DATE;
	case TIMEOID :
	case TIMETZOID :
		return GDA_TYPE_TIME;
	case VARBITOID :
		return GDA_TYPE_BINARY;
	case POINTOID :
		return GDA_TYPE_GEOMETRIC_POINT;
	}

	return GDA_TYPE_UNKNOWN;
}

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

void 
gda_postgres_set_type_value (GdaField *field, GdaType type, const gchar *value)
{
	GDate *date;
	GdaGeometricPoint *point;

	switch (type) {
	case GDA_TYPE_BOOLEAN :
		gda_field_set_gdatype (field, type);
		gda_field_set_boolean_value (field, atoi (value));
		break;
	case GDA_TYPE_STRING :
		gda_field_set_gdatype (field, type);
		gda_field_set_string_value (field, value);
		break;
	case GDA_TYPE_BIGINT :
		gda_field_set_gdatype (field, type);
		gda_field_set_bigint_value (field, atoll (value));
		break;
	case GDA_TYPE_INTEGER :
		gda_field_set_gdatype (field, type);
		gda_field_set_integer_value (field, atol (value));
		break;
	case GDA_TYPE_SMALLINT :
		gda_field_set_gdatype (field, type);
		gda_field_set_smallint_value (field, atoi (value));
		break;
	case GDA_TYPE_SINGLE :
		gda_field_set_gdatype (field, type);
		gda_field_set_single_value (field, atof (value));
		break;
	case GDA_TYPE_DOUBLE :
		gda_field_set_gdatype (field, type);
		gda_field_set_double_value (field, atof (value));
		break;
	case GDA_TYPE_DATE :
		gda_field_set_gdatype (field, type);
		date = g_date_new ();
		g_date_set_parse (date, value);
		if (!g_date_valid (date)) {
			g_warning ("Could not parse '%s' "
				"Setting date to 01/01/0001!\n", field);
			g_date_clear (date, 1);
			g_date_set_dmy (date, 1, 1, 1);
		}
		gda_field_set_date_value (field, date);
		g_date_free (date);
		
		break;
	case GDA_TYPE_GEOMETRIC_POINT :
		point = make_point (value);
		gda_field_set_geometric_point_value (field, point);
		gda_field_set_gdatype (field, type);
		delete_point (point);
		break;
	case GDA_TYPE_TIMESTAMP : //FIXME
	case GDA_TYPE_TIME : //FIXME
	case GDA_TYPE_BINARY : //FIXME
	default :
		gda_field_set_string_value (field, value);
		gda_field_set_gdatype (field, GDA_TYPE_STRING);
	}
}

