/* GNOME DB FreeTDS Provider
 * Copyright (C) 2002 The GNOME Foundation
 *
 * AUTHORS:
 *         Holger Thon <holger.thon@gnome-db.org>
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
#endif

#include <libgda/gda-intl.h>
#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <tds.h>
#include <tdsconvert.h>
#include "gda-freetds.h"
#include "gda-freetds-types.h"

static void
gda_freetds_set_gdavalue_by_datetime (GdaValue     *field,
                                      TDS_DATETIME *dt,
                                      TDSCOLINFO   *col
                                     )
{
	GDate        date;
	GdaTimestamp timestamp;
	gulong       realhours;
	gulong       remainder;

	g_return_if_fail (field != NULL);
	g_return_if_fail (col != NULL);

	memset (&timestamp, 0, sizeof (timestamp));
	
	if (!dt) {
		gda_value_set_null (field);
	} else {
		g_date_clear (&date, 1);
		g_date_set_dmy (&date, 1, 1, 1900);
		g_date_add_days (&date, (guint) dt->dtdays);

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

		gda_value_set_timestamp (field, &timestamp);
	}
}

static void
gda_freetds_set_gdavalue_by_datetime4 (GdaValue      *field,
                                       TDS_DATETIME4 *dt4,
                                       TDSCOLINFO    *col
                                      )
{
	GDate        date;
	GdaTimestamp timestamp;
	gulong       realhours;

	g_return_if_fail (field != NULL);
	g_return_if_fail (col != NULL);

	memset (&timestamp, 0, sizeof (timestamp));
	
	if (!dt4) {
		gda_value_set_null (field);
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

		gda_value_set_timestamp (field, &timestamp);
	}
}

/////////////////////////////////////////////////////////////////////////////
// Public functions
/////////////////////////////////////////////////////////////////////////////

const GdaValueType
gda_freetds_get_value_type (TDSCOLINFO *col)
{
	g_return_val_if_fail (col != NULL, GDA_VALUE_TYPE_UNKNOWN);

	switch (col->column_type) {
		case SYBBIT:
		case SYBBITN:
			return GDA_VALUE_TYPE_BOOLEAN;
		case SYBBINARY:
		case SYBIMAGE:
		case SYBVARBINARY:
			return GDA_VALUE_TYPE_BINARY;
		case SYBCHAR:
		case SYBNVARCHAR:
		case SYBVARCHAR:
		case SYBTEXT:
		case SYBNTEXT:
		case XSYBCHAR:
		case XSYBVARCHAR:
		/* FIXME: unicode types 
		case XSYBNCHAR:
		case XSYBNVARCHAR:
		*/
			return GDA_VALUE_TYPE_STRING;
		case SYBINT4:
			return GDA_VALUE_TYPE_INTEGER;
		case SYBINT2:
			return GDA_VALUE_TYPE_SMALLINT;
		case SYBINT1:
			return GDA_VALUE_TYPE_TINYINT;
		case SYBINTN:
			if (col->column_size == 1) {
				return GDA_VALUE_TYPE_TINYINT;
			} else if (col->column_size == 2) {
				return GDA_VALUE_TYPE_SMALLINT;
			} else if (col->column_size == 4) {
				return GDA_VALUE_TYPE_INTEGER;
			} else if (col->column_size == 8) {
				return GDA_VALUE_TYPE_BIGINT;
			}
			break;
		case SYBDECIMAL:
		case SYBNUMERIC:
			return GDA_VALUE_TYPE_NUMERIC;
		case SYBREAL:
			return GDA_VALUE_TYPE_SINGLE;
		case SYBFLT8:
		case SYBFLTN:
			return GDA_VALUE_TYPE_DOUBLE;
		case SYBDATETIME:
		case SYBDATETIMN:
		case SYBDATETIME4:
			return GDA_VALUE_TYPE_TIMESTAMP;
		default:
			return GDA_VALUE_TYPE_UNKNOWN;
	}
	
	return GDA_VALUE_TYPE_UNKNOWN;
}


void
gda_freetds_set_gdavalue (GdaValue *field, gchar *val, TDSCOLINFO *col,
			  GdaFreeTDSConnectionData *tds_cnc)
{
	const TDS_INT max_size = 255;
	TDS_INT col_size = 0;
	gchar *txt = NULL;
#ifdef HAVE_FREETDS_VER0_6X
	CONV_RESULT tds_conv;
#endif
	GdaNumeric numeric;

	g_return_if_fail (field != NULL);
	g_return_if_fail (col != NULL);

	// perhaps remove ifdef later on
	// tds_cnc is just needed for context structure of 0.6x api for now
#ifdef HAVE_FREETDS_VER0_6X
	g_return_if_fail (tds_cnc != NULL);
	g_return_if_fail (tds_cnc->ctx != NULL);

	// make sure conv union is empty
	memset((void *) &tds_conv, 0, sizeof(tds_conv));
#endif
	
	// Handle null fields
	if (val == NULL) {
		gda_value_set_null (field);
	} else {
		switch (col->column_type) {
			case SYBBIT:
			case SYBBITN:
				gda_value_set_boolean (field, (gboolean) (*(TDS_TINYINT *) val));
				break;
			case SYBBINARY:
			case SYBIMAGE:
				gda_value_set_binary (field, 
				                      (gconstpointer) val,
				                      (glong) col->column_size);
				break;
			case SYBVARBINARY:
				gda_value_set_binary (field,
				                      (gconstpointer ) (&((TDS_VARBINARY *) val)->array),
						      (glong) ((TDS_VARBINARY *) val)->len);
				break;
			// FIXME: TDS_VARCHAR returned for which types?
			case SYBCHAR:
			case SYBNVARCHAR:
			case SYBVARCHAR:
			case SYBTEXT:
			case SYBNTEXT:
			case XSYBCHAR:
			case XSYBVARCHAR:
			/* FIXME: unicode types 
			case XSYBNCHAR:
			case XSYBNVARCHAR:
			 */
				gda_value_set_string (field, val);
				break;
			case SYBINT4:
				gda_value_set_integer (field, *(TDS_INT *) val);
				break;
			case SYBINT2:
				gda_value_set_smallint (field, *(TDS_SMALLINT *) val);
				break;
			case SYBINT1:
				gda_value_set_tinyint (field, (gchar) (*(TDS_TINYINT *) val));
				break;
			case SYBINTN:
				if (col->column_size == 1) {
					gda_value_set_tinyint (field,
					                       (gchar) (*(TDS_TINYINT *) val));
				} else if (col->column_size == 2) {
					gda_value_set_smallint (field,
					                        *(TDS_SMALLINT *) val);
				} else if (col->column_size == 4) {
					gda_value_set_integer (field,
					                        *(TDS_INT *) val);
				} else if (col->column_size == 8) {
					gda_value_set_bigint (field,
					                      *(long long *) val);
				}
				break;
			case SYBDECIMAL:
			case SYBNUMERIC:
				memset (&numeric, 0, sizeof (numeric));
				numeric.number = g_strdup(((TDS_NUMERIC *) val)->array);
				numeric.precision = ((TDS_NUMERIC *) val)->precision;
				numeric.width = strlen (numeric.number);

				gda_value_set_numeric (field, &numeric);

				if (numeric.number) {
					g_free (numeric.number);
					numeric.number = NULL;
				}
				break;
			case SYBREAL:
				gda_value_set_single (field, *(TDS_REAL *) val);
				break;
			case SYBFLT8:
			case SYBFLTN:
				gda_value_set_double (field, *(TDS_FLOAT *) val);
				break;
			case SYBDATETIME:
			case SYBDATETIMN:
				gda_freetds_set_gdavalue_by_datetime (field,
				                                      (TDS_DATETIME *) val,
				                                      col);
				break;
			case SYBDATETIME4:
				gda_freetds_set_gdavalue_by_datetime4 (field,
				                                       (TDS_DATETIME4 *) val,
				                                       col);
				break;
			default:
				if (col->column_size > max_size) {
					col_size = max_size + 1;
				} else {
					col_size = col->column_size + 1;
				}
				txt = g_new0 (gchar, col_size);

				// tds_convert api changed to 0.6x
#ifndef HAVE_FREETDS_VER0_6X
				tds_convert (col->column_type, val,
				             col->column_size, SYBCHAR,
				             txt, col_size - 1);
				gda_value_set_string (field, txt ? txt : "");
#else
				tds_convert (tds_cnc->ctx, 
				             col->column_type, val,
				             col->column_size, SYBCHAR,
				             col_size - 1, &tds_conv);
				gda_value_set_string (field, 
				                      (tds_conv.c
				                        ? tds_conv.c
				                        : (tds_conv.ib
				                            ? tds_conv.ib
				                            : "")));
#endif
				if (txt) {
					g_free (txt);
					val = NULL;
				}
			}
	}
}
