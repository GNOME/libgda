/* GDA FireBird Provider
 * Copyright (C) 1998 - 2005 The GNOME Foundation
 *
 * AUTHORS:
 *         Albi Jeronimo <jeronimoalbi@yahoo.com.ar>
 *         Rodrigo Moya <rodrigo@gnome-db.org>
 *         Vivien Malerba <malerba@gnome-db.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include "gda-firebird-recordset.h"
#include "gda-firebird-provider.h"
#include "gda-firebird-blob.h"
#include <libgda/gda-quark-list.h>
#include <libgda/gda-intl.h>
#include <glib/gprintf.h>
#include <string.h>
#include <math.h>
                                                                                                                             
#ifdef PARENT_TYPE
#undef PARENT_TYPE
#endif
                                                                                                                            
#define PARENT_TYPE GDA_TYPE_DATA_MODEL_BASE

struct _GdaFirebirdRecordsetPrivate {
	GdaConnection *cnc;
	GPtrArray *rows;
	gchar *table_name;
	gint ncolumns;
	gint nrows;
	gchar *sql_dialect;
	
	isc_stmt_handle sql_handle;
	XSQLDA *sql_result;
	ISC_STATUS fetch_stat;
	gboolean sql_prepared;
	gint fetched_rows;	
};

static void 			gda_firebird_recordset_class_init (GdaFirebirdRecordsetClass *klass);
static void 			gda_firebird_recordset_init (GdaFirebirdRecordset *recset,
							     GdaFirebirdRecordsetClass *klass);
static void 			gda_firebird_recordset_finalize (GObject *object);

static gint			gda_firebird_recordset_get_n_rows (GdaDataModelBase *model);
static gint			gda_firebird_recordset_get_n_columns (GdaDataModelBase *model);
static GdaColumn	*gda_firebird_recordset_describe_column (GdaDataModelBase *model,
									 gint col);
static const GdaRow 		*gda_firebird_recordset_get_row (GdaDataModelBase *model,
								 gint row);
static const GdaValue 		*gda_firebird_recordset_get_value_at (GdaDataModelBase *model,
								      gint col,
								      gint row);
static gboolean			gda_firebird_recordset_is_updatable (GdaDataModelBase *model);
static const GdaRow 		*gda_firebird_recordset_append_values (GdaDataModelBase *model,
								    const GList *values);
static gboolean			gda_firebird_recordset_remove_row (GdaDataModelBase *model,
								   const GdaRow *row);
static gboolean			gda_firebird_recordset_update_row (GdaDataModelBase *model,
								   const GdaRow *row);


static GObjectClass *parent_class = NULL;

/*******************************/
/* Firebird utility functions  */
/*******************************/

static GdaValueType
fb_sql_type_to_gda_type (const gshort field_type, gboolean has_decimals)
{
	switch (field_type && ~1) {
		case SQL_BLOB:
		case SQL_BLOB+1:
			return GDA_VALUE_TYPE_BLOB;
		case SQL_TIMESTAMP:
		case SQL_TIMESTAMP+1:
			return GDA_VALUE_TYPE_TIMESTAMP;
		case SQL_TYPE_TIME:
		case SQL_TYPE_TIME+1:
			return GDA_VALUE_TYPE_TIME;
		case SQL_TYPE_DATE:
		case SQL_TYPE_DATE+1:
			return GDA_VALUE_TYPE_DATE;
		case SQL_TEXT:
		case SQL_TEXT+1:
		case SQL_VARYING:
		case SQL_VARYING+1:
			return GDA_VALUE_TYPE_STRING;
		case SQL_SHORT:
		case SQL_SHORT+1:
			return GDA_VALUE_TYPE_SMALLINT;
		case SQL_LONG:
		case SQL_LONG+1:
			if (has_decimals)
				return GDA_VALUE_TYPE_NUMERIC;
			else
				return GDA_VALUE_TYPE_INTEGER;
				
		case SQL_INT64:
		case SQL_INT64+1:
			return GDA_VALUE_TYPE_NUMERIC;
		case SQL_FLOAT:
		case SQL_FLOAT+1:
			return GDA_VALUE_TYPE_SINGLE;
		case SQL_DOUBLE:
		case SQL_DOUBLE+1:
			return GDA_VALUE_TYPE_DOUBLE;
		default:
			return GDA_VALUE_TYPE_UNKNOWN;
	}
}


static void 
fb_sql_result_free (XSQLDA *sql_result)
{
	gint i;

	if (sql_result) {
		if (sql_result->sqlvar) {
			/* Free memory for each column */
			for (i = 0; i < sql_result->sqld; i++) {
				g_free (sql_result->sqlvar[i].sqldata);
				g_free (sql_result->sqlvar[i].sqlind);
			}
		}

		/* Free sql_result */
		g_free (sql_result);
		sql_result = NULL;
	}
}

static void 
fb_sql_result_columns_malloc (XSQLDA *sql_result)
{
	gint i;

	g_assert (sql_result != NULL);

	/* Alloc memory to hold values */
	for (i = 0; i < sql_result->sqld; i++) {
		switch (sql_result->sqlvar[i].sqltype & ~1) {
			case SQL_ARRAY:
	  		case SQL_ARRAY+1:
				break;
			case SQL_BLOB:
			case SQL_BLOB+1:
				sql_result->sqlvar[i].sqldata = (gchar *) g_malloc0 (sizeof (ISC_QUAD));
				break;
			case SQL_TIMESTAMP:
			case SQL_TIMESTAMP+1:
				sql_result->sqlvar[i].sqldata = (gchar *) g_malloc0 (sizeof (ISC_TIMESTAMP));
				break;
			case SQL_TYPE_TIME:
			case SQL_TYPE_TIME+1:
				sql_result->sqlvar[i].sqldata = (gchar *) g_malloc0 (sizeof (ISC_TIME));
				break;
			case SQL_TYPE_DATE:
			case SQL_TYPE_DATE+1:
				sql_result->sqlvar[i].sqldata = (gchar *) g_malloc0 (sizeof (ISC_DATE));
				break;
			case SQL_TEXT:
			case SQL_TEXT+1:
			case SQL_VARYING:
			case SQL_VARYING+1:
				sql_result->sqlvar[i].sqldata = (gchar *) g_malloc0 (sizeof (gchar) * 
											sql_result->sqlvar[i].sqllen);
				break;
			case SQL_SHORT:
			case SQL_SHORT+1:
				sql_result->sqlvar[i].sqldata = (gchar *) g_malloc0 (sizeof (ISC_USHORT));
				break;
			case SQL_LONG:
			case SQL_LONG+1:
				sql_result->sqlvar[i].sqldata = (gchar *) g_malloc0 (sizeof (gint));
				break;																		
			case SQL_INT64:
			case SQL_INT64+1:
				sql_result->sqlvar[i].sqldata = (gchar *) g_malloc0 (sizeof (ISC_INT64));
				break;
			case SQL_FLOAT:
			case SQL_FLOAT+1:
				sql_result->sqlvar[i].sqldata = (gchar *) g_malloc0 (sizeof (gfloat));
				break;
			case SQL_DOUBLE:
			case SQL_DOUBLE+1:
				sql_result->sqlvar[i].sqldata = (gchar *) g_malloc0 (sizeof (gdouble));
				break;
			default:
				break;
		}

		/* Allocate space to hold 'allow NULL mark' */
		sql_result->sqlvar[i].sqlind = (gshort *) g_malloc (sizeof (gshort));
	}                        
}

static void 
fb_sql_result_set_columns_number (GdaFirebirdConnection *fcnc,
				  GdaFirebirdRecordset *recset)
{
	gint cols_number;

	g_return_if_fail (GDA_IS_FIREBIRD_RECORDSET (recset));		
	g_assert (recset->priv->sql_result != NULL);

	/* Get information of retrieved columns */
	isc_dsql_describe (fcnc->status, &(recset->priv->sql_handle), recset->priv->sql_result->version, 
			   recset->priv->sql_result);
						
	cols_number = recset->priv->sql_result->sqld;

	/* if we have more than @sql_result->sqln columns */
	if (cols_number > recset->priv->sql_result->sqln) {

		/* Free result (record members are not initialized) */
		g_free (recset->priv->sql_result);

		/* Allocate memory for result, to hold @col_number columns */
		recset->priv->sql_result = (XSQLDA *) g_malloc0 (XSQLDA_LENGTH (cols_number));

		/* Set SQLDA version and columns number */
		recset->priv->sql_result->version = SQLDA_VERSION1;
		recset->priv->sql_result->sqln = cols_number;

		/* Get information of retrieved columns */
		isc_dsql_describe (fcnc->status, &(recset->priv->sql_handle), recset->priv->sql_result->version,
				   recset->priv->sql_result);
	}
	
	recset->priv->ncolumns = cols_number;
}


/*
 *  fb_sql_get_statement_type
 *
 *  Gets statement type
 *
 *  Returns: 0 if error or isc_info_sql_stmt_select, isc_info_sql_stmt_insert,
 *           isc_info_sql_stmt_update, isc_info_sql_stmt_delete, isc_info_sql_stmt_ddl,
 *           ...(see ibase.h for more)
 */
static gint
fb_sql_get_statement_type (GdaFirebirdConnection *fcnc,
			   GdaFirebirdRecordset *recset)
{
	int length;
	char type_item[] = { 
		isc_info_sql_stmt_type
	};
	char buffer[8];
                                                                                                                            
	g_return_val_if_fail (GDA_IS_FIREBIRD_RECORDSET (recset), -1);
	
	/* Get statement info */
	isc_dsql_sql_info (fcnc->status, &(recset->priv->sql_handle), sizeof (type_item), type_item, 
			   sizeof (buffer), buffer);

	/* If OK return statement type */
	if (buffer[0] == isc_info_sql_stmt_type) {
		length = isc_vax_integer (&(buffer[1]), 2);
		return isc_vax_integer (&(buffer[3]), length);
	}
	
	return 0;
}


/*
 *  fb_sql_prepare
 *
 *  Prepares an unprepared statement
 *
 *  Returns: TRUE if sql prepared successfully, or FALSE otherwise
 */
static gboolean 
fb_sql_prepare (GdaFirebirdConnection *fcnc,
		GdaFirebirdRecordset *recset,
		isc_tr_handle *ftr,
		const gchar * sql)
{
	GdaQuarkList *params;
	gchar *fb_sql_dialect;    

	g_return_val_if_fail (GDA_IS_FIREBIRD_RECORDSET (recset), FALSE);		
	g_return_val_if_fail ((ftr != NULL), FALSE);		

	if (ftr) {
		if (! recset->priv->sql_prepared) {
		
			/* Allocate memory for result to hold 10 columns */
			recset->priv->sql_result = (XSQLDA *) g_malloc0 (XSQLDA_LENGTH (13));
			
			/* Set SQLDA version and columns number */
			recset->priv->sql_result->version = SQLDA_VERSION1;
			recset->priv->sql_result->sqln = 13;
			
			/* Allocate statement */
			recset->priv->sql_handle = NULL;
			if (! isc_dsql_allocate_statement (fcnc->status, &(fcnc->handle), &(recset->priv->sql_handle))) {
				/* Get SQL Dialect */
				params = gda_quark_list_new_from_string (
						gda_connection_get_cnc_string ((GdaConnection *) recset->priv->cnc));
				fb_sql_dialect = (gchar *) gda_quark_list_find (params, "SQL_DIALECT");		
				if (!fb_sql_dialect)
					fb_sql_dialect = "3";
				
				gda_quark_list_free (params);
				
				/* Prepare statement */
				if (! isc_dsql_prepare (fcnc->status, ftr, &(recset->priv->sql_handle), 0, (gchar *) sql, 
							(gushort) atoi (fb_sql_dialect), recset->priv->sql_result)) {
					fb_sql_result_set_columns_number (fcnc, recset);
					fb_sql_result_columns_malloc (recset->priv->sql_result);
					recset->priv->sql_prepared = TRUE;
					
					return TRUE;
				} 
				else
					gda_firebird_connection_make_error (recset->priv->cnc, 0);

			}
			else
				gda_firebird_connection_make_error (recset->priv->cnc, 0);

		} 
		else
			gda_connection_add_error_string (recset->priv->cnc, _("Statement already prepared."));

	}
	else
		gda_connection_add_error_string (recset->priv->cnc, _("Transaction not initialized."));
	
	/* Free cursors */
	isc_dsql_free_statement (fcnc->status, &(recset->priv->sql_handle), DSQL_drop);

	/* Free SQL result */
	fb_sql_result_free (recset->priv->sql_result);

	return FALSE;
}


/*
 *  fb_sql_unprepare
 *
 *  Unprepares a prepared statement
 *
 *  Returns: TRUE if sql unprepared successfully, or FALSE otherwise
 */
static gboolean 
fb_sql_unprepare (GdaFirebirdConnection *fcnc, 
		  GdaFirebirdRecordset *recset)
{
	g_return_val_if_fail (GDA_IS_FIREBIRD_RECORDSET (recset), FALSE);		
	
	if (recset->priv->sql_prepared) {

		/* Free SQL result */
		fb_sql_result_free (recset->priv->sql_result);

		/* Free cursors */
		if (recset->priv->sql_handle)
			isc_dsql_free_statement (fcnc->status, &(recset->priv->sql_handle), DSQL_drop);
		
		return TRUE;
	}
	else
		gda_connection_add_error_string (recset->priv->cnc, _("Statement already prepared."));
	
	return FALSE;
}


/*
 *  fb_sql_execute
 *
 *  Call #isc_dsql_execute to execute statement
 *
 *  Returns: TRUE if sql executed successfully, or FALSE otherwise
 */
static gboolean
fb_sql_execute (GdaFirebirdConnection *fcnc,
		GdaFirebirdRecordset *recset,
		isc_tr_handle *ftr,
		const gchar *sql)
{
	g_return_val_if_fail (GDA_IS_FIREBIRD_RECORDSET (recset), FALSE);		
	g_return_val_if_fail ((ftr != NULL), FALSE);		

	/* Return statement execute status */
	if (fb_sql_get_statement_type (fcnc, recset) == isc_info_sql_stmt_select)
		return (! isc_dsql_execute (fcnc->status, ftr, &(recset->priv->sql_handle), 
					    recset->priv->sql_result->version, recset->priv->sql_result));
	else
		return (! isc_dsql_execute_immediate (fcnc->status, &(fcnc->handle), ftr, strlen (sql), 
						      (gchar *) sql, atoi (recset->priv->sql_dialect), NULL));
}


/*
 *  fb_gda_value_fill
 *
 *  Fill a @gda_value with the value of @value
 *
 *  @type should contain Firebirds SQL datatype of @value
 */
static void
fb_gda_value_fill (GdaValue *gda_value,
		   GdaFirebirdRecordset *recset,
		   const gshort field_type,
		   gint field_number,
		   unsigned long length)
{
	GdaTime a_time;
	GdaDate a_date;
	GdaTimestamp a_times;
	GdaBlob *blob = NULL;
	struct tm *a_date_time;
	gpointer tmp_big, field_data;
	struct vary *var_text;
	gdouble i64_n;
	XSQLDA *sql_result;
	
	g_return_if_fail (GDA_IS_FIREBIRD_RECORDSET (recset));

	sql_result = recset->priv->sql_result;
	field_data = (gchar *) sql_result->sqlvar[field_number].sqldata;
	
	/* If value is null  */
	if (*(sql_result->sqlvar[field_number].sqlind) == -1) {
		gda_value_set_null (gda_value);
		return;
	}	
	
	switch (field_type) {
		case SQL_ARRAY:
		case SQL_ARRAY+1:
			g_print (_("Firebird's ARRAY datatype not supported yet!\n"));
			gda_value_set_null (gda_value);		
			break;
		case SQL_BLOB:
		case SQL_BLOB+1:
			blob = gda_firebird_blob_new (recset->priv->cnc);
			gda_firebird_blob_set_id (blob, (const ISC_QUAD *) field_data);
			gda_value_set_blob (gda_value, (const GdaBlob *) blob);
			break;
		case SQL_TIMESTAMP:
		case SQL_TIMESTAMP+1:
			a_date_time = g_malloc0 (sizeof (struct tm));
			isc_decode_timestamp ((ISC_TIMESTAMP *) field_data, a_date_time);
			a_date_time->tm_mon ++;
			a_date_time->tm_year += 1900;
			a_times.hour = a_date_time->tm_hour;
			a_times.minute = a_date_time->tm_min;
			a_times.second = a_date_time->tm_sec;
			a_times.year = a_date_time->tm_year;
			a_times.month = a_date_time->tm_mon;
			a_times.day = a_date_time->tm_mday;
			a_times.timezone = 0; /* FIXME: timezone */
			a_times.fraction = 0; /* FIXME: fraction */			

			gda_value_set_timestamp (gda_value, (const GdaTimestamp *) &a_times);
			g_free (a_date_time);
			break;
		case SQL_TYPE_TIME:
		case SQL_TYPE_TIME+1:
			a_date_time = g_malloc0 (sizeof (struct tm));
			isc_decode_sql_time ((ISC_TIME *) field_data, a_date_time);
			a_time.hour = a_date_time->tm_hour;
			a_time.minute = a_date_time->tm_min;
			a_time.second = a_date_time->tm_sec;
			a_time.timezone = 0; /* FIXME: timezone */

			gda_value_set_time (gda_value, (const GdaTime *) &a_time);
			g_free (a_date_time);
			break;
		case SQL_TYPE_DATE:
		case SQL_TYPE_DATE+1:
			a_date_time = g_malloc0 (sizeof (struct tm));
			isc_decode_sql_date ((ISC_DATE *) field_data, a_date_time);
			a_date_time->tm_mon++;
			a_date_time->tm_year += 1900;
			a_date.year = a_date_time->tm_year;
			a_date.month = a_date_time->tm_mon;
			a_date.day = a_date_time->tm_mday;
		
			gda_value_set_date (gda_value, (const GdaDate *) &a_date);
			g_free (a_date_time);			
			break;
		case SQL_TEXT:
		case SQL_TEXT+1:
			gda_value_set_string (gda_value, (const gchar *) field_data);
			break;
		case SQL_VARYING:
		case SQL_VARYING+1:
			var_text = (struct vary *) field_data;
			var_text->vary_string[var_text->vary_length] = '\0';

			gda_value_set_string (gda_value, (const gchar *) var_text->vary_string);
			break;
		case SQL_SHORT:
		case SQL_SHORT+1:
			gda_value_set_smallint (gda_value, *((gshort *) field_data));
			break;
		case SQL_LONG:
		case SQL_LONG+1:
			/* If number has decimals then is a numeric type */
			if (sql_result->sqlvar[field_number].sqlscale < 0) {
				tmp_big = g_malloc0 (sizeof (gdouble));
				i64_n = (gdouble) (*((glong *) field_data) / 
						   pow (10, (sql_result->sqlvar[field_number].sqlscale * -1)));

				g_sprintf (tmp_big, "%f", i64_n);
				gda_value_set_from_string (gda_value, tmp_big, GDA_VALUE_TYPE_NUMERIC);
				g_free (tmp_big);
				break;
			}
			else {	/* Normal long type number */
				gda_value_set_integer (gda_value, *((gint *) field_data));
				break;
			}
			
		case SQL_INT64:
		case SQL_INT64+1:
			if (sql_result->sqlvar[field_number].sqlscale < 0) {
				tmp_big = g_malloc (sizeof (gdouble));
				i64_n = (gdouble) (*((ISC_INT64 *) field_data) / 
						   pow (10, (sql_result->sqlvar[field_number].sqlscale * -1)));

				tmp_big = g_strdup_printf ("%f", i64_n);
			}
			else
				tmp_big = g_memdup (field_data, sizeof (gdouble));

			gda_value_set_from_string (gda_value, (const gchar *) tmp_big, GDA_VALUE_TYPE_NUMERIC);
			g_free (tmp_big);
			break;
		case SQL_FLOAT:
		case SQL_FLOAT+1:
			gda_value_set_single (gda_value, *((gfloat *) field_data));
			break;
		case SQL_DOUBLE:
		case SQL_DOUBLE+1:
			gda_value_set_double (gda_value, *((gdouble *) field_data));
			break;
		default:
			g_print (_("Unknown Firebird's field value!\n"));

			gda_value_set_null (gda_value);
			break;
	};
}


/*
 *
 *  fb_sql_fetch_row
 *
 *  Fetch a single row
 *
 *  Returns: TRUE if fetched successfully, or FALSE otherwise
 */
static gboolean
fb_sql_fetch_row (GdaFirebirdConnection *fcnc,
		  GdaFirebirdRecordset *recset)
{
	GdaRow *row;
	gint col_n;
	gshort field_type;

	g_return_val_if_fail (GDA_IS_FIREBIRD_RECORDSET (recset), FALSE);

	/* Fetch a single row */
	recset->priv->fetch_stat = isc_dsql_fetch (fcnc->status, &(recset->priv->sql_handle), 
						   recset->priv->sql_result->version, recset->priv->sql_result);
	/* If we're not at end of file */
	if (! (recset->priv->fetch_stat == 100L)) {

		/* ... and fetch was successful */
		if (recset->priv->fetch_stat == 0) {
			recset->priv->fetched_rows++;

			/* Create a row for the fetched data */
			row = gda_row_new (GDA_DATA_MODEL (recset), recset->priv->ncolumns);

			/* Fill each GdaValue in @row with fetched values */
			for (col_n = 0; col_n < recset->priv->ncolumns; col_n++) {

				/* Get current field type from statement XSQLDA's variable */
				field_type = (((gshort) recset->priv->sql_result->sqlvar[col_n].sqltype) & ~1);

				/* Set row's field value */
				fb_gda_value_fill ((GdaValue *) gda_row_get_value (row, col_n),	recset,
						   (const gint) field_type, col_n, 0);
			}

			/* Add value to rows array */
			g_ptr_array_add (recset->priv->rows, row);
			
			return TRUE;
		}
		else /* Inform error */
			gda_firebird_connection_make_error (recset->priv->cnc, 0);
	}

	return FALSE;
}


/**********************************************/
/* GdaFirebirdRecordset class implementation  */
/**********************************************/

static gint
gda_firebird_recordset_get_n_rows (GdaDataModelBase *model)
{
	GdaFirebirdRecordset *recset = (GdaFirebirdRecordset *) model;
	
	g_return_val_if_fail (GDA_IS_FIREBIRD_RECORDSET (recset), -1);
	
	return (recset->priv->nrows);
}
                                                                                                                            
static gint
gda_firebird_recordset_get_n_columns (GdaDataModelBase *model)
{
	GdaFirebirdRecordset *recset = (GdaFirebirdRecordset *) model;
	
	g_return_val_if_fail (GDA_IS_FIREBIRD_RECORDSET (recset), -1);
	
	return (recset->priv->ncolumns);
}

static GdaColumn *
gda_firebird_recordset_describe_column (GdaDataModelBase *model, 
					gint col)
{
	GdaColumn *fa;
	GdaFirebirdRecordset *recset = (GdaFirebirdRecordset *) model;

	g_return_val_if_fail (GDA_IS_FIREBIRD_RECORDSET (recset), NULL);

	if (col >= recset->priv->ncolumns)
		return NULL;

	fa = gda_column_new ();
	gda_column_set_name (fa, (const gchar *) recset->priv->sql_result->sqlvar[col].sqlname);
	gda_column_set_defined_size (fa, (glong) recset->priv->sql_result->sqlvar[col].sqllen);
	gda_column_set_table (fa, (const gchar *) recset->priv->sql_result->sqlvar[col].relname);
	gda_column_set_scale (fa, (glong) (recset->priv->sql_result->sqlvar[col].sqlscale * -1));
	gda_column_set_gdatype (fa, fb_sql_type_to_gda_type (
							recset->priv->sql_result->sqlvar[col].sqltype,
							(recset->priv->sql_result->sqlvar[col].sqlscale < 0)));
	gda_column_set_position (fa, col);
	
	/* FIXME */
	gda_column_set_allow_null (fa, TRUE);
	gda_column_set_primary_key (fa, FALSE);
	gda_column_set_unique_key (fa, FALSE);
	
	return fa;
}

static const GdaRow *
gda_firebird_recordset_get_row (GdaDataModelBase *model, 
				gint row)
{
	GdaRow *fields = NULL;
	GdaFirebirdConnection *fcnc;
	GdaFirebirdRecordset *recset = (GdaFirebirdRecordset *) model;

	g_return_val_if_fail (GDA_IS_FIREBIRD_RECORDSET (recset), NULL);

	fcnc = g_object_get_data (G_OBJECT (recset->priv->cnc), CONNECTION_DATA);
	if (!fcnc) {
		gda_connection_add_error_string (recset->priv->cnc, _("Invalid Firebird handle"));
		return NULL;
	}

	/* If row doesn't exists */
	if (row >= recset->priv->nrows)
		return NULL;
		
	/* If row was already fetched */
	if (row < recset->priv->fetched_rows)
		return (const GdaRow *) g_ptr_array_index (recset->priv->rows, row);

	/* Fetch if statement is a SELECT */
	if (fb_sql_get_statement_type (fcnc, recset) == isc_info_sql_stmt_select) {

		/* Disable DataModel's change notifications */
		gda_data_model_freeze (GDA_DATA_MODEL (recset));

		while (fb_sql_fetch_row (fcnc, recset) && (row > recset->priv->fetched_rows))
			recset->priv->nrows++;

		/* If row was fetched */
		if (row == recset->priv->fetched_rows)
			return (const GdaRow *) g_ptr_array_index (recset->priv->rows, row);

		/* Enable notificarions again */
		gda_data_model_thaw (GDA_DATA_MODEL (recset));
	}
	
	return (const GdaRow *) fields;		
}

static const GdaValue *
gda_firebird_recordset_get_value_at (GdaDataModelBase *model, 
				     gint col,
				     gint row)
{
	gint n_cols;
	const GdaRow *fields;
	GdaFirebirdRecordset *recset = (GdaFirebirdRecordset *) model;

	g_return_val_if_fail (GDA_IS_FIREBIRD_RECORDSET (recset), NULL);
                                                                                                                            
	n_cols = recset->priv->ncolumns;
	if (col >= n_cols)
		return NULL;

	fields = gda_firebird_recordset_get_row (model, row);
	
	return fields != NULL ? gda_row_get_value ((GdaRow *) fields, col) : NULL;
}

static gboolean
gda_firebird_recordset_is_updatable (GdaDataModelBase *model)
{
	return FALSE;
}
                                                                                                                            
static const GdaRow *
gda_firebird_recordset_append_values (GdaDataModelBase *model,
				   const GList *values)
{
	return NULL;
}

static gboolean
gda_firebird_recordset_remove_row (GdaDataModelBase *model,
				   const GdaRow *row)
{
	return FALSE;
}
                                                                                                                            
static gboolean
gda_firebird_recordset_update_row (GdaDataModelBase *model,
				   const GdaRow *row)
{
	return FALSE;
}


static void
gda_firebird_recordset_init (GdaFirebirdRecordset *recset,
			     GdaFirebirdRecordsetClass *klass)
{
	g_return_if_fail (GDA_IS_FIREBIRD_RECORDSET (recset));

	/* Init private structure and properties */                                                                                  
	recset->priv = g_new0 (GdaFirebirdRecordsetPrivate, 1);
	recset->priv->rows = g_ptr_array_new ();
	recset->priv->cnc = NULL;
	recset->priv->sql_prepared = FALSE;
	recset->priv->sql_result = NULL;
	recset->priv->sql_handle = NULL;    
	recset->priv->ncolumns = 0;
	recset->priv->nrows = 0;
	recset->priv->sql_dialect = "3";
	recset->priv->fetched_rows = 0;	
}			


static void
gda_firebird_recordset_finalize (GObject *object)
{
	GdaFirebirdConnection *fcnc;
	GdaRow *row;
	GdaFirebirdRecordset *recset = (GdaFirebirdRecordset *) object;

	g_return_if_fail (GDA_IS_FIREBIRD_RECORDSET (recset));

	fcnc = g_object_get_data (G_OBJECT (recset->priv->cnc), CONNECTION_DATA);		

	/* Free Firebirds sql allocated space */
	fb_sql_result_free (recset->priv->sql_result);
	fb_sql_unprepare (fcnc, recset);

	/* Free all rows */
	while (recset->priv->rows->len > 0) {

		/* Get row at zero position */
		row = (GdaRow *) g_ptr_array_index (recset->priv->rows, 0);

		/* if it exists then free it */
		if (row != NULL)
			gda_row_free (row);

		/* Move down all existing rows */
		g_ptr_array_remove_index (recset->priv->rows, 0);
	}

	/* Free rows pointer object */
	g_ptr_array_free (recset->priv->rows, TRUE);
	recset->priv->rows = NULL;

	/* Free private properties */
	g_free (recset->priv);
	recset->priv = NULL;

	/* Finally call parents finalize method */
	parent_class->finalize (object);
}	


static void
gda_firebird_recordset_class_init (GdaFirebirdRecordsetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaDataModelBaseClass *model_class = GDA_DATA_MODEL_BASE_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_firebird_recordset_finalize;
	model_class->get_n_rows = gda_firebird_recordset_get_n_rows;
	model_class->get_n_columns = gda_firebird_recordset_get_n_columns;
	model_class->describe_column = gda_firebird_recordset_describe_column;
	model_class->get_row = gda_firebird_recordset_get_row;
	model_class->get_value_at = gda_firebird_recordset_get_value_at;
	model_class->is_updatable = gda_firebird_recordset_is_updatable;
	model_class->append_values = gda_firebird_recordset_append_values;
	model_class->remove_row = gda_firebird_recordset_remove_row;
	model_class->update_row = gda_firebird_recordset_update_row;
}


GType
gda_firebird_recordset_get_type (void)
{
	static GType type = 0;

	if (!type) {
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
		
		type = g_type_register_static (PARENT_TYPE, "GdaFirebirdRecordset", &info, 0);
	}

	return type;
}


GdaFirebirdRecordset *
gda_firebird_recordset_new (GdaConnection *cnc, 
			    isc_tr_handle *ftr,
			    const gchar *sql)
{
	GdaFirebirdRecordset *recset;
	GdaFirebirdConnection *fcnc;
	gint field_n;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_assert (ftr != NULL);

	fcnc = g_object_get_data (G_OBJECT (cnc), CONNECTION_DATA);
	if (!fcnc) {
		gda_connection_add_error_string (cnc, _("Invalid Firebird handle"));
		return NULL;
	}

	/* Create Firebird Recordset object */
	recset = g_object_new (GDA_TYPE_FIREBIRD_RECORDSET, NULL);
	
	/* Save conection */
	recset->priv->cnc = cnc;
	
	/* Prepare statement */
	if (fb_sql_prepare (fcnc, recset, ftr, sql)) {

		/* ... and then execute it */
		if (!fb_sql_execute (fcnc, recset, ftr, sql)) {
			gda_firebird_connection_make_error (cnc, fb_sql_get_statement_type (fcnc, recset));

			/* Free Firebirds sql allocated space */
			fb_sql_result_free (recset->priv->sql_result);
			fb_sql_unprepare (fcnc, recset);

			/* Release recordset */
			g_free (recset);
			
			return NULL;
		}

		/* If statement is not a select release recordset */
		if (fb_sql_get_statement_type (fcnc, recset) != isc_info_sql_stmt_select) {
			g_free (recset);
			return NULL;
		}

		/* Fetch all rows */
		while (fb_sql_fetch_row (fcnc, recset))
			recset->priv->nrows++;

		/* Set column names */
		for (field_n = 0; field_n < recset->priv->ncolumns; field_n++)
			gda_data_model_set_column_title (GDA_DATA_MODEL(recset), field_n, 
							 recset->priv->sql_result->sqlvar[field_n].sqlname);

	}
	else {	/* Release recordset if statement couldn't be prepared */
		g_free (recset);
		recset = NULL;
	}

	return recset;
}					

