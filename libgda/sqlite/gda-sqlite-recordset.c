/* GDA SQLite provider
 * Copyright (C) 1998 - 2006 The GNOME Foundation.
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

#include <stdarg.h>
#include <string.h>
#include <glib/gi18n-lib.h>
#include "gda-sqlite.h"
#include "gda-sqlite-recordset.h"
#include "gda-sqlite-provider.h"
#include <libgda/gda-util.h>

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

static gint gda_sqlite_recordset_get_n_rows (GdaDataModelRow *model);

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
	GdaDataModelRowClass *model_class = GDA_DATA_MODEL_ROW_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_sqlite_recordset_finalize;
	model_class->get_n_rows = gda_sqlite_recordset_get_n_rows;
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
static gint
gda_sqlite_recordset_get_n_rows (GdaDataModelRow *model)
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

	if (G_UNLIKELY (type == 0)) {
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


static GType fuzzy_get_gtype (SQLITEcnc *scnc, SQLITEresult *sres, gint colnum);
static void
gda_sqlite_recordset_fill (GdaSqliteRecordset *model, GdaConnection *cnc, SQLITEresult *sres)
{
	gint i;
	int rc;
	gboolean end = FALSE;
	SQLITEcnc *scnc;

	scnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_SQLITE_HANDLE);

	/* Column titles */
	for (i=0; i < sres->ncols; i++)
		gda_data_model_set_column_title (GDA_DATA_MODEL (model), i,
						 sqlite3_column_name (sres->stmt, i));

	/* filling the model with GValues, and computing data types */
	i = 0;
	while (!end) {
		rc = sqlite3_step (sres->stmt);
		switch (rc) {
		case  SQLITE_ROW: {
			GList *row_values = NULL; /* list of values for a row */
			gint col;

			for (col = 0; col < sres->ncols; col++) {
				GValue *value = NULL;
				int size;
				GType type = sres->types [col];

				if (type == GDA_TYPE_NULL) {
					type = fuzzy_get_gtype (scnc, sres, col);
					if (type != GDA_TYPE_NULL) {
						GdaColumn *column;
						
						sres->types [col] = type;
						column = gda_data_model_describe_column (GDA_DATA_MODEL (model), col);
						gda_column_set_g_type (column, type);
					}
				}

				/* compute GValue */
				if ((sqlite3_column_type (sres->stmt, col) == SQLITE_NULL) ||
				    (type == GDA_TYPE_NULL))
					value = gda_value_new_null ();
				else if (type == G_TYPE_INT)
					g_value_set_int (value = gda_value_new (G_TYPE_INT), 
							 sqlite3_column_int (sres->stmt, col));
				else if (type == G_TYPE_DOUBLE)
					g_value_set_double (value = gda_value_new (G_TYPE_DOUBLE), 
							    sqlite3_column_double (sres->stmt, col));
				else if (type == G_TYPE_STRING)
					g_value_set_string (value = gda_value_new (G_TYPE_STRING), 
							    sqlite3_column_text (sres->stmt, col));
				else if (type == GDA_TYPE_BINARY) {
					GdaBinary *bin;

					bin = g_new0 (GdaBinary, 1);
					bin->binary_length = sqlite3_column_bytes (sres->stmt, col);
					if (bin->binary_length > 0) {
						bin->data = g_new (guchar, bin->binary_length);
						memcpy (bin->data, sqlite3_column_blob (sres->stmt, col),
							bin->binary_length);
					}
					else
						bin->binary_length = 0;
					value = gda_value_new (GDA_TYPE_BINARY);
					gda_value_take_binary (value, bin);
				}
				else
					g_error ("Unhandled GDA type %s in SQLite recordset", 
						 gda_g_type_to_string (sres->types [col]));

				size = sqlite3_column_bytes (sres->stmt, col);
				if (sres->cols_size [col] < size)
					sres->cols_size [col] = size;

				row_values = g_list_prepend (row_values, value);
			}
			row_values = g_list_reverse (row_values);

			/* insert row */
			gda_data_model_append_values (GDA_DATA_MODEL (model), row_values, NULL);
			g_list_foreach (row_values, (GFunc) gda_value_free, NULL);
			g_list_free (row_values);
			
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
#ifdef GDA_DEBUG_NO
		for (i = 0; i < sres->ncols; i++) 
			g_print ("Type for col %d: (GDA:%s)\n",
				 i, gda_g_type_to_string (sres->types [i]));
		
		gda_data_model_dump (GDA_DATA_MODEL (model), stdout);
#endif

	/* filling GdaDataModel columns data */
	for (i = 0; i < sres->ncols; i++) {
		GdaColumn *column;
		const char *tablename, *colname;
		int notnull, autoinc, pkey;

		tablename = sqlite3_column_table_name (sres->stmt, i);
		colname = sqlite3_column_origin_name (sres->stmt, i);
		if (!tablename || !colname || 
		    (sqlite3_table_column_metadata (scnc->connection, NULL, tablename, colname, NULL, NULL,
						    &notnull, &pkey, &autoinc) != SQLITE_OK)) {
			notnull = FALSE;
			autoinc = FALSE;
			pkey = FALSE;
		}

		column = gda_data_model_describe_column (GDA_DATA_MODEL (model), i);
		gda_column_set_name (column, sqlite3_column_name (sres->stmt, i));
		gda_column_set_scale (column, 0);
		gda_column_set_defined_size (column, sres->cols_size [i]);
		gda_column_set_primary_key (column, pkey);
		gda_column_set_unique_key (column, pkey);
		gda_column_set_allow_null (column, !notnull);
		gda_column_set_auto_increment (column, autoinc);
	}

	/* set to read only mode */
	g_object_set (G_OBJECT (model), "read_only", TRUE, NULL);
}

/*
 * the @sres struct is modified and transfered to the new data model created in
 * this function
 */
GdaDataModel *
gda_sqlite_recordset_new_with_types (GdaConnection *cnc, SQLITEresult *sres, gint nbcols, ...)
{
	GdaSqliteRecordset *model;
	SQLITEcnc *scnc;
	gint i;
	va_list ap;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (sres != NULL, NULL);

	scnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_SQLITE_HANDLE);
	
	/* completing sres */
	sres->ncols = sqlite3_column_count (sres->stmt);
	g_return_val_if_fail (sres->ncols < nbcols, NULL);
	sres->nrows = 0;

	/* model creation */
	model = g_object_new (GDA_TYPE_SQLITE_RECORDSET, NULL);
	model->priv->sres = sres;
	model->priv->cnc = cnc;
	model->priv->ncolumns = sres->ncols;
	gda_data_model_hash_set_n_columns (GDA_DATA_MODEL_HASH (model),
					   model->priv->ncolumns);

	/* computing column types and titles */
	sres->types = g_new0 (GType, sres->ncols);
	sres->cols_size = g_new0 (int, sres->ncols);
	if (!scnc->types)
		gda_sqlite_update_types_hash (scnc);

	/* Gda type */
	va_start (ap, nbcols);
	for (i = 0; i < nbcols; i++) 
		sres->types [i] = va_arg (ap, GType);
	
	gda_sqlite_recordset_fill (model, cnc, sres);

	return GDA_DATA_MODEL (model);
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
	sres->types = g_new0 (GType, sres->ncols); /* all types are initialized to GDA_TYPE_NULL */
	sres->cols_size = g_new0 (int, sres->ncols);
	if (!scnc->types)
		gda_sqlite_update_types_hash (scnc);

	gda_sqlite_recordset_fill (model, cnc, sres);

	return GDA_DATA_MODEL (model);
}

static GType
fuzzy_get_gtype (SQLITEcnc *scnc, SQLITEresult *sres, gint colnum)
{
	const gchar *ctype;
	GType gtype = GDA_TYPE_NULL;

	if (sres->types [colnum] != GDA_TYPE_NULL)
		return sres->types [colnum];
	
	ctype = sqlite3_column_decltype (sres->stmt, colnum);
	if (ctype)
		gtype = GPOINTER_TO_INT (g_hash_table_lookup (scnc->types, ctype));
	else {
		int stype;
		stype = sqlite3_column_type (sres->stmt, colnum);

		switch (stype) {
		case SQLITE_INTEGER:
			gtype = G_TYPE_INT;
			break;
		case SQLITE_FLOAT:
			gtype = G_TYPE_DOUBLE;
			break;
		case 0:
		case SQLITE_TEXT:
			gtype = G_TYPE_STRING;
			break;
		case SQLITE_BLOB:
			gtype = GDA_TYPE_BINARY;
			break;
		case SQLITE_NULL:
			gtype = GDA_TYPE_NULL;
			break;
		default:
			g_error ("Unknown SQLite internal data type %d", stype);
			break;
		}
	}
	return gtype;
}
