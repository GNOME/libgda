/*
 * Copyright (C) 2001 - 2002 Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 * Copyright (C) 2001 - 2004 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2004 Jeronimo Albi <jeronimoalbi@yahoo.com.ar>
 * Copyright (C) 2004 - 2014 Vivien Malerba <malerba@gnome-db.org>
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
#include <libgda/gda-debug-macros.h>

#define _GDA_PSTMT(x) ((GdaPStmt*)(x))

#ifndef ISC_INT64_FORMAT

/* Define a format string for printf.  Printing of 64-bit integers
   is not standard between platforms */

#if (defined(_MSC_VER) && defined(WIN32))
#define	ISC_INT64_FORMAT	"I64"
#else
#define	ISC_INT64_FORMAT	"ll"
#endif
#endif

typedef PARAMVARY VARY2;

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

	gint n_columns;
};
static GObjectClass *parent_class = NULL;


/*
 * Object init and finalize
 */
static void
gda_firebird_recordset_init (GdaFirebirdRecordset *recset,
			   GdaFirebirdRecordsetClass *klass)
{
//WHERE_AM_I;
	g_return_if_fail (GDA_IS_FIREBIRD_RECORDSET (recset));
	
	recset->priv = g_new0 (GdaFirebirdRecordsetPrivate, 1);


	/* initialize specific information */
	//TO_IMPLEMENT;
	
}

static void
gda_firebird_recordset_class_init (GdaFirebirdRecordsetClass *klass)
{
//WHERE_AM_I;
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
//WHERE_AM_I;
	g_return_if_fail (GDA_IS_FIREBIRD_RECORDSET (recset));

	if (recset->priv) {
		//if (recset->priv->cnc) 
		//	g_object_unref (recset->priv->cnc);

		/* free specific information */
		//TO_IMPLEMENT;
		
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
		static GMutex registering;
		static const GTypeInfo info = {
			sizeof (GdaFirebirdRecordsetClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_firebird_recordset_class_init,
			NULL,
			NULL,
			sizeof (GdaFirebirdRecordset),
			0,
			(GInstanceInitFunc) gda_firebird_recordset_init,
			0
		};
		g_mutex_lock (&registering);
		if (type == 0) {
#ifdef FIREBIRD_EMBED
			type = g_type_register_static (GDA_TYPE_DATA_SELECT, "GdaFirebirdRecordsetEmbed", &info, 0);
#else
			type = g_type_register_static (GDA_TYPE_DATA_SELECT, "GdaFirebirdRecordset", &info, 0);
#endif
		}
		g_mutex_unlock (&registering);
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
void 
_fb_set_row_data (XSQLVAR *var, GValue *value, GdaRow *row, GType req_col_type){
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
		//if (req_col_type != G_TYPE_BOOLEAN)
		//	g_value_set_string (value, "NULL");
		//else
		//	g_value_set_boolean(value, FALSE);
	}
	else
	{
		switch (dtype)
		{
			case SQL_TEXT:
				sprintf (p, "%.*s", var->sqllen, var->sqldata);
				if (req_col_type != G_TYPE_BOOLEAN)
					g_value_set_string (value, p);
				else
					g_value_set_boolean(value, g_ascii_strcasecmp("1", p) == 0 ? TRUE : FALSE);
				break;
			case SQL_VARYING:
				vary2 = (VARY2*) var->sqldata;
				vary2->vary_string[vary2->vary_length] = '\0';
				g_value_set_string (value, (char *)vary2->vary_string);
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
				
				if (req_col_type != G_TYPE_BOOLEAN){
					switch (req_col_type) {
						case G_TYPE_INT:
							g_value_set_int (value, atoi (p));
							break;
						case G_TYPE_UINT:
							g_value_set_uint (value, atoi (p));
							break;
						case G_TYPE_INT64:
							g_value_set_int64(value, atoll(p));
							break;
						case G_TYPE_UINT64:
							g_value_set_uint64(value, atoll(p));
							break;
						case G_TYPE_LONG:
							g_value_set_long (value, atoll (p));
							break;
						case G_TYPE_ULONG:
							g_value_set_ulong (value, atoll (p));
							break;
						default:
							g_print("Oh flip....did not cater for this\n");
							g_value_set_int64 (value, atoll (p));
							break;
					}
					/*switch (dtype) {
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
					}*/
				}
				else
					g_value_set_boolean(value, atoll(p) == 1 ? TRUE : FALSE);
				}
				break;

			case SQL_FLOAT:
				g_value_set_float (value, *(float *) (var->sqldata));
				break;

			case SQL_DOUBLE:
				g_value_set_double (value, *(double *) (var->sqldata));
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
				sprintf(p, "%*s", 22, date_s);
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
				if (!gda_parse_iso8601_date (&date, date_s)) {
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
				if (!gda_parse_iso8601_time (&timegda, date_s)) {
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
				sprintf(blob_s, "%08x:%08x", (unsigned int)bid.gds_quad_high, (unsigned int)bid.gds_quad_low);
				sprintf(p, "%17s ", blob_s);
				g_value_set_string (value, p);
				break;

			default:
				g_value_set_string(value, "TYPE NOT FOUND");
				break;
		}
		
		//g_print("\t\t%s\n", p);
	}
}

static GdaRow *
new_row_from_firebird_stmt (GdaFirebirdRecordset *imodel, G_GNUC_UNUSED gint rownum, GType *col_types, GError **error)
{
	gint				i;
	gint 				fetch_stat;
	ISC_STATUS			status[20];
	GdaRow*				row = NULL;	
	GdaFirebirdPStmt*	ps = (GdaFirebirdPStmt *) ((GdaDataSelect *) imodel)->prep_stmt;
	//WHERE_AM_I;
	if ((fetch_stat = isc_dsql_fetch(status, &(ps->stmt_h), SQL_DIALECT_V6, ps->sqlda)) == 0) {
		row = gda_row_new (ps->sqlda->sqld);
		for (i = 0; i < ps->sqlda->sqld; i++) {
			GValue *value = gda_row_get_value (row, i);
			GType type = _gda_firebird_type_to_gda((XSQLVAR *) &(ps->sqlda->sqlvar[i]));

			//IF COLUMN TYPES IS NOT EXPLICITLY SPECIFIED THEN WE
			//SET THE COLUMN TYPES TO WHAT FIREBIRD RETURNS
			if (col_types)
				type = col_types[i];
			//g_print("col#: %d\n", i);
			gda_value_reset_with_type (value, type);
			_fb_set_row_data ((XSQLVAR *) &(ps->sqlda->sqlvar[i]), value, row, type);
		}
		rownum++;
	}
	
	return row;
}

/*
 * the @ps struct is modified and transferred to the new data model created in
 * this function
 */
GdaDataModel *
gda_firebird_recordset_new (GdaConnection            *cnc,
			    GdaFirebirdPStmt            *ps,
			    GdaSet                   *exec_params,
			    GdaDataModelAccessFlags   flags,
			    GType                    *col_types)

//(GdaConnection *cnc, GdaFirebirdPStmt *ps, GdaDataModelAccessFlags flags, GType *col_types)
{
	GdaFirebirdRecordset *model;
	FirebirdConnectionData *cdata;
	gint i;
	GdaDataModelAccessFlags rflags;
//WHERE_AM_I;
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (ps, NULL);

	cdata = (FirebirdConnectionData*) gda_connection_internal_get_provider_data_error (cnc, NULL);
	if (!cdata)
		return NULL;

	//g_print("RST-SQL>> %s\n", _GDA_PSTMT (ps)->sql);
	
	if (ps->sqlda == NULL){
		g_print("ERROR: ps->sqlda seems to be NULL\n");
	}
	/* make sure @ps reports the correct number of columns using the API*/
        //if (_GDA_PSTMT (ps)->ncols < 0)
                /*_GDA_PSTMT (ps)->ncols = ...;*/
		//TO_IMPLEMENT;

	/* completing @ps if not yet done */
	if (_GDA_PSTMT (ps)->ncols < 0)
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
		g_print("FB reported %d columns. Gda col-cnt: %d\n", ps->sqlda->sqld, GDA_PSTMT (ps)->ncols);
		for (i=0, list = _GDA_PSTMT (ps)->tmpl_columns; 
			i < GDA_PSTMT (ps)->ncols; 
			i++, list = list->next) {
			GdaColumn*		column;
			GType			gtype;
			XSQLVAR*		var;
			/*
			g_print("\t\tField:%d/%d (fb-cnt=d)\n\t\tSQL-NAME:%s\n\t\tREL-NAME: %s\n\t\tOWN-NAME: %s\n\t\tALIAS: %s\n**************************\n\n"
					, i
					, GDA_PSTMT (ps)->ncols
					//, ps->sqlda->sqld
					, var->sqlname
					, var->relname
					, var->ownname
					, var->aliasname);
			*/
			var = (XSQLVAR *) &(ps->sqlda->sqlvar[i]);
			column = GDA_COLUMN (list->data);
			/* use C API to set columns' information using gda_column_set_*() */
			gtype = _gda_firebird_type_to_gda(var);
			_GDA_PSTMT (ps)->types [i] = gtype;
			if (col_types)
				gda_column_set_g_type (column, col_types[i]);
			else
				gda_column_set_g_type (column, gtype);
				
			gda_column_set_name (column, var->aliasname);
			gda_column_set_description (column, var->aliasname);
		}
	}


	if (ps->input_sqlda != NULL){
		g_print("\n\nPRINTING THE INPUT PARAMETERS\n--------------------------\n");
		for(i =0; i < ps->input_sqlda->sqld; i++){
			g_print("input-paramater #%d: %s\n", i, (ps->input_sqlda->sqlvar[i].sqldata));
			g_print("input-len       #%d: %d\n", i, ps->input_sqlda->sqlvar[i].sqllen);
		}
	}
	
	//RUN ISC_DSQL_DESCRIBE TO GET OUTPUT FIELDS
	//isc_dsql_describe(cdata->status, cdata->ftr, &(ps->stmt_h), ps->sqlda);

	
	g_print("isc_dsql_execute\n");
	/* post init specific code */
	//if (isc_dsql_execute (cdata->status, cdata->ftr, &(ps->stmt_h), SQL_DIALECT_V6, NULL)) {
	if (isc_dsql_execute2 (cdata->status, cdata->ftr, &(ps->stmt_h), SQL_DIALECT_V6, ps->input_sqlda, NULL)) {
		g_print("\nisc error occured: \n");
		isc_print_status(cdata->status);
		g_print("\n");
	}

	isc_dsql_set_cursor_name(cdata->status, &(ps->stmt_h), "dyn_cursor", NULL);

	/* determine access mode: RANDOM or CURSOR FORWARD are the only supported */
	if (flags & GDA_DATA_MODEL_ACCESS_RANDOM){
		rflags = GDA_DATA_MODEL_ACCESS_RANDOM;
		g_print("\nRANDOM ACCESS\n");
	}
	else{
		rflags = GDA_DATA_MODEL_ACCESS_CURSOR_FORWARD;
		g_print("CURSOR FORWARD ACCESS\n");
	}
	/* create data model */
	g_print("Creating the data-model\n");
	model = g_object_new (GDA_TYPE_FIREBIRD_RECORDSET
			, "connection", cnc
			, "prepared-stmt", ps
			, "model-usage", rflags
			, "exec-params", exec_params
			, NULL);
	g_print("point to the connection\n");
	//model->priv->cnc = cnc;
	g_print("set the number of columns\n");
	//model->priv->n_columns	= ps->sqlda->sqld;
	g_print("add reference to connection\n");
	//g_object_ref (model->priv->cnc);
	gda_data_select_set_columns (GDA_DATA_SELECT (model), _GDA_PSTMT (ps)->tmpl_columns);

	gint rownum = 0;
	g_print("populate the model\n");
	GdaRow *row = NULL;
	while ((row = new_row_from_firebird_stmt (model, rownum, col_types, NULL)) != NULL) {
		gda_data_select_take_row ((GdaDataSelect*) model, row, rownum);
		rownum++;
	}
	
	isc_dsql_free_statement(cdata->status, &(ps->stmt_h), DSQL_close);
	
	g_print("SQL-ROWS >> %d\n", rownum);
	((GdaDataSelect *) model)->advertized_nrows = rownum;
	/*
	g_print("\n\ncreating a dump of the table\n\n");
	g_print("%s", gda_data_model_dump_as_string(GDA_DATA_MODEL (model)));
	g_print("\n\nreturning the data model\n\n");
	*/
	return GDA_DATA_MODEL (model);
}


/*
 * Get the number of rows in @model, if possible
 */
static gint
gda_firebird_recordset_fetch_nb_rows (GdaDataSelect *model)
{
	//GdaFirebirdRecordset *imodel;
	//WHERE_AM_I;
	//imodel = GDA_FIREBIRD_RECORDSET (model);
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
	gboolean				result = FALSE;
	GdaFirebirdRecordset*	imodel;
	//WHERE_AM_I;
	imodel = GDA_FIREBIRD_RECORDSET (model);

	if ((*prow = new_row_from_firebird_stmt (imodel, rownum, NULL, NULL)) != NULL) {
		result = TRUE;
		gda_data_select_take_row (model, *prow, rownum);
	}

	return result;
}

/*
 * Create and "give" filled #GdaRow object for all the rows in the model
 */
static gboolean
gda_firebird_recordset_store_all (GdaDataSelect *model, GError **error)
{
	//GdaFirebirdRecordset *imodel;
	gint i;
	//WHERE_AM_I;
	//imodel = GDA_FIREBIRD_RECORDSET (model);

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
	gboolean				result = FALSE;
	GdaFirebirdRecordset*	imodel;
	//WHERE_AM_I;
	imodel = GDA_FIREBIRD_RECORDSET (model);

	if ((*prow = new_row_from_firebird_stmt (imodel, rownum, NULL, NULL)) != NULL) {
		result = TRUE;
		gda_data_select_take_row (model, *prow, rownum);
	}

	return result;
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
	gboolean				result = FALSE;
	GdaFirebirdRecordset*	imodel;
	//WHERE_AM_I;
	imodel = GDA_FIREBIRD_RECORDSET (model);

	if ((*prow = new_row_from_firebird_stmt (imodel, rownum, NULL, NULL)) != NULL) {
		result = TRUE;
		gda_data_select_take_row (model, *prow, rownum);
	}

	return result;

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
	gboolean				result = FALSE;
	GdaFirebirdRecordset*	imodel;
	//WHERE_AM_I;
	imodel = GDA_FIREBIRD_RECORDSET (model);

	if ((*prow = new_row_from_firebird_stmt (imodel, rownum, NULL, NULL)) != NULL) {
		result = TRUE;
		gda_data_select_take_row (model, *prow, rownum);
	}

	return result;

}

