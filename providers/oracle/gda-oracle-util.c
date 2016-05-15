/*
 * Copyright (C) 2009 - 2016 Vivien Malerba <malerba@gnome-db.org>
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

#include <glib/gi18n-lib.h>
#include <stdlib.h>
#include <string.h>
#include "gda-oracle.h"
#include "gda-oracle-util.h"
#include "gda-oracle-blob-op.h"
#include <libgda/gda-debug-macros.h>

#include <libgda/sqlite/keywords_hash.h>
#include "keywords_hash.c" /* this one is dynamically generated */

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
_gda_oracle_make_error (GdaConnection *cnc, dvoid *hndlp, ub4 type, const gchar *file, gint line)
{
	GdaConnectionEvent *error = NULL;
	gchar errbuf[512];
	ub4 errcode = GDA_CONNECTION_EVENT_CODE_UNKNOWN;
	gchar *source;

	if (hndlp != NULL) {
		OCIErrorGet ((dvoid *) hndlp,
			     (ub4) 1, 
			     (text *) NULL, 
			     &errcode, 
			     errbuf, 
			     (ub4) sizeof (errbuf), 
			     (ub4) type);
	
		if (errcode != 1405) {
			error = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_ERROR);
			gda_connection_event_set_description (error, errbuf);
			/*g_warning ("Oracle error:%s", errbuf);*/
			if (errcode == 600)
				g_warning ("Maybe an Oracle bug...");
		}
	} 
	else {
		error = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_ERROR);
		gda_connection_event_set_description (error, _("NO DESCRIPTION"));
	}
	
	if (error) {
		gda_connection_event_set_code (error, errcode);
		source = g_strdup_printf ("gda-oracle:%s:%d", file, line);
		gda_connection_event_set_source (error, source);
		g_free(source);
	}
	
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
			error = _gda_oracle_make_error (cnc, cdata->herr, type, file, line);
			gda_connection_add_event (cnc, error);
			break;
		case OCI_HTYPE_ENV:
			error = _gda_oracle_make_error (cnc, cdata->henv, type, file, line);
			if (error)
				gda_connection_add_event (cnc, error);
			break;
		default:
			if (error)
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

#ifdef GDA_DEBUG
	if (error)
		g_print ("HANDLED error: %s\n", gda_connection_event_get_description (error));
#endif
	return error;
}

GType 
_oracle_sqltype_to_g_type (const ub2 sqltype, sb2 precision, sb1 scale)
{
	/* an incomplete list of all the oracle types */
	switch (sqltype) {
	case SQLT_CHR:
	case SQLT_STR:
	case SQLT_VCS:
	case SQLT_RID:
	case SQLT_AVC:
	case SQLT_AFC:
		return G_TYPE_STRING;
	case SQLT_NUM:
	case SQLT_VNU:
		if (scale == 0)
			return G_TYPE_INT;
		else if ((precision != 0) && (scale == -127))
			return G_TYPE_FLOAT;
		else return GDA_TYPE_NUMERIC;
	case SQLT_INT:
		return G_TYPE_INT;
	case SQLT_UIN:
		return G_TYPE_UINT;
	case SQLT_BFLOAT:
	case SQLT_FLT:
		return G_TYPE_FLOAT;
	case SQLT_BDOUBLE:
		return G_TYPE_DOUBLE;
		
	case SQLT_LVC:
	case SQLT_LVB:
	case SQLT_VBI:
	case SQLT_BIN:
	case SQLT_LNG:
	case SQLT_LBI:
	case SQLT_RDD:
	case SQLT_NTY:
	case SQLT_REF:
		return GDA_TYPE_BINARY;
	case SQLT_BLOB:
	case SQLT_FILE:
	//case SQLT_BFILEE:
	//case SQLT_CFILEE:
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
	//case SQLT_VST:
	//case SQLT_SLS:
	//case SQLT_CUR:
	//case SQLT_OSL:

	//case SQLT_RSET:
	//case SQLT_NCO:
	//case SQLT_INTERVAL_YM:
	//case SQLT_INTERVAL_DS:
	//case SQLT_PDN:
	//case SQLT_NON:
	default:
		return G_TYPE_INVALID;
	}
}

ub2
_g_type_to_oracle_sqltype (GType type)
{
	if (type == G_TYPE_BOOLEAN)
		return SQLT_INT;
	else if (type == G_TYPE_STRING) 
		return SQLT_CHR;
	else if (type == G_TYPE_INT)
		return SQLT_INT;
	else if (type == G_TYPE_DATE)
		return SQLT_ODT;
	else if (type == GDA_TYPE_TIME)
		return SQLT_TIME;
	else if (type == G_TYPE_INT64)
		return SQLT_NUM;
	else if (type == G_TYPE_UINT64)
		return SQLT_UIN;
	else if (type == G_TYPE_UINT)
		return SQLT_UIN;
	else if (type == G_TYPE_ULONG)
		return SQLT_UIN;
	else if (type == G_TYPE_LONG)
		return SQLT_INT;
	else if (type == GDA_TYPE_SHORT)
		return SQLT_INT;
	else if (type == G_TYPE_FLOAT)
		return SQLT_BFLOAT;
	else if (type == G_TYPE_DOUBLE)
		return SQLT_BDOUBLE;
	else if (type == GDA_TYPE_NUMERIC)
		return SQLT_CHR;
	else if (type == GDA_TYPE_GEOMETRIC_POINT)
		return SQLT_CHR;
	else if (type == GDA_TYPE_TIMESTAMP)
		return SQLT_TIMESTAMP;
	else if (type == GDA_TYPE_BINARY)
		return SQLT_LNG;
	else if (type == GDA_TYPE_BLOB)
		return SQLT_BLOB;
	else if (type == G_TYPE_GTYPE)
		return SQLT_CHR;
	else if (type == G_TYPE_CHAR)
		return SQLT_INT;
	else {
		g_warning ("Internal implementation missing: type %s not handled", g_type_name (type));
		return SQLT_LNG;
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
	case SQLT_BFLOAT:
	case SQLT_BDOUBLE:
		return "FLOAT";
	case SQLT_STR:
		return "STRING";
	case SQLT_VNU:
		return "VARNUM";
	//case SQLT_PDN:
		return "";
	case SQLT_LNG:
		return "LONG";
	case SQLT_VCS:
		return "VARCHAR";
	//case SQLT_NON:
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
	//case SQLT_SLS:
		return "";
	case SQLT_LVC:
		return "LONG VARCHAR";
	case SQLT_LVB:
		return "LONG VARRAW";
	case SQLT_AFC:
		return "CHAR";
	case SQLT_AVC:
		return "CHARZ";
	//case SQLT_CUR:
		return "CURSOR";
	case SQLT_RDD:
		return "ROWID";
	//case SQLT_LAB:
		return "LABEL";
	//case SQLT_OSL:
		return "OSLABEL";
	case SQLT_NTY:
		return "NAMED DATA TYPE";
	case SQLT_REF:
		return "REF";
	case SQLT_CLOB:
		return "CLOB";
	case SQLT_BLOB:
		return "BLOB";
	case SQLT_BFILEE:
		return "BFILE";
	case SQLT_CFILEE:
		return "CFILE";
	//case SQLT_RSET:
		return "RESULT SET";
	//case SQLT_NCO:
		return "";
	//case SQLT_VST:
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
	//case SQLT_INTERVAL_YM:
		return "INTERVAL YEAR TO MONTH";
	//case SQLT_INTERVAL_DS:
		return "INTERVAL DAY TO SECOND";
	case SQLT_TIMESTAMP_LTZ:
		return "TIMESTAMP WITH LOCAL TIME ZONE";
	default:
		return "UNDEFINED";
	}
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
		 (type == G_TYPE_UINT64) ||
		 (type == G_TYPE_DOUBLE) ||
		 (type == G_TYPE_INT) ||
		 (type == G_TYPE_UINT) ||
		 (type == GDA_TYPE_NUMERIC) ||
		 (type == G_TYPE_FLOAT) ||
		 (type == GDA_TYPE_SHORT) ||
		 (type == GDA_TYPE_USHORT) ||
		 (type == G_TYPE_LONG) ||
		 (type == G_TYPE_ULONG) ||
		 (type == G_TYPE_CHAR) ||
		 (type == G_TYPE_UCHAR)) {
		gchar *val_str;
		val_str = gda_value_stringify ((GValue *) value);
		if (!val_str)
			return NULL;
		
		ora_value->sql_type = SQLT_CHR;
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
			ora_value->value = gda_binary_get_data (bin);
			ora_value->defined_size = gda_binary_get_size (bin);
		}
		else {
			ora_value->sql_type = SQLT_CHR;
			ora_value->value = g_strdup ("");
			ora_value->defined_size = 0;
		}
	}
	else if (type == GDA_TYPE_BINARY) {
		GdaBinary *bin;

		bin = (GdaBinary *) gda_value_get_binary ((GValue *) value);
		if (bin) {
			ora_value->sql_type = SQLT_LNG;
			ora_value->value = gda_binary_get_data (bin);
			ora_value->defined_size = gda_binary_get_size (bin);
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
	sb2 year;
	ub1 month;
	ub1 day;
	ub1 hour;
	ub1 min;
	ub1 sec;

	if (ora_value->indicator == -1) {
		gda_value_set_null (value);
		return;
	}

	gda_value_reset_with_type (value, ora_value->g_type);
	switch (ora_value->s_type) {
	case GDA_STYPE_INT:
		g_value_set_int (value, *((gint *) ora_value->value));
		break;
	case GDA_STYPE_STRING: {
		gchar *string_buffer, *tmp;
		
		string_buffer = (gchar *) ora_value->value;
		string_buffer [ora_value->rlen] = '\0';
		g_strchomp (string_buffer);
		//tmp = g_locale_to_utf8 (string_buffer, -1, NULL, NULL, NULL);
		//g_value_take_string (value, tmp);
		g_value_set_string (value, string_buffer);
		if (ora_value->use_callback) {
			g_free (string_buffer);
			ora_value->value = NULL;
		}
		break;
	}
	case GDA_STYPE_BOOLEAN:
		g_value_set_boolean (value, (*((gint *) ora_value->value)) ? TRUE: FALSE);
		break;
	case GDA_STYPE_DATE: {
		GDate *date;
		OCIDateGetDate ((CONST OCIDate *) ora_value->value,
				(sb2 *) &year,
				(ub1 *) &month,
				(ub1 *) &day);
		date = g_date_new_dmy (day, month, year);
		g_value_take_boxed (value, date);
		break;
	}
	case GDA_STYPE_TIME: {
		OCIDateGetTime ((CONST OCIDate *) ora_value->value,
				(ub1 *) &hour,
				(ub1 *) &min,
				(ub1 *) &sec);
		gtime.hour = hour;
		gtime.minute = min;
		gtime.second = sec;
		gda_value_set_time (value, &gtime);
		break;
	}
	case GDA_STYPE_TIMESTAMP: {
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
		break;
	}
	case GDA_STYPE_INT64:
		TO_IMPLEMENT; /* test that value fits in */
		g_value_set_int64 (value, atoll (ora_value->value));
		break;
	case GDA_STYPE_UINT64:
		TO_IMPLEMENT; /* test that value fits in */
		g_value_set_uint64 (value, atoll (ora_value->value));
		break;
	case GDA_STYPE_UINT:
		TO_IMPLEMENT; /* test that value fits in */
		g_value_set_uint (value, *((guint*) ora_value->value));
		break;
	case GDA_STYPE_FLOAT:
		g_value_set_float (value, *((gfloat*) ora_value->value));
		break;
	case GDA_STYPE_DOUBLE:
		g_value_set_double (value, *((gdouble*) ora_value->value));
		break;
	case GDA_STYPE_LONG:
		TO_IMPLEMENT;
		break;
	case GDA_STYPE_ULONG:
		TO_IMPLEMENT;
		break;
	case GDA_STYPE_NUMERIC: {
		GdaNumeric *numeric;
		gchar *tmp;
		g_assert (!ora_value->use_callback);
		
		tmp = g_malloc0 (ora_value->defined_size);
		memcpy (tmp, ora_value->value, ora_value->defined_size);
		tmp [ora_value->rlen] = '\0';
		g_strchomp (tmp);

		numeric = gda_numeric_new ();
		gda_numeric_set_from_string (numeric, tmp);
		g_free (tmp);
		gda_numeric_set_precision (numeric, ora_value->precision);
		gda_numeric_set_width (numeric, ora_value->scale);
		g_value_take_boxed (value, numeric);
		break;
	}
	case GDA_STYPE_BINARY: {
		GdaBinary *bin;

		bin = gda_binary_new ();
		if (ora_value->use_callback) {
			gda_binary_take_data (bin, ora_value->value, ora_value->rlen);
			ora_value->value = NULL;
			ora_value->rlen = 0;
		}
		else {
			gda_binary_set_data (bin, ora_value->value, ora_value->rlen);
		}
		gda_value_take_binary (value, bin);
		break;
	}
	case GDA_STYPE_BLOB: {
		GdaBlob *blob;
		GdaBlobOp *op;
		OCILobLocator *lobloc;
		OracleConnectionData *cdata;
		gint result;

		/* REM: we need to make a "copy" of the lob locator to give to the GdaOracleblobOp object */
		cdata = (OracleConnectionData*) gda_connection_internal_get_provider_data_error (cnc, NULL);
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

		blob = gda_blob_new ();
		op = gda_oracle_blob_op_new (cnc, lobloc);
		gda_blob_set_op (blob, op);
		g_object_unref (op);

		gda_value_take_blob (value, blob);
		break;
	}
	case GDA_STYPE_CHAR: {
		TO_IMPLEMENT; /* test that value fits in */
		g_value_set_schar (value, *((gint8*) ora_value->value));
		break;
	}
	case GDA_STYPE_SHORT: {
		TO_IMPLEMENT; /* test that value fits in */
		gda_value_set_short (value, *((gint*) ora_value->value));
		break;
	}
	case GDA_STYPE_GTYPE:
		TO_IMPLEMENT;
		break;
	case GDA_STYPE_GEOMETRIC_POINT:
		TO_IMPLEMENT;
		break;
	case GDA_STYPE_NULL:
		gda_value_set_null (value);
		break;
	default:
		g_assert_not_reached ();
	}
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

GType *static_types = NULL;
GdaStaticType
gda_g_type_to_static_type (GType type)
{
	GdaStaticType st;
	for (st = 0; st < GDA_STYPE_NULL; st++) {
		if (static_types [st] == type)
			return st;
	}
	g_error ("Missing type '%s' in GDA static types", g_type_name (type));
	return 0;
}

#ifdef GDA_DEBUG
void
_gda_oracle_test_keywords (void)
{
        V8test_keywords();
        V9test_keywords();
}
#endif

GdaSqlReservedKeywordsFunc
_gda_oracle_get_reserved_keyword_func (OracleConnectionData *cdata)
{
        if (cdata) {
                switch (cdata->major_version) {
                case 8:
                        return V8is_keyword;
		case 9:
                default:
                        return V9is_keyword;
                break;
                }
        }
	return V9is_keyword;
}
