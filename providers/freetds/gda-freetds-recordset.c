/* GDA FreeTDS provider
 * Copyright (C) 1998-2002 The GNOME Foundation
 *
 * AUTHORS:
 *      Holger Thon <holger.thon@gnome-db.org>
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
#include <libgda/gda-data-model.h>
#include <string.h>
#include <tds.h>
#include <tdsconvert.h>
#include "gda-freetds.h"
#include "gda-freetds-recordset.h"

#ifdef PARENT_TYPE
#undef PARENT_TYPE
#endif

#define PARENT_TYPE GDA_TYPE_DATA_MODEL

/////////////////////////////////////////////////////////////////////////////
// Private declarations and functions
/////////////////////////////////////////////////////////////////////////////

static GObjectClass *parent_class = NULL;

static void gda_freetds_recordset_class_init (GdaFreeTDSRecordsetClass *klass);
static void gda_freetds_recordset_init       (GdaFreeTDSRecordset *recset,
                                              GdaFreeTDSRecordsetClass *klass);
static void gda_freetds_recordset_finalize   (GObject *object);

static gint gda_freetds_recordset_get_n_rows (GdaDataModel *model);
static gint gda_freetds_recordset_get_n_columns (GdaDataModel *model);
static const GdaRow *gda_freetds_recordset_get_row (GdaDataModel *model,
                                                    gint row);
static const GdaValue *gda_freetds_recordset_get_value_at (GdaDataModel *model,
                                                           gint         col,
                                                           gint         row);

// Private utility functions 
static void gda_freetds_set_field(GdaValue *field, TDSCOLINFO *col,
                                  GdaFreeTDSRecordset *recset);
static GdaRow *gda_freetds_get_current_row(GdaFreeTDSRecordset *recset);


static gint
gda_freetds_recordset_get_n_rows (GdaDataModel *model)
{
	GdaFreeTDSRecordset *recset = (GdaFreeTDSRecordset *) model;

        g_return_val_if_fail (GDA_IS_FREETDS_RECORDSET (recset), -1);
        return (recset->priv->rowcnt);
}

static gint
gda_freetds_recordset_get_n_columns (GdaDataModel *model)
{
	GdaFreeTDSRecordset *recset = (GdaFreeTDSRecordset *) model;

        g_return_val_if_fail (GDA_IS_FREETDS_RECORDSET (recset), -1);
        return (recset->priv->colcnt);
}

static const GdaRow 
*gda_freetds_recordset_get_row (GdaDataModel *model, gint row)
{
	GdaFreeTDSRecordset *recset = (GdaFreeTDSRecordset *) model;
	GdaRow *fields = NULL;

	g_return_val_if_fail (GDA_IS_FREETDS_RECORDSET (recset), NULL);
	g_return_val_if_fail (recset->priv != NULL, NULL);
	
	
	if (!recset->priv->rows)
		return NULL;
	
	if (row > recset->priv->rows->len)
		return NULL;

	fields = (GdaRow *) g_ptr_array_index (recset->priv->rows, row);
	
	return (const GdaRow *) fields;
}

static const GdaValue
*gda_freetds_recordset_get_value_at (GdaDataModel *model, gint col, gint row)
{
	GdaFreeTDSRecordset *recset = (GdaFreeTDSRecordset *) model;
	const GdaRow *fields;

	g_return_val_if_fail (GDA_IS_FREETDS_RECORDSET (recset), NULL);
	g_return_val_if_fail (recset->priv != NULL, NULL);

	if (col >= recset->priv->colcnt)
		return NULL;

	fields = gda_freetds_recordset_get_row (model, row);
	return fields != NULL ? gda_row_get_value ((GdaRow *) fields, col) : NULL;
}

static void
gda_freetds_recordset_class_init (GdaFreeTDSRecordsetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaDataModelClass *model_class = GDA_DATA_MODEL_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_freetds_recordset_finalize;
	model_class->get_n_rows = gda_freetds_recordset_get_n_rows;
	model_class->get_n_columns = gda_freetds_recordset_get_n_columns;
	model_class->get_row = gda_freetds_recordset_get_row;
	model_class->get_value_at = gda_freetds_recordset_get_value_at;
}

static void
gda_freetds_recordset_init (GdaFreeTDSRecordset *recset,
                            GdaFreeTDSRecordsetClass *klass)
{
	g_return_if_fail (GDA_IS_FREETDS_RECORDSET (recset));

	recset->priv = g_new0 (GdaFreeTDSRecordsetPrivate, 1);
	recset->priv->rows = g_ptr_array_new ();
	recset->priv->fetched_all_results = FALSE;
}

static void
gda_freetds_recordset_finalize (GObject * object)
{
	GdaFreeTDSRecordset *recset = (GdaFreeTDSRecordset *) object;

	g_return_if_fail (GDA_IS_FREETDS_RECORDSET (recset));

	if (recset->priv) {
		if (recset->priv->rows) {
			while (recset->priv->rows->len > 0) {
				GdaRow *row = (GdaRow *) g_ptr_array_index (recset->priv->rows, 0);
				if (row != NULL)
					gda_row_free (row);
				g_ptr_array_remove_index (recset->priv->rows, 0);
			}
			g_ptr_array_free (recset->priv->rows, TRUE);
			recset->priv->rows = NULL;
		}
			
		g_free(recset->priv);
		recset->priv = NULL;
	}

	parent_class->finalize (object);
}

static void
gda_freetds_set_field(GdaValue *field,
                      TDSCOLINFO *col,
		      GdaFreeTDSRecordset *recset)
{
	const TDS_INT max_size = 255;
	TDS_INT col_size;
	gchar *src = NULL;
	gchar *val = NULL;
	
	g_return_if_fail (field != NULL);
	g_return_if_fail (col != NULL);
	g_return_if_fail (recset != NULL);

	src = &(recset->priv->res->current_row[col->column_offset]);
	switch (col->column_type) {
		case SYBCHAR:
		case SYBVARCHAR:
		case SYBINTN:
		case SYBINT1:
		case SYBINT2:
		case SYBINT4:
		case SYBINT8:
		case SYBFLT8:
		case SYBDATETIME:
		case SYBBIT:
		case SYBTEXT:
		case SYBNTEXT:
		case SYBDATETIME4:
		case SYBREAL:
		default:
			if (col->column_size > max_size) {
				col_size = max_size + 1;
			} else {
				col_size = col->column_size + 1;
			}
			val = g_new0(gchar, col_size);
			tds_convert(col->column_type, src, col->column_size,
			            SYBCHAR, val, col_size - 1);
			gda_value_set_string (field, val ? val : "");
			g_free(val);
			val = NULL;
	}
}

static GdaRow
*gda_freetds_get_current_row(GdaFreeTDSRecordset *recset)
{
	GdaRow *row;
	gint i;
	
	g_return_val_if_fail (GDA_IS_FREETDS_RECORDSET (recset), NULL);
	g_return_val_if_fail (recset->priv != NULL, NULL);
	g_return_val_if_fail (recset->priv->res != NULL, NULL);
//	g_return_val_if_fail (recset->priv->colcnt 
//	                      != recset->priv->res->num_cols, NULL);

	row = gda_row_new(recset->priv->res->num_cols);
	g_return_val_if_fail (row != NULL, NULL);
	
	for (i = 0; i < recset->priv->res->num_cols; i++) {
		GdaValue   *field;
		TDSCOLINFO *col;

		field = gda_row_get_value (row, i);
		col = recset->priv->res->columns[i];
		gda_freetds_set_field(field, col, recset);
	}
	
	return row;
}


/////////////////////////////////////////////////////////////////////////////
// Public functions                                                       
/////////////////////////////////////////////////////////////////////////////

GType
gda_freetds_recordset_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaFreeTDSRecordsetClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_freetds_recordset_class_init,
			NULL,
			NULL,
			sizeof (GdaFreeTDSRecordset),
			0,
			(GInstanceInitFunc) gda_freetds_recordset_init
		};
		type = g_type_register_static (PARENT_TYPE, "GdaFreeTDSRecordset", &info, 0);
	}
	return type;
}

GdaDataModel
*gda_freetds_recordset_new (GdaConnection *cnc, gboolean fetchall)
{
	GdaFreeTDSConnectionData *tds_cnc = NULL;
	GdaFreeTDSRecordset *recset = NULL;
	GdaRow *row = NULL;
	GdaError *error = NULL;
	gboolean columns_set = FALSE;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	tds_cnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_FREETDS_HANDLE);
	g_return_val_if_fail (tds_cnc != NULL, NULL);

	recset = g_object_new (GDA_TYPE_FREETDS_RECORDSET, NULL);
	g_return_val_if_fail (recset != NULL, NULL);

	recset->priv->cnc = cnc;
	recset->priv->tds_cnc = tds_cnc;
	recset->priv->res = tds_cnc->socket->res_info;

	while ((tds_cnc->rc = tds_process_result_tokens(tds_cnc->socket))
	       == TDS_SUCCEED) {
		while ((tds_cnc->rc = tds_process_row_tokens(tds_cnc->socket))
		       == TDS_SUCCEED) {
			recset->priv->res = tds_cnc->socket->res_info;
			if (columns_set == FALSE) {
				columns_set = TRUE;
				recset->priv->colcnt = recset->priv->res->num_cols;
			}

			row = gda_freetds_get_current_row(recset);
			if (row) {
				g_ptr_array_add(recset->priv->rows, row);
				recset->priv->rowcnt++;
			}
		}
		if (tds_cnc->rc == TDS_FAIL) {
			error = gda_freetds_make_error("tds_process_row_tokens() returned TDS_FAIL\n");
			gda_connection_add_error (cnc, error);
			recset->priv->rc = TDS_FAIL;

		}
	}
	

	return GDA_DATA_MODEL (recset);
}
