/* GDA Oracle provider
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 * 	Tim Coleman <tim@timcoleman.com>
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
#include "gda-oracle.h"
#include "gda-oracle-recordset.h"
#include "gda-oracle-provider.h"
#include <oci.h>

#define PARENT_TYPE GDA_TYPE_DATA_MODEL

struct _GdaOracleRecordsetPrivate {
	GdaConnection *cnc;
	GdaOracleConnectionData *cdata;
	GdaValueType *column_types;

	GdaOracleValue *ora_values;
	GPtrArray *rows;

	gint ncolumns;
	gint nrows;
};

struct _GdaOracleValue {
	OCIDefine *hdef;
	OCIParam *pard;
	sb2 indicator;
	ub2 sql_type;
	ub2 defined_size;
	gpointer value;
};

static void gda_oracle_recordset_class_init (GdaOracleRecordsetClass *klass);
static void gda_oracle_recordset_init       (GdaOracleRecordset *recset,
					    GdaOracleRecordsetClass *klass);
static void gda_oracle_recordset_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

/*
 * Private functions
 */

static GdaValueType *
get_column_types (GdaOracleRecordsetPrivate *priv)
{	
	GdaValueType *types;
	gint i;

	types = g_new (GdaValueType, priv->ncolumns);
	for (i = 0; i < priv->ncolumns; i += 1) {
		types[i] = oracle_sqltype_to_gda_type (priv->ora_values[i].sql_type);
	}
	return types;
}

static GdaOracleValue *
define_columns (GdaOracleRecordsetPrivate *priv)
{
	GdaOracleValue *fields;
	gint i;

	fields = g_new (GdaOracleValue, priv->ncolumns);

	for (i = 0; i < priv->ncolumns; i += 1) {

                if (OCI_SUCCESS != 
			OCIParamGet (priv->cdata->hstmt,
					OCI_HTYPE_STMT,
					priv->cdata->herr,
					(dvoid **) &(fields[i].pard),
					(ub4) i+1)) {
			break;
		}

		OCIAttrGet ((dvoid *) (fields[i].pard),
				OCI_DTYPE_PARAM,
				&(fields[i].defined_size),
				0,
				OCI_ATTR_DATA_SIZE,
				priv->cdata->herr);

		OCIAttrGet ((dvoid *) (fields[i].pard),
				OCI_DTYPE_PARAM,
				&(fields[i].sql_type),
				0,
				OCI_ATTR_DATA_TYPE,
				priv->cdata->herr);

		fields[i].value = g_malloc0 (fields[i].defined_size);
		fields[i].hdef = NULL;
		fields[i].indicator = 0;

		if (OCI_SUCCESS !=
			OCIDefineByPos ((OCIStmt *) priv->cdata->hstmt,
					(OCIDefine **) &(fields[i].hdef),
					(OCIError *) priv->cdata->herr,
					(ub4) i + 1,
					(dvoid *)fields[i].value,
					fields[i].defined_size,
					(ub2) fields[i].sql_type,
					(dvoid *) &(fields[i].indicator),
					(ub2) 0,
					(ub2) 0, 
					(ub4) OCI_DEFAULT)) {
			break;
		}
	}
	return fields;
}


/*
 * GdaOracleRecordset class implementation
 */

static GdaFieldAttributes *
gda_oracle_recordset_describe_column (GdaDataModel *model, gint col)
{
	GdaOracleRecordset *recset = (GdaOracleRecordset *) model;
	GdaOracleRecordsetPrivate *priv_data;
	GdaFieldAttributes *field_attrs;
	OCIStmt *stmthp;
	OCIParam *pard;
	gchar *pgchar_dummy;
	glong col_name_len;

	gchar name_buffer[ORA_NAME_BUFFER_SIZE + 1];
	ub2 sql_type;
	ub2 scale;
	ub2 nullable;

	g_return_val_if_fail (GDA_IS_ORACLE_RECORDSET (recset), NULL);
	g_return_val_if_fail (recset->priv != NULL, NULL);

	priv_data = recset->priv;
	stmthp = priv_data->cdata->hstmt;

	if (!stmthp) {
		gda_connection_add_error_string (priv_data->cnc, 
						_("Invalid Oracle handle"));
		return NULL;
	}

	if (col >= priv_data->ncolumns) {
		gda_connection_add_error_string (priv_data->cnc, 
						_("Column out of range"));
		return NULL;
	}

	field_attrs = gda_field_attributes_new ();

	if (OCI_SUCCESS !=
		OCIParamGet (stmthp,
				OCI_HTYPE_STMT,
				priv_data->cdata->herr,
				(dvoid **) &pard,
				(ub4) col)) {
		gda_connection_add_error_string (priv_data->cnc, 
						_("Could not retrieve the parameter information from Oracle"));
	}
	OCIAttrGet ((dvoid *) pard,
			OCI_DTYPE_PARAM,
			&pgchar_dummy,
			(ub4 *) & col_name_len,
			OCI_ATTR_NAME,
			priv_data->cdata->herr);

	memcpy (name_buffer, pgchar_dummy, col_name_len);
	name_buffer[col_name_len] = '\0';

	OCIAttrGet ((dvoid *) pard,
			OCI_DTYPE_PARAM,
			&sql_type,
			0,
			OCI_ATTR_DATA_TYPE,
			priv_data->cdata->herr);
	OCIAttrGet ((dvoid *) pard,
			OCI_DTYPE_PARAM,
			&scale,
			0,
			OCI_ATTR_SCALE,
			priv_data->cdata->herr);
	OCIAttrGet ((dvoid *) pard,
			OCI_DTYPE_PARAM,
			&nullable,
			0,
			OCI_ATTR_IS_NULL,
			priv_data->cdata->herr);

	gda_field_attributes_set_name (field_attrs, name_buffer);
	gda_field_attributes_set_scale (field_attrs, scale);
	gda_field_attributes_set_gdatype (field_attrs, oracle_sqltype_to_gda_type (sql_type));
	gda_field_attributes_set_defined_size (field_attrs, priv_data->ora_values[col].defined_size);
	gda_field_attributes_set_references (field_attrs, "");

	/* FIXME */
	gda_field_attributes_set_primary_key (field_attrs, FALSE); 

	/* FIXME */
	gda_field_attributes_set_unique_key (field_attrs, FALSE);

	gda_field_attributes_set_allow_null (field_attrs, nullable);
	
	return field_attrs;
}

static gint
gda_oracle_recordset_get_n_rows (GdaDataModel *model)
{
	GdaOracleRecordset *recset = (GdaOracleRecordset *) model;
	GdaOracleRecordsetPrivate *priv_data;
	gint i = 0;
	gint fetch_status = OCI_SUCCESS;

	g_return_val_if_fail (GDA_IS_ORACLE_RECORDSET (model), 0);
	g_return_val_if_fail (recset->priv != NULL, 0);

	priv_data = recset->priv;

	if (priv_data->nrows > 0)
		return priv_data->nrows;

	while (fetch_status == OCI_SUCCESS) {
		i += 1;
		fetch_status = OCIStmtFetch (priv_data->cdata->hstmt,
				priv_data->cdata->herr,
				(ub4) 1,
				(ub2) OCI_FETCH_NEXT,
				(ub4) OCI_DEFAULT);
	}

	priv_data->nrows = i-1;

	// reset the result set to the beginning
	OCIStmtExecute (priv_data->cdata->hservice,
			priv_data->cdata->hstmt,
			priv_data->cdata->herr,
			(ub4) ((OCI_STMT_SELECT == priv_data->cdata->stmt_type) ? 0 : 1),
			(ub4) 0,
			(CONST OCISnapshot *) NULL,
			(OCISnapshot *) NULL,
			OCI_DEFAULT);


	return priv_data->nrows;
}

static gint
gda_oracle_recordset_get_n_columns (GdaDataModel *model)
{
	GdaOracleRecordset *recset = (GdaOracleRecordset *) model;
	g_return_val_if_fail (GDA_IS_ORACLE_RECORDSET (model), 0);
	g_return_val_if_fail (recset->priv != NULL, 0);

	return recset->priv->ncolumns;
}

static GdaRow *
fetch_row (GdaOracleRecordset *recset, gint rownum)
{
	gpointer thevalue;
	GdaValueType ftype;
	gboolean isNull;
	GdaValue *value;
	GdaRow *row;
	gint i;
	gchar *id;
	gint defined_size;
	GdaOracleRecordsetPrivate *priv;

	g_return_val_if_fail (GDA_IS_ORACLE_RECORDSET (recset), NULL);
	g_return_val_if_fail (recset->priv != NULL, NULL);

	priv = recset->priv;

	row = gda_row_new (priv->ncolumns);

	if (OCI_SUCCESS !=
		OCIStmtFetch (priv->cdata->hstmt,
				priv->cdata->herr,
				(ub4) rownum + 1,
				(ub2) OCI_FETCH_NEXT,
				(ub4) OCI_DEFAULT)) {
		return NULL;
	}

	for (i = 0; i < priv->ncolumns; i += 1) {
		thevalue = priv->ora_values[i].value;
		ftype = priv->column_types[i];
		defined_size = priv->ora_values[i].defined_size;
		isNull = FALSE; // FIXME
		value = gda_row_get_value (row, i);
		gda_oracle_set_value (value, ftype, thevalue, isNull, defined_size);
	}

	id = g_strdup_printf ("%d", rownum);
	gda_row_set_id (row, id);
	g_free (id);
	return row;
}

static const GdaRow *
gda_oracle_recordset_get_row (GdaDataModel *model, gint row)
{
	GdaOracleRecordset *recset = (GdaOracleRecordset *) model;
	GdaOracleRecordsetPrivate *priv_data;
	GdaRow *fields = NULL;
	gint fetched_rows;
	gint i;

	g_return_val_if_fail (GDA_IS_ORACLE_RECORDSET (recset), NULL);
	g_return_val_if_fail (recset->priv != NULL, 0);

	priv_data = recset->priv;
	if (!priv_data->cdata) {
		gda_connection_add_error_string (priv_data->cnc,
						_("Invalid Oracle handle"));
		return NULL;
	}

	fetched_rows = priv_data->rows->len;
	if (row >= priv_data->nrows)
		return NULL;

	if (row < fetched_rows)
		return (const GdaRow *) g_ptr_array_index (priv_data->rows, row);

	gda_data_model_freeze (GDA_DATA_MODEL (recset));

	for (i = fetched_rows; i <= row; i += 1) {
		fields = fetch_row (recset, 0);
		if (!fields)
			return NULL;
		g_ptr_array_add (priv_data->rows, fields);
	}

	gda_data_model_thaw (GDA_DATA_MODEL (recset));

	return (const GdaRow *) fields;
}

static const GdaValue *
gda_oracle_recordset_get_value_at (GdaDataModel *model, gint col, gint row)
{
	GdaOracleRecordset *recset = (GdaOracleRecordset *) model;
	GdaOracleRecordsetPrivate *priv_data;
	const GdaRow *fields;

	g_return_val_if_fail (GDA_IS_ORACLE_RECORDSET (recset), NULL);
	g_return_val_if_fail (recset->priv != NULL, 0);

	priv_data = recset->priv;

	if (col >= priv_data->ncolumns)
		return NULL;

	fields = gda_oracle_recordset_get_row (model, row);
	return fields != NULL ? gda_row_get_value ((GdaRow *) fields, col) : NULL;
}

static gboolean
gda_oracle_recordset_is_editable (GdaDataModel *model)
{
	return FALSE;
}

static const GdaRow *
gda_oracle_recordset_append_row (GdaDataModel *model, const GList *values)
{
	return NULL;
}

static gboolean
gda_oracle_recordset_remove_row (GdaDataModel *model, const GdaRow *row)
{
	return FALSE;
}

static gboolean
gda_oracle_recordset_update_row (GdaDataModel *model, const GdaRow *row)
{
	return FALSE;
}


/*
 * Object init and finalize 
 */
static void
gda_oracle_recordset_init (GdaOracleRecordset *recset, GdaOracleRecordsetClass *klass)
{
	g_return_if_fail (GDA_IS_ORACLE_RECORDSET (recset));

        recset->priv = g_new0 (GdaOracleRecordsetPrivate, 1);
	recset->priv->cnc = NULL;
	recset->priv->rows = g_ptr_array_new ();
	recset->priv->cdata = NULL;
}

static void
gda_oracle_recordset_class_init (GdaOracleRecordsetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaDataModelClass *model_class = GDA_DATA_MODEL_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_oracle_recordset_finalize;
	model_class->get_n_rows = gda_oracle_recordset_get_n_rows;
	model_class->get_n_columns = gda_oracle_recordset_get_n_columns;
	model_class->describe_column = gda_oracle_recordset_describe_column;
	model_class->get_row = gda_oracle_recordset_get_row;
	model_class->get_value_at = gda_oracle_recordset_get_value_at;
	model_class->is_editable = gda_oracle_recordset_is_editable;
	model_class->append_row = gda_oracle_recordset_append_row;
	model_class->remove_row = gda_oracle_recordset_remove_row;
	model_class->update_row = gda_oracle_recordset_update_row;
}

static void
gda_oracle_recordset_finalize (GObject *object)
{
	GdaOracleRecordset *recset = (GdaOracleRecordset *) object;

	g_return_if_fail (GDA_IS_ORACLE_RECORDSET (recset));

	if (recset->priv->cdata->hstmt)
		OCIHandleFree ((dvoid *)recset->priv->cdata->hstmt, OCI_HTYPE_STMT);

	parent_class->finalize (object);
}


GType
gda_oracle_recordset_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaOracleRecordsetClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_oracle_recordset_class_init,
			NULL,
			NULL,
			sizeof (GdaOracleRecordset),
			0,
			(GInstanceInitFunc) gda_oracle_recordset_init
		};
		type = g_type_register_static (PARENT_TYPE, "GdaOracleRecordset", &info, 0);
	}

	return type;
}

GdaOracleRecordset *
gda_oracle_recordset_new (GdaConnection *cnc, GdaOracleConnectionData *cdata)
{
	GdaOracleRecordset *recset;
	ub4 parcount;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cdata != NULL, NULL);

	OCIAttrGet ((dvoid *) cdata->hstmt,
			(ub4) OCI_HTYPE_STMT,
			(dvoid *) &parcount, 
			0,
			(ub4) OCI_ATTR_PARAM_COUNT,
			cdata->herr);

	recset = g_object_new (GDA_TYPE_ORACLE_RECORDSET, NULL);
	recset->priv->cnc = cnc;
	recset->priv->cdata = cdata;
	recset->priv->ncolumns = parcount;
	recset->priv->nrows = gda_oracle_recordset_get_n_rows (GDA_DATA_MODEL(recset));
	recset->priv->ora_values = define_columns (recset->priv);
	recset->priv->column_types = get_column_types (recset->priv);

	return recset;
}
