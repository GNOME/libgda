/* GDA Postgres provider
 * Copyright (C) 2008 The GNOME Foundation.
 *
 * AUTHORS:
 *      Vivien Malerba <malerba@gnome-db.org>
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
#include <libgda/gda-util.h>
#include <libgda/gda-connection-private.h>
#include "gda-postgres.h"
#include "gda-postgres-recordset.h"
#include "gda-postgres-provider.h"
#include "gda-postgres-blob-op.h"
#include "gda-postgres-util.h"
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#define _GDA_PSTMT(x) ((GdaPStmt*)(x))

static void gda_postgres_recordset_class_init (GdaPostgresRecordsetClass *klass);
static void gda_postgres_recordset_init       (GdaPostgresRecordset *recset,
					       GdaPostgresRecordsetClass *klass);
static void gda_postgres_recordset_dispose   (GObject *object);

static void gda_postgres_recordset_set_property (GObject *object,
						 guint param_id,
						 const GValue *value,
						 GParamSpec *pspec);
static void gda_postgres_recordset_get_property (GObject *object,
						 guint param_id,
						 GValue *value,
						 GParamSpec *pspec);

/* virtual methods */
static gint     gda_postgres_recordset_fetch_nb_rows (GdaPModel *model);
static gboolean gda_postgres_recordset_fetch_random (GdaPModel *model, GdaPRow **prow, gint rownum, GError **error);
static gboolean gda_postgres_recordset_store_all (GdaPModel *model, GError **error);
static gboolean gda_postgres_recordset_fetch_next (GdaPModel *model, GdaPRow **prow, gint rownum, GError **error);
static gboolean gda_postgres_recordset_fetch_prev (GdaPModel *model, GdaPRow **prow, gint rownum, GError **error);
static gboolean gda_postgres_recordset_fetch_at (GdaPModel *model, GdaPRow **prow, gint rownum, GError **error);

/* static helper functions */
static void make_point (GdaGeometricPoint *point, const gchar *value);
static void make_time (GdaTime *timegda, const gchar *value);
static void make_timestamp (GdaTimestamp *timestamp, const gchar *value);
static void set_value (GdaConnection *cnc, GValue *value, GType type, const gchar *thevalue, gboolean isNull, gint length);

static GdaPRow *new_row_from_pg_res (GdaPostgresRecordset *imodel, gint pg_res_rownum);
static gboolean row_is_in_current_pg_res (GdaPostgresRecordset *model, gint row);
static gboolean fetch_next_chunk (GdaPostgresRecordset *model, gboolean *fetch_error, GError **error);
static gboolean fetch_prev_chunk (GdaPostgresRecordset *model, gboolean *fetch_error, GError **error);
static gboolean fetch_row_number_chunk (GdaPostgresRecordset *model, int row_index, gboolean *fetch_error, GError **error);


struct _GdaPostgresRecordsetPrivate {
	/* random access attributes */
	PGresult         *pg_res;

	/* cursor access attributes */
	GdaPRow          *tmp_row; /* used to store a reference to the last #GdaPRow returned */
	gchar            *cursor_name;
	PGconn           *pconn;
	gint              chunk_size; /* Number of rows to fetch at a time when iterating forward or backwards. */
        gint              chunks_read; /* Effectively equal to the number of times that we have iterated forwards or backwards. */
        
        /* Pg cursor's information */
        gint              pg_pos; /* from G_MININT to G_MAXINT */
        gint              pg_res_size; /* The number of rows in the current chunk - usually equal to chunk_size when iterating forward or backward. */
        gint              pg_res_inf; /* The row number of the first row in the current chunk. Don't use if (@pg_res_size <= 0). */
};
static GObjectClass *parent_class = NULL;

/* properties */
enum
{
        PROP_0,
        PROP_CHUNCK_SIZE,
        PROP_CHUNCKS_READ
};

/*
 * Object init and finalize
 */
static void
gda_postgres_recordset_init (GdaPostgresRecordset *recset, GdaPostgresRecordsetClass *klass)
{
	g_return_if_fail (GDA_IS_POSTGRES_RECORDSET (recset));
	recset->priv = g_new0 (GdaPostgresRecordsetPrivate, 1);

	recset->priv->pg_res = NULL;
        recset->priv->pg_pos = G_MININT;
        recset->priv->pg_res_size = 0;

	recset->priv->chunk_size = 10;
        recset->priv->chunks_read = 0;
}

static void
gda_postgres_recordset_class_init (GdaPostgresRecordsetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaPModelClass *pmodel_class = GDA_PMODEL_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = gda_postgres_recordset_dispose;
	pmodel_class->fetch_nb_rows = gda_postgres_recordset_fetch_nb_rows;
	pmodel_class->fetch_random = gda_postgres_recordset_fetch_random;
	pmodel_class->store_all = gda_postgres_recordset_store_all;

	pmodel_class->fetch_next = gda_postgres_recordset_fetch_next;
	pmodel_class->fetch_prev = gda_postgres_recordset_fetch_prev;
	pmodel_class->fetch_at = gda_postgres_recordset_fetch_at;

	/* properties */
        object_class->set_property = gda_postgres_recordset_set_property;
        object_class->get_property = gda_postgres_recordset_get_property;
        g_object_class_install_property (object_class, PROP_CHUNCK_SIZE,
                                         g_param_spec_int ("chunk_size", _("Number of rows fetched at a time"), NULL,
                                                           1, G_MAXINT - 1, 10,
                                                           G_PARAM_CONSTRUCT | G_PARAM_READABLE | G_PARAM_WRITABLE));
        g_object_class_install_property (object_class, PROP_CHUNCKS_READ,
                                         g_param_spec_int ("chunks_read",
                                                           _("Number of rows chunks read since the object creation"), NULL,
                                                           0, G_MAXINT - 1, 0,
                                                           G_PARAM_READABLE));
}

static void
gda_postgres_recordset_dispose (GObject *object)
{
	GdaPostgresRecordset *recset = (GdaPostgresRecordset *) object;

	g_return_if_fail (GDA_IS_POSTGRES_RECORDSET (recset));

	if (recset->priv) {
		if (recset->priv->tmp_row)
			g_object_unref (recset->priv->tmp_row);

		if (recset->priv->pg_res)
			PQclear (recset->priv->pg_res);

		if (recset->priv->cursor_name) {
			gchar *str;
			PGresult *pg_res;
			str = g_strdup_printf ("CLOSE %s", recset->priv->cursor_name);
			pg_res = PQexec (recset->priv->pconn, str);
			g_free (str);
			PQclear (pg_res);
			g_free (recset->priv->cursor_name);
		}

		g_free (recset->priv);
		recset->priv = NULL;
	}

	parent_class->dispose (object);
}

static void
gda_postgres_recordset_set_property (GObject *object,
				     guint param_id,
				     const GValue *value,
				     GParamSpec *pspec)
{
        GdaPostgresRecordset *model = (GdaPostgresRecordset *) object;
        if (model->priv) {
                switch (param_id) {
                case PROP_CHUNCK_SIZE:
                        model->priv->chunk_size = g_value_get_int (value);
                        break;
                default:
                        break;
                }
        }
}

static void
gda_postgres_recordset_get_property (GObject *object,
				     guint param_id,
				     GValue *value,
				     GParamSpec *pspec)
{
        GdaPostgresRecordset *model = (GdaPostgresRecordset *) object;
        if (model->priv) {
                switch (param_id) {
                case PROP_CHUNCK_SIZE:
                        g_value_set_int (value, model->priv->chunk_size);
                        break;
                case PROP_CHUNCKS_READ:
                        g_value_set_int (value, model->priv->chunks_read);
                        break;
                default:
                        break;
                }
        }
}

/*
 * Public functions
 */

GType
gda_postgres_recordset_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdaPostgresRecordsetClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_postgres_recordset_class_init,
			NULL,
			NULL,
			sizeof (GdaPostgresRecordset),
			0,
			(GInstanceInitFunc) gda_postgres_recordset_init
		};
		type = g_type_register_static (GDA_TYPE_PMODEL, "GdaPostgresRecordset", &info, 0);
	}

	return type;
}

static void
finish_prep_stmt_init (PostgresConnectionData *cdata, GdaPostgresPStmt *ps, PGresult *pg_res, GType *col_types)
{
	/* make sure @ps reports the correct number of columns */
        if (_GDA_PSTMT (ps)->ncols < 0) {
		if (pg_res)
			_GDA_PSTMT (ps)->ncols = PQnfields (pg_res);
		else
			_GDA_PSTMT (ps)->ncols = 0;
	}

        /* completing @ps if not yet done */
        if (!_GDA_PSTMT (ps)->types && (_GDA_PSTMT (ps)->ncols > 0)) {
		/* create prepared statement's columns */
		GSList *list;
		gint i;
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
						g_warning (_("Column %d is out of range (0-%d), ignoring its specified type"), i,
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
			Oid postgres_type;
			GType gtype;
			column = GDA_COLUMN (list->data);
			postgres_type = PQftype (pg_res, i);
			gtype = _GDA_PSTMT (ps)->types [i];
			if (gtype == 0) {
				gtype = _gda_postgres_type_oid_to_gda (cdata, postgres_type);
				_GDA_PSTMT (ps)->types [i] = gtype;
			}
			_GDA_PSTMT (ps)->types [i] = gtype;
			gda_column_set_g_type (column, gtype);
			gda_column_set_name (column, PQfname (pg_res, i));
			gda_column_set_title (column, PQfname (pg_res, i));
			gda_column_set_scale (column, (gtype == G_TYPE_DOUBLE) ? DBL_DIG :
					      (gtype == G_TYPE_FLOAT) ? FLT_DIG : 0);
			gda_column_set_defined_size (column, PQfsize (pg_res, i));
			gda_column_set_references (column, "");

			/*
			  FIXME: Use @cnc's associated GdaMetaStore to get the following information:
			  gda_column_set_references (column, "");
			  gda_column_set_table (column, ...);
			  gda_column_set_primary_key (column, ...);
			  gda_column_set_unique_key (column, ...);
			  gda_column_set_allow_null (column, ...);
			  gda_column_set_auto_increment (column, ...);
			*/
		}
        }
}

GdaDataModel *
gda_postgres_recordset_new_random (GdaConnection *cnc, GdaPostgresPStmt *ps, PGresult *pg_res, GType *col_types)
{
	GdaPostgresRecordset *model;
        PostgresConnectionData *cdata;

        g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
        g_return_val_if_fail (ps, NULL);

	cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata)
		return NULL;

	/* finish prepared statement's init */
	finish_prep_stmt_init (cdata, ps, pg_res, col_types);

	/* create data model */
        model = g_object_new (GDA_TYPE_POSTGRES_RECORDSET, "connection", cnc, 
			      "prepared-stmt", ps, 
			      "model-usage", GDA_DATA_MODEL_ACCESS_RANDOM, NULL);
	model->priv->pg_res = pg_res;
	((GdaPModel*) model)->advertized_nrows = PQntuples (model->priv->pg_res);

        return GDA_DATA_MODEL (model);
}

/*
 * Takes ownership of @cursor_name
 */
GdaDataModel *
gda_postgres_recordset_new_cursor (GdaConnection *cnc, GdaPostgresPStmt *ps, gchar *cursor_name, GType *col_types)
{
	GdaPostgresRecordset *model;
        PostgresConnectionData *cdata;

        g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
        g_return_val_if_fail (ps, NULL);

	cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata)
		return NULL;

	/* Fetch the 1st row to finish initialization of @ps */
	gchar *str;
	int status;
	PGresult *pg_res;
	
	str = g_strdup_printf ("FETCH FORWARD 1 FROM %s;", cursor_name);
	pg_res = PQexec (cdata->pconn, str);
	g_free (str);
	status = PQresultStatus (pg_res);
	if (!pg_res || (PQresultStatus (pg_res) != PGRES_TUPLES_OK)) {
		_gda_postgres_make_error (cdata->cnc, cdata->pconn, pg_res, NULL);
		if (pg_res) {
			PQclear (pg_res);
			pg_res = NULL;
		}
	}
	else {
		PGresult *tmp_res;
		str = g_strdup_printf ("MOVE BACKWARD 1 FROM %s;", cursor_name);
		tmp_res = PQexec (cdata->pconn, str);
		g_free (str);
		if (tmp_res)
			PQclear (tmp_res);
	}

	/* finish prepared statement's init */
	finish_prep_stmt_init (cdata, ps, pg_res, col_types);
	if (pg_res)
		PQclear (pg_res);

	/* create model */
	model = g_object_new (GDA_TYPE_POSTGRES_RECORDSET, "connection", cnc, 
			      "prepared-stmt", ps, "model-usage", 
			      GDA_DATA_MODEL_ACCESS_CURSOR_FORWARD | GDA_DATA_MODEL_ACCESS_CURSOR_BACKWARD, NULL);
	model->priv->pconn = cdata->pconn;
	model->priv->cursor_name = cursor_name;
	gboolean fetch_error;
	fetch_next_chunk (model, &fetch_error, NULL);

	return GDA_DATA_MODEL (model);
}

/*
 * Get the number of rows in @model, if possible
 */
static gint
gda_postgres_recordset_fetch_nb_rows (GdaPModel *model)
{
	GdaPostgresRecordset *imodel;

	imodel = GDA_POSTGRES_RECORDSET (model);
	if (model->advertized_nrows >= 0)
		return model->advertized_nrows;

	/* use C API to determine number of rows,if possible */
	if (!imodel->priv->cursor_name)
		model->advertized_nrows = PQntuples (imodel->priv->pg_res);

	return model->advertized_nrows;
}

/*
 * Create a new filled #GdaPRow object for the row at position @rownum.
 *
 * Each new #GdaPRow created is "given" to the #GdaPModel implementation using gda_pmodel_take_row ().
 */
static gboolean
gda_postgres_recordset_fetch_random (GdaPModel *model, GdaPRow **prow, gint rownum, GError **error)
{
	GdaPostgresRecordset *imodel = (GdaPostgresRecordset *) model;

	if (*prow)
		return TRUE;

	if (!imodel->priv->pg_res) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_INTERNAL_ERROR,
			     _("Internal error"));
		return FALSE;
	}

	*prow = new_row_from_pg_res (imodel, rownum);
	gda_pmodel_take_row (model, *prow, rownum);

	if (model->nb_stored_rows == model->advertized_nrows) {
		/* all the rows have been converted from PGresult to GdaPRow objects => we can
		 * discard the PGresult */
		PQclear (imodel->priv->pg_res);
		imodel->priv->pg_res = NULL;
	}

	return TRUE;
}

/*
 * Create and "give" filled #GdaPRow object for all the rows in the model
 */
static gboolean
gda_postgres_recordset_store_all (GdaPModel *model, GError **error)
{
	GdaPostgresRecordset *imodel = (GdaPostgresRecordset*) model;
	gint i;
	
	if (!imodel->priv->pg_res) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_INTERNAL_ERROR,
			     _("Internal error"));
		return FALSE;
	}

	for (i = 0; i < model->advertized_nrows; i++) {
		GdaPRow *prow;
		if (! gda_postgres_recordset_fetch_random (model, &prow, i, error))
			return FALSE;
	}
	return TRUE;
}

/*
 * Create a new filled #GdaPRow object for the next cursor row
 *
 * Each new #GdaPRow created is referenced only by imodel->priv->tmp_row (the #GdaPModel implementation
 * never keeps a reference to it). Before a new #GdaPRow gets created, the previous one, if set, is discarded.
 */
static gboolean
gda_postgres_recordset_fetch_next (GdaPModel *model, GdaPRow **prow, gint rownum, GError **error)
{
	GdaPostgresRecordset *imodel = (GdaPostgresRecordset*) model;

	if (*prow)
		return TRUE;

	if (imodel->priv->tmp_row) {
		g_object_unref (imodel->priv->tmp_row);
		imodel->priv->tmp_row = NULL;
	}

	if (row_is_in_current_pg_res (imodel, rownum)) {
		*prow = new_row_from_pg_res (imodel, rownum - imodel->priv->pg_res_inf);
		imodel->priv->tmp_row = *prow;
		return TRUE;
	}
	else {
		gboolean fetch_error = FALSE;
		if (fetch_next_chunk (imodel, &fetch_error, error)) {
			*prow = new_row_from_pg_res (imodel, rownum - imodel->priv->pg_res_inf);
			imodel->priv->tmp_row = *prow;
			return TRUE;
		}
		else
			return !fetch_error;
	}
}

/*
 * Create a new filled #GdaPRow object for the previous cursor row
 *
 * Each new #GdaPRow created is referenced only by imodel->priv->tmp_row (the #GdaPModel implementation
 * never keeps a reference to it). Before a new #GdaPRow gets created, the previous one, if set, is discarded.
 */
static gboolean
gda_postgres_recordset_fetch_prev (GdaPModel *model, GdaPRow **prow, gint rownum, GError **error)
{
	GdaPostgresRecordset *imodel = (GdaPostgresRecordset*) model;

	if (*prow)
		return TRUE;

	if (imodel->priv->tmp_row) {
		g_object_unref (imodel->priv->tmp_row);
		imodel->priv->tmp_row = NULL;
	}

	if (row_is_in_current_pg_res (imodel, rownum)) {
		*prow = new_row_from_pg_res (imodel, rownum - imodel->priv->pg_res_inf);
		imodel->priv->tmp_row = *prow;
		return TRUE;
	}
	else {
		gboolean fetch_error = FALSE;
		if (fetch_prev_chunk (imodel, &fetch_error, error)) {
			*prow = new_row_from_pg_res (imodel, rownum - imodel->priv->pg_res_inf);
			imodel->priv->tmp_row = *prow;
			return TRUE;
		}
		else
			return !fetch_error;
	}
}

/*
 * Create a new filled #GdaPRow object for the cursor row at position @rownum
 *
 * Each new #GdaPRow created is referenced only by imodel->priv->tmp_row (the #GdaPModel implementation
 * never keeps a reference to it). Before a new #GdaPRow gets created, the previous one, if set, is discarded.
 */
static gboolean
gda_postgres_recordset_fetch_at (GdaPModel *model, GdaPRow **prow, gint rownum, GError **error)
{
	GdaPostgresRecordset *imodel = (GdaPostgresRecordset*) model;

	if (*prow)
		return TRUE;

	if (imodel->priv->tmp_row) {
		g_object_unref (imodel->priv->tmp_row);
		imodel->priv->tmp_row = NULL;
	}

	if (row_is_in_current_pg_res (imodel, rownum)) {
		*prow = new_row_from_pg_res (imodel, rownum - imodel->priv->pg_res_inf);
		imodel->priv->tmp_row = *prow;
		return TRUE;
	}
	else {
		gboolean fetch_error = FALSE;
		if (fetch_row_number_chunk (imodel, rownum, &fetch_error, error)) {
			*prow = new_row_from_pg_res (imodel, rownum - imodel->priv->pg_res_inf);
			imodel->priv->tmp_row = *prow;
			return TRUE;
		}
		else
			return !fetch_error;
	}
}






/*
 * Static helper functions
 */

/* Makes a point from a string like "(3.2,5.6)" */
static void
make_point (GdaGeometricPoint *point, const gchar *value)
{
	value++;
	point->x = atof (value);
	value = strchr (value, ',');
	value++;
	point->y = atof (value);
}

/* Makes a GdaTime from a string like "12:30:15+01" */
static void
make_time (GdaTime *timegda, const gchar *value)
{
	timegda->hour = atoi (value);
	value += 3;
	timegda->minute = atoi (value);
	value += 3;
	timegda->second = atoi (value);
	value += 2;
	if (*value)
		timegda->timezone = atoi (value);
	else
		timegda->timezone = GDA_TIMEZONE_INVALID;
}

/* Makes a GdaTimestamp from a string like "2003-12-13 13:12:01.12+01" */
static void
make_timestamp (GdaTimestamp *timestamp, const gchar *value)
{
	timestamp->year = atoi (value);
	value += 5;
	timestamp->month = atoi (value);
	value += 3;
	timestamp->day = atoi (value);
	value += 3;
	timestamp->hour = atoi (value);
	value += 3;
	timestamp->minute = atoi (value);
	value += 3;
	timestamp->second = atoi (value);
	value += 2;
	if (*value != '.') {
		timestamp->fraction = 0;
	} else {
		gint ndigits = 0;
		gint64 fraction;

		value++;
		fraction = atol (value);
		while (*value && *value != '+') {
			value++;
			ndigits++;
		}

		while (ndigits < 3) {
			fraction *= 10;
			ndigits++;
		}

		while (fraction > 0 && ndigits > 3) {
			fraction /= 10;
			ndigits--;
		}
		
		timestamp->fraction = fraction;
	}

	if (*value != '+') {
		timestamp->timezone = 0;
	} else {
		value++;
		timestamp->timezone = atol (value) * 60 * 60;
	}
}

static void
set_value (GdaConnection *cnc, GValue *value, GType type,
	   const gchar *thevalue, gboolean isNull, gint length)
{
	if (isNull) {
		gda_value_set_null (value);
		return;
	}

	gda_value_reset_with_type (value, type);

	if (type == G_TYPE_BOOLEAN)
		g_value_set_boolean (value, (*thevalue == 't') ? TRUE : FALSE);
	else if (type == G_TYPE_STRING)
		g_value_set_string (value, thevalue);
	else if (type == G_TYPE_INT64)
		g_value_set_int64 (value, atoll (thevalue));
	else if (type == G_TYPE_ULONG)
		g_value_set_ulong (value, atoll (thevalue));
	else if (type == G_TYPE_LONG)
		g_value_set_ulong (value, atoll (thevalue));
	else if (type == G_TYPE_INT)
		g_value_set_int (value, atol (thevalue));
	else if (type == GDA_TYPE_SHORT)
		gda_value_set_short (value, atoi (thevalue));
	else if (type == G_TYPE_FLOAT) {
		setlocale (LC_NUMERIC, "C");
		g_value_set_float (value, atof (thevalue));
		setlocale (LC_NUMERIC, "");
	}
	else if (type == G_TYPE_DOUBLE) {
		setlocale (LC_NUMERIC, "C");
		g_value_set_double (value, atof (thevalue));
		setlocale (LC_NUMERIC, "");
	}
	else if (type == GDA_TYPE_NUMERIC) {
		GdaNumeric numeric;
		numeric.number = g_strdup (thevalue);
		numeric.precision = 0; /* FIXME */
		numeric.width = 0; /* FIXME */
		gda_value_set_numeric (value, &numeric);
		g_free (numeric.number);
	}
	else if (type == G_TYPE_DATE) {
		GDate *gdate;
		gdate = g_date_new ();
		g_date_set_parse (gdate, thevalue);
		if (!g_date_valid (gdate)) {
			g_warning (_("Could not parse date '%s', assuming 01/01/0001"), thevalue);
			g_date_clear (gdate, 1);
			g_date_set_dmy (gdate, 1, 1, 1);
		}
		g_value_take_boxed (value, gdate);
	}
	else if (type == GDA_TYPE_GEOMETRIC_POINT) {
		GdaGeometricPoint point;
		make_point (&point, thevalue);
		gda_value_set_geometric_point (value, &point);
	}
	else if (type == GDA_TYPE_TIMESTAMP) {
		GdaTimestamp timestamp;
		make_timestamp (&timestamp, thevalue);
		gda_value_set_timestamp (value, &timestamp);
	}
	else if (type == GDA_TYPE_TIME) {
		GdaTime timegda;
		make_time (&timegda, thevalue);
		gda_value_set_time (value, &timegda);
	}
	else if (type == GDA_TYPE_BINARY) {
		/*
		 * Requires PQunescapeBytea in libpq (present since 7.3.x)
		 */
		guchar *unescaped;
                size_t pqlength = 0;

		unescaped = PQunescapeBytea ((guchar*)thevalue, &pqlength);
		if (unescaped != NULL) {
			GdaBinary bin;
			bin.data = unescaped;
			bin.binary_length = pqlength;
			gda_value_set_binary (value, &bin);
			PQfreemem (unescaped);
		}
	}
	else if (type == GDA_TYPE_BLOB) {
		GdaBlob *blob;
		GdaBlobOp *op;
		blob = g_new0 (GdaBlob, 1);
		op = gda_postgres_blob_op_new_with_id (cnc, thevalue);
		gda_blob_set_op (blob, op);
		g_object_unref (op);

		gda_value_take_blob (value, blob);
	}
	else if (type == G_TYPE_STRING) 
		g_value_set_string (value, thevalue);
	else {
		g_warning ("Type %s not translated for value '%s' => set as string", g_type_name (type), thevalue);
		gda_value_reset_with_type (value, G_TYPE_STRING);
		g_value_set_string (value, thevalue);
	}
}

static gboolean
row_is_in_current_pg_res (GdaPostgresRecordset *model, gint row)
{
	if ((model->priv->pg_res) && (model->priv->pg_res_size > 0) &&
            (row >= model->priv->pg_res_inf) && (row < model->priv->pg_res_inf + model->priv->pg_res_size))
                return TRUE;
        else
                return FALSE;
}

static GdaPRow *
new_row_from_pg_res (GdaPostgresRecordset *imodel, gint pg_res_rownum)
{
	GdaPRow *prow;
	gchar *thevalue;
	gboolean isNull;
	gint col;

	prow = gda_prow_new (((GdaPModel*) imodel)->prep_stmt->ncols);
	for (col = 0; col < ((GdaPModel*) imodel)->prep_stmt->ncols; col++) {
		thevalue = PQgetvalue (imodel->priv->pg_res, pg_res_rownum, col);
		isNull = thevalue && *thevalue != '\0' ? FALSE : PQgetisnull (imodel->priv->pg_res, pg_res_rownum, col);
		set_value (gda_pmodel_get_connection ((GdaPModel*) imodel),
			   gda_prow_get_value (prow, col), 
			   ((GdaPModel*) imodel)->prep_stmt->types [col], 
			   thevalue, isNull, 
			   PQgetlength (imodel->priv->pg_res, pg_res_rownum, col));
	}
	return prow;
}

static gboolean
fetch_next_chunk (GdaPostgresRecordset *model, gboolean *fetch_error, GError **error)
{
	if (model->priv->pg_res) {
		PQclear (model->priv->pg_res);
		model->priv->pg_res = NULL;
	}
	*fetch_error = FALSE;

	if (model->priv->pg_pos == G_MAXINT) 
		return FALSE;

	gchar *str;
	gboolean retval = TRUE;
	int status;

	str = g_strdup_printf ("FETCH FORWARD %d FROM %s;",
			       model->priv->chunk_size, model->priv->cursor_name);
#ifdef GDA_PG_DEBUG
	g_print ("QUERY: %s\n", str);
#endif
        model->priv->pg_res = PQexec (model->priv->pconn, str);
        g_free (str);
        status = PQresultStatus (model->priv->pg_res);
	model->priv->chunks_read ++;
        if (status != PGRES_TUPLES_OK) {
		_gda_postgres_make_error (gda_pmodel_get_connection ((GdaPModel*) model), 
					  model->priv->pconn, model->priv->pg_res, error);
                PQclear (model->priv->pg_res);
                model->priv->pg_res = NULL;
		model->priv->pg_res_size = 0;
                retval = FALSE;
		*fetch_error = TRUE;
        }
	else {
#ifdef GDA_PG_DEBUG
		dump_pg_res (model->priv->pg_res);
#endif

                //PQntuples() returns the number of rows in the result:
                const gint nbtuples = PQntuples (model->priv->pg_res);
		model->priv->pg_res_size = nbtuples;

                if (nbtuples > 0) {
			/* model->priv->pg_res_inf */
			if (model->priv->pg_pos == G_MININT)
				model->priv->pg_res_inf = 0;
			else
				model->priv->pg_res_inf = model->priv->pg_pos + 1;

			/* GDA_PMODEL (model)->advertized_nrows and model->priv->pg_pos */
			if (nbtuples < model->priv->chunk_size) {
				if (model->priv->pg_pos == G_MININT) 
					GDA_PMODEL (model)->advertized_nrows = nbtuples;
				else
					GDA_PMODEL (model)->advertized_nrows = model->priv->pg_pos + nbtuples + 1;

				model->priv->pg_pos = G_MAXINT;				
			}
			else {
				if (model->priv->pg_pos == G_MININT)
					model->priv->pg_pos = nbtuples - 1;
				else
					model->priv->pg_pos += nbtuples;
			}
		}
		else {
			if (model->priv->pg_pos == G_MININT)
				GDA_PMODEL (model)->advertized_nrows = 0;
			else
				GDA_PMODEL (model)->advertized_nrows = model->priv->pg_pos + 1; /* total number of rows */
			model->priv->pg_pos = G_MAXINT;
			retval = FALSE;
		}
	}

#ifdef GDA_PG_DEBUG
	g_print ("--> SIZE = %d (inf = %d) nrows = %d, pg_pos = %d\n", model->priv->pg_res_size, model->priv->pg_res_inf,
		 GDA_PMODEL (model)->advertized_nrows, model->priv->pg_pos);
#endif

	return retval;
}

static gboolean
fetch_prev_chunk (GdaPostgresRecordset *model, gboolean *fetch_error, GError **error)
{
	if (model->priv->pg_res) {
		PQclear (model->priv->pg_res);
		model->priv->pg_res = NULL;
	}
	*fetch_error = FALSE;

	if (model->priv->pg_pos == G_MININT) 
		return FALSE;
	else if (model->priv->pg_pos == G_MAXINT) 
		g_assert (GDA_PMODEL (model)->advertized_nrows >= 0); /* total number of rows MUST be known at this point */

	gchar *str;
	gboolean retval = TRUE;
	int status;
	gint noffset;
	
	if (model->priv->pg_pos == G_MAXINT)
		noffset = model->priv->chunk_size + 1;
	else
		noffset = model->priv->pg_res_size + model->priv->chunk_size;
	str = g_strdup_printf ("MOVE BACKWARD %d FROM %s; FETCH FORWARD %d FROM %s;",
			       noffset, model->priv->cursor_name,
			       model->priv->chunk_size, model->priv->cursor_name);
#ifdef GDA_PG_DEBUG
	g_print ("QUERY: %s\n", str);
#endif
        model->priv->pg_res = PQexec (model->priv->pconn, str);
        g_free (str);
        status = PQresultStatus (model->priv->pg_res);
	model->priv->chunks_read ++;
        if (status != PGRES_TUPLES_OK) {
		_gda_postgres_make_error (gda_pmodel_get_connection ((GdaPModel*) model), 
					  model->priv->pconn, model->priv->pg_res, error);
                PQclear (model->priv->pg_res);
                model->priv->pg_res = NULL;
		model->priv->pg_res_size = 0;
                retval = FALSE;
		*fetch_error = TRUE;
        }
	else {
#ifdef GDA_PG_DEBUG
		dump_pg_res (model->priv->pg_res);
#endif

                //PQntuples() returns the number of rows in the result:
                const gint nbtuples = PQntuples (model->priv->pg_res);
		model->priv->pg_res_size = nbtuples;

                if (nbtuples > 0) {
			/* model->priv->pg_res_inf */
			if (model->priv->pg_pos == G_MAXINT)
				model->priv->pg_res_inf = GDA_PMODEL (model)->advertized_nrows - nbtuples;
			else
				model->priv->pg_res_inf = 
					MAX (model->priv->pg_res_inf - (noffset - model->priv->chunk_size), 0);

			/* model->priv->pg_pos */
			if (nbtuples < model->priv->chunk_size) {
				model->priv->pg_pos = G_MAXINT;
			}
			else {
				if (model->priv->pg_pos == G_MAXINT)
					model->priv->pg_pos = GDA_PMODEL (model)->advertized_nrows - 1;
				else
					model->priv->pg_pos = MAX (model->priv->pg_pos - noffset, -1) + nbtuples;
			}
		}
		else {
			model->priv->pg_pos = G_MAXINT;
			retval = FALSE;
		}
	}

#ifdef GDA_PG_DEBUG
	g_print ("<-- SIZE = %d (inf = %d) nrows = %d, pg_pos = %d\n", model->priv->pg_res_size, model->priv->pg_res_inf,
		 GDA_PMODEL (model)->advertized_nrows, model->priv->pg_pos);
#endif

	return retval;
}

static gboolean
fetch_row_number_chunk (GdaPostgresRecordset *model, int row_index, gboolean *fetch_error, GError **error)
{
        if (model->priv->pg_res) {
                PQclear (model->priv->pg_res);
                model->priv->pg_res = NULL;
        }
	*fetch_error = FALSE;

        gchar *str;
        gboolean retval = TRUE;
        int status;

        /* Postgres's FETCH ABSOLUTE seems to use a 1-based index: */
        str = g_strdup_printf ("FETCH ABSOLUTE %d FROM %s;",
                               row_index + 1, model->priv->cursor_name);
#ifdef GDA_PG_DEBUG
        g_print ("QUERY: %s\n", str);
#endif
        model->priv->pg_res = PQexec (model->priv->pconn, str);
        g_free (str);
        status = PQresultStatus (model->priv->pg_res);
        model->priv->chunks_read ++; /* Not really correct, because we are only fetching 1 row, not a whole chunk of rows. */
        if (status != PGRES_TUPLES_OK) {
		_gda_postgres_make_error (gda_pmodel_get_connection ((GdaPModel*) model), 
					  model->priv->pconn, model->priv->pg_res, error);
                PQclear (model->priv->pg_res);
                model->priv->pg_res = NULL;
                model->priv->pg_res_size = 0;
                retval = FALSE;
		*fetch_error = TRUE;
        }
        else {
#ifdef GDA_PG_DEBUG
                dump_pg_res (model->priv->pg_res);
#endif

                //PQntuples() returns the number of rows in the result:
                const gint nbtuples = PQntuples (model->priv->pg_res);
                model->priv->pg_res_size = nbtuples;

                if (nbtuples > 0) {
                        /* Remember the row number for the start of this chunk:
                         * (actually a chunk of just 1 record in this case.) */
                        model->priv->pg_res_inf = row_index;

                        /* don't change model->priv->nrows because we can't know if we have reached the end */
                        model->priv->pg_pos = row_index;
                }
                else {
                        model->priv->pg_pos = G_MAXINT;
                        retval = FALSE;
                }
        }

#ifdef GDA_PG_DEBUG
        g_print ("--> SIZE = %d (inf = %d) nrows = %d, pg_pos = %d\n", model->priv->pg_res_size, model->priv->pg_res_inf,
                 model->priv->nrows, model->priv->pg_pos);
#endif

        return retval;
}
