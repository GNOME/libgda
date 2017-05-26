/*
 * Copyright (C) 2001 - 2003 Gonzalo Paniagua Javier <gonzalo@ximian.com>
 * Copyright (C) 2001 - 2004 Rodrigo Moya <rodrigo@ximian.com>
 * Copyright (C) 2002 Zbigniew Chyla <cyba@gnome.pl>
 * Copyright (C) 2003 Akira TAGOH <tagoh@gnome-db.org>
 * Copyright (C) 2003 Laurent Sansonetti <laurent@datarescue.be>
 * Copyright (C) 2004 Andrew Hill <andru@src.gnome.org>
 * Copyright (C) 2004 - 2005 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2004 Szalai Ferenc <szferi@einstein.ki.iif.hu>
 * Copyright (C) 2004 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2005 Alex <alex@igalia.com>
 * Copyright (C) 2005 Álvaro Peña <alvaropg@telefonica.net>
 * Copyright (C) 2006 - 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2007 Armin Burgmeier <armin@openismus.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2011-2017 Daniel Espinosa <esodan@gmail.com>
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
#include <libgda/gda-util.h>
#include <libgda/gda-connection-private.h>
#include "gda-postgres.h"
#include "gda-postgres-recordset.h"
#include "gda-postgres-provider.h"
#include "gda-postgres-blob-op.h"
#include "gda-postgres-util.h"
#include <libgda/libgda-global-variables.h>
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
static gint     gda_postgres_recordset_fetch_nb_rows (GdaDataSelect *model);
static gboolean gda_postgres_recordset_fetch_random (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error);
static gboolean gda_postgres_recordset_store_all (GdaDataSelect *model, GError **error);
static gboolean gda_postgres_recordset_fetch_next (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error);
static gboolean gda_postgres_recordset_fetch_prev (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error);
static gboolean gda_postgres_recordset_fetch_at (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error);

/* static helper functions */
static void make_point (GdaGeometricPoint *point, const gchar *value);
static void set_value (GdaConnection *cnc, GdaRow *row, GValue *value, GType type, const gchar *thevalue, gint length, GError **error);

static void     set_prow_with_pg_res (GdaPostgresRecordset *imodel, GdaRow *prow, gint pg_res_rownum, GError **error);
static GdaRow *new_row_from_pg_res (GdaPostgresRecordset *imodel, gint pg_res_rownum, GError **error);
static gboolean row_is_in_current_pg_res (GdaPostgresRecordset *model, gint row);
static gboolean fetch_next_chunk (GdaPostgresRecordset *model, gboolean *fetch_error, GError **error);
static gboolean fetch_prev_chunk (GdaPostgresRecordset *model, gboolean *fetch_error, GError **error);
static gboolean fetch_row_number_chunk (GdaPostgresRecordset *model, int row_index, gboolean *fetch_error, GError **error);


struct _GdaPostgresRecordsetPrivate {
	/* random access attributes */
	PGresult         *pg_res;

	/* cursor access attributes */
	GdaRow           *tmp_row; /* used to store a reference to the last #GdaRow returned */
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
gda_postgres_recordset_init (GdaPostgresRecordset *recset, G_GNUC_UNUSED GdaPostgresRecordsetClass *klass)
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
	GdaDataSelectClass *pmodel_class = GDA_DATA_SELECT_CLASS (klass);

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
                                         g_param_spec_int ("chunk-size", _("Number of rows fetched at a time"), NULL,
                                                           1, G_MAXINT - 1, 10,
                                                           G_PARAM_CONSTRUCT | G_PARAM_READABLE | G_PARAM_WRITABLE));
        g_object_class_install_property (object_class, PROP_CHUNCKS_READ,
                                         g_param_spec_int ("chunks-read",
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
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
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
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
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
		static GMutex registering;
		static const GTypeInfo info = {
			sizeof (GdaPostgresRecordsetClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_postgres_recordset_class_init,
			NULL,
			NULL,
			sizeof (GdaPostgresRecordset),
			0,
			(GInstanceInitFunc) gda_postgres_recordset_init,
			0
		};
		g_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GDA_TYPE_DATA_SELECT, "GdaPostgresRecordset", &info, 0);
		g_mutex_unlock (&registering);
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

		/* create prepared statement's types, all types are initialized to GDA_TYPE_NULL */
		_GDA_PSTMT (ps)->types = g_new (GType, _GDA_PSTMT (ps)->ncols);
		for (i = 0; i < _GDA_PSTMT (ps)->ncols; i++)
			_GDA_PSTMT (ps)->types [i] = GDA_TYPE_NULL;

		if (col_types) {
			for (i = 0; ; i++) {
				if (col_types [i] > 0) {
					if (col_types [i] == G_TYPE_NONE)
						break;
					if (i >= _GDA_PSTMT (ps)->ncols) {
						g_warning (_("Column %d out of range (0-%d), ignoring its specified type"), i,
							   _GDA_PSTMT (ps)->ncols - 1);
						break;
					}
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
			if (gtype == GDA_TYPE_NULL) {
				gtype = _gda_postgres_type_oid_to_gda (cdata->cnc, cdata->reuseable, postgres_type);
				_GDA_PSTMT (ps)->types [i] = gtype;
			}
			_GDA_PSTMT (ps)->types [i] = gtype;
			gda_column_set_g_type (column, gtype);
			gda_column_set_name (column, PQfname (pg_res, i));
			gda_column_set_description (column, PQfname (pg_res, i));
		}
        }
}

GdaDataModel *
gda_postgres_recordset_new_random (GdaConnection *cnc, GdaPostgresPStmt *ps, GdaSet *exec_params, 
				   PGresult *pg_res, GType *col_types)
{
	GdaPostgresRecordset *model;
        PostgresConnectionData *cdata;

        g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
        g_return_val_if_fail (ps, NULL);

	cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data_error (cnc, NULL);
	if (!cdata)
		return NULL;

	/* finish prepared statement's init */
	finish_prep_stmt_init (cdata, ps, pg_res, col_types);

	/* create data model */
        model = g_object_new (GDA_TYPE_POSTGRES_RECORDSET, "connection", cnc, 
			      "prepared-stmt", ps, 
			      "model-usage", GDA_DATA_MODEL_ACCESS_RANDOM, 
			      "exec-params", exec_params, NULL);
	model->priv->pg_res = pg_res;
	((GdaDataSelect*) model)->advertized_nrows = PQntuples (model->priv->pg_res);

        return GDA_DATA_MODEL (model);
}

/*
 * Takes ownership of @cursor_name
 */
GdaDataModel *
gda_postgres_recordset_new_cursor (GdaConnection *cnc, GdaPostgresPStmt *ps, GdaSet *exec_params,
				   gchar *cursor_name, GType *col_types)
{
	GdaPostgresRecordset *model;
        PostgresConnectionData *cdata;

        g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
        g_return_val_if_fail (ps, NULL);

	cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data_error (cnc, NULL);
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
	if (!pg_res || (status != PGRES_TUPLES_OK)) {
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
			      GDA_DATA_MODEL_ACCESS_CURSOR_FORWARD | GDA_DATA_MODEL_ACCESS_CURSOR_BACKWARD, 
			      "exec-params", exec_params, NULL);
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
gda_postgres_recordset_fetch_nb_rows (GdaDataSelect *model)
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
 * Create a new filled #GdaRow object for the row at position @rownum.
 *
 * Each new #GdaRow created is "given" to the #GdaDataSelect implementation using gda_data_select_take_row ().
 */
static gboolean
gda_postgres_recordset_fetch_random (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error)
{
	GdaPostgresRecordset *imodel = (GdaPostgresRecordset *) model;

	if (!imodel->priv->pg_res) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_INTERNAL_ERROR,
			     "%s", _("Internal error"));
		return TRUE;
	}

	*prow = new_row_from_pg_res (imodel, rownum, error);
	gda_data_select_take_row (model, *prow, rownum);

	if (model->nb_stored_rows == model->advertized_nrows) {
		/* all the rows have been converted from PGresult to GdaRow objects => we can
		 * discard the PGresult */
		PQclear (imodel->priv->pg_res);
		imodel->priv->pg_res = NULL;
	}

	return TRUE;
}

/*
 * Create and "give" filled #GdaRow object for all the rows in the model
 */
static gboolean
gda_postgres_recordset_store_all (GdaDataSelect *model, GError **error)
{
	GdaPostgresRecordset *imodel = (GdaPostgresRecordset*) model;
	gint i;
	
	if (!imodel->priv->pg_res) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_INTERNAL_ERROR,
			     "%s", _("Internal error"));
		return FALSE;
	}

	for (i = 0; i < model->advertized_nrows; i++) {
		GdaRow *prow;
		if (! gda_postgres_recordset_fetch_random (model, &prow, i, error))
			return FALSE;
	}
	return TRUE;
}

/*
 * Create a new filled #GdaRow object for the next cursor row, and put it into *prow.
 *
 * Each new #GdaRow created is referenced only by imodel->priv->tmp_row (the #GdaDataSelect implementation
 * never keeps a reference to it).
 */
static gboolean
gda_postgres_recordset_fetch_next (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error)
{
	GdaPostgresRecordset *imodel = (GdaPostgresRecordset*) model;

	if (row_is_in_current_pg_res (imodel, rownum)) {
		if (imodel->priv->tmp_row)
			set_prow_with_pg_res (imodel, imodel->priv->tmp_row, rownum - imodel->priv->pg_res_inf, error);
		else
			imodel->priv->tmp_row = new_row_from_pg_res (imodel, rownum - imodel->priv->pg_res_inf, error);
		*prow = imodel->priv->tmp_row;
	}
	else {
		gboolean fetch_error = FALSE;
		if (fetch_next_chunk (imodel, &fetch_error, error)) {
			if (imodel->priv->tmp_row)
				set_prow_with_pg_res (imodel, imodel->priv->tmp_row, rownum - imodel->priv->pg_res_inf, error);
			else
				imodel->priv->tmp_row = new_row_from_pg_res (imodel, rownum - imodel->priv->pg_res_inf, error);
			*prow = imodel->priv->tmp_row;
		}
	}
	return TRUE;
}

/*
 * Create a new filled #GdaRow object for the previous cursor row, and put it into *prow.
 *
 * Each new #GdaRow created is referenced only by imodel->priv->tmp_row (the #GdaDataSelect implementation
 * never keeps a reference to it).
 */
static gboolean
gda_postgres_recordset_fetch_prev (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error)
{
	GdaPostgresRecordset *imodel = (GdaPostgresRecordset*) model;

	if (row_is_in_current_pg_res (imodel, rownum)) {
		if (imodel->priv->tmp_row)
			set_prow_with_pg_res (imodel, imodel->priv->tmp_row, rownum - imodel->priv->pg_res_inf, error);
		else
			imodel->priv->tmp_row = new_row_from_pg_res (imodel, rownum - imodel->priv->pg_res_inf, error);
		*prow = imodel->priv->tmp_row;
	}
	else {
		gboolean fetch_error = FALSE;
		if (fetch_prev_chunk (imodel, &fetch_error, error)) {
			if (imodel->priv->tmp_row)
				set_prow_with_pg_res (imodel, imodel->priv->tmp_row, rownum - imodel->priv->pg_res_inf, error);
			else
				imodel->priv->tmp_row = new_row_from_pg_res (imodel, rownum - imodel->priv->pg_res_inf, error);
			*prow = imodel->priv->tmp_row;
		}
	}
	return TRUE;
}

/*
 * Create a new filled #GdaRow object for the cursor row at position @rownum, and put it into *prow.
 *
 * Each new #GdaRow created is referenced only by imodel->priv->tmp_row (the #GdaDataSelect implementation
 * never keeps a reference to it).
 */
static gboolean
gda_postgres_recordset_fetch_at (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error)
{
	GdaPostgresRecordset *imodel = (GdaPostgresRecordset*) model;

	if (imodel->priv->tmp_row) {
		g_object_unref (imodel->priv->tmp_row);
		imodel->priv->tmp_row = NULL;
	}

	if (row_is_in_current_pg_res (imodel, rownum)) {
		*prow = new_row_from_pg_res (imodel, rownum - imodel->priv->pg_res_inf, error);
		imodel->priv->tmp_row = *prow;
	}
	else {
		gboolean fetch_error = FALSE;
		if (fetch_row_number_chunk (imodel, rownum, &fetch_error, error)) {
			*prow = new_row_from_pg_res (imodel, rownum - imodel->priv->pg_res_inf, error);
			imodel->priv->tmp_row = *prow;
		}
	}
	return TRUE;
}






/*
 * Static helper functions
 */

/* Makes a point from a string like "(3.2,5.6)" */
static void
make_point (GdaGeometricPoint *point, const gchar *value)
{
	value++;
	gda_geometricpoint_set_x (point, g_ascii_strtod (value, NULL));
	value = strchr (value, ',');
	value++;
	gda_geometricpoint_set_y (point, g_ascii_strtod (value, NULL));
}

static void
set_value (GdaConnection *cnc, GdaRow *row, GValue *value, GType type, const gchar *thevalue,
	   G_GNUC_UNUSED gint length, GError **error)
{
	gda_value_reset_with_type (value, type);

	if (type == G_TYPE_BOOLEAN)
		g_value_set_boolean (value, (*thevalue == 't') ? TRUE : FALSE);
	else if (type == G_TYPE_STRING) 
		g_value_set_string (value, thevalue);
	else if (type == G_TYPE_INT)
		g_value_set_int (value, atol (thevalue));
	else if (type == G_TYPE_UINT)
		g_value_set_uint (value, (guint) g_ascii_strtoull (thevalue, NULL, 10));
	else if (type == G_TYPE_DATE) {
		PostgresConnectionData *cdata;
		cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
		if (cdata) {
			GDate date;
			if (gda_parse_formatted_date (&date, thevalue, cdata->date_first, cdata->date_second,
						      cdata->date_third, cdata->date_sep))
				g_value_set_boxed (value, &date);
			else {
				gda_row_invalidate_value (row, value);
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
					     GDA_SERVER_PROVIDER_DATA_ERROR,
					     _("Invalid date format '%s'"), thevalue);
			}
		}
		else
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
				     GDA_SERVER_PROVIDER_INTERNAL_ERROR,
				     "%s", _("Internal error"));
	}
	else if (type == GDA_TYPE_TIME) {
		GdaTime timegda;
		if (!gda_parse_iso8601_time (&timegda, thevalue)) {
			gda_row_invalidate_value (row, value); 
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
				     GDA_SERVER_PROVIDER_DATA_ERROR,
				     _("Invalid time '%s' (time format should be HH:MM:SS[.ms])"), thevalue);
		}
		else {
			if (timegda.timezone == GDA_TIMEZONE_INVALID)
				timegda.timezone = 0; /* set to GMT */
			gda_value_set_time (value, &timegda);
		}
	}
	else if (type == G_TYPE_INT64)
		g_value_set_int64 (value, atoll (thevalue));
	else if (type == G_TYPE_ULONG)
		g_value_set_ulong (value, atoll (thevalue));
	else if (type == G_TYPE_LONG)
		g_value_set_ulong (value, atoll (thevalue));
	else if (type == GDA_TYPE_SHORT)
		gda_value_set_short (value, atoi (thevalue));
	else if (type == G_TYPE_FLOAT) {
		g_value_set_float (value, g_ascii_strtod (thevalue, NULL));
	}
	else if (type == G_TYPE_DOUBLE) {
		g_value_set_double (value, g_ascii_strtod (thevalue, NULL));
	}
	else if (type == GDA_TYPE_NUMERIC) {
		GdaNumeric* numeric = gda_numeric_new ();
		gda_numeric_set_from_string (numeric, thevalue);
		gda_numeric_set_precision (numeric, 0); /* FIXME */
		gda_numeric_set_width (numeric, 0); /* FIXME */
		gda_value_set_numeric (value, numeric);
		gda_numeric_free (numeric);
	}
	else if (type == GDA_TYPE_GEOMETRIC_POINT) {
		GdaGeometricPoint* point = gda_geometric_point_new ();
		make_point (&point, thevalue);
		gda_value_set_geometric_point (value, &point);
		gda_geometric_point_free (point);
	}
	else if (type == GDA_TYPE_TIMESTAMP) {
		PostgresConnectionData *cdata;
		cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
		if (cdata) {
			GdaTimestamp* timestamp = gda_timestamp_new ();
			if (gda_parse_formatted_timestamp (timestamp, thevalue, cdata->date_first, cdata->date_second,
							   cdata->date_third, cdata->date_sep)) {
				if (gda_timestamp_get_timezone (timestamp) == GDA_TIMEZONE_INVALID)
					gda_timestamp_set_timezone (timestamp, 0); /* set to GMT */
				gda_value_set_timestamp (value, timestamp);
			}
			else {
				gda_row_invalidate_value (row, value);
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
					     GDA_SERVER_PROVIDER_DATA_ERROR,
					     _("Invalid timestamp value '%s'"), thevalue);
			}
			gda_timestamp_free (timestamp);
		}
		else
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
				     GDA_SERVER_PROVIDER_INTERNAL_ERROR,
				     "%s", _("Internal error"));
	}
	else if (type == GDA_TYPE_BINARY) {
		/*
		 * Requires PQunescapeBytea in libpq (present since 7.3.x)
		 */
		guchar *unescaped;
                size_t pqlength = 0;
		PostgresConnectionData *cdata;
		gboolean valueset = FALSE;

		cdata = (PostgresConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
		if (cdata) {
			if ((thevalue[0] == '\\') && (thevalue[1] == 'x')) {
				guint len;
				len = strlen (thevalue + 2);
				if (!(len % 2)) {
					guint i;
					const gchar *ptr;
					pqlength = len / 2;
					unescaped = g_new (guchar, pqlength);
					for (i = 0, ptr = thevalue + 2; *ptr; i++, ptr += 2) {
						gchar c;
						c = ptr[0];
						if ((c >= 'a') && (c <= 'z'))
							unescaped [i] = c - 'a' + 10;
						else if ((c >= 'A') && (c <= 'Z'))
							unescaped [i] = c - 'A' + 10;
						else if ((c >= '0') && (c <= '9'))
							unescaped [i] = c - '0';
						else
							break;
						unescaped [i] <<= 4;

						c = ptr[1];
						if ((c >= 'a') && (c <= 'z'))
							unescaped [i] += c - 'a' + 10;
						else if ((c >= 'A') && (c <= 'Z'))
							unescaped [i] += c - 'A' + 10;
						else if ((c >= '0') && (c <= '9'))
							unescaped [i] += c - '0';
						else
							break;
					}
					if (! *ptr) {
						GdaBinary *bin;
						bin = gda_binary_new ();
						gda_binary_set_data (bin, unescaped, pqlength);
						gda_value_take_binary (value, bin);
						valueset = TRUE;
					}
				}
			}
			else {
				unescaped = PQunescapeBytea ((guchar*) thevalue, &pqlength);
				if (unescaped) {
					GdaBinary* bin = gda_binary_new ();
					gda_binary_set_data (bin, unescaped, pqlength);
					gda_value_take_binary (value, bin);
					PQfreemem (unescaped);
					valueset = TRUE;
				}
			}
		}
		if (!valueset) {
			gchar *tmp;
			tmp = g_strndup (thevalue, 20);
			gda_row_invalidate_value (row, value);
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
				     GDA_SERVER_PROVIDER_DATA_ERROR,
				     _("Invalid binary string representation '%s ...'"), 
				     tmp);
			g_free (tmp);
		}
	}
	else if (type == GDA_TYPE_BLOB) {
		GdaBlob *blob;
		GdaBlobOp *op;
		blob = gda_blob_new ();
		op = gda_postgres_blob_op_new_with_id (cnc, thevalue);
		gda_blob_set_op (blob, op);
		g_object_unref (op);

		gda_value_take_blob (value, blob);
	}
	else if (type == G_TYPE_GTYPE)
		g_value_set_gtype (value, gda_g_type_from_string (thevalue));
	else {
		/*g_warning ("Type %s not translated for value '%s' => set as string", g_type_name (type), thevalue);*/
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

static void
set_prow_with_pg_res (GdaPostgresRecordset *imodel, GdaRow *prow, gint pg_res_rownum, GError **error)
{
	gchar *thevalue;
	gint col;

	for (col = 0; col < ((GdaDataSelect*) imodel)->prep_stmt->ncols; col++) {
		thevalue = PQgetvalue (imodel->priv->pg_res, pg_res_rownum, col);
		if (thevalue && (*thevalue != '\0' ? FALSE : PQgetisnull (imodel->priv->pg_res, pg_res_rownum, col)))
			gda_value_set_null (gda_row_get_value (prow, col));
		else
			set_value (gda_data_select_get_connection ((GdaDataSelect*) imodel),
				   prow, gda_row_get_value (prow, col), 
				   ((GdaDataSelect*) imodel)->prep_stmt->types [col], 
				   thevalue, 
				   PQgetlength (imodel->priv->pg_res, pg_res_rownum, col), error);
	}
}

static GdaRow *
new_row_from_pg_res (GdaPostgresRecordset *imodel, gint pg_res_rownum, GError **error)
{
	GdaRow *prow;

	prow = gda_row_new (((GdaDataSelect*) imodel)->prep_stmt->ncols);
	set_prow_with_pg_res (imodel, prow, pg_res_rownum, error);
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
		_gda_postgres_make_error (gda_data_select_get_connection ((GdaDataSelect*) model), 
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

			/* GDA_DATA_SELECT (model)->advertized_nrows and model->priv->pg_pos */
			if (nbtuples < model->priv->chunk_size) {
				if (model->priv->pg_pos == G_MININT) 
					GDA_DATA_SELECT (model)->advertized_nrows = nbtuples;
				else
					GDA_DATA_SELECT (model)->advertized_nrows = model->priv->pg_pos + nbtuples + 1;

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
				GDA_DATA_SELECT (model)->advertized_nrows = 0;
			else
				GDA_DATA_SELECT (model)->advertized_nrows = model->priv->pg_pos + 1; /* total number of rows */
			model->priv->pg_pos = G_MAXINT;
			retval = FALSE;
		}
	}

#ifdef GDA_PG_DEBUG
	g_print ("--> SIZE = %d (inf = %d) nrows = %d, pg_pos = %d\n", model->priv->pg_res_size, model->priv->pg_res_inf,
		 GDA_DATA_SELECT (model)->advertized_nrows, model->priv->pg_pos);
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
		g_assert (GDA_DATA_SELECT (model)->advertized_nrows >= 0); /* total number of rows MUST be known at this point */

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
		_gda_postgres_make_error (gda_data_select_get_connection ((GdaDataSelect*) model),
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
				model->priv->pg_res_inf = GDA_DATA_SELECT (model)->advertized_nrows - nbtuples;
			else
				model->priv->pg_res_inf = 
					MAX (model->priv->pg_res_inf - (noffset - model->priv->chunk_size), 0);

			/* model->priv->pg_pos */
			if (nbtuples < model->priv->chunk_size) {
				model->priv->pg_pos = G_MAXINT;
			}
			else {
				if (model->priv->pg_pos == G_MAXINT)
					model->priv->pg_pos = GDA_DATA_SELECT (model)->advertized_nrows - 1;
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
		 GDA_DATA_SELECT (model)->advertized_nrows, model->priv->pg_pos);
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
		_gda_postgres_make_error (gda_data_select_get_connection ((GdaDataSelect*) model), 
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
