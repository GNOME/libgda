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

#include <glib/gi18n-lib.h>
#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <tds.h>
#include <tdsconvert.h>
#include "gda-freetds.h"
#include "gda-freetds-types.h"

static void
gda_freetds_set_gdavalue_by_datetime (GValue     *field,
                                      TDS_DATETIME *dt,
                                      _TDSCOLINFO   *col
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

		realhours = dt->dttime / 1080000; /* div (60 * 60 * 300) */
		timestamp.hour = (gushort) (realhours % 24);

		/* this should not happen... */
		if (realhours > 23) {
			g_date_add_days (&date, (guint) (realhours / 24));
		}
		remainder = dt->dttime - (realhours * 1080000);
		timestamp.minute = (gushort) (remainder / 18000); /* (div 60*300) */
		remainder = remainder - (timestamp.minute * 18000);
		timestamp.second = (gushort) (remainder / 300);
		remainder = remainder - (timestamp.second * 300);
		
		/* FIXME: What is correct fraction for timestamp? */
		timestamp.fraction = remainder / 3;
		
		timestamp.year = g_date_get_year (&date);
		timestamp.month = g_date_get_month (&date);
		timestamp.day = g_date_get_day (&date);

		gda_value_set_timestamp (field, &timestamp);
	}
}

static void
gda_freetds_set_gdavalue_by_datetime4 (GValue      *field,
                                       TDS_DATETIME4 *dt4,
                                       _TDSCOLINFO    *col
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
		
		/* this should not happen... */
		if (realhours > 23) {
			g_date_add_days (&date, (guint) (realhours / 24));
		}
		
		timestamp.year = g_date_get_year (&date);
		timestamp.month = g_date_get_month (&date);
		timestamp.day = g_date_get_day (&date);

		gda_value_set_timestamp (field, &timestamp);
	}
}

/*
 * Public functions
 */

const GType
gda_freetds_get_value_type (_TDSCOLINFO *col)
{
	g_return_val_if_fail (col != NULL, G_TYPE_INVALID);

	switch (col->column_type) {
		case SYBBIT:
		case SYBBITN:
			return G_TYPE_BOOLEAN;
		case SYBBINARY:
		case SYBIMAGE:
		case SYBVARBINARY:
			return GDA_TYPE_BINARY;
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
			return G_TYPE_STRING;
		case SYBINT4:
			return G_TYPE_INT;
		case SYBINT2:
			return GDA_TYPE_SHORT;
		case SYBINT1:
			return G_TYPE_CHAR;
		case SYBINTN:
			if (col->column_size == 1) {
				return G_TYPE_CHAR;
			} else if (col->column_size == 2) {
				return GDA_TYPE_SHORT;
			} else if (col->column_size == 4) {
				return G_TYPE_INT;
			} else if (col->column_size == 8) {
				return G_TYPE_INT64;
			}
			break;
		case SYBDECIMAL:
		case SYBNUMERIC:
			return GDA_TYPE_NUMERIC;
		case SYBREAL:
			return G_TYPE_FLOAT;
		case SYBFLT8:
		case SYBFLTN:
			return G_TYPE_DOUBLE;
		case SYBDATETIME:
		case SYBDATETIMN:
		case SYBDATETIME4:
			return GDA_TYPE_TIMESTAMP;
		default:
			return G_TYPE_INVALID;
	}
	
	return G_TYPE_INVALID;
}


void
gda_freetds_set_gdavalue (GValue *field, gchar *val, _TDSCOLINFO *col,
			  GdaFreeTDSConnectionData *tds_cnc)
{
	const TDS_INT max_size = 255;
	TDS_INT col_size = 0;
	gchar *txt = NULL;
#if defined(HAVE_FREETDS_VER0_61_62) || defined(HAVE_FREETDS_VER0_63) 
	CONV_RESULT tds_conv;
#endif
	GdaNumeric numeric;
	GdaBinary bin;

	g_return_if_fail (field != NULL);
	g_return_if_fail (col != NULL);

	/* perhaps remove ifdef later on
	 * tds_cnc is just needed for context structure of 0.6x api for now
	 */
#if defined(HAVE_FREETDS_VER0_61_62) || defined(HAVE_FREETDS_VER0_63) 
	g_return_if_fail (tds_cnc != NULL);
	g_return_if_fail (tds_cnc->ctx != NULL);

	/* make sure conv union is empty */
	memset((void *) &tds_conv, 0, sizeof(tds_conv));
#endif
	
	/* Handle null fields */
	if (val == NULL) {
		gda_value_set_null (field);
	} else {
		switch (col->column_type) {
			case SYBBIT:
			case SYBBITN:
				g_value_set_boolean (field, (gboolean) (*(TDS_TINYINT *) val));
				break;
			case SYBBINARY:
			case SYBIMAGE:
				bin.data = val;
				bin.binary_length = (glong) col->column_size;
				gda_value_set_binary (field, &bin);
				break;
			case SYBVARBINARY:
				bin.data = (&((TDS_VARBINARY *) val)->array);
				bin.binary_length = ((TDS_VARBINARY *) val)->len;
				gda_value_set_binary (field, &bin);
				break;
			/* FIXME: TDS_VARCHAR returned for which types? */
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
				g_value_set_string (field, val);
				break;
			case SYBINT4:
				g_value_set_int (field, *(TDS_INT *) val);
				break;
			case SYBINT2:
				gda_value_set_short (field, *(TDS_SMALLINT *) val);
				break;
			case SYBINT1:
				g_value_set_char (field, (gchar) (*(TDS_TINYINT *) val));
				break;
			case SYBINTN:
				if (col->column_size == 1) {
					g_value_set_char (field,
					                       (gchar) (*(TDS_TINYINT *) val));
				} else if (col->column_size == 2) {
					gda_value_set_short (field,
					                        *(TDS_SMALLINT *) val);
				} else if (col->column_size == 4) {
					g_value_set_int (field,
					                        *(TDS_INT *) val);
				} else if (col->column_size == 8) {
					g_value_set_int64 (field,
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
				g_value_set_float (field, *(TDS_REAL *) val);
				break;
			case SYBFLT8:
			case SYBFLTN:
				g_value_set_double (field, *(TDS_FLOAT *) val);
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

				/* tds_convert api changed to 0.6x */
#if defined(HAVE_FREETDS_VER0_61_62) || defined(HAVE_FREETDS_VER0_63) 
				if (tds_convert (tds_cnc->ctx,
						 col->column_type, val,
						 col->column_size, SYBCHAR,
						 &tds_conv) < 0) {
					g_value_set_string (field, "");
				} else {
					g_value_set_string (field, 
						(tds_conv.c ? tds_conv.c : (tds_conv.ib ? tds_conv.ib : "")));
				}
#elif HAVE_FREETDS_VER0_60
				tds_convert (tds_cnc->ctx, 
				             col->column_type, val,
				             col->column_size, SYBCHAR,
				             col_size - 1, &tds_conv);
				g_value_set_string (field, 
				                      (tds_conv.c
				                        ? tds_conv.c
				                        : (tds_conv.ib
				                            ? tds_conv.ib
				                            : "")));
#else
				tds_convert (col->column_type, val,
				             col->column_size, SYBCHAR,
				             txt, col_size - 1);
				g_value_set_string (field, txt ? txt : "");
#endif
				if (txt) {
					g_free (txt);
					val = NULL;
				}
			}
	}
}
