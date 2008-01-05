/* GDA SQLite provider
 * Copyright (C) 1998 - 2008 The GNOME Foundation.
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
#include <libgda/gda-connection-private.h>

#define _GDA_PSTMT(x) ((GdaPStmt*)(x))

static void gda_sqlite_recordset_class_init (GdaSqliteRecordsetClass *klass);
static void gda_sqlite_recordset_init       (GdaSqliteRecordset *recset,
					     GdaSqliteRecordsetClass *klass);
static void gda_sqlite_recordset_dispose   (GObject *object);

/* virtual methods */
static gint    gda_sqlite_recordset_fetch_nb_rows (GdaPModel *model);
static GdaPRow *gda_sqlite_recordset_fetch_random (GdaPModel *model, gint rownum, GError **error);

static GdaPRow *gda_sqlite_recordset_fetch_next (GdaPModel *model, gint rownum, GError **error);


static GdaPRow *fetch_next_sqlite_row (GdaSqliteRecordset *model, gboolean do_store, GError **error);

struct _GdaSqliteRecordsetPrivate {
	GdaConnection *cnc;
	gint           next_row_num;
	GdaPRow       *tmp_row; /* used in cursor mode */
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
	recset->priv->cnc = NULL;
	recset->priv->next_row_num = 0;
}

static void
gda_sqlite_recordset_class_init (GdaSqliteRecordsetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaPModelClass *pmodel_class = GDA_PMODEL_CLASS (klass);

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
		GDA_SQLITE_PSTMT (GDA_PMODEL (object)->prep_stmt)->stmt_used = FALSE;
		if (recset->priv->cnc) 
			g_object_unref (recset->priv->cnc);
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
		type = g_type_register_static (GDA_TYPE_PMODEL, "GdaSqliteRecordset", &info, 0);
	}

	return type;
}

static void
read_rows_to_init_col_types (GdaSqliteRecordset *model)
{
	gint i;
	gint *missing_cols, nb_missing;
	GdaPModel *pmodel = (GdaPModel*) model;

	missing_cols = g_new (gint, pmodel->prep_stmt->ncols);
	for (nb_missing = 0, i = 0; i < pmodel->prep_stmt->ncols; i++) {
		if (pmodel->prep_stmt->types[i] == GDA_TYPE_NULL)
			missing_cols [nb_missing++] = i;
	}
	for (; nb_missing > 0; ) {
		GdaPRow *prow;
		/*g_print ("Reading row %d\n", model->priv->next_row_num);*/
		prow = fetch_next_sqlite_row (model, TRUE, NULL);
		if (!prow)
			break;
		for (i = nb_missing - 1; i >= 0; i--) {
			if (pmodel->prep_stmt->types [missing_cols [i]] != GDA_TYPE_NULL) {
				/*g_print ("Found type '%s' for col %d\n", 
					 g_type_name (pmodel->prep_stmt->types [missing_cols [i]]),
					 missing_cols [i]);*/
				memmove (missing_cols + i, missing_cols + i + 1, sizeof (gint) * (nb_missing - i - 1));
				nb_missing --;
			}
		}
	}
	g_free (missing_cols);
}

/*
 * the @ps struct is modified and transfered to the new data model created in
 * this function
 */
GdaDataModel *
gda_sqlite_recordset_new_with_types (GdaConnection *cnc, GdaSqlitePStmt *ps, GdaDataModelAccessFlags flags, gint nbcols, ...)
{
	GdaSqliteRecordset *model;
        SQLITEcnc *scnc;
        gint i;
	GdaDataModelAccessFlags rflags;

        g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
        g_return_val_if_fail (ps != NULL, NULL);

	scnc = (SQLITEcnc*) gda_connection_internal_get_provider_data (cnc);
	if (!scnc)
		return NULL;

        if (!scnc->types)
                _gda_sqlite_update_types_hash (scnc);

        /* make sure @ps reports the correct number of columns */
	if (_GDA_PSTMT (ps)->ncols < 0) 
		_GDA_PSTMT (ps)->ncols = sqlite3_column_count (ps->sqlite_stmt);

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
		va_list ap;
		va_start (ap, nbcols);
		for (i = 0; (i < nbcols) && (i < _GDA_PSTMT (ps)->ncols); i++)
			_GDA_PSTMT (ps)->types [i] = va_arg (ap, GType);
		va_end (ap);
		
		/* fill GdaColumn's data */
		for (i=0, list = _GDA_PSTMT (ps)->tmpl_columns; 
		     i < GDA_PSTMT (ps)->ncols; 
		     i++, list = list->next) {
			GdaColumn *column;
			const char *tablename, *colname, *ctype;
			int notnull, autoinc, pkey;
			
			column = GDA_COLUMN (list->data);
			gda_column_set_title (column, sqlite3_column_name (ps->sqlite_stmt, i));
			gda_column_set_name (column, sqlite3_column_name (ps->sqlite_stmt, i));

			tablename = sqlite3_column_table_name (ps->sqlite_stmt, i);
			colname = sqlite3_column_origin_name (ps->sqlite_stmt, i);
			if (!tablename || !colname ||
			    (sqlite3_table_column_metadata (scnc->connection, NULL, tablename, colname, NULL, NULL,
							    &notnull, &pkey, &autoinc) != SQLITE_OK)) {
				notnull = FALSE;
				autoinc = FALSE;
				pkey = FALSE;
			}
			gda_column_set_scale (column, -1);
			gda_column_set_defined_size (column, -1);
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
						gchar *copy = g_strdup (start + 1);
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
        }

	/* determine access mode: RANDOM or CURSOR FORWARD are the only supported */
	if (flags & GDA_DATA_MODEL_ACCESS_RANDOM)
		rflags = GDA_DATA_MODEL_ACCESS_RANDOM;
	else
		rflags = GDA_DATA_MODEL_ACCESS_CURSOR_FORWARD;

	/* create data model */
        model = g_object_new (GDA_TYPE_SQLITE_RECORDSET, "prepared-stmt", ps, "model-usage", rflags, NULL);
        model->priv->cnc = cnc;
	g_object_ref (cnc);

        /* fill the data model */
        read_rows_to_init_col_types (model);

        return GDA_DATA_MODEL (model);
}


/*
 * the @ps struct is modified and transfered to the new data model created in
 * this function
 */
GdaDataModel *
gda_sqlite_recordset_new (GdaConnection *cnc, GdaSqlitePStmt *ps, GdaDataModelAccessFlags flags)
{
	return gda_sqlite_recordset_new_with_types (cnc, ps, flags, 0);
}

static GType
fuzzy_get_gtype (SQLITEcnc *scnc, GdaSqlitePStmt *ps, gint colnum)
{
	const gchar *ctype;
	GType gtype = GDA_TYPE_NULL;

	if (_GDA_PSTMT (ps)->types [colnum] != GDA_TYPE_NULL)
		return _GDA_PSTMT (ps)->types [colnum];
	
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

static GdaPRow *
fetch_next_sqlite_row (GdaSqliteRecordset *model, gboolean do_store, GError **error)
{
	int rc;
	SQLITEcnc *scnc;
	GdaSqlitePStmt *ps;
	GdaPRow *prow = NULL;

	scnc = (SQLITEcnc*) gda_connection_internal_get_provider_data (model->priv->cnc);
	if (!scnc)
		return NULL;
	ps = GDA_SQLITE_PSTMT (GDA_PMODEL (model)->prep_stmt);

	rc = sqlite3_step (ps->sqlite_stmt);
	switch (rc) {
	case  SQLITE_ROW: {
		gint col;
		
		prow = gda_prow_new (_GDA_PSTMT (ps)->ncols);
		for (col = 0; col < _GDA_PSTMT (ps)->ncols; col++) {
			GValue *value;
			GType type = _GDA_PSTMT (ps)->types [col];
			
			if (type == GDA_TYPE_NULL) {
				type = fuzzy_get_gtype (scnc, ps, col);
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
			value = gda_prow_get_value (prow, col);
			if (type != GDA_TYPE_NULL)
				g_value_init (value, type);
			
			if (type == GDA_TYPE_NULL)
				;
			else if (type == G_TYPE_INT)
				g_value_set_int (value, sqlite3_column_int (ps->sqlite_stmt, col));
			else if (type == G_TYPE_DOUBLE)
				g_value_set_double (value, sqlite3_column_double (ps->sqlite_stmt, col));
			else if (type == G_TYPE_STRING)
				g_value_set_string (value, sqlite3_column_text (ps->sqlite_stmt, col));
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
				gda_value_take_binary (value, bin);
			}
			else 
				g_error ("Unhandled GDA type %s in SQLite recordset", 
					 gda_g_type_to_string (_GDA_PSTMT (ps)->types [col]));
		}
		
		if (do_store) {
			/* insert row */
			gda_pmodel_take_row (GDA_PMODEL (model), prow, model->priv->next_row_num);
		}
		model->priv->next_row_num ++;
		break;
	}
	case SQLITE_BUSY:
		/* nothing to do */
		break;
	case SQLITE_ERROR:
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
			     GDA_SERVER_PROVIDER_INTERNAL_ERROR, sqlite3_errmsg (scnc->connection));
		break;
	case SQLITE_DONE:
		GDA_PMODEL (model)->advertized_nrows = model->priv->next_row_num;
		break;
	case SQLITE_MISUSE:
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
			     GDA_SERVER_PROVIDER_INTERNAL_ERROR, 
			     _("SQLite provider fatal internal error"));
		break;
	}
	return prow;
}


/*
 * GdaPModel virtual methods
 */
static gint
gda_sqlite_recordset_fetch_nb_rows (GdaPModel *model)
{
	GdaSqliteRecordset *imodel;
	GdaPRow *prow = NULL;

	imodel = GDA_SQLITE_RECORDSET (model);
	if (model->advertized_nrows >= 0)
		return model->advertized_nrows;

	for (prow = fetch_next_sqlite_row (imodel, TRUE, NULL); 
	     prow; 
	     prow = fetch_next_sqlite_row (imodel, TRUE, NULL));
	return model->advertized_nrows;
}

static GdaPRow *
gda_sqlite_recordset_fetch_random (GdaPModel *model, gint rownum, GError **error)
{
	GdaSqliteRecordset *imodel;
	GdaPRow *prow = NULL;

	imodel = GDA_SQLITE_RECORDSET (model);
	for (; imodel->priv->next_row_num <= rownum; ) {
		prow = fetch_next_sqlite_row (imodel, TRUE, error);
		if (!prow) {
			/*if (GDA_PMODEL (model)->advertized_nrows >= 0), it's not an error */
			if ((GDA_PMODEL (model)->advertized_nrows >= 0) && 
			    (imodel->priv->next_row_num < rownum)) {
				g_set_error (error, 0,
					     GDA_DATA_MODEL_ROW_OUT_OF_RANGE_ERROR,
					     _("Row %d not found"), rownum);
			}
			break;
		}
	}

	return prow;
}

static GdaPRow *
gda_sqlite_recordset_fetch_next (GdaPModel *model, gint rownum, GError **error)
{
	GdaPRow *prow;
	GdaSqliteRecordset *imodel = (GdaSqliteRecordset*) model;

	prow = gda_pmodel_get_stored_row (model, rownum);
	if (prow)
		return prow;
	if (imodel->priv->tmp_row) 
		g_object_unref (imodel->priv->tmp_row);

	prow = fetch_next_sqlite_row (imodel, FALSE, error);
	imodel->priv->tmp_row = prow;

	return prow;
}
