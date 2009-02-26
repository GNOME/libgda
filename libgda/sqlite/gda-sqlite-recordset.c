/* GDA SQLite provider
 * Copyright (C) 1998 - 2009 The GNOME Foundation.
 *
 * AUTHORS:
 *	   Rodrigo Moya <rodrigo@gnome-db.org>
 *         Carlos Perell� Mar�n <carlos@gnome-db.org>
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
#include "gda-sqlite-blob-op.h"
#include <libgda/gda-util.h>
#include <libgda/gda-connection-private.h>

#define _GDA_PSTMT(x) ((GdaPStmt*)(x))

static void gda_sqlite_recordset_class_init (GdaSqliteRecordsetClass *klass);
static void gda_sqlite_recordset_init       (GdaSqliteRecordset *recset,
					     GdaSqliteRecordsetClass *klass);
static void gda_sqlite_recordset_dispose   (GObject *object);

/* virtual methods */
static gint    gda_sqlite_recordset_fetch_nb_rows (GdaDataSelect *model);
static gboolean gda_sqlite_recordset_fetch_random (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error);

static gboolean gda_sqlite_recordset_fetch_next (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error);


static GdaRow *fetch_next_sqlite_row (GdaSqliteRecordset *model, gboolean do_store, GError **error);

struct _GdaSqliteRecordsetPrivate {
	gboolean      empty_forced;
	gint          next_row_num;
	GdaRow       *tmp_row; /* used in cursor mode */
};
static GObjectClass *parent_class = NULL;

/*
 * Object init and finalize
 */
static void
gda_sqlite_recordset_init (GdaSqliteRecordset *recset,
			   GdaSqliteRecordsetClass *klass)
{
	g_return_if_fail (GDA_IS_SQLITE_RECORDSET (recset));
	recset->priv = g_new0 (GdaSqliteRecordsetPrivate, 1);
	recset->priv->next_row_num = 0;
	recset->priv->empty_forced = FALSE;
}

static void
gda_sqlite_recordset_class_init (GdaSqliteRecordsetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaDataSelectClass *pmodel_class = GDA_DATA_SELECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = gda_sqlite_recordset_dispose;
	pmodel_class->fetch_nb_rows = gda_sqlite_recordset_fetch_nb_rows;
	pmodel_class->fetch_random = gda_sqlite_recordset_fetch_random;

	pmodel_class->fetch_next = gda_sqlite_recordset_fetch_next;
	pmodel_class->fetch_prev = NULL;
	pmodel_class->fetch_at = NULL;
}

static void
gda_sqlite_recordset_dispose (GObject *object)
{
	GdaSqliteRecordset *recset = (GdaSqliteRecordset *) object;

	g_return_if_fail (GDA_IS_SQLITE_RECORDSET (recset));

	if (recset->priv) {
		GDA_SQLITE_PSTMT (GDA_DATA_SELECT (object)->prep_stmt)->stmt_used = FALSE;
		if (recset->priv->tmp_row)
			g_object_unref (recset->priv->tmp_row);
		g_free (recset->priv);
		recset->priv = NULL;
	}

	parent_class->dispose (object);
}

/*
 * Public functions
 */

GType
gda_sqlite_recordset_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
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
		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GDA_TYPE_DATA_SELECT, "GdaSqliteRecordset", &info, 0);
		g_static_mutex_unlock (&registering);
	}

	return type;
}

static void
read_rows_to_init_col_types (GdaSqliteRecordset *model)
{
	gint i;
	gint *missing_cols, nb_missing;
	GdaDataSelect *pmodel = (GdaDataSelect*) model;

	missing_cols = g_new (gint, pmodel->prep_stmt->ncols);
	for (nb_missing = 0, i = 0; i < pmodel->prep_stmt->ncols; i++) {
		if (pmodel->prep_stmt->types[i] == GDA_TYPE_NULL)
			missing_cols [nb_missing++] = i;
	}
	/*
	if (nb_missing == 0)
		g_print ("Hey!, all columns are known for prep stmt %p\n", pmodel->prep_stmt);
	*/
	for (; nb_missing > 0; ) {
		GdaRow *prow;
		prow = fetch_next_sqlite_row (model, TRUE, NULL);
		if (!prow)
			break;
#ifdef GDA_DEBUG_NO
		g_print ("Read row %d for prep stmt %p\n", model->priv->next_row_num - 1, pmodel->prep_stmt);
#endif
		for (i = nb_missing - 1; i >= 0; i--) {
			if (pmodel->prep_stmt->types [missing_cols [i]] != GDA_TYPE_NULL) {
#ifdef GDA_DEBUG_NO
				g_print ("Found type '%s' for col %d\n", 
					 g_type_name (pmodel->prep_stmt->types [missing_cols [i]]),
					 missing_cols [i]);
#endif
				memmove (missing_cols + i, missing_cols + i + 1, sizeof (gint) * (nb_missing - i - 1));
				nb_missing --;
			}
		}
	}

#ifdef GDA_DEBUG_NO
	if (nb_missing > 0)
		g_print ("Hey!, some columns are still not known for prep stmt %p\n", pmodel->prep_stmt);
#endif

	g_free (missing_cols);
}

/*
 * the @ps struct is modified and transfered to the new data model created in
 * this function
 */
GdaDataModel *
gda_sqlite_recordset_new (GdaConnection *cnc, GdaSqlitePStmt *ps, GdaSet *exec_params,
			  GdaDataModelAccessFlags flags, GType *col_types, gboolean force_empty)
{
	GdaSqliteRecordset *model;
        SqliteConnectionData *cdata;
        gint i;
	GdaDataModelAccessFlags rflags;

        g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
        g_return_val_if_fail (ps != NULL, NULL);

	cdata = (SqliteConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata)
		return NULL;

        if (!cdata->types)
                _gda_sqlite_compute_types_hash (cdata);

        /* make sure @ps reports the correct number of columns */
	if (_GDA_PSTMT (ps)->ncols < 0)
		_GDA_PSTMT (ps)->ncols = sqlite3_column_count (ps->sqlite_stmt) - ps->nb_rowid_columns;

        /* completing ps */
	g_assert (! ps->stmt_used);
        ps->stmt_used = TRUE;
        if (!_GDA_PSTMT (ps)->types && (_GDA_PSTMT (ps)->ncols > 0)) {
		/* create prepared statement's columns */
		GSList *list;
		for (i = 0; i < _GDA_PSTMT (ps)->ncols; i++)
			_GDA_PSTMT (ps)->tmpl_columns = g_slist_prepend (_GDA_PSTMT (ps)->tmpl_columns, 
									 gda_column_new ());
		_GDA_PSTMT (ps)->tmpl_columns = g_slist_reverse (_GDA_PSTMT (ps)->tmpl_columns);

		/* create prepared statement's types */
		_GDA_PSTMT (ps)->types = g_new0 (GType, _GDA_PSTMT (ps)->ncols); /* all types are initialized to GDA_TYPE_NULL */
		if (col_types) {
			for (i = 0; ; i++) {
				if (col_types [i] > 0) {
					if (col_types [i] == G_TYPE_NONE)
						break;
					if (i >= _GDA_PSTMT (ps)->ncols)
						g_warning (_("Column %d out of range (0-%d), ignoring its specified type"), i,
							   _GDA_PSTMT (ps)->ncols - 1);
					else 
						_GDA_PSTMT (ps)->types [i] = col_types [i];
				}
			}
		}
		
		/* fill GdaColumn's data */
		for (i=0, list = _GDA_PSTMT (ps)->tmpl_columns; 
		     i < GDA_PSTMT (ps)->ncols; 
		     i++, list = list->next) {
			GdaColumn *column;
			gint real_col = i + ps->nb_rowid_columns;
			
			column = GDA_COLUMN (list->data);
			gda_column_set_description (column, sqlite3_column_name (ps->sqlite_stmt, real_col));
			gda_column_set_name (column, sqlite3_column_name (ps->sqlite_stmt, real_col));
			gda_column_set_dbms_type (column, sqlite3_column_decltype (ps->sqlite_stmt, real_col));
			if (_GDA_PSTMT (ps)->types [i] != GDA_TYPE_NULL)
				gda_column_set_g_type (column, _GDA_PSTMT (ps)->types [i]);
		}
        }

	/* determine access mode: RANDOM or CURSOR FORWARD are the only supported; if CURSOR BACKWARD
	 * is requested, then we need RANDOM mode */
	if (flags & GDA_DATA_MODEL_ACCESS_RANDOM)
		rflags = GDA_DATA_MODEL_ACCESS_RANDOM;
	else if (flags & GDA_DATA_MODEL_ACCESS_CURSOR_BACKWARD)
		rflags = GDA_DATA_MODEL_ACCESS_RANDOM;
	else
		rflags = GDA_DATA_MODEL_ACCESS_CURSOR_FORWARD;

	/* create data model */
        model = g_object_new (GDA_TYPE_SQLITE_RECORDSET, "connection", cnc, 
			      "prepared-stmt", ps, "model-usage", rflags, 
			      "exec-params", exec_params, 
			      "auto-reset", force_empty, NULL);

        /* fill the data model */
        read_rows_to_init_col_types (model);

        return GDA_DATA_MODEL (model);
}

static GType
fuzzy_get_gtype (SqliteConnectionData *cdata, GdaSqlitePStmt *ps, gint colnum)
{
	const gchar *ctype;
	GType gtype = GDA_TYPE_NULL;
	gint real_col = colnum + ps->nb_rowid_columns;

	if (_GDA_PSTMT (ps)->types [colnum] != GDA_TYPE_NULL)
		return _GDA_PSTMT (ps)->types [colnum];
	
	ctype = sqlite3_column_origin_name (ps->sqlite_stmt, real_col);
	if (ctype && !strcmp (ctype, "rowid"))
		gtype = G_TYPE_INT64;
	else {
		ctype = sqlite3_column_decltype (ps->sqlite_stmt, real_col);
		
		if (ctype)
			gtype = GPOINTER_TO_INT (g_hash_table_lookup (cdata->types, ctype));
		if (gtype == GDA_TYPE_NULL) 
			gtype = _gda_sqlite_compute_g_type (sqlite3_column_type (ps->sqlite_stmt, real_col));
	}

	return gtype;
}

static GdaRow *
fetch_next_sqlite_row (GdaSqliteRecordset *model, gboolean do_store, GError **error)
{
	int rc;
	SqliteConnectionData *cdata;
	GdaSqlitePStmt *ps;
	GdaRow *prow = NULL;

	cdata = (SqliteConnectionData*) gda_connection_internal_get_provider_data 
		(gda_data_select_get_connection ((GdaDataSelect*) model));
	if (!cdata)
		return NULL;
	ps = GDA_SQLITE_PSTMT (GDA_DATA_SELECT (model)->prep_stmt);

	if (model->priv->empty_forced)
		rc = SQLITE_DONE;
	else
		rc = sqlite3_step (ps->sqlite_stmt);
	switch (rc) {
	case  SQLITE_ROW: {
		gint col, real_col;
		
		prow = gda_row_new (_GDA_PSTMT (ps)->ncols);
		for (col = 0; col < _GDA_PSTMT (ps)->ncols; col++) {
			GValue *value;
			GType type = _GDA_PSTMT (ps)->types [col];
			
			real_col = col + ps->nb_rowid_columns;

			if (type == GDA_TYPE_NULL) {
				type = fuzzy_get_gtype (cdata, ps, col);
				if (type != GDA_TYPE_NULL) {
					GdaColumn *column;
					
					_GDA_PSTMT (ps)->types [col] = type;
					column = gda_data_model_describe_column (GDA_DATA_MODEL (model), col);
					gda_column_set_g_type (column, type);
					column = (GdaColumn *) g_slist_nth_data (_GDA_PSTMT (ps)->tmpl_columns, col);
					gda_column_set_g_type (column, type);
				}
			}
			
			/* fill GValue */
			value = gda_row_get_value (prow, col);
			if (sqlite3_column_text (ps->sqlite_stmt, real_col) == NULL) {
				/* we have a NULL value */
				gda_value_set_null (value);
			}
			else {
				if (type != GDA_TYPE_NULL)
					g_value_init (value, type);
				
				if (type == GDA_TYPE_NULL)
					;
				else if (type == G_TYPE_INT) {
					gint64 i;
					i = sqlite3_column_int64 (ps->sqlite_stmt, real_col);
					if ((i > G_MAXINT) || (i < G_MININT)) {
						g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
							     GDA_SERVER_PROVIDER_DATA_ERROR,
							     "%s", _("Integer value is too big"));
						gda_row_invalidate_value (prow, value);
					}
					else
						g_value_set_int (value, (gint) i);
				}
				else if (type == G_TYPE_UINT) {
					guint64 i;
					i = (gint64) sqlite3_column_int64 (ps->sqlite_stmt, real_col);
					if (i > G_MAXUINT) {
						g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
							     GDA_SERVER_PROVIDER_DATA_ERROR,
							     "%s", _("Integer value is too big"));
						gda_row_invalidate_value (prow, value);
					}
					else
						g_value_set_uint (value, (gint) i);
				}
				else if (type == G_TYPE_INT64)
					g_value_set_int64 (value, sqlite3_column_int64 (ps->sqlite_stmt, real_col));
				else if (type == G_TYPE_UINT64)
					g_value_set_uint64 (value, (guint64) sqlite3_column_int64 (ps->sqlite_stmt,
												   real_col));
				else if (type == G_TYPE_DOUBLE)
					g_value_set_double (value, sqlite3_column_double (ps->sqlite_stmt,
											  real_col));
				else if (type == G_TYPE_STRING)
					g_value_set_string (value, (gchar *) sqlite3_column_text (ps->sqlite_stmt,
												  real_col));
				else if (type == GDA_TYPE_BINARY) {
					GdaBinary *bin;
					
					bin = g_new0 (GdaBinary, 1);
					bin->binary_length = sqlite3_column_bytes (ps->sqlite_stmt, real_col);
					if (bin->binary_length > 0) {
						bin->data = g_new (guchar, bin->binary_length);
						memcpy (bin->data, sqlite3_column_blob (ps->sqlite_stmt,
											real_col),
							bin->binary_length);
					}
					else
						bin->binary_length = 0;
					gda_value_take_binary (value, bin);
				}
				else if (type == GDA_TYPE_BLOB) {
					GdaBlobOp *bop = NULL;
					gint oidcol = 0;

					if (ps->rowid_hash) {
						const char *ctable;
						ctable = sqlite3_column_name (ps->sqlite_stmt, real_col);
						if (ctable)
							oidcol = GPOINTER_TO_INT (g_hash_table_lookup (ps->rowid_hash,
												       ctable));
						if (oidcol == 0) {
							ctable = sqlite3_column_table_name (ps->sqlite_stmt, real_col);
							if (ctable)
								oidcol = GPOINTER_TO_INT (g_hash_table_lookup (ps->rowid_hash, 
													       ctable));
						}
					}
					if (oidcol != 0) {
						gint64 rowid;
						rowid = sqlite3_column_int64 (ps->sqlite_stmt, oidcol - 1); /* remove 1
													       because it was added in the first place */
						bop = gda_sqlite_blob_op_new (cdata,
									      sqlite3_column_database_name (ps->sqlite_stmt, 
													    real_col),
									      sqlite3_column_table_name (ps->sqlite_stmt,
													 real_col),
									      sqlite3_column_origin_name (ps->sqlite_stmt,
													  real_col),
									      rowid);
					}
					if (!bop) {
						g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
							     GDA_SERVER_PROVIDER_DATA_ERROR,
							     "%s", _("Unable to open BLOB"));
						gda_row_invalidate_value (prow, value);
					}
					else {
						GdaBlob *blob;
						blob = g_new0 (GdaBlob, 1);
						blob->op = bop;
						gda_value_take_blob (value, blob);
					}
				}
				else if (type == G_TYPE_BOOLEAN)
					g_value_set_boolean (value, sqlite3_column_int (ps->sqlite_stmt, real_col) == 0 ? FALSE : TRUE);
				else if (type == G_TYPE_DATE) {
					GDate date;
					if (!gda_parse_iso8601_date (&date, 
								     (gchar *) sqlite3_column_text (ps->sqlite_stmt, 
												    real_col))) {
						g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
							     GDA_SERVER_PROVIDER_DATA_ERROR,
							     _("Invalid date '%s' (date format should be YYYY-MM-DD)"), 
							     (gchar *) sqlite3_column_text (ps->sqlite_stmt, real_col));
						gda_row_invalidate_value (prow, value);
					}
					else
						g_value_set_boxed (value, &date);
				}
				else if (type == GDA_TYPE_TIME) {
					GdaTime timegda;
					if (!gda_parse_iso8601_time (&timegda, 
								     (gchar *) sqlite3_column_text (ps->sqlite_stmt, 
												    real_col))) {
						g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
							     GDA_SERVER_PROVIDER_DATA_ERROR,
							     _("Invalid time '%s' (time format should be HH:MM:SS[.ms])"), 
							     (gchar *) sqlite3_column_text (ps->sqlite_stmt, real_col));
						gda_row_invalidate_value (prow, value);
					}
					else
						gda_value_set_time (value, &timegda);
				}
				else if (type == GDA_TYPE_TIMESTAMP) {
					GdaTimestamp timestamp;
					if (!gda_parse_iso8601_timestamp (&timestamp, 
									  (gchar *) sqlite3_column_text (ps->sqlite_stmt,
													 real_col))) {
						g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
							     GDA_SERVER_PROVIDER_DATA_ERROR,
							     _("Invalid timestamp '%s' (format should be YYYY-MM-DD HH:MM:SS[.ms])"), 
							     (gchar *) sqlite3_column_text (ps->sqlite_stmt, real_col));
						gda_row_invalidate_value (prow, value);
					}
					else
						gda_value_set_timestamp (value, &timestamp);
				}
				else 
					g_error ("Unhandled GDA type %s in SQLite recordset", 
						 gda_g_type_to_string (_GDA_PSTMT (ps)->types [col]));
			}
		}
		
		if (do_store) {
			/* insert row */
			gda_data_select_take_row (GDA_DATA_SELECT (model), prow, model->priv->next_row_num);
		}
		model->priv->next_row_num ++;
		break;
	}
	case SQLITE_BUSY:
		/* nothing to do */
		break;
	case SQLITE_ERROR:
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
			     GDA_SERVER_PROVIDER_INTERNAL_ERROR,  "%s", sqlite3_errmsg (cdata->connection));
		break;
	case SQLITE_DONE:
		GDA_DATA_SELECT (model)->advertized_nrows = model->priv->next_row_num;
		break;
	case SQLITE_MISUSE:
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
			     GDA_SERVER_PROVIDER_INTERNAL_ERROR, 
			      "%s", _("SQLite provider fatal internal error"));
		break;
	}
	return prow;
}


/*
 * GdaDataSelect virtual methods
 */
static gint
gda_sqlite_recordset_fetch_nb_rows (GdaDataSelect *model)
{
	GdaSqliteRecordset *imodel;
	GdaRow *prow = NULL;

	imodel = GDA_SQLITE_RECORDSET (model);
	if (model->advertized_nrows >= 0)
		return model->advertized_nrows;

	for (prow = fetch_next_sqlite_row (imodel, TRUE, NULL); 
	     prow; 
	     prow = fetch_next_sqlite_row (imodel, TRUE, NULL));
	return model->advertized_nrows;
}

/*
 * Create a new filled #GdaRow object for the row at position @rownum.
 *
 * Each new #GdaRow created is "given" to the #GdaDataSelect implementation using gda_data_select_take_row ().
 */
static gboolean
gda_sqlite_recordset_fetch_random (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error)
{
	GdaSqliteRecordset *imodel;

	if (*prow)
		return TRUE;

	imodel = GDA_SQLITE_RECORDSET (model);
	for (; imodel->priv->next_row_num <= rownum; ) {
		*prow = fetch_next_sqlite_row (imodel, TRUE, error);
		if (!*prow) {
			/*if (GDA_DATA_SELECT (model)->advertized_nrows >= 0), it's not an error */
			if ((GDA_DATA_SELECT (model)->advertized_nrows >= 0) && 
			    (imodel->priv->next_row_num < rownum)) {
				g_set_error (error, 0,
					     GDA_DATA_MODEL_ROW_OUT_OF_RANGE_ERROR,
					     _("Row %d not found"), rownum);
			}
			return FALSE;
		}
	}

	return TRUE;
}

/*
 * Create a new filled #GdaRow object for the next cursor row
 *
 * Each new #GdaRow created is referenced only by imodel->priv->tmp_row (the #GdaDataSelect implementation
 * never keeps a reference to it). Before a new #GdaRow gets created, the previous one, if set, is discarded.
 */
static gboolean
gda_sqlite_recordset_fetch_next (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error)
{
	GdaSqliteRecordset *imodel = (GdaSqliteRecordset*) model;

	if (imodel->priv->tmp_row) 
		g_object_unref (imodel->priv->tmp_row);

	*prow = fetch_next_sqlite_row (imodel, FALSE, error);
	imodel->priv->tmp_row = *prow;

	return TRUE;
}
