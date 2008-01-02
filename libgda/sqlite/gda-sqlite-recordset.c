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
	SQLitePreparedStatement  *ps;
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

	if (recset->priv->ps) {
		recset->priv->ps->stmt_used = FALSE;
		_gda_sqlite_prepared_statement_free (recset->priv->ps);
		recset->priv->ps = NULL;
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


static GType fuzzy_get_gtype (SQLITEcnc *scnc, SQLitePreparedStatement *ps, gint colnum);
static void
gda_sqlite_recordset_fill (GdaSqliteRecordset *model, GdaConnection *cnc, SQLitePreparedStatement *ps)
{
	gint i;
	int rc;
	gboolean end = FALSE;
	SQLITEcnc *scnc;

	scnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_SQLITE_HANDLE);

	/* Column titles */
	for (i=0; i < ps->ncols; i++)
		gda_data_model_set_column_title (GDA_DATA_MODEL (model), i,
						 sqlite3_column_name (ps->sqlite_stmt, i));

	/* filling the model with GValues, and computing data types */
	i = 0;
	while (!end) {
		rc = sqlite3_step (ps->sqlite_stmt);
		switch (rc) {
		case  SQLITE_ROW: {
			GList *row_values = NULL; /* list of values for a row */
			gint col;

			for (col = 0; col < ps->ncols; col++) {
				GValue *value = NULL;
				int size;
				GType type = ps->types [col];

				if (type == GDA_TYPE_NULL) {
					type = fuzzy_get_gtype (scnc, ps, col);
					if (type != GDA_TYPE_NULL) {
						GdaColumn *column;
						
						ps->types [col] = type;
						column = gda_data_model_describe_column (GDA_DATA_MODEL (model), col);
						gda_column_set_g_type (column, type);
					}
				}

				/* compute GValue */
				if ((sqlite3_column_type (ps->sqlite_stmt, col) == SQLITE_NULL) ||
				    (type == GDA_TYPE_NULL))
					value = gda_value_new_null ();
				else if (type == G_TYPE_INT)
					g_value_set_int (value = gda_value_new (G_TYPE_INT), 
							 sqlite3_column_int (ps->sqlite_stmt, col));
				else if (type == G_TYPE_DOUBLE)
					g_value_set_double (value = gda_value_new (G_TYPE_DOUBLE), 
							    sqlite3_column_double (ps->sqlite_stmt, col));
				else if (type == G_TYPE_STRING)
					g_value_set_string (value = gda_value_new (G_TYPE_STRING), 
							    sqlite3_column_text (ps->sqlite_stmt, col));
				else if (type == GDA_TYPE_BINARY) {
					GdaBinary *bin;

					bin = g_new0 (GdaBinary, 1);
					bin->binary_length = sqlite3_column_bytes (ps->sqlite_stmt, col);
					if (bin->binary_length > 0) {
						bin->data = g_new (guchar, bin->binary_length);
						memcpy (bin->data, sqlite3_column_blob (ps->sqlite_stmt, col),
							bin->binary_length);
					}
					else
						bin->binary_length = 0;
					value = gda_value_new (GDA_TYPE_BINARY);
					gda_value_take_binary (value, bin);
				}
				else
					g_error ("Unhandled GDA type %s in SQLite recordset", 
						 gda_g_type_to_string (ps->types [col]));

				size = sqlite3_column_bytes (ps->sqlite_stmt, col);
				if (ps->cols_size [col] < size)
					ps->cols_size [col] = size;

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

	ps->nrows = i;
	model->priv->nrows = ps->nrows;

	/* show types */
#ifdef GDA_DEBUG_NO
		for (i = 0; i < ps->ncols; i++) 
			g_print ("Type for col %d: (GDA:%s)\n",
				 i, gda_g_type_to_string (ps->types [i]));
		
		gda_data_model_dump (GDA_DATA_MODEL (model), stdout);
#endif

	/* filling GdaDataModel columns data */
	for (i = 0; i < ps->ncols; i++) {
		GdaColumn *column;
		const char *tablename, *colname, *ctype;
		int notnull, autoinc, pkey;

		tablename = sqlite3_column_table_name (ps->sqlite_stmt, i);
		colname = sqlite3_column_origin_name (ps->sqlite_stmt, i);
		if (!tablename || !colname || 
		    (sqlite3_table_column_metadata (scnc->connection, NULL, tablename, colname, NULL, NULL,
						    &notnull, &pkey, &autoinc) != SQLITE_OK)) {
			notnull = FALSE;
			autoinc = FALSE;
			pkey = FALSE;
		}

		column = gda_data_model_describe_column (GDA_DATA_MODEL (model), i);
		gda_column_set_name (column, sqlite3_column_name (ps->sqlite_stmt, i));
		gda_column_set_scale (column, -1);
		gda_column_set_defined_size (column, -1 /*ps->cols_size [i]*/);
		gda_column_set_primary_key (column, pkey);
		gda_column_set_unique_key (column, pkey);
		gda_column_set_allow_null (column, !notnull);
		gda_column_set_auto_increment (column, autoinc);

		ctype = sqlite3_column_decltype (ps->sqlite_stmt, i);
		if (ctype) {
			gchar *start, *end;
			for (start = (gchar *) ctype; *start && (*start != '('); start++);
			if (*start == '(') {
				for (end = start+1; *end && (*end != ')'); end++);
				if (*end == ')') {
					gchar *copy = g_strdup (start);
					copy[end - start] = 0;
					for (start = copy; *start && (*start != ','); start++);
					if (*start != 0) {
						*start = 0;
						gda_column_set_defined_size (column, atoi (copy));
						gda_column_set_scale (column, atoi (start+1));
					}
					else
						gda_column_set_defined_size (column, atoi (copy));
					
					g_free (copy);
				}
			}
		}
	}

	/* set to read only mode */
	g_object_set (G_OBJECT (model), "read_only", TRUE, NULL);
}

/*
 * the @ps struct is modified and transfered to the new data model created in
 * this function
 */
GdaDataModel *
gda_sqlite_recordset_new_with_types (GdaConnection *cnc, SQLitePreparedStatement *ps, gint nbcols, ...)
{
	GdaSqliteRecordset *model;
	SQLITEcnc *scnc;
	gint i;
	va_list ap;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (ps != NULL, NULL);

	scnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_SQLITE_HANDLE);
	
	/* completing ps */
	ps->stmt_used = TRUE;
	ps->ncols = sqlite3_column_count (ps->sqlite_stmt);
	g_return_val_if_fail (ps->ncols < nbcols, NULL);
	ps->nrows = 0;

	/* model creation */
	model = g_object_new (GDA_TYPE_SQLITE_RECORDSET, NULL);
	model->priv->ps = ps;
	ps->ref_count ++;
	model->priv->cnc = cnc;
	model->priv->ncolumns = ps->ncols;
	gda_data_model_hash_set_n_columns (GDA_DATA_MODEL_HASH (model),
					   model->priv->ncolumns);

	/* computing column types and titles */
	if (!ps->types) {
		ps->types = g_new0 (GType, ps->ncols);
		ps->cols_size = g_new0 (int, ps->ncols);
		if (!scnc->types)
			_gda_sqlite_update_types_hash (scnc);
	}

	/* Gda type */
	va_start (ap, nbcols);
	for (i = 0; i < nbcols; i++) 
		ps->types [i] = va_arg (ap, GType);
	
	gda_sqlite_recordset_fill (model, cnc, ps);

	return GDA_DATA_MODEL (model);
}


/*
 * the @ps struct is modified and transfered to the new data model created in
 * this function
 */
GdaDataModel *
gda_sqlite_recordset_new (GdaConnection *cnc, SQLitePreparedStatement *ps)
{
	GdaSqliteRecordset *model;
	SQLITEcnc *scnc;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (ps != NULL, NULL);

	scnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_SQLITE_HANDLE);
	
	/* completing ps */
	ps->stmt_used = TRUE;
	ps->ncols = sqlite3_column_count (ps->sqlite_stmt);
	ps->nrows = 0;

	/* model creation */
	model = g_object_new (GDA_TYPE_SQLITE_RECORDSET, NULL);
	model->priv->ps = ps;
	ps->ref_count ++;
	model->priv->cnc = cnc;
	model->priv->ncolumns = ps->ncols;
	gda_data_model_hash_set_n_columns (GDA_DATA_MODEL_HASH (model),
					   model->priv->ncolumns);

	/* computing column types and titles */
	if (!ps->types) {
		ps->types = g_new0 (GType, ps->ncols); /* all types are initialized to GDA_TYPE_NULL */
		ps->cols_size = g_new0 (int, ps->ncols);
		if (!scnc->types)
			_gda_sqlite_update_types_hash (scnc);
	}

	gda_sqlite_recordset_fill (model, cnc, ps);

	return GDA_DATA_MODEL (model);
}

static GType
fuzzy_get_gtype (SQLITEcnc *scnc, SQLitePreparedStatement *ps, gint colnum)
{
	const gchar *ctype;
	GType gtype = GDA_TYPE_NULL;

	if (ps->types [colnum] != GDA_TYPE_NULL)
		return ps->types [colnum];
	
	ctype = sqlite3_column_decltype (ps->sqlite_stmt, colnum);
	if (ctype)
		gtype = GPOINTER_TO_INT (g_hash_table_lookup (scnc->types, ctype));
	else {
		int stype;
		stype = sqlite3_column_type (ps->sqlite_stmt, colnum);

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
