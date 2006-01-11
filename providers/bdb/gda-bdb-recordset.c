/* GDA Berkeley-DB Provider
 * Copyright (C) 1998-2002 The GNOME Foundation
 *
 * AUTHORS:
 *      Laurent Sansonetti <lrz@gnome.org>
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
#include <string.h>
#include "gda-bdb.h"

#ifdef PARENT_TYPE
#undef PARENT_TYPE
#endif

#define PARENT_TYPE GDA_TYPE_DATA_MODEL_HASH

struct _GdaBdbRecordsetPrivate {
	GdaConnection *cnc;
	DBC *dbcp;
	gint nrows, ncolumns;
};

static void gda_bdb_recordset_class_init (GdaBdbRecordsetClass *klass);
static void gda_bdb_recordset_init       (GdaBdbRecordset *recset,
					  GdaBdbRecordsetClass *klass);
static void gda_bdb_recordset_finalize   (GObject *object);

static const GdaValue *gda_bdb_recordset_get_value_at (GdaDataModelRow *model, 
						       gint col,
						       gint row);
static gint gda_bdb_recordset_get_n_rows 	      (GdaDataModelRow *model);
static const GdaRow *gda_bdb_recordset_get_row 	      (GdaDataModelRow *model,
						       gint rownum, GError **error);

static GObjectClass *parent_class = NULL;

/*
 * Private functions
 */

/*
 * Object init and finalize
 */
static void
gda_bdb_recordset_init (GdaBdbRecordset *recset,
			GdaBdbRecordsetClass *klass)
{
	g_return_if_fail (GDA_IS_BDB_RECORDSET (recset));

	recset->priv = g_new0 (GdaBdbRecordsetPrivate, 1);
}

static void
gda_bdb_recordset_class_init (GdaBdbRecordsetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaDataModelRowClass *model_class = GDA_DATA_MODEL_ROW_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_bdb_recordset_finalize;
	model_class->get_n_rows = gda_bdb_recordset_get_n_rows;
	model_class->get_value_at = gda_bdb_recordset_get_value_at;
	model_class->get_row = gda_bdb_recordset_get_row;
}

static void
gda_bdb_recordset_finalize (GObject * object)
{
	GdaBdbRecordset *recset = (GdaBdbRecordset *) object;

	g_return_if_fail (GDA_IS_BDB_RECORDSET (recset));

	g_free (recset->priv);
	recset->priv = NULL;

	parent_class->finalize (object);
}

static const GdaRow *
gda_bdb_recordset_get_row (GdaDataModelRow *model, gint row_num, GError **error)
{
	GdaBdbRecordset *recset;
	DBT key, data;
	GdaRow *row;
	int i, ret;
	DBC *dbcp;
	GdaBinary bin;

	recset = (GdaBdbRecordset *) model;
	g_return_val_if_fail (GDA_IS_BDB_RECORDSET (recset), NULL);
	g_return_val_if_fail (recset->priv != NULL, 0);

	if (row_num < 0 || row_num >= recset->priv->nrows) {
		gchar *str;

		str = g_strdup_printf (_("Row number out of range 0 - %d"),
				       recset->priv->nrows - 1);
				       
		gda_connection_add_event_string (recset->priv->cnc, str);
		g_set_error (error, 0, 0, str);
		g_free (str);
		return NULL;
	}

	row = (GdaRow *) gda_data_model_hash_get_row (model, row_num);
	if (row != NULL)
		return (const GdaRow *) row;

	/* move cursor to the right record (we can't use DB_SET_RECNO
	   since it can be used only with Btree schemas, and won't
	   work with other kind of databases). */
	dbcp = recset->priv->dbcp;
	memset (&key, 0, sizeof key);
	memset (&data, 0, sizeof data);
	ret = dbcp->c_get (dbcp, &key, &data, DB_FIRST);
	if (ret != 0) {
		gda_connection_add_event (recset->priv->cnc,
					  gda_bdb_make_error (ret));
		return NULL;
	}
	for (i = 0; i < row_num; i++) {
		memset (&key, 0, sizeof key);
		memset (&data, 0, sizeof data);
		ret = dbcp->c_get (dbcp, &key, &data, DB_NEXT);
		if (ret != 0) {
			gda_connection_add_event (recset->priv->cnc,
						  gda_bdb_make_error (ret));
			return NULL;
		}
	}
 
	row = gda_row_new (GDA_DATA_MODEL (model), 2);
	bin.data = key.data;
	bin.binary_length = key.size;
	gda_value_set_binary ((GdaValue *) gda_row_get_value (row, 0), &bin);
	bin.data = data.data;
	bin.binary_length = data.size;
	gda_value_set_binary ((GdaValue *) gda_row_get_value (row, 1), &bin);

	return row;
}

static const GdaValue *
gda_bdb_recordset_get_value_at (GdaDataModelRow *model, gint col_num, gint row_num)
{
	GdaBdbRecordset *recset;
	GdaRow *row;

	recset = (GdaBdbRecordset *) model;
	g_return_val_if_fail (GDA_IS_BDB_RECORDSET (recset), NULL);
	g_return_val_if_fail (recset->priv != NULL, 0);

	row = (GdaRow *) gda_bdb_recordset_get_row (model, row_num, NULL);
	if (row == NULL)
		return NULL;
	if (col_num < 0 || col_num >= gda_row_get_length (row)) {
		gda_connection_add_event_string (recset->priv->cnc,
						 _("Column number out "
						   "of range"));
		return NULL;
	}
	return gda_row_get_value (row, col_num);
}

static gint
gda_bdb_recordset_get_n_rows (GdaDataModelRow *model)
{
	GdaBdbRecordset *recset = (GdaBdbRecordset *) model;

	g_return_val_if_fail (GDA_IS_BDB_RECORDSET (model), 0);
	g_return_val_if_fail (recset->priv != NULL, 0);

	return recset->priv->nrows; 
}

/*
 * Public functions
 */

GType
gda_bdb_recordset_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaBdbRecordsetClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_bdb_recordset_class_init,
			NULL,
			NULL,
			sizeof (GdaBdbRecordset),
			0,
			(GInstanceInitFunc) gda_bdb_recordset_init
		};
		type = g_type_register_static (PARENT_TYPE,
					       "GdaBdbRecordset",
					       &info,
					       0);
	}
	return type;
}

GdaDataModel *
gda_bdb_recordset_new (GdaConnection *cnc, DB *dbp)
{
	GdaBdbConnectionData *cnc_priv_data;
	GdaBdbRecordset *model;
	DB_BTREE_STAT *statp;
	int ret, nrows;
	DBC *dbcp;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (dbp != NULL, NULL);

	cnc_priv_data = g_object_get_data (G_OBJECT (cnc),
					   OBJECT_DATA_BDB_HANDLE);

	/* get the number of records in the database */
	ret = dbp->stat (dbp,
#if BDB_VERSION > 40300
			 NULL,
#endif
			 &statp,

#if BDB_VERSION < 40000
			 NULL,
#endif
			 0);
	if (ret != 0) {
		gda_connection_add_event (cnc, gda_bdb_make_error (ret));
		return NULL;
	}
	nrows = (int) statp->bt_ndata;
	free (statp);
	if (nrows <= 0) {
		gda_connection_add_event_string (cnc, _("Database is empty"));
		return NULL;
	}
	
	/* acquire a cursor on the database */
	ret = dbp->cursor (dbp, NULL, &dbcp, 0);
	if (ret != 0) {
		gda_connection_add_event (cnc, gda_bdb_make_error (ret));
		return NULL;
	}

	/* set private data */
	model = g_object_new (GDA_TYPE_BDB_RECORDSET, NULL);
	model->priv->dbcp = dbcp;
	model->priv->nrows = nrows; 
	model->priv->ncolumns = 2;	/* key and data */
	model->priv->cnc = cnc;

	gda_data_model_hash_set_n_columns (GDA_DATA_MODEL_HASH (model),
					   model->priv->ncolumns);

	return GDA_DATA_MODEL (model);
}
