/*
 * Copyright (C) 2001 - 2002 Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 * Copyright (C) 2001 - 2004 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2002 Holger Thon <holger@src.gnome.org>
 * Copyright (C) 2004 Jeronimo Albi <jeronimoalbi@yahoo.com.ar>
 * Copyright (C) 2004 - 2008 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2005 Christopher Taylor <christophth@tiscali.it>
 * Copyright (C) 2005 Álvaro Peña <alvaropg@telefonica.net>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
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
#include "gda-firebird.h"
#include "gda-firebird-recordset.h"
#include "gda-firebird-provider.h"
#include <libgda/libgda-global-variables.h>

#define _GDA_PSTMT(x) ((GdaPStmt*)(x))

static void gda_firebird_recordset_class_init (GdaFirebirdRecordsetClass *klass);
static void gda_firebird_recordset_init       (GdaFirebirdRecordset *recset,
					     GdaFirebirdRecordsetClass *klass);
static void gda_firebird_recordset_dispose   (GObject *object);

/* virtual methods */
static gint     gda_firebird_recordset_fetch_nb_rows (GdaDataSelect *model);
static gboolean gda_firebird_recordset_fetch_random (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error);
static gboolean gda_firebird_recordset_fetch_next (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error);
static gboolean gda_firebird_recordset_fetch_prev (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error);
static gboolean gda_firebird_recordset_fetch_at (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error);


struct _GdaFirebirdRecordsetPrivate {
	GdaConnection *cnc;
	/* TO_ADD: specific information */
	XSQLDA *xsqlda;
	gint n_columns;
};
static GObjectClass *parent_class = NULL;

typedef PARAMVARY VARY2;


#ifndef ISC_INT64_FORMAT

/* Define a format string for printf.  Printing of 64-bit integers
   is not standard between platforms */

#if (defined(_MSC_VER) && defined(WIN32))
#define	ISC_INT64_FORMAT	"I64"
#else
#define	ISC_INT64_FORMAT	"ll"
#endif
#endif


/*
 * Object init and finalize
 */
static void
gda_firebird_recordset_init (GdaFirebirdRecordset *recset,
			   GdaFirebirdRecordsetClass *klass)
{
	g_return_if_fail (GDA_IS_FIREBIRD_RECORDSET (recset));
	recset->priv = g_new0 (GdaFirebirdRecordsetPrivate, 1);
	recset->priv->cnc = NULL;

	/* initialize specific information */
	//TO_IMPLEMENT;
	recset->priv->xsqlda	= NULL;
}

static void
gda_firebird_recordset_class_init (GdaFirebirdRecordsetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaDataSelectClass *pmodel_class = GDA_DATA_SELECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = gda_firebird_recordset_dispose;
	pmodel_class->fetch_nb_rows = gda_firebird_recordset_fetch_nb_rows;
	pmodel_class->fetch_random = gda_firebird_recordset_fetch_random;

	pmodel_class->fetch_next = gda_firebird_recordset_fetch_next;
	pmodel_class->fetch_prev = gda_firebird_recordset_fetch_prev;
	pmodel_class->fetch_at = gda_firebird_recordset_fetch_at;
}

static void
gda_firebird_recordset_dispose (GObject *object)
{
	GdaFirebirdRecordset *recset = (GdaFirebirdRecordset *) object;

	g_return_if_fail (GDA_IS_FIREBIRD_RECORDSET (recset));

	if (recset->priv) {
		if (recset->priv->cnc) 
			g_object_unref (recset->priv->cnc);

		/* free specific information */
		//TO_IMPLEMENT;
		if (NULL != recset->priv->xsqlda)
			g_free(recset->priv->xsqlda);

		g_free (recset->priv);
		recset->priv = NULL;
	}

	parent_class->dispose (object);
}

/*
 * Public functions
 */

GType
gda_firebird_recordset_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static const GTypeInfo info = {
			sizeof (GdaFirebirdRecordsetClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_firebird_recordset_class_init,
			NULL,
			NULL,
			sizeof (GdaFirebirdRecordset),
			0,
			(GInstanceInitFunc) gda_firebird_recordset_init
		};
		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GDA_TYPE_DATA_SELECT, "GdaFirebirdRecordset", &info, 0);
		g_static_mutex_unlock (&registering);
	}

	return type;
}

static GType
_gda_firebird_type_to_gda (XSQLVAR *var){
	short       dtype;
	GType gtype;
	dtype = var->sqltype & ~1;
	/*
	if ((var->sqltype & 1) && (*var->sqlind < 0)){
		return GDA_TYPE_NULL;
	}
	*/
	switch (dtype){
		case SQL_TEXT:
		case SQL_VARYING:
			gtype = G_TYPE_STRING;
			//g_print("SQL_TEXT\n");
			break;
		case SQL_LONG:
			gtype = G_TYPE_ULONG;
			//g_print("SQL_TYPE_LONG\n");
			break;
		case SQL_SHORT:
		case SQL_INT64:
			gtype = G_TYPE_INT;
			//g_print("SQL_TYPE_INT\n");
			break;
		case SQL_FLOAT:
			gtype = G_TYPE_FLOAT;
			//g_print("SQL_TYPE_FLOAT\n");
			break;
		case SQL_DOUBLE:
			gtype = G_TYPE_DOUBLE;
			//g_print("SQL_TYPE_DOUBLE\n");
			break;
		case SQL_TIMESTAMP:
			//gtype = G_TYPE_DATE_TIME;
			gtype = GDA_TYPE_TIMESTAMP;
			//g_print("SQL_TIMESTAMP\n");
			break;
		case SQL_TYPE_DATE:
			gtype = G_TYPE_DATE;
			//g_print("SQL_TYPE_DATE\n");
			break;
		case SQL_TYPE_TIME:
			gtype = GDA_TYPE_TIME;
			//g_print("SQL_TYPE_TIME\n");
			break;
		case SQL_BLOB:
		case SQL_ARRAY:
		default:
			gtype = GDA_TYPE_BLOB;
			//g_print("SQL_BLOB\n");
			break;
	}

	return gtype;
}

/*
 *    Print column's data.
 */
void _fb_set_row_data (XSQLVAR *var, GValue *value, GdaRow *row){
	short       dtype;
	char        data[2048], *p;
	char        blob_s[20], date_s[25];
	VARY2        *vary2;
	short       len;
	struct tm   times;
	ISC_QUAD    bid;

	dtype = var->sqltype & ~1;
	p = data;

	/* Null handling.  If the column is nullable and null */
	if ((var->sqltype & 1) && (*var->sqlind < 0))
	{
		switch (dtype)
		{
			case SQL_TEXT:
			case SQL_VARYING:
				len = var->sqllen;
				break;
			case SQL_SHORT:
				len = 6;
				if (var->sqlscale > 0) len += var->sqlscale;
				break;
			case SQL_LONG:
				len = 11;
				if (var->sqlscale > 0) len += var->sqlscale;
				break;
			case SQL_INT64:
				len = 21;
				if (var->sqlscale > 0) len += var->sqlscale;
				break;
			case SQL_FLOAT:
				len = 15;
				break;
			case SQL_DOUBLE:
				len = 24;
				break;
			case SQL_TIMESTAMP:
				len = 24;
				break;
			case SQL_TYPE_DATE:
				len = 10;
				break;
			case SQL_TYPE_TIME:
				len = 13;
				break;
				case SQL_BLOB:
				case SQL_ARRAY:
				default:
					len = 17;
					break;
		}
		g_value_set_string (value, "NULL");
	}
	else
	{
		switch (dtype)
		{
			case SQL_TEXT:
				sprintf (p, "%.*s ", var->sqllen, var->sqldata);
				g_value_set_string (value, p);
				break;
			case SQL_VARYING:
				vary2 = (VARY2*) var->sqldata;
				vary2->vary_string[vary2->vary_length] = '\0';	
				sprintf(p, "%-*s ", var->sqllen, vary2->vary_string);
				g_value_set_string (value, p);
				break;

			case SQL_SHORT:
			case SQL_LONG:
			case SQL_INT64: {
				ISC_INT64	fb_value;
				short		field_width;
				short		dscale;
				switch (dtype) {
					case SQL_SHORT:
						fb_value = (ISC_INT64)*(short *) var->sqldata;
						field_width = 6;
						break;
					case SQL_LONG:
						fb_value = (ISC_INT64)*(int *) var->sqldata;
						field_width = 11;
						break;
					case SQL_INT64:
						fb_value = (ISC_INT64)*(ISC_INT64 *) var->sqldata;
						field_width = 21;
						break;
					default:
						field_width = 11;
						fb_value = 0;
						break;
				}

				dscale = var->sqlscale;
				if (dscale < 0) {
					ISC_INT64	tens;
					short	i;

					tens = 1;
					for (i = 0; i > dscale; i--)
						tens *= 10;

					if (fb_value >= 0)
						sprintf (p, "%*" ISC_INT64_FORMAT "d.%0*" ISC_INT64_FORMAT "d",
							 field_width - 1 + dscale, 
							 (ISC_INT64) fb_value / tens,
							 -dscale, 
							 (ISC_INT64) fb_value % tens);
					else if ((fb_value / tens) != 0)
						sprintf (p, "%*" ISC_INT64_FORMAT "d.%0*" ISC_INT64_FORMAT "d",
							 field_width - 1 + dscale, 
							 (ISC_INT64) (fb_value / tens),
							 -dscale, 
							 (ISC_INT64) -(fb_value % tens));
					else
						sprintf (p, "%*s.%0*" ISC_INT64_FORMAT "d",
							 field_width - 1 + dscale, 
							 "-0",
							 -dscale, 
							 (ISC_INT64) -(fb_value % tens));
				}
				else if (dscale){
					sprintf (p, "%*" ISC_INT64_FORMAT "d%0*d", 
						 field_width, 
						 (ISC_INT64) fb_value,
						 dscale, 0);
				}
				else{
					sprintf (p, "%*" ISC_INT64_FORMAT "d",
						 field_width, 
						 (ISC_INT64) fb_value);
				}

				switch (dtype) {
					case SQL_SHORT:
						gda_value_set_short (value, atoi (p));
						break;
					case SQL_LONG:
						g_value_set_ulong (value, atoll (p));
						break;
					case SQL_INT64:
					default:
						g_value_set_int64 (value, atoll (p));
						break;
				}
				}
				break;

			case SQL_FLOAT:
				sprintf(p, "%15g ", *(float *) (var->sqldata));
				//setlocale (LC_NUMERIC, "C");
				g_value_set_float (value, atof (p));
				//setlocale (LC_NUMERIC, gda_numeric_locale);
				break;

			case SQL_DOUBLE:
				sprintf(p, "%24f ", *(double *) (var->sqldata));
				//setlocale (LC_NUMERIC, "C");
				g_value_set_double (value, atof (p));
				//setlocale (LC_NUMERIC, gda_numeric_locale);
				break;

			case SQL_TIMESTAMP:
				isc_decode_timestamp((ISC_TIMESTAMP *)var->sqldata, &times);
				sprintf(date_s, "%04d-%02d-%02d %02d:%02d:%02d",
						times.tm_year + 1900,
						times.tm_mon+1,
						times.tm_mday,
						times.tm_hour,
						times.tm_min,
						times.tm_sec);
				sprintf(p, "%*s ", 22, date_s);
				//g_value_set_string (value, p);

				GdaTimestamp timestamp;
				if (! gda_parse_iso8601_timestamp (&timestamp, date_s)) {
					g_print("****ERROR CONVERTING TO TIMESTAMP: %s\n", date_s);

					//gda_row_invalidate_value (row, value);
					/*
					g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
							GDA_SERVER_PROVIDER_DATA_ERROR,
							_("Invalid timestamp '%s' (format should be YYYY-MM-DD HH:MM:SS[.ms])"), 
							p);
					*/
				}
				else
					gda_value_set_timestamp (value, &timestamp);
				break;

			case SQL_TYPE_DATE:
				isc_decode_sql_date((ISC_DATE *)var->sqldata, &times);
				sprintf(date_s, "%04d-%02d-%02d",
						times.tm_year + 1900,
						times.tm_mon+1,
						times.tm_mday);
				sprintf(p, "%*s ", 10, date_s);
				GDate date;
				if (!gda_parse_iso8601_date (&date, p)) {
					//gda_row_invalidate_value (row, value);
					/*
					g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
							 GDA_SERVER_PROVIDER_DATA_ERROR,
							 _("Invalid date '%s' (date format should be YYYY-MM-DD)"), p);
					*/
				}
				else
					g_value_set_boxed (value, &date);
				break;

			case SQL_TYPE_TIME:
				isc_decode_sql_time((ISC_TIME *)var->sqldata, &times);
				sprintf(date_s, "%02d:%02d:%02d",
						times.tm_hour,
						times.tm_min,
						times.tm_sec);
				sprintf(p, "%*s ", 11, date_s);
				GdaTime timegda;
				if (!gda_parse_iso8601_time (&timegda, p)) {
					//gda_row_invalidate_value (row, value); 
					/*
					g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
							 GDA_SERVER_PROVIDER_DATA_ERROR,
							 _("Invalid time '%s' (time format should be HH:MM:SS[.ms])"), p);
					*/
				}
				else
					gda_value_set_time (value, &timegda);
				break;

			case SQL_BLOB:
			case SQL_ARRAY:
				/* Print the blob id on blobs or arrays */
				bid = *(ISC_QUAD *) var->sqldata;
				sprintf(blob_s, "%08x:%08x", bid.gds_quad_high, bid.gds_quad_low);
				sprintf(p, "%17s ", blob_s);
				g_value_set_string (value, p);
				break;

			default:
				g_value_set_string(value, "TYPE NOT FOUND");
				break;
		}
	}
}


/*
 * the @ps struct is modified and transferred to the new data model created in
 * this function
 */
GdaDataModel *
gda_firebird_recordset_new (GdaConnection *cnc, GdaFirebirdPStmt *ps, GdaDataModelAccessFlags flags, GType *col_types)
{
	GdaFirebirdRecordset *model;
	FirebirdConnectionData *cdata;
	gint i;
	gint fetch_stat;
	GdaDataModelAccessFlags rflags;

        g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
        g_return_val_if_fail (ps, NULL);

	cdata = (FirebirdConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata)
		return NULL;

	/* make sure @ps reports the correct number of columns using the API*/
        if (_GDA_PSTMT (ps)->ncols < 0)
                /*_GDA_PSTMT (ps)->ncols = ...;*/
		TO_IMPLEMENT;

        /* completing @ps if not yet done */
	if (_GDA_PSTMT (ps)->ncols < 0)
		/*_GDA_PSTMT (ps)->ncols = ...;*/
		_GDA_PSTMT (ps)->ncols = ps->sqlda->sqld;

	/* completing @ps if not yet done */
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
			GdaColumn       *column;
			GType           gtype;
			XSQLVAR         *var;

			var = (XSQLVAR *) &(ps->sqlda->sqlvar[i]);
			column = GDA_COLUMN (list->data);
			/* use C API to set columns' information using gda_column_set_*() */
			gtype = _gda_firebird_type_to_gda(var);

			_GDA_PSTMT (ps)->types [i] = gtype;
			gda_column_set_g_type (column, gtype);
			gda_column_set_name (column, var->aliasname);
			gda_column_set_description (column, var->aliasname);

			//g_print("\t\tSQL-NAME:%s\n\t\tREL-NAME: %s\n\t\tOWN-NAME: %s\n\t\tALIAS: %s\n**************************\n\n"
			//      , var->sqlname
			//      , var->relname
			//      , var->ownname
			//      , var->aliasname);
		}
	}


	/* determine access mode: RANDOM or CURSOR FORWARD are the only supported */
	if (flags & GDA_DATA_MODEL_ACCESS_RANDOM)
		rflags = GDA_DATA_MODEL_ACCESS_RANDOM;
	else
		rflags = GDA_DATA_MODEL_ACCESS_CURSOR_FORWARD;

	/* create data model */
	model = g_object_new (GDA_TYPE_FIREBIRD_RECORDSET, "prepared-stmt", ps, "model-usage", rflags, NULL);
	model->priv->cnc = cnc;
	model->priv->n_columns	= ps->sqlda->sqld;
	g_object_ref (cnc);

	gda_data_select_set_columns (GDA_DATA_SELECT (model), _GDA_PSTMT (ps)->tmpl_columns);

	/* post init specific code */
	if (isc_dsql_execute (cdata->status, cdata->ftr, &(ps->stmt_h), SQL_DIALECT_V6, NULL)) {
		isc_print_status(cdata->status);
		g_print("\n");
	}

	gint rownum = 0;
	while ((fetch_stat = isc_dsql_fetch(cdata->status, &(ps->stmt_h), SQL_DIALECT_V6, ps->sqlda)) == 0) {
		GdaRow *row = gda_row_new (ps->sqlda->sqld);
		for (i = 0; i < ps->sqlda->sqld; i++) {
			GValue *value = gda_row_get_value (row, i);
			GType type =_gda_firebird_type_to_gda((XSQLVAR *) &(ps->sqlda->sqlvar[i]));
			//char *data = mysql_row[i];

			//if (!data || (type == GDA_TYPE_NULL))
			//	continue;

			gda_value_reset_with_type (value, type);
			//g_value_set_string (value, "faghmie");
			_fb_set_row_data ((XSQLVAR *) &(ps->sqlda->sqlvar[i]), value, row);
			//print_column((XSQLVAR *) &sqlda->sqlvar[i]);
		}

		gda_data_select_take_row ((GdaDataSelect*) model, row, rownum);
		rownum++;
	}

	((GdaDataSelect *) model)->advertized_nrows = rownum;

	return GDA_DATA_MODEL (model);
}


/*
 * Get the number of rows in @model, if possible
 */
static gint
gda_firebird_recordset_fetch_nb_rows (GdaDataSelect *model)
{
	GdaFirebirdRecordset *imodel;

	imodel = GDA_FIREBIRD_RECORDSET (model);
	if (model->advertized_nrows >= 0)
		return model->advertized_nrows;

	/* use C API to determine number of rows,if possible */
	TO_IMPLEMENT;

	return model->advertized_nrows;
}

/*
 * Create a new filled #GdaRow object for the row at position @rownum, and put it into *prow.
 *
 * NOTES:
 * - @prow will NOT be NULL, but *prow WILL be NULL.
 * - a new #GdaRow object has to be created, corresponding to the @rownum row
 * - memory management for that new GdaRow object is left to the implementation, which
 *   can use gda_data_select_take_row() to "give" the GdaRow to @model (in this case
 *   this method won't be called anymore for the same @rownum), or may decide to
 *   keep a cache of GdaRow object and "recycle" them.
 * - implementing this method is MANDATORY if the data model supports random access
 * - this method is only called when data model is used in random access mode
 */
static gboolean 
gda_firebird_recordset_fetch_random (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error)
{
	GdaFirebirdRecordset *imodel;

	imodel = GDA_FIREBIRD_RECORDSET (model);

	TO_IMPLEMENT;

	return TRUE;
}

/*
 * Create and "give" filled #GdaRow object for all the rows in the model
 */
static gboolean
gda_firebird_recordset_store_all (GdaDataSelect *model, GError **error)
{
	GdaFirebirdRecordset *imodel;
	gint i;

	imodel = GDA_FIREBIRD_RECORDSET (model);

	/* default implementation */
	for (i = 0; i < model->advertized_nrows; i++) {
		GdaRow *prow;
		if (! gda_firebird_recordset_fetch_random (model, &prow, i, error))
			return FALSE;
	}
	return TRUE;
}

/*
 * Create a new filled #GdaRow object for the next cursor row, and put it into *prow.
 *
 * NOTES:
 * - @prow will NOT be NULL, but *prow WILL be NULL.
 * - a new #GdaRow object has to be created, corresponding to the @rownum row
 * - memory management for that new GdaRow object is left to the implementation, which
 *   can use gda_data_select_take_row() to "give" the GdaRow to @model (in this case
 *   this method won't be called anymore for the same @rownum), or may decide to
 *   keep a cache of GdaRow object and "recycle" them.
 * - implementing this method is MANDATORY
 * - this method is only called when data model is used in cursor access mode
 */
static gboolean 
gda_firebird_recordset_fetch_next (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error)
{
	GdaFirebirdRecordset *imodel = (GdaFirebirdRecordset*) model;

	TO_IMPLEMENT;

	return TRUE;
}

/*
 * Create a new filled #GdaRow object for the previous cursor row, and put it into *prow.
 *
 * NOTES:
 * - @prow will NOT be NULL, but *prow WILL be NULL.
 * - a new #GdaRow object has to be created, corresponding to the @rownum row
 * - memory management for that new GdaRow object is left to the implementation, which
 *   can use gda_data_select_take_row() to "give" the GdaRow to @model (in this case
 *   this method won't be called anymore for the same @rownum), or may decide to
 *   keep a cache of GdaRow object and "recycle" them.
 * - implementing this method is OPTIONAL (in this case the data model is assumed not to
 *   support moving iterators backward)
 * - this method is only called when data model is used in cursor access mode
 */
static gboolean 
gda_firebird_recordset_fetch_prev (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error)
{
	GdaFirebirdRecordset *imodel = (GdaFirebirdRecordset*) model;

	TO_IMPLEMENT;

	return TRUE;
}

/*
 * Create a new filled #GdaRow object for the cursor row at position @rownum, and put it into *prow.
 *
 * NOTES:
 * - @prow will NOT be NULL, but *prow WILL be NULL.
 * - a new #GdaRow object has to be created, corresponding to the @rownum row
 * - memory management for that new GdaRow object is left to the implementation, which
 *   can use gda_data_select_take_row() to "give" the GdaRow to @model (in this case
 *   this method won't be called anymore for the same @rownum), or may decide to
 *   keep a cache of GdaRow object and "recycle" them.
 * - implementing this method is OPTIONAL and usefull only if there is a method quicker
 *   than iterating one step at a time to the correct position.
 * - this method is only called when data model is used in cursor access mode
 */
static gboolean 
gda_firebird_recordset_fetch_at (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error)
{
	GdaFirebirdRecordset *imodel = (GdaFirebirdRecordset*) model;
	
	TO_IMPLEMENT;

	return TRUE;
}

