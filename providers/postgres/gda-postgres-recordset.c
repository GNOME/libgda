/* GDA Postgres provider
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
#include "gda-postgres-recordset.h"

#define OBJECT_DATA_RECSET_HANDLE "GDA_Postgres_RecsetHandle"

typedef struct {
	PGresult *pg_res;
	GdaConnection *gda_conn;

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

static const GdaValue *
get_value_at_func (GdaDataModel *model, gint col, gint row)
{
	GdaPostgresRecordsetPrivate *priv_data;
	gint field_count;
	gint row_count;
	PGresult *pg_res;
	GdaValue *value;
	gchar *thevalue;
	GdaValueType ftype;
	gboolean isNull;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);
	priv_data = g_object_get_data (G_OBJECT (model), OBJECT_DATA_RECSET_HANDLE);
	g_return_val_if_fail (priv_data != NULL, 0);

	pg_res = priv_data->pg_res;
	if (!pg_res) {
		gda_connection_add_error_string (priv_data->gda_conn,
						_("Invalid PostgreSQL handle"));
		return NULL;
	}

	row_count = PQntuples (pg_res);
	field_count = PQnfields (pg_res);

	if (row == row_count)
		return NULL; // For the last row don't add an error.

	if (row < 0 || row > row_count) {
		gda_connection_add_error_string (priv_data->gda_conn,
						_("Row number out of range"));
		return NULL;
	}

	if (field_count < 0 || col > field_count) {
		gda_connection_add_error_string (priv_data->gda_conn,
						_("Column number out of range"));
		return NULL;
	}

	thevalue = PQgetvalue(pg_res, row, col);

	ftype = gda_postgres_type_oid_to_gda (priv_data->type_data,
					      priv_data->ntypes, 
					      PQftype (pg_res, col));
	isNull = *thevalue != '\0' ? FALSE : PQgetisnull (pg_res, row, col);

	value = gda_value_new_null ();
	gda_postgres_set_value (value, PQfname (pg_res, col), ftype,
			 	thevalue, PQfsize (pg_res, col), isNull);

	return value;
}

static GdaFieldAttributes *
describe_func (GdaDataModel *model, gint col)
{
	GdaPostgresRecordsetPrivate *priv_data;
	PGresult *pg_res;
	gint field_count;
	GdaValueType ftype;
	gint scale;
	GdaFieldAttributes *field_attrs;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);
	priv_data = g_object_get_data (G_OBJECT (model), OBJECT_DATA_RECSET_HANDLE);
	g_return_val_if_fail (priv_data != NULL, 0);

	pg_res = priv_data->pg_res;
	if (!pg_res) {
		gda_connection_add_error_string (priv_data->gda_conn,
						_("Invalid PostgreSQL handle"));
		return NULL;
	}

	field_count = PQnfields (pg_res);
	if (field_count >= col){
		gda_connection_add_error_string (priv_data->gda_conn,
						_("Column out of range"));
		return NULL;
	}

	field_attrs = gda_field_attributes_new ();
	gda_field_attributes_set_name (field_attrs, PQfname (pg_res, col));

	ftype = gda_postgres_type_oid_to_gda (priv_data->type_data,
					      priv_data->ntypes, 
					      PQftype (pg_res, col));

	scale = (ftype == GDA_VALUE_TYPE_DOUBLE) ? DBL_DIG :
		(ftype == GDA_VALUE_TYPE_SINGLE) ? FLT_DIG : 0;

	gda_field_attributes_set_scale (field_attrs, scale);
	gda_field_attributes_set_gdatype (field_attrs, ftype);

	// PQfsize() == -1 => variable length
	gda_field_attributes_set_defined_size (field_attrs, PQfsize (pg_res, col));
	gda_field_attributes_set_references (field_attrs, "");
	gda_field_attributes_set_primary_key (field_attrs, FALSE);
	gda_field_attributes_set_unique_key (field_attrs, FALSE);
	//FIXME: set_allow_null?

	return field_attrs;
}

static gint
get_n_columns_func (GdaDataModel *model)
{
	GdaPostgresRecordsetPrivate *priv_data;
	PGresult *pg_res;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), 0);
	priv_data = g_object_get_data (G_OBJECT (model), OBJECT_DATA_RECSET_HANDLE);
	g_return_val_if_fail (priv_data != NULL, 0);

	pg_res = priv_data->pg_res;
	if (!pg_res)
		return 0;

	return PQnfields (pg_res);
}

static gint
get_n_rows_func (GdaDataModel *model)
{
	GdaPostgresRecordsetPrivate *priv_data;
	PGresult *pg_res;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), 0);
	priv_data = g_object_get_data (G_OBJECT (model), OBJECT_DATA_RECSET_HANDLE);
	g_return_val_if_fail (priv_data != NULL, 0);

	pg_res = priv_data->pg_res;
	if (!pg_res)
		return 0;

	return PQntuples (pg_res);
}

/*
 * Public functions
 */

GdaDataModel *
gda_postgres_recordset_new (GdaConnection *cnc, PGresult *pg_res)
{
	GdaDataModel *model;
	GdaDataModelClass *klass;
	GdaPostgresRecordsetPrivate *priv_data;
	GdaPostgresConnectionData *cnc_priv_data;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (pg_res != NULL, NULL);

	cnc_priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);

	priv_data = g_new (GdaPostgresRecordsetPrivate, 1);
	priv_data->pg_res = pg_res;
	priv_data->gda_conn = cnc;
	priv_data->ntypes = cnc_priv_data->ntypes;
	priv_data->type_data = cnc_priv_data->type_data;
	priv_data->h_table = cnc_priv_data->h_table;

	model = g_object_new (GDA_TYPE_DATA_MODEL, NULL);
	klass = GDA_DATA_MODEL_CLASS (G_OBJECT_GET_CLASS (model));

	klass->changed = NULL;
	klass->begin_edit = NULL;
	klass->cancel_edit = NULL;
	klass->end_edit = NULL;
	klass->get_n_rows = get_n_rows_func;
	klass->get_n_columns = get_n_columns_func;
	klass->describe_column = describe_func;
	klass->get_value_at = get_value_at_func;

	g_object_set_data_full (G_OBJECT (model), OBJECT_DATA_RECSET_HANDLE,
				priv_data, (GDestroyNotify) free_postgres_res);

	return model;
}

