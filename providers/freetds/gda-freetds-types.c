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

#include <config.h>
#include <libgda/gda-intl.h>
#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <tds.h>
#include <tdsconvert.h>
#include "gda-freetds.h"
#include "gda-freetds-types.h"

const GdaValueType
gda_freetds_get_value_type (TDSCOLINFO *col)
{
	g_return_val_if_fail (col != NULL, GDA_VALUE_TYPE_UNKNOWN);

	switch (col->column_type) {
		case SYBCHAR:
		case SYBNVARCHAR:
		case SYBVARCHAR:
		case SYBTEXT:
		case SYBNTEXT:
		case XSYBCHAR:
		case XSYBNCHAR:
		case XSYBNVARCHAR:
		case XSYBVARCHAR:
			return GDA_VALUE_TYPE_STRING;
		case SYBINT4:
			return GDA_VALUE_TYPE_INTEGER;
		case SYBINT2:
			return GDA_VALUE_TYPE_SMALLINT;
		case SYBINT1:
			return GDA_VALUE_TYPE_TINYINT;
		case SYBINTN:
			if (col->column_size == 1) {
			}
		default:
			return GDA_VALUE_TYPE_STRING;
	}
	
	return GDA_VALUE_TYPE_UNKNOWN;
}


void
gda_freetds_set_gdavalue (GdaValue *field, gchar *val, TDSCOLINFO *col)
{
	const TDS_INT max_size = 255;
	TDS_INT col_size = 0;
	gchar *txt = NULL;

	g_return_if_fail (field != NULL);
	g_return_if_fail (col != NULL);

	// Handle null fields
	if (val == NULL) {
		gda_value_set_null (field);
	} else {
		switch (col->column_type) {
			case SYBCHAR:
			case SYBNVARCHAR:
			case SYBVARCHAR:
			case SYBTEXT:
			case SYBNTEXT:
			case XSYBCHAR:
			case XSYBNCHAR:
			case XSYBNVARCHAR:
			case XSYBVARCHAR:
				gda_value_set_string (field, val);
				break;
			case SYBINT4:
				gda_value_set_integer (field, *(TDS_INT *) val);
				break;
			case SYBINT2:
				gda_value_set_smallint (field, *(TDS_SMALLINT *) val);
				break;
			case SYBINT1:
				gda_value_set_tinyint (field, *(TDS_TINYINT *) val);
				break;
			case SYBINTN:
				if (col->column_size == 1) {
				}
			default:
				if (col->column_size > max_size) {
					col_size = max_size + 1;
				} else {
					col_size = col->column_size + 1;
				}
				txt = g_new0(gchar, col_size);
				tds_convert(col->column_type, val,
				            col->column_size, SYBCHAR,
				            txt, col_size - 1);
				gda_value_set_string (field, txt ? txt : "");
				if (txt) {
					g_free(txt);
					val = NULL;
				}
			}
	}
}
