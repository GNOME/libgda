/*
 * Copyright (C) 2001 - 2002 Carlos Perelló Marín <carlos@gnome-db.org>
 * Copyright (C) 2002 - 2003 Gonzalo Paniagua Javier <gonzalo@src.gnome.org>
 * Copyright (C) 2002 - 2005 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2005 Denis Fortin <denis.fortin@free.fr>
 * Copyright (C) 2005 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2005 Álvaro Peña <alvaropg@telefonica.net>
 * Copyright (C) 2006 - 2008 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2007 Armin Burgmeier <arminb@src.gnome.org>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <stdarg.h>
#include <string.h>
#include <glib/gi18n-lib.h>
#include "gda-sqlite.h"
#include "gda-sqlite-util.h"
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
GHashTable *error_blobs_hash = NULL;

/*
 * Object init and finalize
 */
static void
gda_sqlite_recordset_init (GdaSqliteRecordset *recset,
			   G_GNUC_UNUSED GdaSqliteRecordsetClass *klass)
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

	g_assert (!error_blobs_hash);
	error_blobs_hash = g_hash_table_new (NULL, NULL);
}

static void
gda_sqlite_recordset_dispose (GObject *object)
{
	GdaSqliteRecordset *recset = (GdaSqliteRecordset *) object;

	g_return_if_fail (GDA_IS_SQLITE_RECORDSET (recset));

	if (recset->priv) {
		GdaSqlitePStmt *ps;
		ps = GDA_SQLITE_PSTMT (GDA_DATA_SELECT (object)->prep_stmt);
		ps->stmt_used = FALSE;
		SQLITE3_CALL (sqlite3_reset) (ps->sqlite_stmt);

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
_gda_sqlite_recordset_get_type (void)
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
			(GInstanceInitFunc) gda_sqlite_recordset_init,
			0
		};
		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GDA_TYPE_DATA_SELECT, CLASS_PREFIX "Recordset", &info, 0);
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

#ifdef GDA_DEBUG_NO
	if (nb_missing == 0)
		g_print ("All columns are known for model %p\n", pmodel);
#endif

	for (; nb_missing > 0; ) {
		GdaRow *prow;
		prow = fetch_next_sqlite_row (model, TRUE, NULL);
		if (!prow)
			break;
#ifdef GDA_DEBUG_NO
		g_print ("Prefetched row %d of model %p\n", model->priv->next_row_num - 1, pmodel);
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
 * the @ps struct is modified and transferred to the new data model created in
 * this function
 */
GdaDataModel *
_gda_sqlite_recordset_new (GdaConnection *cnc, GdaSqlitePStmt *ps, GdaSet *exec_params,
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

        if (!cdata->types_hash)
                _gda_sqlite_compute_types_hash (cdata);

        /* make sure @ps reports the correct number of columns */
	if (_GDA_PSTMT (ps)->ncols < 0)
		_GDA_PSTMT (ps)->ncols = SQLITE3_CALL (sqlite3_column_count) (ps->sqlite_stmt) -
			ps->nb_rowid_columns;

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

		/* create prepared statement's types, all types are initialized to GDA_TYPE_NULL */
		_GDA_PSTMT (ps)->types = g_new (GType, _GDA_PSTMT (ps)->ncols);
		for (i = 0; i < _GDA_PSTMT (ps)->ncols; i++)
			_GDA_PSTMT (ps)->types [i] = GDA_TYPE_NULL;

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
			gda_column_set_description (column, SQLITE3_CALL (sqlite3_column_name) (ps->sqlite_stmt, real_col));
			gda_column_set_name (column, SQLITE3_CALL (sqlite3_column_name) (ps->sqlite_stmt, real_col));
			gda_column_set_dbms_type (column, SQLITE3_CALL (sqlite3_column_decltype) (ps->sqlite_stmt, real_col));
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
	
	ctype = SQLITE3_CALL (sqlite3_column_origin_name) (ps->sqlite_stmt, real_col);
	if (ctype && !strcmp (ctype, "rowid"))
		gtype = G_TYPE_INT64;
	else {
		ctype = SQLITE3_CALL (sqlite3_column_decltype) (ps->sqlite_stmt, real_col);
		
		if (ctype) {
			GType *pg;
			pg = g_hash_table_lookup (cdata->types_hash, ctype);
			gtype = pg ? *pg : GDA_TYPE_NULL;
		}
		if (gtype == GDA_TYPE_NULL) 
			gtype = _gda_sqlite_compute_g_type (SQLITE3_CALL (sqlite3_column_type) (ps->sqlite_stmt, real_col));
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
		rc = SQLITE3_CALL (sqlite3_step) (ps->sqlite_stmt);
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
				if (type == GDA_TYPE_BLOB) {
					/* extra check: make sure we have a rowid for this blob, or fallback to binary */
					if (ps->rowid_hash) {
						gint oidcol = 0;
						const char *ctable;
						ctable = SQLITE3_CALL (sqlite3_column_name) (ps->sqlite_stmt, real_col);
						if (ctable)
							oidcol = GPOINTER_TO_INT (g_hash_table_lookup (ps->rowid_hash,
												       ctable));
						if (oidcol == 0) {
							ctable = SQLITE3_CALL (sqlite3_column_table_name) (ps->sqlite_stmt, real_col);
							if (ctable)
								oidcol = GPOINTER_TO_INT (g_hash_table_lookup (ps->rowid_hash, 
													       ctable));
						}
						if (oidcol == 0)
							type = GDA_TYPE_BINARY;
					}
					else
						type = GDA_TYPE_BINARY;
				}
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
			GError *may_error;
			may_error = (GError*) SQLITE3_CALL (sqlite3_column_blob) (ps->sqlite_stmt, real_col);
			if (may_error && g_hash_table_lookup (error_blobs_hash, may_error)) {
				/*g_print ("Row invalidated: [%s]\n", may_error->message);*/
				gda_row_invalidate_value_e (prow, value, may_error);
				g_hash_table_remove (error_blobs_hash, may_error);
			}
			else if (SQLITE3_CALL (sqlite3_column_text) (ps->sqlite_stmt, real_col) == NULL) {
				/* we have a NULL value */
				gda_value_set_null (value);
			}
			else {
				gda_value_reset_with_type (value, type);
				
				if (type == GDA_TYPE_NULL)
					;
				else if (type == G_TYPE_INT) {
					gint64 i;
					i = SQLITE3_CALL (sqlite3_column_int64) (ps->sqlite_stmt, real_col);
					if ((i > G_MAXINT) || (i < G_MININT)) {
						GError *lerror = NULL;
						g_set_error (&lerror, GDA_SERVER_PROVIDER_ERROR,
							     GDA_SERVER_PROVIDER_DATA_ERROR,
							     "%s", _("Integer value is too big"));
						gda_row_invalidate_value_e (prow, value, lerror);
					}
					else
						g_value_set_int (value, (gint) i);
				}
				else if (type == G_TYPE_UINT) {
					guint64 i;
					i = (gint64) SQLITE3_CALL (sqlite3_column_int64) (ps->sqlite_stmt, real_col);
					if (i > G_MAXUINT) {
						GError *lerror = NULL;
						g_set_error (&lerror, GDA_SERVER_PROVIDER_ERROR,
							     GDA_SERVER_PROVIDER_DATA_ERROR,
							     "%s", _("Integer value is too big"));
						gda_row_invalidate_value_e (prow, value, lerror);
					}
					else
						g_value_set_uint (value, (gint) i);
				}
				else if (type == G_TYPE_INT64)
					g_value_set_int64 (value, SQLITE3_CALL (sqlite3_column_int64) (ps->sqlite_stmt, real_col));
				else if (type == G_TYPE_UINT64)
					g_value_set_uint64 (value, (guint64) SQLITE3_CALL (sqlite3_column_int64) (ps->sqlite_stmt,
												   real_col));
				else if (type == G_TYPE_DOUBLE)
					g_value_set_double (value, SQLITE3_CALL (sqlite3_column_double) (ps->sqlite_stmt,
											  real_col));
				else if (type == G_TYPE_STRING)
					g_value_set_string (value, (gchar *) SQLITE3_CALL (sqlite3_column_text) (ps->sqlite_stmt,
												  real_col));
				else if (type == GDA_TYPE_BINARY) {
					GdaBinary *bin;
					
					bin = g_new0 (GdaBinary, 1);
					bin->binary_length = SQLITE3_CALL (sqlite3_column_bytes) (ps->sqlite_stmt, real_col);
					if (bin->binary_length > 0) {
						bin->data = g_new (guchar, bin->binary_length);
						memcpy (bin->data, SQLITE3_CALL (sqlite3_column_blob) (ps->sqlite_stmt, /* Flawfinder: ignore */
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
						ctable = SQLITE3_CALL (sqlite3_column_name) (ps->sqlite_stmt, real_col);
						if (ctable)
							oidcol = GPOINTER_TO_INT (g_hash_table_lookup (ps->rowid_hash,
												       ctable));
						if (oidcol == 0) {
							ctable = SQLITE3_CALL (sqlite3_column_table_name) (ps->sqlite_stmt, real_col);
							if (ctable)
								oidcol = GPOINTER_TO_INT (g_hash_table_lookup (ps->rowid_hash, 
													       ctable));
						}
					}
					if (oidcol != 0) {
						gint64 rowid;
						rowid = SQLITE3_CALL (sqlite3_column_int64) (ps->sqlite_stmt, oidcol - 1); /* remove 1
													       because it was added in the first place */
						bop = _gda_sqlite_blob_op_new (cdata,
									       SQLITE3_CALL (sqlite3_column_database_name) (ps->sqlite_stmt, 
													    real_col),
									       SQLITE3_CALL (sqlite3_column_table_name) (ps->sqlite_stmt,
													 real_col),
									       SQLITE3_CALL (sqlite3_column_origin_name) (ps->sqlite_stmt,
													  real_col),
									      rowid);
					}
					if (!bop) {
						GError *lerror = NULL;
						g_set_error (&lerror, GDA_SERVER_PROVIDER_ERROR,
							     GDA_SERVER_PROVIDER_DATA_ERROR,
							     "%s", _("Unable to open BLOB"));
						gda_row_invalidate_value_e (prow, value, lerror);
					}
					else {
						GdaBlob *blob;
						blob = g_new0 (GdaBlob, 1);
						gda_blob_set_op (blob, bop);
						g_object_unref (bop);
						gda_value_take_blob (value, blob);
					}
				}
				else if (type == G_TYPE_BOOLEAN)
					g_value_set_boolean (value, SQLITE3_CALL (sqlite3_column_int) (ps->sqlite_stmt, real_col) == 0 ? FALSE : TRUE);
				else if (type == G_TYPE_DATE) {
					GDate date;
					if (!gda_parse_iso8601_date (&date, 
								     (gchar *) SQLITE3_CALL (sqlite3_column_text) (ps->sqlite_stmt, 
												    real_col))) {
						GError *lerror = NULL;
						g_set_error (&lerror, GDA_SERVER_PROVIDER_ERROR,
							     GDA_SERVER_PROVIDER_DATA_ERROR,
							     _("Invalid date '%s' (date format should be YYYY-MM-DD)"), 
							     (gchar *) SQLITE3_CALL (sqlite3_column_text) (ps->sqlite_stmt, real_col));
						gda_row_invalidate_value_e (prow, value, lerror);
					}
					else
						g_value_set_boxed (value, &date);
				}
				else if (type == GDA_TYPE_TIME) {
					GdaTime timegda;
					if (!gda_parse_iso8601_time (&timegda, 
								     (gchar *) SQLITE3_CALL (sqlite3_column_text) (ps->sqlite_stmt, 
												    real_col))) {
						GError *lerror = NULL;
						g_set_error (&lerror, GDA_SERVER_PROVIDER_ERROR,
							     GDA_SERVER_PROVIDER_DATA_ERROR,
							     _("Invalid time '%s' (time format should be HH:MM:SS[.ms])"), 
							     (gchar *) SQLITE3_CALL (sqlite3_column_text) (ps->sqlite_stmt, real_col));
						gda_row_invalidate_value_e (prow, value, lerror);
					}
					else
						gda_value_set_time (value, &timegda);
				}
				else if (type == GDA_TYPE_TIMESTAMP) {
					GdaTimestamp timestamp;
					if (!gda_parse_iso8601_timestamp (&timestamp, 
									  (gchar *) SQLITE3_CALL (sqlite3_column_text) (ps->sqlite_stmt,
													 real_col))) {
						GError *lerror = NULL;
						g_set_error (&lerror, GDA_SERVER_PROVIDER_ERROR,
							     GDA_SERVER_PROVIDER_DATA_ERROR,
							     _("Invalid timestamp '%s' (format should be YYYY-MM-DD HH:MM:SS[.ms])"), 
							     (gchar *) SQLITE3_CALL (sqlite3_column_text) (ps->sqlite_stmt, real_col));
						gda_row_invalidate_value_e (prow, value, lerror);
					}
					else
						gda_value_set_timestamp (value, &timestamp);
				}
				else if (type == G_TYPE_CHAR) {
					gint64 i;
					i = (gint64) SQLITE3_CALL (sqlite3_column_int64) (ps->sqlite_stmt, real_col);
					if ((i > G_MAXINT8) || (i < G_MININT8)) {
						GError *lerror = NULL;
						g_set_error (&lerror, GDA_SERVER_PROVIDER_ERROR,
							     GDA_SERVER_PROVIDER_DATA_ERROR,
							     "%s", _("Integer value is too big"));
						gda_row_invalidate_value_e (prow, value, lerror);
					}
					else
						g_value_set_char (value, (gchar) i);
				}
				else if (type == G_TYPE_UCHAR) {
					gint64 i;
					i = (gint64) SQLITE3_CALL (sqlite3_column_int64) (ps->sqlite_stmt, real_col);
					if ((i > G_MAXUINT8) || (i < 0)) {
						GError *lerror = NULL;
						g_set_error (&lerror, GDA_SERVER_PROVIDER_ERROR,
							     GDA_SERVER_PROVIDER_DATA_ERROR,
							     "%s", _("Integer value is too big"));
						gda_row_invalidate_value_e (prow, value, lerror);
					}
					else
						g_value_set_uchar (value, (guchar) i);
				}
				else if (type == GDA_TYPE_SHORT) {
					gint64 i;
					i = (gint64) SQLITE3_CALL (sqlite3_column_int64) (ps->sqlite_stmt, real_col);
					if ((i > G_MAXSHORT) || (i < G_MINSHORT)) {
						GError *lerror = NULL;
						g_set_error (&lerror, GDA_SERVER_PROVIDER_ERROR,
							     GDA_SERVER_PROVIDER_DATA_ERROR,
							     "%s", _("Integer value is too big"));
						gda_row_invalidate_value_e (prow, value, lerror);
					}
					else
						gda_value_set_short (value, (guchar) i);
				}
				else if (type == GDA_TYPE_USHORT) {
					gint64 i;
					i = (gint64) SQLITE3_CALL (sqlite3_column_int64) (ps->sqlite_stmt, real_col);
					if ((i > G_MAXUSHORT) || (i < 0)) {
						GError *lerror = NULL;
						g_set_error (&lerror, GDA_SERVER_PROVIDER_ERROR,
							     GDA_SERVER_PROVIDER_DATA_ERROR,
							     "%s", _("Integer value is too big"));
						gda_row_invalidate_value_e (prow, value, lerror);
					}
					else
						gda_value_set_ushort (value, (guchar) i);
				}
				else {
					GError *lerror = NULL;
					g_set_error (&lerror, GDA_SERVER_PROVIDER_ERROR,
						     GDA_SERVER_PROVIDER_DATA_ERROR,
						     "Unhandled type '%s' in SQLite recordset",
						     gda_g_type_to_string (_GDA_PSTMT (ps)->types [col]));
					gda_row_invalidate_value_e (prow, value, lerror);
				}
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
	case SQLITE_DONE:
		GDA_DATA_SELECT (model)->advertized_nrows = model->priv->next_row_num;
		SQLITE3_CALL (sqlite3_reset) (ps->sqlite_stmt);
		break;
	case SQLITE_READONLY:
	case SQLITE_MISUSE:
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
			     GDA_SERVER_PROVIDER_INTERNAL_ERROR, 
			      "%s", _("SQLite provider fatal internal error"));
		break;
	case SQLITE_ERROR:
	default: {
		GError *lerror = NULL;
		SQLITE3_CALL (sqlite3_reset) (ps->sqlite_stmt);
		if (rc == SQLITE_IOERR_TRUNCATE)
			g_set_error (&lerror, GDA_DATA_MODEL_ERROR,
				     GDA_DATA_MODEL_TRUNCATED_ERROR, _("Tuncated data"));
		else
			g_set_error (&lerror, GDA_SERVER_PROVIDER_ERROR,
				     GDA_SERVER_PROVIDER_INTERNAL_ERROR, 
				     "%s", SQLITE3_CALL (sqlite3_errmsg) (cdata->connection));
		gda_data_select_add_exception (GDA_DATA_SELECT (model), lerror);
		if (rc == SQLITE_ERROR)
			g_propagate_error (error, g_error_copy (lerror));
		GDA_DATA_SELECT (model)->advertized_nrows = model->priv->next_row_num;
		break;
	}
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
 * Each new #GdaRow created needs to be "given" to the #GdaDataSelect implementation using
 * gda_data_select_take_row() because backward iterating is not supported.
 */
static gboolean
gda_sqlite_recordset_fetch_random (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error)
{
	GdaSqliteRecordset *imodel;

	imodel = GDA_SQLITE_RECORDSET (model);
	if (imodel->priv->next_row_num >= rownum) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
			     GDA_SERVER_PROVIDER_INTERNAL_ERROR, 
			     "%s", _("Requested row could not be found"));
		return TRUE;
	}
	for (*prow = fetch_next_sqlite_row (imodel, TRUE, error);
	     *prow && (imodel->priv->next_row_num < rownum);
	     *prow = fetch_next_sqlite_row (imodel, TRUE, error));

	return TRUE;
}

/*
 * Create a new filled #GdaRow object for the next cursor row
 *
 * Each new #GdaRow created is referenced only by imodel->priv->tmp_row (the #GdaDataSelect implementation
 * never keeps a reference to it).
 * Before a new #GdaRow gets created, the previous one, if set, is discarded.
 */
static gboolean
gda_sqlite_recordset_fetch_next (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error)
{
	GdaSqliteRecordset *imodel = (GdaSqliteRecordset*) model;

	if (imodel->priv->tmp_row) {
		g_object_unref (imodel->priv->tmp_row);
		imodel->priv->tmp_row = NULL;
	}
	if (imodel->priv->next_row_num != rownum) {
		GError *lerror = NULL;
		*prow = NULL;
		g_set_error (&lerror, GDA_DATA_MODEL_ERROR,
			     GDA_DATA_MODEL_ROW_NOT_FOUND_ERROR,
			     "%s", _("Can't set iterator on requested row"));
		gda_data_select_add_exception (GDA_DATA_SELECT (model), lerror);
		if (error)
			g_propagate_error (error, g_error_copy (lerror));
		return TRUE;
	}
	*prow = fetch_next_sqlite_row (imodel, FALSE, error);
	imodel->priv->tmp_row = *prow;

	return TRUE;
}
