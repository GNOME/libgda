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

#ifdef PARENT_TYPE
#undef PARENT_TYPE
#endif

#define PARENT_TYPE GDA_TYPE_DATA_MODEL_ARRAY

struct _GdaPostgresRecordsetPrivate {
	PGresult *pg_res;
	GdaConnection *cnc;
	GdaValueType *column_types;
	gint ncolumns;
	gint nrows;

	gint ntypes;
	GdaPostgresTypeOid *type_data;
	GHashTable *h_table;
};

static void gda_postgres_recordset_class_init (GdaPostgresRecordsetClass *klass);
static void gda_postgres_recordset_init       (GdaPostgresRecordset *recset,
					       GdaPostgresRecordsetClass *klass);
static void gda_postgres_recordset_finalize   (GObject *object);

static const GdaValue *gda_postgres_recordset_get_value_at    (GdaDataModel *model, gint col, gint row);
static GdaFieldAttributes *gda_postgres_recordset_describe    (GdaDataModel *model, gint col);
static gint gda_postgres_recordset_get_n_rows 		      (GdaDataModel *model);

static GObjectClass *parent_class = NULL;

/*
 * Private functions
 */

/*
 * Object init and finalize
 */
static void
gda_postgres_recordset_init (GdaPostgresRecordset *recset,
			     GdaPostgresRecordsetClass *klass)
{
	g_return_if_fail (GDA_IS_POSTGRES_RECORDSET (recset));

	recset->priv = g_new0 (GdaPostgresRecordsetPrivate, 1);
}

static void
gda_postgres_recordset_class_init (GdaPostgresRecordsetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaDataModelClass *model_class = GDA_DATA_MODEL_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_postgres_recordset_finalize;
	model_class->get_n_rows = gda_postgres_recordset_get_n_rows;
	model_class->describe_column = gda_postgres_recordset_describe;
	model_class->get_value_at = gda_postgres_recordset_get_value_at;
}

static void
gda_postgres_recordset_finalize (GObject * object)
{
	GdaPostgresRecordset *recset = (GdaPostgresRecordset *) object;

	g_return_if_fail (GDA_IS_POSTGRES_RECORDSET (recset));

	if (recset->priv->pg_res != NULL) {
		PQclear (recset->priv->pg_res);
		recset->priv->pg_res = NULL;
	}

	g_free (recset->priv->column_types);
	recset->priv->column_types = NULL;
	g_free (recset->priv);
	recset->priv = NULL;

	parent_class->finalize (object);
}

static GdaValueType *
get_column_types (GdaPostgresRecordsetPrivate *priv)
{
	gint i, ncols;
	GdaValueType *types;

	ncols = PQnfields (priv->pg_res);
	types = g_new (GdaValueType, ncols);
	for (i = 0; i < ncols; i++)
		types [i] = gda_postgres_type_oid_to_gda (priv->type_data,
							  priv->ntypes, 
							  PQftype (priv->pg_res, i));

	return types;
}

static GList *
get_row (GdaPostgresRecordsetPrivate *priv, gint row)
{
	gchar *thevalue;
	GdaValueType ftype;
	gboolean isNull;
	GdaValue *value;
	gint i;
	GList *row_list = NULL;

	for (i = 0; i < priv->ncolumns; i++) {
		thevalue = PQgetvalue(priv->pg_res, row, i);
		ftype = priv->column_types [i];
		isNull = *thevalue != '\0' ? FALSE : PQgetisnull (priv->pg_res, row, i);
		value = gda_value_new_null ();

		gda_postgres_set_value (value, ftype, thevalue,
					PQfsize (priv->pg_res, i),
					isNull);

		row_list = g_list_append (row_list, value);
	}
	return row_list;
}

/*
 * Overrides
 */

static const GdaValue *
gda_postgres_recordset_get_value_at (GdaDataModel *model, gint col, gint row)
{
	GdaPostgresRecordset *recset = (GdaPostgresRecordset *) model;
	GdaPostgresRecordsetPrivate *priv_data;
	gint field_count;
	gint row_count;
	PGresult *pg_res;
	gint current_row, fetched_rows;
	GList *row_list;

	g_return_val_if_fail (GDA_IS_POSTGRES_RECORDSET (recset), NULL);
	g_return_val_if_fail (recset->priv != NULL, 0);
	
	fetched_rows = GDA_DATA_MODEL_CLASS (parent_class)->get_n_rows (model);
	if (row >= 0 && row < fetched_rows)
		return GDA_DATA_MODEL_CLASS (parent_class)->get_value_at (model, col, row);

	priv_data = recset->priv;
	pg_res = priv_data->pg_res;
	if (!pg_res) {
		gda_connection_add_error_string (priv_data->cnc,
						 _("Invalid PostgreSQL handle"));
		return NULL;
	}

	row_count = priv_data->nrows;
	field_count = priv_data->ncolumns;

	if (row == row_count)
		return NULL; // For the last row don't add an error.

	if (row < 0 || row > row_count) {
		gda_connection_add_error_string (priv_data->cnc,
						_("Row number out of range"));
		return NULL;
	}

	if (field_count < 0 || col >= field_count) {
		gda_connection_add_error_string (priv_data->cnc,
						_("Column number out of range"));
		return NULL;
	}

	for (current_row = fetched_rows; current_row <= row; current_row++){
		row_list = get_row (priv_data, current_row);
		gda_data_model_append_row (GDA_DATA_MODEL (model), row_list);
		g_list_foreach (row_list, (GFunc) gda_value_free, NULL);
	}

	return GDA_DATA_MODEL_CLASS (parent_class)->get_value_at (model, col, row);
}

static GdaFieldAttributes *
gda_postgres_recordset_describe (GdaDataModel *model, gint col)
{
	GdaPostgresRecordset *recset = (GdaPostgresRecordset *) model;
	GdaPostgresRecordsetPrivate *priv_data;
	PGresult *pg_res;
	gint field_count;
	GdaValueType ftype;
	gint scale;
	GdaFieldAttributes *field_attrs;

	g_return_val_if_fail (GDA_IS_POSTGRES_RECORDSET (recset), NULL);
	g_return_val_if_fail (recset->priv != NULL, 0);

	priv_data = recset->priv;
	pg_res = priv_data->pg_res;
	if (!pg_res) {
		gda_connection_add_error_string (priv_data->cnc,
						_("Invalid PostgreSQL handle"));
		return NULL;
	}

	field_count = PQnfields (pg_res);
	if (field_count >= col){
		gda_connection_add_error_string (priv_data->cnc,
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
gda_postgres_recordset_get_n_rows (GdaDataModel *model)
{
	GdaPostgresRecordset *recset = (GdaPostgresRecordset *) model;
	GdaPostgresRecordsetPrivate *priv_data;
	PGresult *pg_res;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), 0);
	g_return_val_if_fail (recset->priv != NULL, 0);

	priv_data = recset->priv;
	pg_res = priv_data->pg_res;
	if (!pg_res)
		return 0;

	return PQntuples (pg_res);
}

/*
 * Public functions
 */

GType
gda_postgres_recordset_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaPostgresRecordsetClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_postgres_recordset_class_init,
			NULL,
			NULL,
			sizeof (GdaPostgresRecordset),
			0,
			(GInstanceInitFunc) gda_postgres_recordset_init
		};
		type = g_type_register_static (PARENT_TYPE, "GdaPostgresRecordset", &info, 0);
	}
	return type;
}

GdaDataModel *
gda_postgres_recordset_new (GdaConnection *cnc, PGresult *pg_res)
{
	GdaPostgresRecordset *model;
	GdaPostgresConnectionData *cnc_priv_data;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (pg_res != NULL, NULL);

	cnc_priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);

	model = g_object_new (GDA_TYPE_POSTGRES_RECORDSET, NULL);
	model->priv->pg_res = pg_res;
	model->priv->cnc = cnc;
	model->priv->ntypes = cnc_priv_data->ntypes;
	model->priv->type_data = cnc_priv_data->type_data;
	model->priv->h_table = cnc_priv_data->h_table;
	model->priv->ncolumns = PQnfields (pg_res);
	model->priv->nrows = PQntuples (pg_res);
	model->priv->column_types = get_column_types (model->priv);
	gda_data_model_array_set_n_columns (GDA_DATA_MODEL_ARRAY (model),
					    model->priv->ncolumns);

	return GDA_DATA_MODEL (model);
}

