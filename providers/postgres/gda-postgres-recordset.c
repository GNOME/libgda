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

#define PARENT_TYPE GDA_TYPE_DATA_MODEL_HASH

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
static const GdaRow *gda_postgres_recordset_get_row 	      (GdaDataModel *model, gint rownum);

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
	model_class->get_row = gda_postgres_recordset_get_row;
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
	gint i;
	GdaValueType *types;

	types = g_new (GdaValueType, priv->ncolumns);
	for (i = 0; i < priv->ncolumns; i++)
		types [i] = gda_postgres_type_oid_to_gda (priv->type_data,
							  priv->ntypes, 
							  PQftype (priv->pg_res, i));

	return types;
}

static GdaRow *
get_row (GdaDataModel *model, GdaPostgresRecordsetPrivate *priv, gint rownum)
{
	gchar *thevalue;
	GdaValueType ftype;
	gboolean isNull;
	GdaValue *value;
	GdaRow *row;
	gint i;
	gchar *id;
	gint length;
	
	row = gda_row_new (model, priv->ncolumns);
	for (i = 0; i < priv->ncolumns; i++) {
		thevalue = PQgetvalue(priv->pg_res, rownum, i);
		length = PQgetlength (priv->pg_res, rownum, i);
		ftype = priv->column_types [i];
		isNull = *thevalue != '\0' ? FALSE : PQgetisnull (priv->pg_res, rownum, i);
		value = gda_row_get_value (row, i);
		gda_postgres_set_value (value, ftype, thevalue, isNull, length);
	}

	id = g_strdup_printf ("%d", rownum);
	gda_row_set_id (row, id); // FIXME: by now, the rowid is just the row number
	g_free (id);
	return row;
}

/*
 * Overrides
 */

static const GdaRow *
gda_postgres_recordset_get_row (GdaDataModel *model, gint row)
{
	GdaPostgresRecordset *recset = (GdaPostgresRecordset *) model;
	GdaPostgresRecordsetPrivate *priv_data;
	GdaRow *row_list;
	
	g_return_val_if_fail (GDA_IS_POSTGRES_RECORDSET (recset), NULL);
	g_return_val_if_fail (recset->priv != NULL, 0);

	row_list = (GdaRow *) gda_data_model_hash_get_row (model, row);
	if (row_list != NULL)
		return (const GdaRow *)row_list;

	priv_data = recset->priv;
	if (!priv_data->pg_res) {
		gda_connection_add_error_string (priv_data->cnc,
						 _("Invalid PostgreSQL handle"));
		return NULL;
	}

	if (row == priv_data->nrows)
		return NULL; // For the last row don't add an error.

	if (row < 0 || row > priv_data->nrows) {
		gda_connection_add_error_string (priv_data->cnc,
						_("Row number out of range"));
		return NULL;
	}

	row_list = get_row (model, priv_data, row);
	gda_data_model_hash_insert_row (GDA_DATA_MODEL_HASH (model),
					 row, row_list);

	return (const GdaRow *) row_list;
}

static const GdaValue *
gda_postgres_recordset_get_value_at (GdaDataModel *model, gint col, gint row)
{
	GdaPostgresRecordset *recset = (GdaPostgresRecordset *) model;
	GdaPostgresRecordsetPrivate *priv_data;
	PGresult *pg_res;
	GdaRow *row_list;
	const GdaValue *value;

	g_return_val_if_fail (GDA_IS_POSTGRES_RECORDSET (recset), NULL);
	g_return_val_if_fail (recset->priv != NULL, 0);
	
	value = gda_data_model_hash_get_value_at (model, col, row);
	if (value != NULL)
		return value;

	priv_data = recset->priv;
	pg_res = priv_data->pg_res;
	if (!pg_res) {
		gda_connection_add_error_string (priv_data->cnc,
						 _("Invalid PostgreSQL handle"));
		return NULL;
	}

	if (row == priv_data->nrows)
		return NULL; // For the last row don't add an error.

	if (row < 0 || row > priv_data->nrows) {
		gda_connection_add_error_string (priv_data->cnc,
						_("Row number out of range"));
		return NULL;
	}

	if (col >= priv_data->ncolumns) {
		gda_connection_add_error_string (priv_data->cnc,
						_("Column number out of range"));
		return NULL;
	}

	row_list = get_row (model, priv_data, row);
	gda_data_model_hash_insert_row (GDA_DATA_MODEL_HASH (model),
					 row, row_list);
	return gda_row_get_value (row_list, col);
}

static GdaFieldAttributes *
gda_postgres_recordset_describe (GdaDataModel *model, gint col)
{
	GdaPostgresRecordset *recset = (GdaPostgresRecordset *) model;
	GdaPostgresRecordsetPrivate *priv_data;
	PGresult *pg_res;
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

	if (col >= priv_data->ncolumns){
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

	g_return_val_if_fail (GDA_IS_POSTGRES_RECORDSET (model), 0);
	g_return_val_if_fail (recset->priv != NULL, 0);

	return recset->priv->nrows;
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
	gchar *cmd_tuples;
	gchar *endptr [1];

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
	cmd_tuples = PQcmdTuples (pg_res);
	if (cmd_tuples == NULL || *cmd_tuples == '\0'){
		model->priv->nrows = PQntuples (pg_res);
	} else {
		model->priv->nrows = strtol (cmd_tuples, endptr, 10);
		if (**endptr != '\0')
			g_warning (_("Tuples:\"%s\""), cmd_tuples);

	}
	model->priv->column_types = get_column_types (model->priv);
	gda_data_model_hash_set_n_columns (GDA_DATA_MODEL_HASH (model),
					    model->priv->ncolumns);

	return GDA_DATA_MODEL (model);
}

