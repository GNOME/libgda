/* GDA Oracle provider
 * Copyright (C) 2002 The GNOME Foundation.
 *
 * AUTHORS:
 * 	Tim Coleman <tim@timcoleman.com>
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

#include <libgda/gda-intl.h>
#include <stdlib.h>
#include <string.h>
#include "gda-oracle.h"
#include "gda-oracle-provider.h"

/* 
 * gda_oracle_make_error
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
GdaError *
gda_oracle_make_error (dvoid *hndlp, ub4 type)
{
	GdaError *error;
	gchar errbuf[512];
	ub4 errcode;

	error = gda_error_new ();

	if (hndlp != NULL) {
		OCIErrorGet ((dvoid *) hndlp,
				(ub4) 1, 
				(text *) NULL, 
				&errcode, 
				errbuf, 
				(ub4) sizeof (errbuf), 
				(ub4) type);
	
		gda_error_set_description (error, errbuf);	
	} else {
		gda_error_set_description (error, _("NO DESCRIPTION"));
	}
		
	gda_error_set_number (error, errcode);
	gda_error_set_source (error, "gda-oracle");
	gda_error_set_sqlstate (error, _("Not available"));
	
	return error;
}

/* 
 * gda_oracle_check_result
 * This function is used for checking the result of an OCI
 * call.  The result itself is the return value, and we
 * need the GdaConnection and GdaOracleConnectionData to do
 * our thing.  The type is OCI_HTYPE_ENV or OCI_HTYPE_ERROR
 * as described for OCIErrorGet.
 *
 * The return value is true if there is no error, or false otherwise
 *
 * If the return value is OCI_SUCCESS or OCI_SUCCESS_WITH_INFO,
 * we return TRUE.  Otherwise, if it is OCI_ERROR, try to get the
 * Oracle error message using gda_oracle_make_error.  Otherwise,
 * make an error using the given message.
 */
gboolean 
gda_oracle_check_result (gint result, GdaConnection *cnc, 
		GdaOracleConnectionData *priv_data, ub4 type, const gchar *msg)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	switch (result) {
	case OCI_SUCCESS:
	case OCI_SUCCESS_WITH_INFO:
		return TRUE;
	case OCI_ERROR:
		if (type == OCI_HTYPE_ERROR) 
			gda_connection_add_error (cnc, gda_oracle_make_error (priv_data->herr, type));
		if (type == OCI_HTYPE_ENV)
			gda_connection_add_error (cnc, gda_oracle_make_error (priv_data->henv, type));
		break;
	default:
		gda_connection_add_error_string (cnc, msg);
	}
	return FALSE;
}

GdaValueType 
oracle_sqltype_to_gda_type (const ub2 sqltype)
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
	case SQLT_CLOB:
	case SQLT_CFILEE:
		return GDA_VALUE_TYPE_STRING;
	case SQLT_NUM:
		return GDA_VALUE_TYPE_NUMERIC;
	case SQLT_INT:
		return GDA_VALUE_TYPE_INTEGER;
	case SQLT_FLT:
		return GDA_VALUE_TYPE_SINGLE;
	case SQLT_VBI:
	case SQLT_BIN:
	case SQLT_LBI:
	case SQLT_LVB:
	case SQLT_BLOB:
	case SQLT_BFILEE:
		return GDA_VALUE_TYPE_BINARY;
	case SQLT_UIN:
		return GDA_VALUE_TYPE_INTEGER;
	case SQLT_DAT:
	case SQLT_ODT:
	case SQLT_DATE:
		return GDA_VALUE_TYPE_DATE;
	case SQLT_TIME:
	case SQLT_TIME_TZ:
		return GDA_VALUE_TYPE_TIME;
	case SQLT_TIMESTAMP:
	case SQLT_TIMESTAMP_TZ:
	case SQLT_TIMESTAMP_LTZ:
		return GDA_VALUE_TYPE_TIMESTAMP;
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
		return GDA_VALUE_TYPE_UNKNOWN;
	}
}

gchar * 
oracle_sqltype_to_string (const ub2 sqltype)
{
	/* an incomplete list of all the oracle types */
	switch (sqltype) {
	case SQLT_CHR:
		return "VARCHAR2";
	case SQLT_NUM:
		return "NUMERIC";
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
gda_oracle_value_to_sql_string (GdaValue *value)
{
	gchar *val_str;
	gchar *ret;

	g_return_val_if_fail (value != NULL, NULL);

	val_str = gda_value_stringify (value);
	if (!val_str)
		return NULL;

	switch (value->type) {
	case GDA_VALUE_TYPE_BIGINT :
	case GDA_VALUE_TYPE_DOUBLE :
	case GDA_VALUE_TYPE_INTEGER :
	case GDA_VALUE_TYPE_NUMERIC :
	case GDA_VALUE_TYPE_SINGLE :
	case GDA_VALUE_TYPE_SMALLINT :
	case GDA_VALUE_TYPE_TINYINT :
		ret = g_strdup (val_str);
		break;
	default :
		ret = g_strdup_printf ("\"%s\"", val_str);
	}

	g_free (val_str);

	return ret;
}

GdaOracleValue *
gda_value_to_oracle_value (GdaValue *value)
{
	GdaOracleValue *ora_value;
	gchar *val_str;
	OCIDate *oci_date;
	GdaDate *gda_date;

	val_str = gda_value_stringify (value);
	if (!val_str)
		return NULL;

	ora_value = g_new0 (GdaOracleValue, 1);

	ora_value->gda_type = value->type;
	ora_value->indicator = 0;
	ora_value->hdef = (OCIDefine *) 0;
	ora_value->pard = (OCIParam *) 0;

	switch (ora_value->gda_type) {
	case GDA_VALUE_TYPE_NULL:
		ora_value->indicator = -1;
		break;
	case GDA_VALUE_TYPE_BIGINT :
	case GDA_VALUE_TYPE_DOUBLE :
	case GDA_VALUE_TYPE_INTEGER :
	case GDA_VALUE_TYPE_NUMERIC :
	case GDA_VALUE_TYPE_SINGLE :
	case GDA_VALUE_TYPE_SMALLINT :
	case GDA_VALUE_TYPE_TINYINT :
		ora_value->sql_type = SQLT_NUM;
		ora_value->value = (void *) val_str;
		ora_value->defined_size = strlen (val_str);
		break;
	case GDA_VALUE_TYPE_DATE :
		ora_value->sql_type = SQLT_ODT;
		oci_date = (OCIDate *) 0;
		gda_date = gda_value_get_date (value);
		ora_value->defined_size = sizeof (OCIDate);
		OCIDateSetDate (oci_date, gda_date->year, gda_date->month, gda_date->day);
		break;
	default :
		ora_value->sql_type = SQLT_CHR;
		ora_value->value = g_malloc0 (strlen (val_str));
		ora_value->value = val_str;
		ora_value->defined_size = strlen (val_str);
	}

	/*g_free (val_str);*/
	return ora_value;
}

void
gda_oracle_set_value (GdaValue *value, 
			GdaOracleValue *ora_value,
			GdaConnection *cnc)
{
	GDate *gdate;
	GdaDate date;
	GdaNumeric numeric;
	sb2 year;
	ub1 month;
	ub1 day;

	gchar *string_buffer;

	if (-1 == (ora_value->indicator)) {
		gda_value_set_null (value);
		return;
	}

	switch (ora_value->gda_type) {
	case GDA_VALUE_TYPE_BOOLEAN:
		gda_value_set_boolean (value, (atoi (ora_value->value)) ? TRUE: FALSE);
		break;
	case GDA_VALUE_TYPE_STRING:
		string_buffer = g_malloc0 (ora_value->defined_size+1);
		memcpy (string_buffer, ora_value->value, ora_value->defined_size);
		string_buffer[ora_value->defined_size] = '\0';
		g_strchomp(string_buffer);
		gda_value_set_string (value, string_buffer);
		break;
	case GDA_VALUE_TYPE_BIGINT:
		gda_value_set_bigint (value, atoll (ora_value->value));
		break;
	case GDA_VALUE_TYPE_INTEGER:
		gda_value_set_integer (value, atol (ora_value->value));
		break;
	case GDA_VALUE_TYPE_SMALLINT:
		gda_value_set_smallint (value, atoi (ora_value->value));
		break;
	case GDA_VALUE_TYPE_SINGLE:
		gda_value_set_single (value, atof (ora_value->value));
		break;
	case GDA_VALUE_TYPE_DOUBLE:
		gda_value_set_double (value, atof (ora_value->value));
		break;
	case GDA_VALUE_TYPE_NUMERIC:
		numeric.number = ora_value->value;
		numeric.precision = 5; /* FIXME */
		numeric.width = 5; /* FIXME */
		gda_value_set_numeric (value, &numeric);
		break;
	case GDA_VALUE_TYPE_DATE:
		gdate = g_date_new ();
		g_date_clear (gdate, 1);
		OCIDateGetDate ((CONST OCIDate *) ora_value->value,
				(sb2 *) &year,
				(ub1 *) &month,
				(ub1 *) &day);
		date.year = year;
		date.month = month;
		date.day = day;
		gda_value_set_date (value, &date);
		g_date_free (gdate);
		break;
	case GDA_VALUE_TYPE_GEOMETRIC_POINT:
		break;
	case GDA_VALUE_TYPE_NULL:
		gda_value_set_null (value);
		break;
	case GDA_VALUE_TYPE_TIMESTAMP:
		break;
	case GDA_VALUE_TYPE_TIME:
		break;
	case GDA_VALUE_TYPE_BINARY:
		/* FIXME */
		break;
	default:
		gda_value_set_string (value, ora_value->value);
	}
}

