/* GDA Sybase provider
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *         Mike Wingert <wingert.3@postbox.acs.ohio-state.edu>
 *         Holger Thon <holger.thon@gnome-db.org>
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

#if defined(HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <libgda/gda-intl.h>
#include <libgda/gda-data-model.h>
#include <string.h>
#include <ctpublic.h>
#include <cspublic.h>

#include "gda-sybase-recordset.h"
#include "gda-sybase-types.h"

#ifdef PARENT_TYPE
#  undef PARENT_TYPE
#endif

#define PARENT_TYPE GDA_TYPE_DATA_MODEL

static GObjectClass *parent_class = NULL;

static void gda_sybase_recordset_class_init (GdaSybaseRecordsetClass *klass);
static void gda_sybase_recordset_init (GdaSybaseRecordset *recset,
                                       GdaSybaseRecordsetClass *klass);
static void gda_sybase_recordset_finalize (GObject *object);

static GdaFieldAttributes *gda_sybase_recordset_describe_column (GdaDataModel *model,
                                                                 gint col);
static gint gda_sybase_recordset_get_n_rows (GdaDataModel *model);
static gint gda_sybase_recordset_get_n_columns (GdaDataModel *model);
static const GdaRow *gda_sybase_recordset_get_row (GdaDataModel *model,
                                                   gint row);
static const GdaValue *gda_sybase_recordset_get_value_at (GdaDataModel *model,
                                                          gint col,
                                                          gint row);

static GdaFieldAttributes *
gda_sybase_recordset_describe_column (GdaDataModel *model, gint col)
{
	GdaSybaseRecordset *recset = (GdaSybaseRecordset *) model;
	CS_DATAFMT         *colinfo = NULL;
	GdaFieldAttributes *attribs = NULL;
	gchar              name[256];

	g_return_val_if_fail (GDA_IS_SYBASE_RECORDSET (recset), NULL);
	g_return_val_if_fail (recset->priv != NULL, NULL);
	g_return_val_if_fail (recset->priv->columns != NULL, NULL);

	if (col >= recset->priv->columns->len) {
		return NULL;
	}
	colinfo = g_ptr_array_index (recset->priv->columns, col);

	if (!colinfo) {
		return NULL;
	}

	attribs = gda_field_attributes_new ();

	if (!attribs) {
		return NULL;
	}

	memcpy (name, colinfo->name,
	        colinfo->namelen);
	//name[colinfo->namelen + 1] = '\0';

	gda_field_attributes_set_name (attribs, name);
	gda_field_attributes_set_scale (attribs, colinfo->scale);
	gda_field_attributes_set_gdatype (attribs,
	         gda_sybase_get_value_type (colinfo->datatype));
	gda_field_attributes_set_defined_size (attribs, colinfo->maxlength);

	// FIXME:
	gda_field_attributes_set_references (attribs, "");
	gda_field_attributes_set_primary_key (attribs, FALSE);
	//gda_field_attributes_set_primary_key (attribs,
	//         (colinfo->status & CS_KEY) == CS_KEY);
	gda_field_attributes_set_unique_key (attribs, FALSE);
	
	gda_field_attributes_set_allow_null (attribs,
	         (colinfo->status & CS_CANBENULL) == CS_CANBENULL);
	
	return attribs;
}

static gint
gda_sybase_recordset_get_n_rows (GdaDataModel *model)
{
	GdaSybaseRecordset *recset = (GdaSybaseRecordset *) model;

	g_return_val_if_fail (GDA_IS_SYBASE_RECORDSET (recset), -1);
	return (recset->priv->rowcnt);
}

static gint
gda_sybase_recordset_get_n_columns (GdaDataModel *model)
{
	GdaSybaseRecordset *recset = (GdaSybaseRecordset *) model;

	g_return_val_if_fail (GDA_IS_SYBASE_RECORDSET (recset), -1);
	return (recset->priv->colcnt);
}

static const GdaRow *
gda_sybase_recordset_get_row (GdaDataModel *model, gint row)
{
	GdaSybaseRecordset *recset = (GdaSybaseRecordset *) model;

	g_return_val_if_fail (GDA_IS_SYBASE_RECORDSET (recset), NULL);
	g_return_val_if_fail (recset->priv != NULL, NULL);

	if (!recset->priv->rows)
		return NULL;

	if (row >= recset->priv->rows->len)
		return NULL;

	return (const GdaRow *) g_ptr_array_index (recset->priv->rows, row);
}

static const GdaValue *
gda_sybase_recordset_get_value_at (GdaDataModel *model, gint col, gint row)
{
	GdaSybaseRecordset *recset = (GdaSybaseRecordset *) model;
	const GdaRow *fields;

	g_return_val_if_fail (GDA_IS_SYBASE_RECORDSET (recset), NULL);
	g_return_val_if_fail (recset->priv != NULL, NULL);

	if (col >= recset->priv->colcnt)
		return NULL;

	fields = gda_sybase_recordset_get_row (model, row);
	return fields != NULL ? gda_row_get_value ((GdaRow *)fields, col) : NULL;
}

static void 
gda_sybase_recordset_class_init (GdaSybaseRecordsetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaDataModelClass *model_class = GDA_DATA_MODEL_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_sybase_recordset_finalize;
	model_class->describe_column = gda_sybase_recordset_describe_column;
	model_class->get_n_rows = gda_sybase_recordset_get_n_rows;
	model_class->get_n_columns = gda_sybase_recordset_get_n_columns;
	model_class->get_row = gda_sybase_recordset_get_row;
	model_class->get_value_at = gda_sybase_recordset_get_value_at;
}

static void
gda_sybase_recordset_init (GdaSybaseRecordset *recset,
                           GdaSybaseRecordsetClass *klass)
{
	g_return_if_fail (GDA_IS_SYBASE_RECORDSET (recset));

	recset->priv = g_new0 (GdaSybaseRecordsetPrivate, 1);
	recset->priv->rows = g_ptr_array_new ();
	recset->priv->columns = g_ptr_array_new ();
	recset->priv->fetched_all_results = FALSE;
}

static void
gda_sybase_recordset_finalize (GObject *object)
{
	GdaSybaseRecordset *recset = (GdaSybaseRecordset *) object;

	g_return_if_fail (GDA_IS_SYBASE_RECORDSET (recset));

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
				GdaSybaseField *field = (GdaSybaseField *) g_ptr_array_index (recset->priv->columns, 0);

				if (field != NULL) {
					if (field->data != NULL) {
						g_free (field->data);
						field->data = NULL;
					}
					g_free (field);
					field = NULL;
				}
				g_ptr_array_remove_index (recset->priv->columns, 0);
			}
			g_ptr_array_free (recset->priv->columns, TRUE);
			recset->priv->columns = NULL;
		}
		g_free (recset->priv);
		recset->priv = NULL;
	}

	parent_class->finalize (object);
}

static GdaRow *
gda_sybase_create_current_row (GdaSybaseRecordset *recset)
{
	GdaRow *row = NULL;
	gint i = 0;

	g_return_val_if_fail (GDA_IS_SYBASE_RECORDSET (recset), NULL);
	g_return_val_if_fail (recset->priv != NULL, NULL);
	g_return_val_if_fail (recset->priv != NULL, NULL);

	row = gda_row_new (recset->priv->columns->len);
	g_return_val_if_fail (row != NULL, NULL);

	for (i = 0; i < recset->priv->columns->len; i++) {
		GdaValue       *value;
		GdaSybaseField *sfield;

		value = gda_row_get_value (row, i);
		sfield = (GdaSybaseField *) g_ptr_array_index (recset->priv->columns, i);

		gda_sybase_set_gda_value (recset->priv->scnc, value, sfield);
	}

	return row;
}

GType
gda_sybase_recordset_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaSybaseRecordsetClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_sybase_recordset_class_init,
			NULL,
			NULL,
			sizeof (GdaSybaseRecordset),
			0,
			(GInstanceInitFunc) gda_sybase_recordset_init
		};
		type = g_type_register_static (PARENT_TYPE,
		                               "GdaSybaseRecordset",
		                               &info, 0);
	}
	return type;
}

static GdaSybaseRecordset *
gda_sybase_process_row_result (GdaConnection           *cnc,
                               GdaSybaseConnectionData *scnc,
                               gboolean                fetchall)
{
	GdaSybaseRecordset *srecset = NULL;
	GdaSybaseField     *sfield = NULL;
	GdaRow             *row = NULL;
	GdaError           *error = NULL;
	gboolean           columns_set = FALSE;
	gboolean           sfields_allocated = TRUE;
	gint               i = 0;
	CS_INT             rows_read = 0;
	gint               row_cnt = 0;

	// called within gda_sybase_recordset_new
	// scnc->cmd != NULL, cnc != NULL
	
	srecset = g_object_new (GDA_TYPE_SYBASE_RECORDSET, NULL);
	if ((srecset != NULL) && (srecset->priv != NULL)
	    && (srecset->priv->columns != NULL) 
	    && (srecset->priv->rows != NULL)) {
		srecset->priv->cnc = cnc;
		srecset->priv->scnc = scnc;
	} else {
		// most probably a lack of ram failure, 
		// so don't alloc e.g. an instance of GdaError
		if (srecset) {
			g_object_unref (srecset);
			srecset = NULL;
		}
		sybase_error_msg (_("Could not allocate datamodel. No results will be returned."));
		scnc->ret = ct_cancel (NULL, scnc->cmd, CS_CANCEL_CURRENT);
		if (scnc->ret != CS_SUCCEED) {
			sybase_error_msg (_("ct_cancel() failed."));
		}
		return NULL;
	}

//	g_object_unref (srecset);

	// step 1: determine count of columns
	scnc->ret = ct_res_info(scnc->cmd, CS_NUMDATA,
	                        &srecset->priv->colcnt, CS_UNUSED, NULL);
	if (scnc->ret != CS_SUCCEED) {
		error = gda_sybase_make_error (scnc,
		                               _("%s failed while processing a row result."),
		                               "ct_res_info()");
		gda_connection_add_error (cnc, error);
		return NULL;
	}
	if (srecset->priv->colcnt <= 0) {
		error = gda_sybase_make_error (scnc,
		                               _("%s returned <= 0 columns."),
		                               "ct_res_info()");
		gda_connection_add_error (cnc, error);
		return NULL;
	}

	// step 2: allocate memory for column metainfo
	for (i = 0; i < srecset->priv->colcnt; i++) {
		sfield = g_new0 (GdaSybaseField, 1);
		if (sfield != NULL) {
			g_ptr_array_add (srecset->priv->columns, sfield);
		} else {
			sfields_allocated = FALSE;
		}
	}

	// out of memory? fatal, just log and cancel all
	if (!sfields_allocated) {
		g_object_unref (srecset);
		srecset = NULL;
		sybase_error_msg (_("Could not allocate structure for column metainformation."));

		scnc->ret = ct_cancel (NULL, scnc->cmd, CS_CANCEL_ALL);
		if (scnc->ret != CS_SUCCEED) {
			sybase_error_msg (_("Could not call %s while processing row resultset."),
			                  "ct_cancel()");
		}
		return srecset;
	}

//	sybase_debug_msg (_("Counted %d columns, allocated metainf for %d"),
//	                  srecset->priv->colcnt, srecset->priv->columns->len);
	
	// step 3: collect information for each column,
	//         allocate a buffer for data and ct_bind it
	for (i = 0; i < srecset->priv->colcnt; i++) {
		sfield = (GdaSybaseField *) g_ptr_array_index (srecset->priv->columns, i);
		memset (&sfield->fmt, 0, sizeof(sfield->fmt));
		
		scnc->ret = ct_describe (scnc->cmd, i + 1, &sfield->fmt);
		if (scnc->ret != CS_SUCCEED) {
			error = gda_sybase_make_error (scnc, 
			                               _("Could not run %s on column %d"), "ct_describe()", i);
			gda_connection_add_error (cnc, error);
			break;
		}

		sfield->data = g_new0 (gchar, sfield->fmt.maxlength + 1);
		if (sfield->data == NULL) {
			error = gda_sybase_make_error (scnc,
			                               _("Could not allocate data placeholder for column %d"), i);
			gda_connection_add_error (cnc, error);
			break;
		}

		scnc->ret = ct_bind (scnc->cmd, i + 1, &sfield->fmt,
		                     (CS_CHAR *) sfield->data,
				     &sfield->datalen,
				     &sfield->indicator);
		if (scnc->ret != CS_SUCCEED) {
			error = gda_sybase_make_error (scnc,
			                               _("Could not run %s on column %d"), "ct_bind()", i);
			gda_connection_add_error (cnc, error);
			break;
		}

		if (sfield->fmt.namelen > 0) {
			gda_data_model_set_column_title (GDA_DATA_MODEL (srecset),
			                                 i, sfield->fmt.name);
		}
	}

	// catch error: ct_describe or g_new0 or ct_bind failed
	if ((scnc->ret != CS_SUCCEED) || (sfield->data == NULL)) {
		g_object_unref (srecset);
		srecset = NULL;

		scnc->ret = ct_cancel (NULL, scnc->cmd, CS_CANCEL_ALL);
		if (scnc->ret != CS_SUCCEED) {
			error = gda_sybase_make_error (scnc, _("Could not run %s to cancel processing row resultset."), "ct_cancel");
			gda_connection_add_error (cnc, error);
		}
		
		return srecset;
	}
	columns_set = TRUE;

	// step 4: now proceed with fetching the data
	while ((scnc->ret = ct_fetch (scnc->cmd, CS_UNUSED, CS_UNUSED,
	                              CS_UNUSED, &rows_read) == CS_SUCCEED)
	       || (scnc->ret == CS_ROW_FAIL)) {
		row_cnt += rows_read;
		
		if (scnc->ret == CS_ROW_FAIL) {
			error = gda_sybase_make_error (scnc, _("%s failed on row %d"), "ct_fetch()", row_cnt);
			gda_connection_add_error (cnc, error);
		}
		
		// srecset->columns contains the data;
		// make a row out of it
		row = gda_sybase_create_current_row (srecset);
		if (row) {
			g_ptr_array_add(srecset->priv->rows, row);
			srecset->priv->rowcnt++;
		}
	}

//	sybase_debug_msg (_("Got %d of %d rows"), srecset->priv->rowcnt, row_cnt);
	// step 5: verify ct_fetch's last return code
	switch (scnc->ret) {
		case CS_END_DATA:
//			sybase_debug_msg (_("Row processing succeeded."));
			break;
		case CS_CANCELED:
//			sybase_debug_msg (_("Row processing canceled."));
			break;
		case CS_FAIL:
//			sybase_debug_msg (_("%s returned CS_FAIL. Probably o.k."), "ct_fetch()");
			break;
		default:
			error = gda_sybase_make_error (scnc, _("%s terminated with unexpected return code."), "ct_fetch()");
			gda_connection_add_error (cnc, error);
	}
	
	return srecset;
}

// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// gda_sybase_recordset_new is a general result-processing routine
// for processing a single ct_results() step.
// 
// Therefore it may also return no result, e.g. if the resulttype is
// CS_CMD_DONE, i.e. we finished processing a command.
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
GdaDataModel *
gda_sybase_recordset_new (GdaConnection *cnc,
                          gboolean fetchall)
{
	GdaSybaseConnectionData *scnc = NULL;
	GdaSybaseRecordset      *srecset = NULL;
//	GdaSybaseField          *sfield = NULL;
//	GdaRow                  *row = NULL;
	GdaError                *error = NULL;
//	gboolean                columns_set = FALSE;
//	gint                    i = 0;

	g_return_val_if_fail (GDA_IS_CONNECTION(cnc), NULL);
	scnc = (GdaSybaseConnectionData *) g_object_get_data (G_OBJECT (cnc),
	                                           OBJECT_DATA_SYBASE_HANDLE);
	g_return_val_if_fail (scnc != NULL, NULL);
	g_return_val_if_fail (scnc->cmd != NULL, NULL);

	// now nothing may fail;
	// a result not processed may block the entire connection
	// each process_* routine must check if recset is valid,
	// otherwise alt. a) too process the results (eventually discard)
	//           alt. b) call ct_cancel, dropping _all_ results of
	//                   the entire query
	
	
	// we are within the resultset processing loop,
	// ct_results (scnc->cmd, &scnc->res_type) = CS_SUCCEED

	switch (scnc->res_type) {
		case CS_ROW_RESULT:
			srecset = gda_sybase_process_row_result (cnc, scnc,
			                                         TRUE);
			break;
	// FIXME: implement CS_CURSOR_RESULT .. CS_COMPUTEFMT_RESULT
		case CS_CURSOR_RESULT:
		case CS_PARAM_RESULT:
		case CS_STATUS_RESULT:
		case CS_COMPUTE_RESULT:
		case CS_MSG_RESULT:
		case CS_DESCRIBE_RESULT:
		case CS_ROWFMT_RESULT:
		case CS_COMPUTEFMT_RESULT:
			scnc->ret = ct_cancel (NULL, scnc->cmd,
			                       CS_CANCEL_CURRENT);
			break;
		case CS_CMD_DONE:
//			sybase_debug_msg (_("Done with resultset."));
			break;
		case CS_CMD_SUCCEED:
//			sybase_debug_msg (_("Command returned no rows."));
			srecset = g_object_new (GDA_TYPE_SYBASE_RECORDSET, NULL);
			if ((srecset != NULL) && (srecset->priv != NULL)
			    && (srecset->priv->columns != NULL) 
			    && (srecset->priv->rows != NULL)) {
				srecset->priv->cnc = cnc;
				srecset->priv->scnc = scnc;
			}
			break;
		case CS_CMD_FAIL:
			sybase_debug_msg (_("%s returned %s"),
			                  "ct_results()", "CS_CMD_FAIL");
			break;
	}

	if (srecset) 
		return GDA_DATA_MODEL (srecset);
	else
		return NULL;
}
