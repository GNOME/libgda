/* GDA Oracle provider
 * Copyright (C) 2002 - 2009 The GNOME Foundation.
 *
 * AUTHORS:
 * 	Tim Coleman <tim@timcoleman.com>
 *      Vivien Malerba <malerba@gnome-db.org>
 *
 * Borrowed from Mysql utils.c, written by:
 *      Michael Lausch <michael@lausch.at>
 *	Rodrigo Moya <rodrigo@gnome-db.org>
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

#include <glib/gi18n-lib.h>
#include <stdlib.h>
#include <string.h>
#include "gda-oracle.h"
#include "gda-oracle-util.h"
#include "gda-oracle-blob-op.h"

/* 
 * _gda_oracle_make_error
 * This function uses OCIErrorGet to get a description of the error to
 * display.  The parameters are one of an OCIError or a OCIEnv handle
 * depending on the type, which is either OCI_HTYPE_ERROR or OCI_HTYPE_ENV.
 * Read the documentation on OCIErrorGet to understand why this is
 *
 * If the handle is NULL, which is an unhappy situation, it will set the
 * error description to "NO DESCRIPTION".
 *
 * The error number is set to the Oracle error code.
 */
GdaConnectionEvent *
_gda_oracle_make_error (dvoid *hndlp, ub4 type, const gchar *file, gint line)
{
	GdaConnectionEvent *error;
	gchar errbuf[512];
	ub4 errcode;
	gchar *source;

	error = gda_connection_event_new (GDA_CONNECTION_EVENT_ERROR);

	if (hndlp != NULL) {
		OCIErrorGet ((dvoid *) hndlp,
			     (ub4) 1, 
			     (text *) NULL, 
			     &errcode, 
			     errbuf, 
			     (ub4) sizeof (errbuf), 
			     (ub4) type);
	
		gda_connection_event_set_description (error, errbuf);
		/*g_warning ("Oracle error:%s", errbuf);*/
	} 
	else 
		gda_connection_event_set_description (error, _("NO DESCRIPTION"));
	
	gda_connection_event_set_code (error, errcode);
	source = g_strdup_printf ("gda-oracle:%s:%d", file, line);
	gda_connection_event_set_source (error, source);
	g_free(source);
	
	return error;
}

/* 
 * _gda_oracle_handle_error
 * This function is used for checking the result of an OCI
 * call.  The result itself is the return value, and we
 * need the GdaConnection and OracleConnectionData to do
 * our thing.  The type is OCI_HTYPE_ENV or OCI_HTYPE_ERROR
 * as described for OCIErrorGet.
 *
 * The return value is true if there is no error, or false otherwise
 *
 * If the return value is OCI_SUCCESS or OCI_SUCCESS_WITH_INFO,
 * we return TRUE.  Otherwise, if it is OCI_ERROR, try to get the
 * Oracle error message using _gda_oracle_make_error.  Otherwise,
 * make an error using the given message.
 */
GdaConnectionEvent * 
_gda_oracle_handle_error (gint result, GdaConnection *cnc, 
			  OracleConnectionData *cdata,
			  ub4 type, const gchar *msg,
			  const gchar *file, gint line)
{
	GdaConnectionEvent *error = NULL;
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	switch (result) {
	case OCI_SUCCESS:
	case OCI_SUCCESS_WITH_INFO:
		return NULL;
	case OCI_ERROR:
		switch(type) {
		case OCI_HTYPE_ERROR:
			error = _gda_oracle_make_error (cdata->herr, type, file, line);
			gda_connection_add_event (cnc, error);
			break;
		case OCI_HTYPE_ENV:
			error = _gda_oracle_make_error (cdata->henv, type, file, line);
			gda_connection_add_event (cnc, error);
			break;
		default:
			error = gda_connection_add_event_string (cnc, msg);
			gda_connection_add_event (cnc, error);
			break;
		}
		break;
	case OCI_NO_DATA:
		g_warning ("Internal implementation error: OCI_NO_DATA not handled!\n");
		break;
	case OCI_INVALID_HANDLE:
		g_warning ("Internal error: Invalid handle!");
	default:
		error = gda_connection_add_event_string (cnc, msg);
	}

	if (error)
		g_print ("HANDLED error: %s\n", gda_connection_event_get_description (error));
	return error;
}

GType 
_oracle_sqltype_to_g_type (const ub2 sqltype)
{
	/* an incomplete list of all the oracle types */
	switch (sqltype) {
	case SQLT_CHR:
	case SQLT_STR:
	case SQLT_VCS:
	case SQLT_RID:
	case SQLT_LNG:
	case SQLT_LVC:
	case SQLT_AFC:
	case SQLT_AVC:
	case SQLT_LAB:
	case SQLT_VST:
		return G_TYPE_STRING;
	case SQLT_NUM:
		return GDA_TYPE_NUMERIC;
	case SQLT_INT:
		return G_TYPE_INT;
	case SQLT_UIN:
		return G_TYPE_UINT;
	case SQLT_FLT:
		return G_TYPE_FLOAT;
	case SQLT_VBI:
	case SQLT_BIN:
	case SQLT_LBI:
	case SQLT_LVB:
	case SQLT_BLOB:
	case SQLT_BFILEE:
	case SQLT_CFILEE:
	case SQLT_CLOB:
		return GDA_TYPE_BLOB;
	case SQLT_DAT:
	case SQLT_ODT:
	case SQLT_DATE:
		return G_TYPE_DATE;
	case SQLT_TIME:
	case SQLT_TIME_TZ:
		return GDA_TYPE_TIME;
	case SQLT_TIMESTAMP:
	case SQLT_TIMESTAMP_TZ:
	case SQLT_TIMESTAMP_LTZ:
		return GDA_TYPE_TIMESTAMP;
	case SQLT_SLS:
	case SQLT_CUR:
	case SQLT_RDD:
	case SQLT_OSL:
	case SQLT_NTY:
	case SQLT_REF:
	case SQLT_RSET:
	case SQLT_NCO:
	case SQLT_INTERVAL_YM:
	case SQLT_INTERVAL_DS:
	case SQLT_VNU:
	case SQLT_PDN:
	case SQLT_NON:
	default:
		return G_TYPE_INVALID;
	}
}

gchar * 
_oracle_sqltype_to_string (const ub2 sqltype)
{
	/* an incomplete list of all the oracle types */
	switch (sqltype) {
	case SQLT_CHR:
		return "VARCHAR2";
	case SQLT_NUM:
		return "NUMBER";
	case SQLT_INT:
		return "INTEGER";
	case SQLT_FLT:
		return "FLOAT";
	case SQLT_STR:
		return "STRING";
	case SQLT_VNU:
		return "VARNUM";
	case SQLT_PDN:
		return "";
	case SQLT_LNG:
		return "LONG";
	case SQLT_VCS:
		return "VARCHAR";
	case SQLT_NON:
		return "";
	case SQLT_RID:
		return "ROWID";
	case SQLT_DAT:
		return "DATE";
	case SQLT_VBI:
		return "VARRAW";
	case SQLT_BIN:
		return "RAW";
	case SQLT_LBI:
		return "LONG RAW";
	case SQLT_UIN:
		return "UNSIGNED INT";
	case SQLT_SLS:
		return "";
	case SQLT_LVC:
		return "LONG VARCHAR";
	case SQLT_LVB:
		return "LONG VARRAW";
	case SQLT_AFC:
		return "CHAR";
	case SQLT_AVC:
		return "CHARZ";
	case SQLT_CUR:
		return "CURSOR";
	case SQLT_RDD:
		return "ROWID";
	case SQLT_LAB:
		return "LABEL";
	case SQLT_OSL:
		return "OSLABEL";
	case SQLT_NTY:
		return "";
	case SQLT_REF:
		return "";
	case SQLT_CLOB:
		return "CLOB";
	case SQLT_BLOB:
		return "BLOB";
	case SQLT_BFILEE:
		return "BFILE";
	case SQLT_CFILEE:
		return "CFILE";
	case SQLT_RSET:
		return "RESULT SET";
	case SQLT_NCO:
		return "";
	case SQLT_VST:
		return "";
	case SQLT_ODT:
		return "OCI DATE";
	case SQLT_DATE:
		return "ANSI DATE";
	case SQLT_TIME:
		return "TIME";
	case SQLT_TIME_TZ:
		return "TIME WITH TIME ZONE";
	case SQLT_TIMESTAMP:
		return "TIMESTAMP";
	case SQLT_TIMESTAMP_TZ:
		return "TIMESTAMP WITH TIME ZONE";
	case SQLT_INTERVAL_YM:
		return "INTERVAL YEAR TO MONTH";
	case SQLT_INTERVAL_DS:
		return "INTERVAL DAY TO SECOND";
	case SQLT_TIMESTAMP_LTZ:
		return "TIMESTAMP WITH LOCAL TIME ZONE";
	default:
		return "UNKNOWN";
	}
}

gchar *
_gda_oracle_value_to_sql_string (GValue *value)
{
	gchar *val_str;
	gchar *ret;
	GType type;

	g_return_val_if_fail (value != NULL, NULL);

	val_str = gda_value_stringify (value);
	if (!val_str)
		return NULL;

	type = G_VALUE_TYPE (value);
	if ((type == G_TYPE_INT64) ||
	    (type == G_TYPE_DOUBLE) ||
	    (type == G_TYPE_INT) ||
	    (type == GDA_TYPE_NUMERIC) ||
	    (type == G_TYPE_FLOAT) ||
	    (type == GDA_TYPE_SHORT) ||
	    (type == G_TYPE_CHAR))
		ret = val_str;
	else {
		ret = g_strdup_printf ("\"%s\"", val_str);
		g_free(val_str);
	}
	
	return ret;
}

GdaOracleValue *
_gda_value_to_oracle_value (const GValue *value)
{
	GdaOracleValue *ora_value;
	OCIDate *oci_date;
	GType type;

	ora_value = g_new0 (GdaOracleValue, 1);

	ora_value->g_type = G_VALUE_TYPE (value);
	ora_value->indicator = 0;
	ora_value->hdef = (OCIDefine *) 0;
	ora_value->pard = (OCIParam *) 0;
	type = ora_value->g_type;

	if (type == GDA_TYPE_NULL)
		ora_value->indicator = -1;
	else if ((type == G_TYPE_INT64) ||
		 (type == G_TYPE_DOUBLE) ||
		 (type == G_TYPE_INT) ||
		 (type == GDA_TYPE_NUMERIC) ||
		 (type == G_TYPE_FLOAT) ||
		 (type == GDA_TYPE_SHORT) ||
		 (type == G_TYPE_CHAR)) {
		gchar *val_str;
		val_str = gda_value_stringify ((GValue *) value);
		if (!val_str)
			return NULL;
		
		ora_value->sql_type = SQLT_NUM;
		ora_value->value = (void *) val_str;
		ora_value->defined_size = strlen (val_str);
	}
	else if (type == G_TYPE_DATE) {
		GDate *gda_date;
		ora_value->sql_type = SQLT_ODT;
		gda_date = (GDate*) g_value_get_boxed (value);
		oci_date = g_new0(OCIDate, 1);
		OCIDateSetDate(oci_date, gda_date->year, gda_date->month, gda_date->day);
		ora_value->defined_size = sizeof (OCIDate);
		ora_value->value = oci_date;
	}
	else if (type == GDA_TYPE_TIME) {
		GdaTime *gda_time;
		ora_value->sql_type = SQLT_ODT;
		gda_time = (GdaTime *) gda_value_get_time ((GValue *) value);
		oci_date = g_new0(OCIDate, 1);
		OCIDateSetTime(oci_date, gda_time->hour, gda_time->minute, gda_time->second);
		ora_value->defined_size = sizeof (OCIDate);
		ora_value->value = oci_date;
	}
	else if (type == GDA_TYPE_TIMESTAMP) {
		GdaTimestamp *gda_timestamp;
		ora_value->sql_type = SQLT_ODT;
		gda_timestamp = (GdaTimestamp *) gda_value_get_timestamp ((GValue *) value);
		oci_date = g_new0(OCIDate, 1);
		OCIDateSetDate(oci_date, gda_timestamp->year, gda_timestamp->month, gda_timestamp->day);
		OCIDateSetTime(oci_date, gda_timestamp->hour, gda_timestamp->minute, gda_timestamp->second);
		ora_value->defined_size = sizeof (OCIDate);
		ora_value->value = oci_date;
	}
	else if (type == GDA_TYPE_BLOB) {
		GdaBinary *bin;

		bin = (GdaBinary *) gda_value_get_blob ((GValue *) value);
		if (bin) {
			ora_value->sql_type = SQLT_LNG;
			ora_value->value = bin->data;
			ora_value->defined_size = bin->binary_length;
		}
		else {
			ora_value->sql_type = SQLT_CHR;
			ora_value->value = g_strdup ("");
			ora_value->defined_size = 0;
		}
	}
	else {
		gchar *val_str;
		val_str = gda_value_stringify ((GValue *) value);
		if (!val_str)
			return NULL;

		ora_value->sql_type = SQLT_CHR;
		ora_value->value = val_str;
		ora_value->defined_size = strlen (val_str);
	}

	return ora_value;
}

void
_gda_oracle_set_value (GValue *value, 
		       GdaOracleValue *ora_value,
		       GdaConnection *cnc)
{
	GdaTime gtime;
	GdaTimestamp timestamp;
	GdaNumeric numeric;
	sb2 year;
	ub1 month;
	ub1 day;
	ub1 hour;
	ub1 min;
	ub1 sec;
	GType type;
	gchar *string_buffer, *tmp;

	if (-1 == (ora_value->indicator)) {
		gda_value_set_null (value);
		return;
	}

	type = ora_value->g_type;
	gda_value_reset_with_type (value, type);
	if (type == G_TYPE_BOOLEAN) 
		g_value_set_boolean (value, (atoi (ora_value->value)) ? TRUE: FALSE);
	else if (type == G_TYPE_STRING) {
		string_buffer = g_malloc0 (ora_value->defined_size+1);
		memcpy (string_buffer, ora_value->value, ora_value->defined_size);
		string_buffer[ora_value->defined_size] = '\0';
		g_strchomp(string_buffer);
		tmp = g_locale_to_utf8 (string_buffer, -1, NULL, NULL, NULL);
		g_value_set_string (value, tmp);
		g_free (tmp);
		g_free (string_buffer);
	}
	else if (type == G_TYPE_INT64)
		g_value_set_int64 (value, atoll (ora_value->value));
	else if (type == G_TYPE_INT)
		g_value_set_int (value, atol (ora_value->value));
	else if (type == G_TYPE_UINT)
		g_value_set_uint (value, atol (ora_value->value));
	else if (type == GDA_TYPE_SHORT)
		gda_value_set_short (value, atoi (ora_value->value));
	else if (type == G_TYPE_FLOAT)
		g_value_set_float (value, atof (ora_value->value));
	else if (type == G_TYPE_DOUBLE)
		g_value_set_double (value, atof (ora_value->value));
	else if (type == GDA_TYPE_NUMERIC) {
		string_buffer = g_malloc0 (ora_value->defined_size+1);
		memcpy (string_buffer, ora_value->value, ora_value->defined_size);
		string_buffer [ora_value->defined_size] = '\0';
		g_strchomp (string_buffer);
		numeric.number = string_buffer;
		numeric.precision = 0; /* FIXME */
		numeric.width = 0; /* FIXME */
		gda_value_set_numeric (value, &numeric);
		g_free (string_buffer);
	}
	else if (type == G_TYPE_DATE) {
		GDate *date;
		OCIDateGetDate ((CONST OCIDate *) ora_value->value,
				(sb2 *) &year,
				(ub1 *) &month,
				(ub1 *) &day);
		date = g_date_new_dmy (day, month, year);
		g_value_take_boxed (value, date);
	}
	else if (type == GDA_TYPE_GEOMETRIC_POINT) {}
	else if (type == GDA_TYPE_NULL)
		gda_value_set_null (value);
	else if (type == GDA_TYPE_TIMESTAMP) {
		OCIDateGetDate ((CONST OCIDate *) ora_value->value,
				(sb2 *) &year,
				(ub1 *) &month,
				(ub1 *) &day);
		OCIDateGetTime ((CONST OCIDate *) ora_value->value,
				(ub1 *) &hour,
				(ub1 *) &min,
				(ub1 *) &sec);
		timestamp.year = year;
		timestamp.month = month;
		timestamp.day = day;
		timestamp.hour = hour;
		timestamp.minute = min;
		timestamp.second = sec;
		timestamp.fraction = 0;
		timestamp.timezone = 0;
		gda_value_set_timestamp(value, &timestamp);
	}
	else if (type == GDA_TYPE_TIME) {
		OCIDateGetTime ((CONST OCIDate *) ora_value->value,
				(ub1 *) &hour,
				(ub1 *) &min,
				(ub1 *) &sec);
		gtime.hour = hour;
		gtime.minute = min;
		gtime.second = sec;
		gda_value_set_time (value, &gtime);
	}
	else if (type == GDA_TYPE_BINARY) {
		g_warning ("GdaBinary type is not supported by Oracle");
	}
	else if (type == GDA_TYPE_BLOB) {
		GdaBlob *blob;
		GdaBlobOp *op;
		OCILobLocator *lobloc;
		OracleConnectionData *cdata;
		gint result;

		/* REM: we need to make a "copy" of the lob locator to give to the GdaOracleblobOp object */
		cdata = (OracleConnectionData*) gda_connection_internal_get_provider_data (cnc);
		if (!cdata) {
			gda_connection_add_event_string (cnc, _("Invalid Oracle handle"));
			gda_value_set_null (value);
			return;
		}

		result = OCIDescriptorAlloc ((dvoid *) cdata->henv, (dvoid **) &lobloc, 
					     (ub4) gda_oracle_blob_type (ora_value->sql_type), (size_t) 0, (dvoid **) 0);
		if (gda_oracle_check_result (result, cnc, cdata, OCI_HTYPE_ERROR,
					     _("Could not allocate Lob locator"))) {
			gda_value_set_null (value);
			return;
		}

		result = OCILobAssign ((dvoid *) cdata->henv, (dvoid *) cdata->herr, ora_value->value, &lobloc);
		if (gda_oracle_check_result (result, cnc, cdata, OCI_HTYPE_ERROR,
					     _("Could not copy Lob locator"))) {
			gda_value_set_null (value);
			return;
		}

		blob = g_new0 (GdaBlob, 1);
		op = gda_oracle_blob_op_new (cnc, lobloc);
		gda_blob_set_op (blob, op);
		g_object_unref (op);

		gda_value_take_blob (value, blob);
	}
	else
		g_value_set_string (value, ora_value->value);
}

void
_gda_oracle_value_free (GdaOracleValue *ora_value)
{
	if (ora_value->g_type == GDA_TYPE_BLOB)
                OCIDescriptorFree ((dvoid *) ora_value->value, (ub4) gda_oracle_blob_type (ora_value->sql_type));
        else
                g_free (ora_value->value);
        OCIDescriptorFree ((dvoid *) ora_value->pard, OCI_DTYPE_PARAM);
        g_free (ora_value);
}
