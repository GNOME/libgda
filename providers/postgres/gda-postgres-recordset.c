/* GDA DB Postgres provider
 * Copyright (C) 1998-2002 The GNOME Foundation
 *
 * AUTHORS:
 *      Michael Lausch <michael@lausch.at>
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *      Vivien Malerba <malerba@gnome-db.org>
 *      Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
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
#include <string.h>
#include "gda-postgres.h"

#define OBJECT_DATA_RECSET_HANDLE "GDA_Postgres_RecsetHandle"

typedef struct {
	PGresult *pg_res;
	gint ntypes;
	GdaPostgresTypeOid *type_data;
	GHashTable *h_table;
} GdaPostgresRecordsetPrivate;

/*
 * Private functions
 */

static void
free_postgres_res (gpointer data)
{
	GdaPostgresRecordsetPrivate *priv_data = (GdaPostgresRecordsetPrivate *) data;

	g_return_if_fail (priv_data != NULL);

	PQclear (priv_data->pg_res);
	g_free (priv_data);
}

static GdaRow *
fetch_func (GdaRecordset *recset, gulong rownum)
{
	GdaPostgresRecordsetPrivate *priv_data;
	GdaRow *row;
	gint field_count;
	gint row_count;
	gint i;
	PGresult *pg_res;

	g_return_val_if_fail (GDA_IS_RECORDSET (recset), NULL);

	priv_data = g_object_get_data (G_OBJECT (recset), OBJECT_DATA_RECSET_HANDLE);
	pg_res = priv_data->pg_res;
	if (!pg_res) {
		gda_connection_add_error_string (
			gda_recordset_get_connection (recset),
			_("Invalid PostgreSQL handle"));
		return NULL;
	}

	row_count = PQntuples (pg_res);
	field_count = PQnfields (pg_res);

	if (rownum == row_count)
		return NULL; // For the last row don't add an error.

	if (rownum < 0 || rownum > row_count) {
		gda_connection_add_error_string (
			gda_recordset_get_connection (recset),
			_("Row number out of range"));
		return NULL;
	}

	row = gda_row_new (field_count);

	for (i = 0; i < field_count; i++) {
		GdaField *field;
		gchar *thevalue;
		GdaValueType ftype;
		gboolean isNull;

		field = gda_row_get_field (row, i);
		thevalue = PQgetvalue(pg_res, rownum, i);

		ftype = gda_postgres_type_oid_to_gda (priv_data->type_data,
						      priv_data->ntypes, 
						      PQftype (pg_res, i));
		isNull = *thevalue != '\0' ? FALSE : PQgetisnull (pg_res, rownum, i);

		gda_postgres_set_field_data (field, PQfname (pg_res, i), ftype,
					     thevalue, PQfsize (pg_res, i), isNull);
	}

	return row;
}

static GdaRowAttributes *
describe_func (GdaRecordset *recset)
{
	GdaPostgresRecordsetPrivate *priv_data;
	PGresult *pg_res;
	gint field_count;
	gint i;
	GdaRowAttributes *attrs;

	g_return_val_if_fail (GDA_IS_RECORDSET (recset), NULL);

	priv_data = g_object_get_data (G_OBJECT (recset), OBJECT_DATA_RECSET_HANDLE);
	pg_res = priv_data->pg_res;
	if (!pg_res) {
		gda_connection_add_error_string (
			gda_recordset_get_connection (recset),
			_("Invalid PostgreSQL handle"));
		return NULL;
	}

	/* create the GdaRowAttributes to be returned */
	field_count = PQnfields (pg_res);
	attrs = gda_row_attributes_new (field_count);
	
	for (i = 0; i < field_count; i++) {
		GdaFieldAttributes *field_attrs;
		GdaValueType ftype;
		gint scale;

		field_attrs = gda_row_attributes_get_field (attrs, i);
		gda_field_attributes_set_name (field_attrs, PQfname (pg_res, i));

		ftype = gda_postgres_type_oid_to_gda (priv_data->type_data,
						      priv_data->ntypes, 
						      PQftype (pg_res, i));

		scale = (ftype == GDA_VALUE_TYPE_DOUBLE) ? DBL_DIG :
			(ftype == GDA_VALUE_TYPE_SINGLE) ? FLT_DIG : 0;

		gda_field_attributes_set_scale (field_attrs, scale);
		gda_field_attributes_set_gdatype (field_attrs, ftype);

		// PQfsize() == -1 => variable length
		gda_field_attributes_set_defined_size (field_attrs, 
							PQfsize (pg_res, i));
		gda_field_attributes_set_references (field_attrs, "");
		gda_field_attributes_set_primary_key (field_attrs, FALSE);
		gda_field_attributes_set_unique_key (field_attrs, FALSE);
	}

	return attrs;
}

/*
 * Public functions
 */

GdaRecordset *
gda_postgres_recordset_new (GdaConnection *cnc, PGresult *pg_res)
{
	GdaRecordset *recset;
	GdaPostgresRecordsetPrivate *priv_data;
	GdaPostgresConnectionData *cnc_priv_data;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (pg_res != NULL, NULL);

	recset = gda_recordset_new (cnc, fetch_func, describe_func);

	cnc_priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);

	priv_data = g_new (GdaPostgresRecordsetPrivate, 1);
	priv_data->pg_res = pg_res;
	priv_data->ntypes = cnc_priv_data->ntypes;
	priv_data->type_data = cnc_priv_data->type_data;
	priv_data->h_table = cnc_priv_data->h_table;

	g_object_set_data_full (G_OBJECT (recset), OBJECT_DATA_RECSET_HANDLE,
				priv_data, (GDestroyNotify) free_postgres_res);

	return recset;
}

