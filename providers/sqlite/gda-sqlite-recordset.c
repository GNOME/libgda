/* GDA SQLite provider
 * Copyright (C) 1998 - 2005 The GNOME Foundation.
 *
 * AUTHORS:
 *	   Rodrigo Moya <rodrigo@gnome-db.org>
 *         Carlos Perelló Marín <carlos@gnome-db.org>
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

#include <string.h>
#include <libgda/gda-intl.h>
#include "gda-sqlite.h"
#include "gda-sqlite-recordset.h"
#include "gda-sqlite-provider.h"

struct _GdaSqliteRecordsetPrivate {
	SQLITEresult  *sres;
	GdaConnection *cnc;
	gint           ncolumns;
	gint           nrows;
};

static void gda_sqlite_recordset_class_init (GdaSqliteRecordsetClass *klass);
static void gda_sqlite_recordset_init       (GdaSqliteRecordset *recset,
					     GdaSqliteRecordsetClass *klass);
static void gda_sqlite_recordset_finalize   (GObject *object);

static gint gda_sqlite_recordset_get_n_rows (GdaDataModelBase *model);
static GdaColumn *gda_sqlite_recordset_describe_column (GdaDataModelBase *model, gint col);

static GObjectClass *parent_class = NULL;

/*
 * Private functions
 */

/*
 * Object init and finalize
 */
static void
gda_sqlite_recordset_init (GdaSqliteRecordset *recset,
			   GdaSqliteRecordsetClass *klass)
{
	g_return_if_fail (GDA_IS_SQLITE_RECORDSET (recset));

	recset->priv = g_new0 (GdaSqliteRecordsetPrivate, 1);
}

static void
gda_sqlite_recordset_class_init (GdaSqliteRecordsetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaDataModelBaseClass *model_class = GDA_DATA_MODEL_BASE_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_sqlite_recordset_finalize;
	model_class->get_n_rows = gda_sqlite_recordset_get_n_rows;
	model_class->describe_column = gda_sqlite_recordset_describe_column;
}

static void
gda_sqlite_recordset_finalize (GObject *object)
{
	GdaSqliteRecordset *recset = (GdaSqliteRecordset *) object;

	g_return_if_fail (GDA_IS_SQLITE_RECORDSET (recset));

	if (recset->priv->sres != NULL) {
		gda_sqlite_free_result (recset->priv->sres);
		recset->priv->sres = NULL;
	}

	g_free (recset->priv);
	recset->priv = NULL;
	
	parent_class->finalize (object);
}


/*
 * Overrides
 */
static GdaColumn *
gda_sqlite_recordset_describe_column (GdaDataModelBase *model, gint col)
{
	GdaSqliteRecordset *recset = (GdaSqliteRecordset *) model;
	GdaSqliteRecordsetPrivate *priv_data;
	SQLITEresult *sres;
	GdaColumn *field_attrs;

	g_return_val_if_fail (GDA_IS_SQLITE_RECORDSET (recset), NULL);
	g_return_val_if_fail (recset->priv != NULL, NULL);

	priv_data = recset->priv;
	sres = priv_data->sres;
	
	if (!sres) {
		gda_connection_add_event_string (priv_data->cnc,
						 _("Invalid SQLite handle"));
		return NULL;
	}

	if (col >= priv_data->ncolumns) {
		gda_connection_add_event_string (priv_data->cnc,
						 _("Column number out of range"));
		return NULL;
	}

	field_attrs = gda_column_new ();
	gda_column_set_name (field_attrs, sqlite3_column_name (sres->stmt, col));
	gda_column_set_scale (field_attrs, 0);
	gda_column_set_gdatype (field_attrs, GDA_VALUE_TYPE_STRING);
	gda_column_set_defined_size (field_attrs, sres->cols_size [col]);
	gda_column_set_primary_key (field_attrs, FALSE);
	gda_column_set_unique_key (field_attrs, FALSE);
	gda_column_set_allow_null (field_attrs, TRUE);
	gda_column_set_auto_increment (field_attrs, FALSE);

	return field_attrs;
}

static gint
gda_sqlite_recordset_get_n_rows (GdaDataModelBase *model)
{
	GdaSqliteRecordset *recset = (GdaSqliteRecordset *) model;

	g_return_val_if_fail (GDA_IS_SQLITE_RECORDSET (recset), 0);
	g_return_val_if_fail (recset->priv != NULL, 0);

	return recset->priv->nrows;
}

/*
 * Public functions
 */

GType
gda_sqlite_recordset_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaSqliteRecordsetClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_sqlite_recordset_class_init,
			NULL,
			NULL,
			sizeof (GdaSqliteRecordset),
			0,
			(GInstanceInitFunc) gda_sqlite_recordset_init
		};
		type = g_type_register_static (GDA_TYPE_DATA_MODEL_HASH, "GdaSqliteRecordset", &info, 0);
	}

	return type;
}

/*
 * the @sres struct is modified and transfered to the new data model created in
 * this function
 */
GdaDataModel *
gda_sqlite_recordset_new (GdaConnection *cnc, SQLITEresult *sres)
{
	GdaSqliteRecordset *model;
	SQLITEcnc *scnc;
	gint i;
	int rc;
	gboolean end = FALSE;
	gboolean types_done = FALSE;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (sres != NULL, NULL);

	scnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_SQLITE_HANDLE);
	
	/* completing sres */
	sres->ncols = sqlite3_column_count (sres->stmt);
	sres->nrows = 0;

	/* model creation */
	model = g_object_new (GDA_TYPE_SQLITE_RECORDSET, NULL);
	model->priv->sres = sres;
	model->priv->cnc = cnc;
	model->priv->ncolumns = sres->ncols;
	gda_data_model_hash_set_n_columns (GDA_DATA_MODEL_HASH (model),
					   model->priv->ncolumns);

	/* computing column types and titles */
	sres->types = g_new0 (GdaValueType, sres->ncols);
	sres->sqlite_types = g_new0 (int, sres->ncols);
	sres->cols_size = g_new0 (int, sres->ncols);
	gda_sqlite_update_types_hash (scnc);

	/* Gda default type & column titles */
	for (i=0; i < sres->ncols; i++) {
		gda_data_model_set_column_title (GDA_DATA_MODEL (model), i,
						 sqlite3_column_name (sres->stmt, i));
		sres->types [i] = GDA_VALUE_TYPE_NULL;
		sres->sqlite_types [i] = SQLITE_NULL;
	}

	/* filling the model with GValues, and computing data types */
	i = 0;
	while (!end) {
		rc = sqlite3_step (sres->stmt);
		switch (rc) {
		case  SQLITE_ROW: {
			/* make a new GdaRow GdaValue */
			GdaRow *row;
			gint col;
			gchar *id;

			row = gda_row_new (GDA_DATA_MODEL (model), sres->ncols);
			for (col = 0; col < sres->ncols; col++) {
				GdaValue *value = gda_row_get_value (row, col);
				int size;
				const gchar *ctype;
				int stype;
				GdaValueType gtype;

				/* SQLite type */
				stype = sqlite3_column_type (sres->stmt, col);
				if (stype != SQLITE_NULL) {
					if (sres->sqlite_types [col] != SQLITE_NULL) {
						if (sres->sqlite_types [col] != stype)
							g_error ("SQLite data types differ in the same column : %d / %d\n", 
								 sres->sqlite_types [col], stype);
					}
					else
						sres->sqlite_types [col] = stype;
				}
				
				/* Gda type */
				ctype = sqlite3_column_decltype (sres->stmt, col);
				if (ctype)
					gtype = GPOINTER_TO_INT (g_hash_table_lookup (scnc->types, ctype));
				else {
					switch (sres->sqlite_types [col]) {
					case SQLITE_INTEGER:
						gtype = GDA_VALUE_TYPE_INTEGER;
						break;
					case SQLITE_FLOAT:
						gtype = GDA_VALUE_TYPE_DOUBLE;
						break;
					case SQLITE_TEXT:
						gtype = GDA_VALUE_TYPE_STRING;
						break;
					case SQLITE_BLOB:
						gtype = GDA_VALUE_TYPE_BLOB;
						break;
					case SQLITE_NULL:
						gtype = GDA_VALUE_TYPE_NULL;
						break;
					default:
						g_error ("Unknown SQLite internal data type %d", sres->sqlite_types [col]);
						break;
					}
				}
				if (gtype != GDA_VALUE_TYPE_NULL) {
					if (sres->types [col] != GDA_VALUE_TYPE_NULL) {
						if (sres->types [col] != gtype)
							g_error ("GDA data types differ in the same column : %d / %d\n", 
								 sres->types [col], gtype);
					}
					else
						sres->types [col] = gtype;
				}
				
				/* compute GdaValue */
				switch (sres->sqlite_types [col]) {
				case SQLITE_INTEGER:
					gda_value_set_integer (value, 
							       sqlite3_column_int (sres->stmt, col));
					break;
				case SQLITE_FLOAT:
					gda_value_set_double (value, 
							      sqlite3_column_double (sres->stmt, col));
					break;
				case SQLITE_TEXT:
					gda_value_set_string (value, 
							      sqlite3_column_text (sres->stmt, col));
					break;
				case SQLITE_BLOB:
					gda_value_set_null (value);
					g_error ("SQLite BLOBS not yet implemented");
					break;
				case SQLITE_NULL:
					gda_value_set_null (value);
					break;
				default:
					g_error ("Unknown SQLite internal data type %d", sres->sqlite_types [col]);
					break;
				}

				size = sqlite3_column_bytes (sres->stmt, col);
				if (sres->cols_size [col] < size)
					sres->cols_size [col] = size;
			}
			
			/* by now, the rowid is just the row number */
			id = g_strdup_printf ("%d", i);
			gda_row_set_id (row, id);
			g_free (id);

			/* insert row */
			gda_data_model_append_row (GDA_DATA_MODEL (model), row);
			types_done = TRUE;
			i++;

			break;
		}
		case SQLITE_BUSY:
			/* nothing to do */
			break;
		case SQLITE_ERROR:
			g_warning ("Sqlite provider internal error: %s", sqlite3_errmsg (scnc->connection));
		case SQLITE_DONE:
			end = TRUE;
			break;
		case SQLITE_MISUSE:
			g_error ("SQLite provider fatal internal error");
			break;
		}
	}

	sres->nrows = i;
	model->priv->nrows = sres->nrows;

	/* show types */
	if (0) {
		for (i = 0; i < sres->ncols; i++) 
			g_print ("Type for col %d: (GDA:%s) (SQLite:%d)\n",
				 i, gda_type_to_string (sres->types [i]),
				 sres->sqlite_types [i]);
		
		gda_data_model_dump (GDA_DATA_MODEL (model), stdout);
	}

	return GDA_DATA_MODEL (model);
}
