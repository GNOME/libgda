/* GDA IBM DB2 provider
 * Copyright (C) 1998-2003 The GNOME Foundation
 *
 * AUTHORS:
 *	Sergey N. Belinsky <sergey_be@mail.ru>
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

#include <libgda/gda-intl.h>
#include <string.h>
#include "gda-ibmdb2.h"
#include "gda-ibmdb2-recordset.h"
#include "gda-ibmdb2-types.h"

#ifdef PARENT_TYPE
#undef PARENT_TYPE
#endif

#define PARENT_TYPE GDA_TYPE_DATA_MODEL

static void gda_ibmdb2_recordset_class_init (GdaIBMDB2RecordsetClass *klass);
static void gda_ibmdb2_recordset_init       (GdaIBMDB2Recordset *recset, GdaIBMDB2RecordsetClass *klass);
static void gda_ibmdb2_recordset_finalize   (GObject *object);

static const GdaValue 		*gda_ibmdb2_recordset_get_value_at (GdaDataModel *model, gint col, gint row);
static GdaFieldAttributes 	*gda_ibmdb2_recordset_describe     (GdaDataModel *model, gint col);
static gint			gda_ibmdb2_recordset_get_n_rows    (GdaDataModel *model);
static gint			gda_ibmdb2_recordset_get_n_columns (GdaDataModel *model);
static const GdaRow 		*gda_ibmdb2_recordset_get_row 	   (GdaDataModel *model, gint rownum);

static GObjectClass	*parent_class = NULL;

/*
 * Private functions
 */

/*
 * Object init and finalize
 */

static void
gda_ibmdb2_recordset_init (GdaIBMDB2Recordset *recset,
			     GdaIBMDB2RecordsetClass *klass)
{
	g_return_if_fail (GDA_IS_IBMDB2_RECORDSET (recset));

	recset->priv = g_new0 (GdaIBMDB2RecordsetPrivate, 1);
	
        recset->priv->rows = g_ptr_array_new ();
        recset->priv->columns = g_ptr_array_new ();
}

static void
gda_ibmdb2_recordset_class_init (GdaIBMDB2RecordsetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaDataModelClass *model_class = GDA_DATA_MODEL_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_ibmdb2_recordset_finalize;
	model_class->get_n_rows = gda_ibmdb2_recordset_get_n_rows;
	model_class->get_n_columns = gda_ibmdb2_recordset_get_n_columns;
	model_class->describe_column = gda_ibmdb2_recordset_describe;
	model_class->get_value_at = gda_ibmdb2_recordset_get_value_at;
	model_class->get_row = gda_ibmdb2_recordset_get_row;
}

static void
gda_ibmdb2_recordset_finalize (GObject * object)
{
	GdaIBMDB2Recordset *recset = (GdaIBMDB2Recordset *) object;

	g_return_if_fail (GDA_IS_IBMDB2_RECORDSET (recset));

        if (recset->priv) {
                if (recset->priv->rows) {
                        while (recset->priv->rows->len > 0) {
                                GdaRow *row = (GdaRow *) g_ptr_array_index (recset->priv->rows, 0);
                                if (row != NULL) {
                                        gda_row_free (row);
                                        row = NULL;
                                }
                                g_ptr_array_remove_index (recset->priv->rows, 0);
                        }
                        g_ptr_array_free (recset->priv->rows, TRUE);
                        recset->priv->rows = NULL;
                }
                if (recset->priv->columns) {
                        while (recset->priv->columns->len > 0) {
                                GdaIBMDB2Field *field = (GdaIBMDB2Field *) g_ptr_array_index (recset->priv->columns, 0);
                                if (field != NULL) {
                                	g_free (field);
                                	field = NULL;
                                }
                                g_ptr_array_remove_index (recset->priv->columns, 0);
                        }
                        g_ptr_array_free (recset->priv->columns, TRUE);
                        recset->priv->columns = NULL;
                }

	
		if (recset->priv->hstmt != SQL_NULL_HANDLE) {
			recset->priv->conn_data->rc = SQLFreeHandle (SQL_HANDLE_STMT, recset->priv->conn_data->hstmt);
			if (recset->priv->conn_data->rc != SQL_SUCCESS) {
				gda_ibmdb2_emit_error(recset->priv->cnc, 
						      recset->priv->conn_data->henv, 
						      recset->priv->conn_data->hdbc, 
						      SQL_NULL_HANDLE);
			}
			recset->priv->conn_data->hstmt = SQL_NULL_HANDLE;
		}

		g_free (recset->priv);
		recset->priv = NULL;
	}

	parent_class->finalize (object);
}

/*
 * Overrides
 */

static const GdaRow *
gda_ibmdb2_recordset_get_row (GdaDataModel *model, gint row)
{
        GdaIBMDB2Recordset *recset = (GdaIBMDB2Recordset *) model;

        g_return_val_if_fail (GDA_IS_IBMDB2_RECORDSET (recset), NULL);
        g_return_val_if_fail (recset->priv != NULL, NULL);

        if (!recset->priv->nrows)
                return NULL;

        if (row >= recset->priv->rows->len)
                return NULL;

        return (const GdaRow *) g_ptr_array_index (recset->priv->rows, row);
}

static const GdaValue *
gda_ibmdb2_recordset_get_value_at (GdaDataModel *model, gint col, gint row)
{
	GdaIBMDB2Recordset *recset = (GdaIBMDB2Recordset *) model;
        const GdaRow *fields;
	
	g_return_val_if_fail (GDA_IS_IBMDB2_RECORDSET (recset), NULL);
	g_return_val_if_fail (recset->priv != NULL, 0);

        if (col >= recset->priv->ncols)
                return NULL;

        fields = gda_ibmdb2_recordset_get_row (model, row);
        return fields != NULL ? gda_row_get_value ((GdaRow *)fields, col) : NULL;
	
}

static GdaFieldAttributes *
gda_ibmdb2_recordset_describe (GdaDataModel *model, gint col)
{
	GdaIBMDB2Recordset *recset = (GdaIBMDB2Recordset *) model;
        GdaIBMDB2Field     *field = NULL;
        GdaFieldAttributes *attribs = NULL;

        g_return_val_if_fail (GDA_IS_IBMDB2_RECORDSET (recset), NULL);
        g_return_val_if_fail (recset->priv != NULL, NULL);
        g_return_val_if_fail (recset->priv->columns != NULL, NULL);

        if (col >= recset->priv->columns->len) {
                return NULL;
        }

	field = g_ptr_array_index (recset->priv->columns, col);
        if (!field) {
                return NULL;
        }

        attribs = gda_field_attributes_new ();
	if (!attribs) {
                return NULL;
        }
	
	gda_field_attributes_set_name (attribs, field->column_name);
	gda_field_attributes_set_scale (attribs, field->column_scale);
        gda_field_attributes_set_gdatype (attribs, gda_ibmdb2_get_value_type (field));
        gda_field_attributes_set_defined_size (attribs, field->column_size);

        gda_field_attributes_set_unique_key (attribs, FALSE);
	gda_field_attributes_set_references (attribs, "");
        gda_field_attributes_set_primary_key (attribs, FALSE);

        gda_field_attributes_set_allow_null (attribs, field->column_nullable == SQL_NULLABLE);

	return attribs;
}

static gint
gda_ibmdb2_recordset_get_n_rows (GdaDataModel *model)
{
	GdaIBMDB2Recordset *recset = (GdaIBMDB2Recordset *) model;

	g_return_val_if_fail (GDA_IS_IBMDB2_RECORDSET (recset), 0);
	g_return_val_if_fail (recset->priv != NULL, 0);
	
	return recset->priv->nrows;
}

static gint
gda_ibmdb2_recordset_get_n_columns (GdaDataModel *model)
{
	GdaIBMDB2Recordset *recset = (GdaIBMDB2Recordset *) model;

	g_return_val_if_fail (GDA_IS_IBMDB2_RECORDSET (recset), 0);
	g_return_val_if_fail (recset->priv != NULL, 0);
	
	return recset->priv->ncols;
}

static GdaRow*
gda_ibmdb2_create_current_row(GdaIBMDB2Recordset *recset)
{
	GdaRow *row = NULL;
        GdaIBMDB2Field *field;
	GdaValue       *value;
        gint i = 0;

        g_return_val_if_fail (GDA_IS_IBMDB2_RECORDSET (recset), NULL);
        g_return_val_if_fail (recset->priv != NULL, NULL);

        row = gda_row_new (GDA_DATA_MODEL(recset), recset->priv->columns->len);
        g_return_val_if_fail (row != NULL, NULL);

        for (i = 0; i < recset->priv->columns->len; i++) {
                value = gda_row_get_value (row, i);
                field = (GdaIBMDB2Field *) g_ptr_array_index (recset->priv->columns, i);

                gda_ibmdb2_set_gdavalue(value, field);
        }
        return row;
}

/*
 * Public functions
 */

GType
gda_ibmdb2_recordset_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaIBMDB2RecordsetClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_ibmdb2_recordset_class_init,
			NULL,
			NULL,
			sizeof (GdaIBMDB2Recordset),
			0,
			(GInstanceInitFunc) gda_ibmdb2_recordset_init
		};
		type = g_type_register_static (PARENT_TYPE, "GdaIBMDB2Recordset", &info, 0);
	}
	return type;
}

GdaDataModel*
gda_ibmdb2_recordset_new (GdaConnection *cnc, SQLHANDLE hstmt)
{
	GdaIBMDB2Recordset *recset;
	GdaIBMDB2ConnectionData *conn_data;
	GdaIBMDB2Field *field;
	gint i;
	gchar *tmp;
	GdaRow *row;
	SQLSMALLINT ncols = 0;
	SQLINTEGER  nrows = 0;
	SQLINTEGER ind;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (hstmt != SQL_NULL_HANDLE, NULL);

	conn_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_IBMDB2_HANDLE);

	recset = g_object_new (GDA_TYPE_IBMDB2_RECORDSET, NULL);

	recset->priv->hstmt = hstmt;
	recset->priv->cnc = cnc;
	recset->priv->ncols = 0;
	recset->priv->nrows = 0;
	recset->priv->conn_data = conn_data;

	if (hstmt == SQL_NULL_HANDLE) {
		return GDA_DATA_MODEL (recset);
	}
		
	/* Set number of columns */
	conn_data->rc = SQLNumResultCols (hstmt, &ncols);
	if (conn_data->rc != SQL_SUCCESS) {
		gda_ibmdb2_emit_error (cnc, conn_data->henv, conn_data->hdbc, hstmt);
	}

	g_warning ("Num Cols: %d", ncols);
	
	if (ncols == 0) {
		return GDA_DATA_MODEL (recset);
	}

	recset->priv->ncols = ncols;
	
	/* Allocate fields info	*/
	for (i = 0; i < ncols; i++) {
		field = g_new0 (GdaIBMDB2Field, 1);
		if (field != NULL) {
			g_ptr_array_add (recset->priv->columns, field);
		} else {
			/* FIXME: Out of memory */
		}
	}

	/* Fill field info for columns */
	for (i = 0; i < ncols; i++) {
		field = (GdaIBMDB2Field*) g_ptr_array_index (recset->priv->columns, i);
		conn_data->rc = SQLDescribeCol (hstmt, (SQLUSMALLINT)i + 1, 
						(SQLCHAR *)&field->column_name, 
						sizeof(field->column_name), 
						&field->column_name_len,
						&field->column_type,
						&field->column_size,
						&field->column_scale,
						&field->column_nullable);
		if (conn_data->rc != SQL_SUCCESS) {
			gda_ibmdb2_emit_error (cnc, conn_data->henv, conn_data->hdbc, hstmt);
		}
	
		/* g_message("Name: %s Type : %d Size : %d\n", (gchar*)field->column_name,
		 (gint)field->column_type, (gint)field->column_size); */
		
		field->column_data = g_new0(gchar, field->column_size + 1);
		
		conn_data->rc = SQLBindCol (hstmt, (SQLUSMALLINT)i + 1, 
					    SQL_C_DEFAULT,
					    field->column_data, 
					    field->column_size + field->column_scale + 1,
					    &ind);
		
		if (conn_data->rc != SQL_SUCCESS) {
			gda_ibmdb2_emit_error (cnc, conn_data->henv, conn_data->hdbc, hstmt);
		}
		
		tmp = g_strndup (field->column_name, field->column_name_len);
		gda_data_model_set_column_title (GDA_DATA_MODEL(recset), i, tmp);
		g_free (tmp);

	}

	/* Fetch all data from statement */
	conn_data->rc = SQLFetch(hstmt);	
	
	while (conn_data->rc != SQL_NO_DATA) {
	
		row = gda_ibmdb2_create_current_row (recset);

		if (row) {
			g_ptr_array_add(recset->priv->rows, row);
			nrows++;
		}
		/* Free old bind data */
		
		for(i = 0; i < ncols; i++)
		{
			field = (GdaIBMDB2Field*) g_ptr_array_index (recset->priv->columns, i);
			memset (field->column_data, 0, field->column_size);

		}
		conn_data->rc = SQLFetch(hstmt);
	}
	recset->priv->nrows = nrows;
	
	return GDA_DATA_MODEL (recset);
}

