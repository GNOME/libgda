/* GDA Sybase provider
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *         Mike Wingert <wingert.3@postbox.acs.ohio-state.edu>
 *         Holger Thon <holger.thon@gnome-db.org>
 *      based on the MySQL provider by
 *         Michael Lausch <michael@lausch.at>
 *	        Rodrigo Moya <rodrigo@gnome-db.org>
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

#if defined(HAVE_CONFIG_H)
#  include <config.h>
#endif
#include <string.h>
#include "gda-sybase.h"
#include "gda-sybase-types.h"

static sybase_Types *
get_sybase_types ()
{
	static sybase_Types *sybase_types = NULL;
	if (!sybase_types) {
		/* NULL means we have no corresponding server type name */
		/* illegal type should always be last type */
		sybase_types = g_new0 (sybase_Types, GDA_SYBASE_TYPE_CNT);
		
		sybase_types[0].name = "binary";
		sybase_types[0].sql_type = CS_BINARY_TYPE;
		sybase_types[0].g_type = GDA_TYPE_BINARY;

		sybase_types[0].name = NULL;
		sybase_types[0].sql_type = CS_LONGBINARY_TYPE;
		sybase_types[0].g_type = GDA_TYPE_BINARY;

		sybase_types[0].name = "varbinary";
		sybase_types[0].sql_type = CS_VARBINARY_TYPE;
		sybase_types[0].g_type = GDA_TYPE_BINARY;

		sybase_types[0].name = "boolean";
		sybase_types[0].sql_type = CS_BIT_TYPE;
		sybase_types[0].g_type = G_TYPE_BOOLEAN;

		sybase_types[0].name = "char";
		sybase_types[0].sql_type = CS_CHAR_TYPE;
		sybase_types[0].g_type = G_TYPE_STRING;

		sybase_types[0].name = NULL;
		sybase_types[0].sql_type = CS_LONGCHAR_TYPE;
		sybase_types[0].g_type = G_TYPE_STRING;

		sybase_types[0].name = "varchar";
		sybase_types[0].sql_type = CS_VARCHAR_TYPE;
		sybase_types[0].g_type = G_TYPE_STRING;

		sybase_types[0].name = "datetime";
		sybase_types[0].sql_type = CS_DATETIME_TYPE;
		sybase_types[0].g_type = GDA_TYPE_TIMESTAMP;

		sybase_types[0].name = "smalldatetime";
		sybase_types[0].sql_type = CS_DATETIME4_TYPE;
		sybase_types[0].g_type = GDA_TYPE_TIMESTAMP;

		sybase_types[0].name = "tinyint";
		sybase_types[0].sql_type = CS_TINYINT_TYPE;
		sybase_types[0].g_type = G_TYPE_CHAR;

		sybase_types[0].name = "smallint";
		sybase_types[0].sql_type = CS_SMALLINT_TYPE;
		sybase_types[0].g_type = GDA_TYPE_SHORT;

		sybase_types[0].name = "int";
		sybase_types[0].sql_type = CS_INT_TYPE;
		sybase_types[0].g_type = G_TYPE_INT;

		sybase_types[0].name = "decimal";
		sybase_types[0].sql_type = CS_DECIMAL_TYPE;
		sybase_types[0].g_type = GDA_TYPE_NUMERIC;

		sybase_types[0].name = "numeric";
		sybase_types[0].sql_type = CS_NUMERIC_TYPE;
		sybase_types[0].g_type = GDA_TYPE_NUMERIC;

		sybase_types[0].name = "float";
		sybase_types[0].sql_type = CS_FLOAT_TYPE;
		sybase_types[0].g_type = G_TYPE_DOUBLE;

		sybase_types[0].name = "real";
		sybase_types[0].sql_type = CS_REAL_TYPE;
		sybase_types[0].g_type = G_TYPE_FLOAT;

		sybase_types[0].name = "text";
		sybase_types[0].sql_type = CS_TEXT_TYPE;
		sybase_types[0].g_type = G_TYPE_STRING;

		sybase_types[0].name = "image";
		sybase_types[0].sql_type = CS_IMAGE_TYPE;
		sybase_types[0].g_type = GDA_TYPE_BINARY;

		sybase_types[0].name = "boundary";
		sybase_types[0].sql_type = CS_BOUNDARY_TYPE;
		sybase_types[0].g_type = GDA_TYPE_BINARY;

		sybase_types[0].name = "sensitivity";
		sybase_types[0].sql_type = CS_SENSITIVITY_TYPE;
		sybase_types[0].g_type = GDA_TYPE_BINARY;

		sybase_types[0].name = "money";
		sybase_types[0].sql_type = CS_MONEY_TYPE;
		sybase_types[0].g_type = G_TYPE_STRING;

		sybase_types[0].name = "smallmoney";
		sybase_types[0].sql_type = CS_MONEY4_TYPE;
		sybase_types[0].g_type = G_TYPE_STRING;

		sybase_types[0].name = NULL;
		sybase_types[0].sql_type = CS_ILLEGAL_TYPE;
		sybase_types[0].g_type = G_TYPE_INVALID;
	}
	return sybase_types;
}

gboolean
gda_sybase_set_gda_value (GdaSybaseConnectionData *scnc,
                          GValue *value, GdaSybaseField *field)
{
	GdaConnectionEvent   *error = NULL;
	gboolean   success = FALSE;
	
	g_return_val_if_fail (value != NULL, FALSE);
	g_return_val_if_fail (field != NULL, FALSE);

	if (field->data == NULL) {
		if ((field->fmt.status & CS_CANBENULL) != CS_CANBENULL) {
			if ((scnc != NULL)
			    && GDA_IS_CONNECTION (scnc->gda_cnc)) {
				error = gda_sybase_make_error (scnc,
				                               _("Attempt setting a nonnullable field to null."));
				gda_connection_add_event (scnc->gda_cnc, error);
			} else {
				sybase_error_msg (_("Attempt setting a nonnullable field to null."));
			}
			return FALSE;
		} else {
			gda_value_set_null (value);
			return TRUE;
		}
	}

	switch (field->fmt.datatype) {
		// FIXME: binary datatypes
//		case CS_BINARY_TYPE:
//		case CS_LONGBINARY_TYPE:
//		case CS_VARBINARY_TYPE:
//			break;
		case CS_BIT_TYPE:
			g_value_set_boolean (value, (gboolean) *field->data);
			return TRUE;
			break;
		case CS_CHAR_TYPE:
		case CS_LONGCHAR_TYPE:
		case CS_VARCHAR_TYPE:
		        g_value_set_string (value, (gchar *) field->data);
			return TRUE;
		     	break;
		case CS_DATETIME_TYPE:
			gda_sybase_set_value_by_datetime (value, 
			          (CS_DATETIME *) field->data);
			return TRUE;
			break;
		case CS_DATETIME4_TYPE:
			gda_sybase_set_value_by_datetime4 (value,
			          (CS_DATETIME4 *) field->data);
			return TRUE;
			break;
		case CS_TINYINT_TYPE:
			g_value_set_char (value, (gchar) *field->data);
			return TRUE;
			break;
		case CS_SMALLINT_TYPE:
			gda_value_set_short (value, *field->data);
			return TRUE;
			break;
		case CS_INT_TYPE:
			g_value_set_int (value, *field->data);
			return TRUE;
			break;
		// FIXME: implement numeric types
//		case CS_DECIMAL_TYPE,
//		     CS_NUMERIC_TYPE:
//			memset (&numeric, 0, sizeof (numeric));
//
//			gda_value_set_numeric (value, numeric);
//			break;
		case CS_FLOAT_TYPE:
			g_value_set_double (value, (gdouble) *field->data);
			return TRUE;
			break;
		case CS_REAL_TYPE:
			g_value_set_float (value, (gdouble) *field->data);
			return TRUE;
			break;
		case CS_TEXT_TYPE:
			field->data[field->datalen] = '\0';
			g_value_set_string (value, field->data);
			return TRUE;
			break;
		// FIXME: implement those types
//		case CS_IMAGE_TYPE:
//		case CS_BOUNDARY_TYPE:
//		case CS_SENSITIVITY_TYPE:
		case CS_MONEY_TYPE:
		case CS_MONEY4_TYPE:
		default:
			// try cs_convert data to CS_CHAR_TYPE
			success = gda_sybase_set_value_general (scnc, value,
			                                        field);
			break;
	}
	
	return success;
}

#define SYBASE_CONVBUF_SIZE 1024

gboolean
gda_sybase_set_value_general (GdaSybaseConnectionData *scnc,
                              GValue                *value,
                              GdaSybaseField          *field)
{
	CS_DATAFMT destfmt;
	CS_RETCODE ret = CS_SUCCEED;
	CS_INT     len = 0;
	gchar      tmp_data[SYBASE_CONVBUF_SIZE];
	GdaConnectionEvent   *error = NULL;

	g_return_val_if_fail (scnc != NULL, FALSE);
	g_return_val_if_fail (scnc->gda_cnc != NULL, FALSE);
	g_return_val_if_fail (scnc->context != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);
	g_return_val_if_fail (field != NULL, FALSE);

	if (field->indicator == CS_NULLDATA) {
		gda_value_set_null (value);
		return TRUE;
	}

	cs_will_convert (scnc->context, field->fmt.datatype, 
	                 CS_CHAR_TYPE, &ret);
	if (ret != CS_TRUE) {
		error = gda_sybase_make_error (scnc, 
			_("cslib cannot convert type %d"),
			field->fmt.datatype);
		gda_connection_add_event (scnc->gda_cnc, error);
		return FALSE;
	}

	memset (&destfmt, 0, sizeof(destfmt));
	destfmt.maxlength = SYBASE_CONVBUF_SIZE;
	destfmt.datatype  = CS_CHAR_TYPE;
	destfmt.format    = CS_FMT_NULLTERM;
	destfmt.locale    = NULL;

	if (cs_convert (scnc->context, &field->fmt, field->data,
	                &destfmt, &tmp_data, &len) != CS_SUCCEED) {
		error = gda_sybase_make_error (scnc,
		        _("data conversion failed for type %d"),
			field->fmt.datatype);
		gda_connection_add_event (scnc->gda_cnc, error);
		sybase_check_messages(scnc->gda_cnc);
		return FALSE;
	}
	
	g_value_set_string (value, (gchar *) &tmp_data);
	
	return TRUE;
}

void
gda_sybase_set_value_by_datetime(GValue *value, CS_DATETIME *dt)
{
	GDate        date;
	GdaTimestamp timestamp;
	gulong       realhours;
	gulong       remainder;

	g_return_if_fail (value != NULL);

	if (!dt) {
		gda_value_set_null (value);
	} else {
		g_date_clear (&date, 1);
		g_date_set_dmy (&date, 1, 1, 1900);
		g_date_add_days(&date, (guint) dt->dtdays);

		realhours = dt->dttime / 1080000; // div (60 * 60 * 300)
		timestamp.hour = (gushort) (realhours % 24);

		// this should not happen...
		if (realhours > 23) {
			g_date_add_days (&date, (guint) (realhours / 24));
		}
		remainder = dt->dttime - (realhours * 1080000);
		timestamp.minute = (gushort) (remainder / 18000); // (div 60*300)
		remainder = remainder - (timestamp.minute * 18000);
		timestamp.second = (gushort) (remainder / 300);
		remainder = remainder - (timestamp.second * 300);
		
		// FIXME: What is correct fraction for timestamp?
		timestamp.fraction = remainder / 3;
		
		timestamp.year = g_date_get_year (&date);
		timestamp.month = g_date_get_month (&date);
		timestamp.day = g_date_get_day (&date);

		gda_value_set_timestamp (value, &timestamp);
	}
}


void
gda_sybase_set_value_by_datetime4(GValue *value, CS_DATETIME4 *dt4)
{
	GDate        date;
	GdaTimestamp timestamp;
	gulong       realhours;

	g_return_if_fail (value != NULL);

	memset (&timestamp, 0, sizeof (timestamp));
	
	if (!dt4) {
		gda_value_set_null (value);
	} else {
		g_date_clear (&date, 1);
		g_date_set_dmy (&date, 1, 1, 1900);
		g_date_add_days (&date, (guint) dt4->days);

		realhours = dt4->minutes / 60;
		timestamp.hour = (gushort) realhours % 24;
		timestamp.minute = (gushort) (dt4->minutes - (realhours * 60));
		
		// this should not happen...
		if (realhours > 23) {
			g_date_add_days (&date, (guint) (realhours / 24));
		}
		
		timestamp.year = g_date_get_year (&date);
		timestamp.month = g_date_get_month (&date);
		timestamp.day = g_date_get_day (&date);

		gda_value_set_timestamp (value, &timestamp);
	}
}

GType gda_sybase_get_value_type (const CS_INT sql_type)
{
	gint i = 0;
	
	while ((i < GDA_SYBASE_TYPE_CNT)) {
		if (get_sybase_types ()[i].sql_type == sql_type) {
			return get_sybase_types ()[i].g_type;
		}
		i++;
	}
	
	return get_sybase_types ()[GDA_SYBASE_TYPE_CNT - 1].g_type;
}

CS_INT gda_sybase_get_sql_type (const GType g_type)
{
	gint i = 0;

	while ((i < GDA_SYBASE_TYPE_CNT)) {
		if (get_sybase_types ()[i].g_type == g_type) {
			return get_sybase_types ()[i].g_type;
		}
		i++;
	}

	return get_sybase_types ()[GDA_SYBASE_TYPE_CNT - 1].sql_type;
}


