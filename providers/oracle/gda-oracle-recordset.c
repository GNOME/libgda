/* GDA Oracle provider
 * Copyright (C) 1998 - 2005 The GNOME Foundation.
 *
 * AUTHORS:
 * 	Tim Coleman <tim@timcoleman.com>
 *      Vivien Malerba <malerba@gnome-db.org>
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
#include "gda-oracle.h"
#include "gda-oracle-recordset.h"
#include "gda-oracle-provider.h"
#include <libgda/gda-data-model.h>
#include <oci.h>
#include <libgda/gda-data-model-private.h>

#define PARENT_TYPE GDA_TYPE_DATA_MODEL_ROW

struct _GdaOracleRecordsetPrivate {
	GdaConnection *cnc;
	GdaOracleConnectionData *cdata;

	GList *ora_values;
	GPtrArray *rows;
	OCIStmt *hstmt;

	gint ncolumns;
	gint nrows;
};


static void gda_oracle_recordset_class_init (GdaOracleRecordsetClass *klass);
static void gda_oracle_recordset_init       (GdaOracleRecordset *recset,
					    GdaOracleRecordsetClass *klass);
static void gda_oracle_recordset_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

/*
 * Private functions
 */

static GList *
define_columns (GdaOracleRecordsetPrivate *priv)
{
	GList *fields = NULL;
	gint i;
	gint result;
	ub2 fake_type;

	for (i = 0; i < priv->ncolumns; i += 1) {
		GdaOracleValue *ora_value = g_new0 (GdaOracleValue, 1);

		result = OCIParamGet (priv->hstmt,
				      OCI_HTYPE_STMT,
				      priv->cdata->herr,
				      (dvoid **) &(ora_value->pard),
				      (ub4) i+1);
		if (!gda_oracle_check_result (result, priv->cnc, priv->cdata, OCI_HTYPE_ERROR,
					      _("Could not get the Oracle parameter")))
			return NULL;

		result = OCIAttrGet ((dvoid *) (ora_value->pard),
				     OCI_DTYPE_PARAM,
				     &(ora_value->defined_size),
				     0,
				     OCI_ATTR_DATA_SIZE,
				     priv->cdata->herr);
		if (!gda_oracle_check_result (result, priv->cnc, priv->cdata, OCI_HTYPE_ERROR,
					      _("Could not get the parameter defined size"))) {
			OCIDescriptorFree ((dvoid *) ora_value->pard, OCI_DTYPE_PARAM);
			return NULL;
		}
		
		result = OCIAttrGet ((dvoid *) (ora_value->pard),
				     OCI_DTYPE_PARAM,
				     &(ora_value->sql_type),
				     0,
				     OCI_ATTR_DATA_TYPE,
				     priv->cdata->herr);
		
		if (!gda_oracle_check_result (result, priv->cnc, priv->cdata, OCI_HTYPE_ERROR,
					      _("Could not get the parameter data type"))) {
			OCIDescriptorFree ((dvoid *) ora_value->pard, OCI_DTYPE_PARAM);
			return NULL;
		}
		
		/* for data fetching  */
		fake_type = ora_value->sql_type;
		switch (ora_value->sql_type) {
                case SQLT_NUM: /* Numerics are coerced to string */
                        fake_type = SQLT_CHR;
                        break;
                case SQLT_DAT: /* Convert SQLT_DAT to OCIDate */
                        fake_type = SQLT_ODT;
                        break;
                }

		if (ora_value->defined_size == 0)
			ora_value->value = NULL;
		else
			ora_value->value = g_malloc0 (ora_value->defined_size+1);
		ora_value->hdef = (OCIDefine *) 0;
		ora_value->indicator = 0;
		ora_value->gda_type = oracle_sqltype_to_gda_type (ora_value->sql_type);
		result = OCIDefineByPos ((OCIStmt *) priv->hstmt,
					 (OCIDefine **) &(ora_value->hdef),
					 (OCIError *) priv->cdata->herr,
					 (ub4) i + 1,
					 ora_value->value,
					 ora_value->defined_size+1,
					 (ub2) fake_type,
					 (dvoid *) &(ora_value->indicator),
					 (ub2 *) 0,
					 (ub2 *) 0, 
					 (ub4) OCI_DEFAULT);
		if (!gda_oracle_check_result (result, priv->cnc, priv->cdata, OCI_HTYPE_ERROR,
					      _("Could not define by position"))) {
			OCIDescriptorFree ((dvoid *) ora_value->pard, OCI_DTYPE_PARAM);
			return NULL;
		}
		
		fields = g_list_append (fields, ora_value);
	}
	return fields;
}


/*
 * GdaOracleRecordset class implementation
 */

static void
gda_oracle_recordset_describe_column (GdaDataModel *model, gint col)
{
	GdaOracleRecordset *recset = (GdaOracleRecordset *) model;
	GdaOracleRecordsetPrivate *priv_data;
	GdaColumn *field_attrs;
	OCIParam *pard;
	gchar *pgchar_dummy;
	glong col_name_len;

	gchar name_buffer[ORA_NAME_BUFFER_SIZE + 1];
	ub2 sql_type;
	sb1 scale;
	ub1 nullable;
	ub2 defined_size;
	gint result;

	g_return_if_fail (GDA_IS_ORACLE_RECORDSET (recset));
	g_return_if_fail (recset->priv != NULL);
	
	priv_data = recset->priv;
	
	if (!priv_data->cdata) {
		gda_connection_add_event_string (priv_data->cnc, 
						 _("Invalid Oracle handle"));
		return;
	}
	
	if (col >= priv_data->ncolumns) {
		gda_connection_add_event_string (priv_data->cnc, 
						 _("Column out of range"));
		return;
	}
	
	field_attrs = gda_data_model_describe_column (model, col);
	
	result = OCIParamGet ((dvoid *) priv_data->hstmt,
			      (ub4) OCI_HTYPE_STMT,
			      (OCIError *) priv_data->cdata->herr,
			      (dvoid **) &pard,
			      (ub4) col + 1);
	if (!gda_oracle_check_result (result, priv_data->cnc, priv_data->cdata, OCI_HTYPE_ERROR,
				      _("Could not get the Oracle parameter information")))
		return;

	result = OCIAttrGet ((dvoid *) pard,
			     (ub4) OCI_DTYPE_PARAM,
			     (dvoid **) &pgchar_dummy,
			     (ub4 *) & col_name_len,
			     (ub4) OCI_ATTR_NAME,
			     (OCIError *) priv_data->cdata->herr);
	if (!gda_oracle_check_result (result, priv_data->cnc, priv_data->cdata, OCI_HTYPE_ERROR,
				      _("Could not get the Oracle column name"))) {
		OCIDescriptorFree ((dvoid *) pard, OCI_DTYPE_PARAM);
		return;
	}
	
	if (col_name_len > ORA_NAME_BUFFER_SIZE)
		col_name_len = ORA_NAME_BUFFER_SIZE;
		
	memcpy (name_buffer, pgchar_dummy, col_name_len);
	name_buffer[col_name_len] = '\0';
	
	result = OCIAttrGet ((dvoid *) pard,
			     (ub4) OCI_DTYPE_PARAM,
			     (dvoid *) &sql_type,
			     (ub4 *) 0,
			     (ub4) OCI_ATTR_DATA_TYPE,
			     (OCIError *) priv_data->cdata->herr);
	if (!gda_oracle_check_result (result, priv_data->cnc, priv_data->cdata, OCI_HTYPE_ERROR,
				      _("Could not get the Oracle column data type"))) {
		OCIDescriptorFree ((dvoid *) pard, OCI_DTYPE_PARAM);
		return;
	}
	
	result = OCIAttrGet ((dvoid *) pard,
			     (ub4) OCI_DTYPE_PARAM,
			     (sb1 *) &scale,
			     (ub4 *) 0,
			     (ub4) OCI_ATTR_SCALE,
			     (OCIError *) priv_data->cdata->herr);
	if (!gda_oracle_check_result (result, priv_data->cnc, priv_data->cdata, OCI_HTYPE_ERROR,
				      _("Could not get the Oracle column scale"))) {
		OCIDescriptorFree ((dvoid *) pard, OCI_DTYPE_PARAM);
		return;
	}
	
	result = OCIAttrGet ((dvoid *) pard,
			     (ub4) OCI_DTYPE_PARAM,
			     (ub1 *) &nullable,
			     (ub4 *) 0,
			     (ub4) OCI_ATTR_IS_NULL,
			     (OCIError *) priv_data->cdata->herr);
	if (!gda_oracle_check_result (result, priv_data->cnc, priv_data->cdata, OCI_HTYPE_ERROR,
				      _("Could not get the Oracle column nullable attribute"))) {
		OCIDescriptorFree ((dvoid *) pard, OCI_DTYPE_PARAM);
		return;
	}
	
	result = OCIAttrGet ((dvoid *) pard,
			     OCI_DTYPE_PARAM,
			     &defined_size,
			     0,
			     OCI_ATTR_DATA_SIZE,
			     priv_data->cdata->herr);
	if (!gda_oracle_check_result (result, priv_data->cnc, priv_data->cdata, OCI_HTYPE_ERROR,
				      _("Could not get the Oracle defined size"))) {
		OCIDescriptorFree ((dvoid *) pard, OCI_DTYPE_PARAM);
		return;
	}
	
	gda_column_set_name (field_attrs, name_buffer);
	gda_column_set_scale (field_attrs, scale);
	gda_column_set_gda_type (field_attrs, oracle_sqltype_to_gda_type (sql_type));
	gda_column_set_defined_size (field_attrs, defined_size);
	
	/* FIXME */
	gda_column_set_references (field_attrs, "");
	
	/* FIXME */
	gda_column_set_primary_key (field_attrs, FALSE); 
	
	/* FIXME */
	gda_column_set_unique_key (field_attrs, FALSE);

	gda_column_set_allow_null (field_attrs, !(nullable == 0));
	
	OCIDescriptorFree ((dvoid *) pard, OCI_DTYPE_PARAM);
}

static gint
gda_oracle_recordset_get_n_rows (GdaDataModelRow *model)
{
	GdaOracleRecordset *recset = (GdaOracleRecordset *) model;
	GdaOracleRecordsetPrivate *priv_data;
	gint result = OCI_SUCCESS;
	gint nrows;

	g_return_val_if_fail (GDA_IS_ORACLE_RECORDSET (model), 0);
	g_return_val_if_fail (recset->priv != NULL, 0);

	priv_data = recset->priv;

	nrows = priv_data->nrows;

	if (nrows >= 0) 
		return nrows;

	while (result == OCI_SUCCESS) {
		nrows += 1;
		result = OCIStmtFetch (priv_data->hstmt,
					priv_data->cdata->herr,
					(ub4) 1,
					(ub2) OCI_FETCH_NEXT,
					(ub4) OCI_DEFAULT);
	}

	/* reset the result set to the beginning */
	result = OCIStmtExecute (priv_data->cdata->hservice,
				priv_data->hstmt,
				priv_data->cdata->herr,
				(ub4) ((OCI_STMT_SELECT == priv_data->cdata->stmt_type) ? 0 : 1),
				(ub4) 0,
				(CONST OCISnapshot *) NULL,
				(OCISnapshot *) NULL,
				OCI_DEFAULT);
	
	if (!gda_oracle_check_result (result, priv_data->cnc, priv_data->cdata, 
		OCI_HTYPE_ERROR, "Could not execute Oracle statement")) {
		return 0;
	}

	return nrows;
}

static gint
gda_oracle_recordset_get_n_columns (GdaDataModelRow *model)
{
	GdaOracleRecordset *recset = (GdaOracleRecordset *) model;
	g_return_val_if_fail (GDA_IS_ORACLE_RECORDSET (model), 0);
	g_return_val_if_fail (recset->priv != NULL, 0);

	return recset->priv->ncolumns;
}

static GdaRow *
fetch_row (GdaOracleRecordset *recset, gint rownum)
{
	GdaRow *row;
	gint i;
	gchar *id;
	GdaOracleRecordsetPrivate *priv;
	GList *node;
	gint result;

	g_return_val_if_fail (GDA_IS_ORACLE_RECORDSET (recset), NULL);
	g_return_val_if_fail (recset->priv != NULL, NULL);

	priv = recset->priv;

	row = gda_row_new (GDA_DATA_MODEL (recset), priv->ncolumns);

	result = OCIStmtFetch ((OCIStmt *) priv->hstmt,
				(OCIError *) priv->cdata->herr,
				(ub4) 1,
				(ub2) OCI_FETCH_NEXT,
				(ub4) OCI_DEFAULT); 
	if (result == OCI_NO_DATA) 
		return NULL;

	if (!gda_oracle_check_result (result, priv->cnc, priv->cdata, OCI_HTYPE_ERROR,
			_("Could not fetch a row"))) 
		return NULL;


	i = 0;
	for (node = g_list_first (priv->ora_values); node != NULL; 
	     node = g_list_next (node)) {
		GdaOracleValue *ora_value = (GdaOracleValue *) node->data;
		GValue *value = gda_row_get_value (row, i);
		gda_oracle_set_value (value, ora_value, priv->cnc);
		i += 1;
	}

	id = g_strdup_printf ("%d", rownum);
	gda_row_set_id (row, id);
	g_free (id);
	return row;
}

static const GdaRow *
gda_oracle_recordset_get_row (GdaDataModelRow *model, gint row, GError **error)
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
		gda_connection_add_event_string (priv_data->cnc,
						_("Invalid Oracle handle"));
		return NULL;
	}

	fetched_rows = priv_data->rows->len;

	if (row >= priv_data->nrows && priv_data->nrows >= 0)
		return NULL;

	if (row < fetched_rows) 
		return (const GdaRow *) g_ptr_array_index (priv_data->rows, row);

	gda_data_model_freeze (GDA_DATA_MODEL (recset));

	for (i = fetched_rows; i <= row; i += 1) {
		fields = fetch_row (recset, i);
		if (!fields) 
			return NULL;
		g_ptr_array_add (priv_data->rows, fields);
	}

	gda_data_model_thaw (GDA_DATA_MODEL (recset));

	return (const GdaRow *) fields;
}

static const GValue *
gda_oracle_recordset_get_value_at (GdaDataModelRow *model, gint col, gint row)
{
	GdaOracleRecordset *recset = (GdaOracleRecordset *) model;
	GdaOracleRecordsetPrivate *priv_data;
	const GdaRow *fields;

	g_return_val_if_fail (GDA_IS_ORACLE_RECORDSET (recset), NULL);
	g_return_val_if_fail (recset->priv != NULL, 0);

	priv_data = recset->priv;

	if (col >= priv_data->ncolumns)
		return NULL;

	fields = gda_oracle_recordset_get_row (model, row, NULL);
	return fields != NULL ? gda_row_get_value ((GdaRow *) fields, col) : NULL;
}

static gboolean
gda_oracle_recordset_is_updatable (GdaDataModelRow *model)
{
	GdaCommandType cmd_type;
	GdaOracleRecordset *recset = (GdaOracleRecordset *) model;

	g_return_val_if_fail (GDA_IS_ORACLE_RECORDSET (recset), FALSE);

	g_object_get (G_OBJECT (model), "command_type", &cmd_type, NULL);

	return cmd_type == GDA_COMMAND_TYPE_TABLE ? TRUE : FALSE;
}

static const GdaRow *
gda_oracle_recordset_append_values (GdaDataModelRow *model, const GList *values)
{
	GString *sql;
	GdaRow *row;
	gint i;
	GList *l;
	GdaOracleRecordset *recset = (GdaOracleRecordset *) model;
	GdaOracleRecordsetPrivate *priv_data;

	g_return_val_if_fail (GDA_IS_ORACLE_RECORDSET (recset), NULL);
	g_return_val_if_fail (values != NULL, NULL);
	g_return_val_if_fail (gda_data_model_is_updatable (GDA_DATA_MODEL (model)), NULL);
	g_return_val_if_fail (recset->priv != NULL, 0);

	priv_data = recset->priv;

	if (priv_data->ncolumns != g_list_length ((GList *) values)) {
		gda_connection_add_event_string (
			recset->priv->cnc,
			_("Attempt to insert a row with an invalid number of columns"));
		return NULL;
	}

	sql = g_string_new ("INSERT INTO ");
	sql = g_string_append (sql, gda_data_model_get_command_text (GDA_DATA_MODEL (model)));
	sql = g_string_append (sql, "(");

	for (i = 0; i < priv_data->ncolumns; i += 1) {
		GdaColumn *fa;

		fa = gda_data_model_describe_column (GDA_DATA_MODEL (model), i);
		if (!fa) {
			gda_connection_add_event_string (
				recset->priv->cnc,
				_("Could not retrieve column's information"));
			g_string_free (sql, TRUE);
			return NULL;
		}

		if (i != 0) 
			sql = g_string_append (sql, ", ");
		sql = g_string_append (sql, gda_column_get_name (fa));
	}
	sql = g_string_append (sql, ") VALUES (");

	for (l = (GList *) values, i = 0; i < priv_data->ncolumns; i += 1, l = l->next) {
		gchar *val_str;
		const GValue *val = (const GValue *) l->data;

		if (!val) { 
			gda_connection_add_event_string (
				recset->priv->cnc,
				_("Could not retrieve column's value"));
			g_string_free (sql, TRUE);
			return NULL;
		}

		if (i != 0) 
			sql = g_string_append (sql, ", ");
		val_str = gda_oracle_value_to_sql_string (val);
		sql = g_string_append (sql, val_str);

		g_free (val_str);
	}
	sql = g_string_append (sql, ")");

	/* execute the UPDATE command */
	/* not sure what to do here yet. */

	g_string_free (sql, TRUE);

	row = gda_row_new_from_list (GDA_DATA_MODEL (model), values);
	g_ptr_array_add (recset->priv->rows, row);

	return (const GdaRow *) row;
}

static gboolean
gda_oracle_recordset_remove_row (GdaDataModelRow *model, const GdaRow *row)
{
	return FALSE;
}

static gboolean
gda_oracle_recordset_update_row (GdaDataModelRow *model, const GdaRow *row)
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
        recset->priv->rows = g_ptr_array_new ();
}

static void
gda_oracle_recordset_class_init (GdaOracleRecordsetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaDataModelRowClass *model_class = GDA_DATA_MODEL_ROW_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_oracle_recordset_finalize;
	model_class->get_n_rows = gda_oracle_recordset_get_n_rows;
	model_class->get_n_columns = gda_oracle_recordset_get_n_columns;
	model_class->get_row = gda_oracle_recordset_get_row;
	model_class->get_value_at = gda_oracle_recordset_get_value_at;
	model_class->is_updatable = gda_oracle_recordset_is_updatable;
	model_class->append_values = gda_oracle_recordset_append_values;
	model_class->remove_row = gda_oracle_recordset_remove_row;
	model_class->update_row = gda_oracle_recordset_update_row;
}

static void 
gda_oracle_free_values (GdaOracleValue *ora_value)
{
	g_free (ora_value->value);
	OCIDescriptorFree ((dvoid *) ora_value->pard, OCI_DTYPE_PARAM);
	g_free (ora_value);
}

static void
gda_oracle_recordset_finalize (GObject *object)
{
	GdaOracleRecordset *recset = (GdaOracleRecordset *) object;
	GdaOracleRecordsetPrivate *priv_data;

	g_return_if_fail (recset->priv != NULL);

	priv_data = recset->priv;

	if (!priv_data->cdata) {
		gda_connection_add_event_string (priv_data->cnc, 
						_("Invalid Oracle handle"));
		return;
	}

	g_return_if_fail (GDA_IS_ORACLE_RECORDSET (recset));

	while (priv_data->rows->len > 0) {
		GdaRow *row = (GdaRow *) g_ptr_array_index (priv_data->rows, 0);

		if (row != NULL) 
			g_object_unref (row);
		g_ptr_array_remove_index (priv_data->rows, 0);
	}
	g_ptr_array_free (priv_data->rows, TRUE);
	recset->priv->rows = NULL;

	g_list_foreach (priv_data->ora_values, (GFunc) gda_oracle_free_values, NULL);
	if (priv_data->hstmt)
		OCIHandleFree ((dvoid *) priv_data->hstmt, OCI_HTYPE_STMT);
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
gda_oracle_recordset_new (GdaConnection *cnc, 
			  GdaOracleConnectionData *cdata,
			  OCIStmt *stmthp)
{
	GdaOracleRecordset *recset;
	ub4 parcount;
	gint i;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cdata != NULL, NULL);

	OCIAttrGet ((dvoid *) stmthp,
		    (ub4) OCI_HTYPE_STMT,
		    (dvoid *) &parcount, 
		    0,
		    (ub4) OCI_ATTR_PARAM_COUNT,
		    cdata->herr);

	recset = g_object_new (GDA_TYPE_ORACLE_RECORDSET, NULL);
	recset->priv->nrows = -1;
	recset->priv->cnc = cnc;
	recset->priv->cdata = cdata;
	recset->priv->hstmt = stmthp;
	recset->priv->ncolumns = parcount;
	recset->priv->ora_values = define_columns (recset->priv);
	recset->priv->nrows = gda_oracle_recordset_get_n_rows (GDA_DATA_MODEL_ROW (recset));

	/* define GdaColumn attributes */
	for (i = 0; i < recset->priv->ncolumns; i += 1) 
		gda_oracle_recordset_describe_column (GDA_DATA_MODEL (recset), i);		

	return recset;
}
