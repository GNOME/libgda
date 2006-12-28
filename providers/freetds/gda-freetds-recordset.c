/* GDA FreeTDS provider
 * Copyright (C) 1998 - 2005 The GNOME Foundation
 *
 * AUTHORS:
 *         Holger Thon <holger.thon@gnome-db.org>
 *         Vivien Malerba <malerba@gnome-db.org>
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
#endif

#include <glib/gi18n-lib.h>
#include <libgda/gda-data-model.h>
#include <string.h>
#include <tds.h>
#include <tdsconvert.h>
#include "gda-freetds.h"
#include "gda-freetds-recordset.h"
#include "gda-freetds-defs.h"

#ifdef PARENT_TYPE
#undef PARENT_TYPE
#endif

#define PARENT_TYPE GDA_TYPE_DATA_MODEL_ROW

/*
 * Private declarations and functions
 */

static GObjectClass *parent_class = NULL;

static void gda_freetds_recordset_class_init (GdaFreeTDSRecordsetClass *klass);
static void gda_freetds_recordset_init       (GdaFreeTDSRecordset *recset,
                                              GdaFreeTDSRecordsetClass *klass);
static void gda_freetds_recordset_finalize   (GObject *object);

static void gda_freetds_recordset_describe_column (GdaDataModel *model, gint col);
static gint gda_freetds_recordset_get_n_rows (GdaDataModelRow *model);
static gint gda_freetds_recordset_get_n_columns (GdaDataModelRow *model);
static const GdaRow *gda_freetds_recordset_get_row (GdaDataModelRow *model,
                                                    gint row, GError **error);
static const GValue *gda_freetds_recordset_get_value_at (GdaDataModelRow *model,
                                                           gint         col,
                                                           gint         row);

/* Private utility functions */

/* w/o results */
static _TDSCOLINFO *gda_freetds_dup_tdscolinfo (_TDSCOLINFO *col);
static GdaRow *gda_freetds_get_current_row(GdaFreeTDSRecordset *recset);

static gint
gda_freetds_recordset_get_n_rows (GdaDataModelRow *model)
{
	GdaFreeTDSRecordset *recset = (GdaFreeTDSRecordset *) model;

        g_return_val_if_fail (GDA_IS_FREETDS_RECORDSET (recset), -1);
        return (recset->priv->rowcnt);
}

static gint
gda_freetds_recordset_get_n_columns (GdaDataModelRow *model)
{
	GdaFreeTDSRecordset *recset = (GdaFreeTDSRecordset *) model;

        g_return_val_if_fail (GDA_IS_FREETDS_RECORDSET (recset), -1);
        return (recset->priv->colcnt);
}

static const GdaRow *
gda_freetds_recordset_get_row (GdaDataModelRow *model, gint row, GError **error)
{
	GdaFreeTDSRecordset *recset = (GdaFreeTDSRecordset *) model;

	g_return_val_if_fail (GDA_IS_FREETDS_RECORDSET (recset), NULL);
	g_return_val_if_fail (recset->priv != NULL, NULL);
	
	
	if (!recset->priv->rows)
		return NULL;
	
	if (row >= recset->priv->rows->len)
		return NULL;

	return (const GdaRow *) g_ptr_array_index (recset->priv->rows, row);
}

static const GValue *
gda_freetds_recordset_get_value_at (GdaDataModelRow *model, gint col, gint row)
{
	GdaFreeTDSRecordset *recset = (GdaFreeTDSRecordset *) model;
	const GdaRow *fields;

	g_return_val_if_fail (GDA_IS_FREETDS_RECORDSET (recset), NULL);
	g_return_val_if_fail (recset->priv != NULL, NULL);

	if (col >= recset->priv->colcnt)
		return NULL;

	fields = gda_freetds_recordset_get_row (model, row, NULL);
	return fields != NULL ? gda_row_get_value ((GdaRow *) fields, col) : NULL;
}

static void
gda_freetds_recordset_class_init (GdaFreeTDSRecordsetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaDataModelRowClass *model_class = GDA_DATA_MODEL_ROW_CLASS (klass);

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
	recset->priv->columns = g_ptr_array_new ();
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
				if (row != NULL) {
					g_object_unref (row);
					row = NULL;
				}
				g_ptr_array_remove_index (recset->priv->rows, 0);
			}
			g_ptr_array_free (recset->priv->rows, TRUE);
			recset->priv->rows = NULL;
		}
		if (recset->priv->columns) {
			while (recset->priv->columns->len > 0) {
				_TDSCOLINFO *col = (_TDSCOLINFO *) g_ptr_array_index (recset->priv->columns, 0);
				if (col != NULL) {
					g_free (col);
					col = NULL;
				}
				g_ptr_array_remove_index (recset->priv->columns, 0);
			}
			g_ptr_array_free (recset->priv->columns, TRUE);
			recset->priv->columns = NULL;
		}
			
		g_free(recset->priv);
		recset->priv = NULL;
	}

	parent_class->finalize (object);
}

static GdaRow *
gda_freetds_get_current_row(GdaFreeTDSRecordset *recset)
{
	GdaRow *row = NULL;
	gchar *val = NULL;
	gint i = 0;
	
	g_return_val_if_fail (GDA_IS_FREETDS_RECORDSET (recset), NULL);
	g_return_val_if_fail (recset->priv != NULL, NULL);
	g_return_val_if_fail (recset->priv->res != NULL, NULL);
/*
	g_return_val_if_fail (recset->priv->colcnt 
	                      != recset->priv->res->num_cols, NULL);
*/

	row = gda_row_new(GDA_DATA_MODEL (recset), recset->priv->res->num_cols);
	g_return_val_if_fail (row != NULL, NULL);
	
	for (i = 0; i < recset->priv->res->num_cols; i++) {
		GValue   *field;
		_TDSCOLINFO *col;

		field = gda_row_get_value (row, i);
		col = recset->priv->res->columns[i];
		val = &(recset->priv->res->current_row[col->column_offset]);

		gda_freetds_set_gdavalue (field, val, col,
		                          recset->priv->tds_cnc);
	}
	
	return row;
}

static _TDSCOLINFO *
gda_freetds_dup_tdscolinfo (_TDSCOLINFO *col)
{
	_TDSCOLINFO *copy = NULL;

	g_return_val_if_fail (col != NULL, NULL);

	copy = g_new0(_TDSCOLINFO, 1);
	if (copy) {
		memcpy(copy, col, sizeof(_TDSCOLINFO));
		
		/* set pointers to NULL */
		copy->column_nullbind = NULL;
#if FREETDS_VERSION > 6000
		copy->column_varaddr = NULL;
#else
		copy->varaddr = NULL;
		copy->column_textvalue = NULL;
#endif
		copy->column_lenbind = NULL;
	}

	return copy;
}

/*
 * Public functions                                                       
 */

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

static void
gda_freetds_recordset_describe_column (GdaDataModel *model, gint col)
{
	GdaFreeTDSRecordset *recset = (GdaFreeTDSRecordset *) model;
	_TDSCOLINFO          *colinfo = NULL;
	GdaColumn           *attribs;
	gchar               name[256];

	g_return_if_fail (GDA_IS_FREETDS_RECORDSET (recset));
	g_return_if_fail (recset->priv != NULL);
	g_return_if_fail (recset->priv->columns != NULL);
	g_return_if_fail (col < recset->priv->columns->len);

	colinfo = (_TDSCOLINFO *) g_ptr_array_index (recset->priv->columns, col);
	g_return_if_fail (colinfo);
	
	attribs = gda_data_model_describe_column (model, col);

	gda_column_set_title (attribs, colinfo->column_name);

	memcpy (name, colinfo->column_name,
	        colinfo->column_namelen);
	name[colinfo->column_namelen] = '\0';

	gda_column_set_name (attribs, name);
	gda_column_set_scale (attribs, colinfo->column_scale);
	gda_column_set_g_type (attribs,
	                                  gda_freetds_get_value_type (colinfo));
	gda_column_set_defined_size (attribs, colinfo->column_size);

	/* FIXME: */
	gda_column_set_references (attribs, "");
	gda_column_set_primary_key (attribs, FALSE);
	gda_column_set_unique_key (attribs, FALSE);

	gda_column_set_allow_null (attribs, 
	                                     !(colinfo->column_nullable == 0));
}

GdaDataModel *
gda_freetds_recordset_new (GdaConnection *cnc, gboolean fetchall)
{
	GdaFreeTDSConnectionData *tds_cnc = NULL;
	GdaFreeTDSRecordset *recset = NULL;
	_TDSCOLINFO *col = NULL;
	GdaRow *row = NULL;
	GdaConnectionEvent *error = NULL;
	gboolean columns_set = FALSE;
	gint i;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	tds_cnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_FREETDS_HANDLE);
	g_return_val_if_fail (tds_cnc != NULL, NULL);

	recset = g_object_new (GDA_TYPE_FREETDS_RECORDSET, NULL);
	g_return_val_if_fail (recset != NULL, NULL);

	recset->priv->cnc = cnc;
	recset->priv->tds_cnc = tds_cnc;
	recset->priv->res = tds_cnc->tds->res_info;

#if FREETDS_VERSION > 6000
#if FREETDS_VERSION >= 6400
	while ((tds_cnc->rc = tds_process_tokens (tds_cnc->tds,
							 &tds_cnc->result_type, NULL,
							 TDS_RETURN_ROWFMT | TDS_RETURN_COMPUTEFMT |
							 TDS_RETURN_DONE | TDS_STOPAT_ROW |
							 TDS_STOPAT_COMPUTE | TDS_RETURN_PROC))
#elif FREETDS_VERSION >= 6200
	while ((tds_cnc->rc = tds_process_result_tokens (tds_cnc->tds,
							 &tds_cnc->result_type, NULL))
#else
	while ((tds_cnc->rc = tds_process_result_tokens (tds_cnc->tds,
							 &tds_cnc->result_type))
#endif  /* FREETDS_VERSION >= 6400 */
	       == TDS_SUCCEED) {
		if (tds_cnc->result_type == TDS_ROW_RESULT) {
			gint row_type, compute_id;

#if FREETDS_VERSION >= 6400
			while ((tds_cnc->rc = tds_process_tokens(tds_cnc->tds, &row_type, &compute_id, TDS_STOPAT_ROWFMT | TDS_RETURN_DONE | TDS_RETURN_ROW | TDS_RETURN_COMPUTE))
#else
			while ((tds_cnc->rc = tds_process_row_tokens(tds_cnc->tds, &row_type, &compute_id))
#endif
#else
			       while ((tds_cnc->rc = tds_process_result_tokens(tds_cnc->tds))
	       == TDS_SUCCEED) {
		if (tds_cnc->tds->res_info->rows_exist) {
			while ((tds_cnc->rc = tds_process_row_tokens(tds_cnc->tds))
#endif
			       == TDS_SUCCEED) {
				recset->priv->res = tds_cnc->tds->res_info;
				if (columns_set == FALSE) {
					columns_set = TRUE;
					recset->priv->colcnt = recset->priv->res->num_cols;
					for (i = 0; i < recset->priv->colcnt; i++) {
						col = gda_freetds_dup_tdscolinfo (recset->priv->res->columns[i]);
						g_ptr_array_add (recset->priv->columns,
						                 col);
					}
				}

				row = gda_freetds_get_current_row(recset);
				if (row) {
					g_ptr_array_add(recset->priv->rows, row);
					recset->priv->rowcnt++;
				}
			}
			if (tds_cnc->rc == TDS_FAIL) {
				error = gda_freetds_make_error(tds_cnc->tds,
				                               _("Error processing result rows.\n"));
				gda_connection_add_event (cnc, error);
				g_object_unref (recset);
				recset = NULL;
				return NULL;
#if FREETDS_VERSION >= 6400
			} else if (tds_cnc->rc != TDS_NO_MORE_RESULTS) {
#else
			} else if (tds_cnc->rc != TDS_NO_MORE_ROWS) {
#endif
				error = gda_freetds_make_error(tds_cnc->tds,
				                               _("Unexpected freetds return code in tds_process_row_tokens().\n"));
				gda_connection_add_event (cnc, error);
				g_object_unref (recset);
				recset = NULL;
				return NULL;
			}
		}
	}
	if (tds_cnc->rc == TDS_FAIL) {
		error = gda_freetds_make_error(tds_cnc->tds,
		                               _("Error processing results.\n"));
		gda_connection_add_event(cnc, error);
		g_object_unref (recset);
		recset = NULL;
		return NULL;
	} else if (tds_cnc->rc != TDS_NO_MORE_RESULTS) {
		error = gda_freetds_make_error(tds_cnc->tds,
		                               _("Unexpected freetds return code in tds_process_result_tokens\n"));
		gda_connection_add_event(cnc, error);
		g_object_unref (recset);
		recset = NULL;
		return NULL;
	}
	for (i = 0; i < recset->priv->columns->len; i++)
		gda_freetds_recordset_describe_column (GDA_DATA_MODEL (recset), i);

	return GDA_DATA_MODEL (recset);
}
