/* GDA mSQL Provider
 * Copyright (C) 1998 - 2006 The GNOME Foundation
 *
 * AUTHORS:
 * 	   Danilo Schoeneberg <dj@starfire-programming.net>
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

#include "gda-msql.h"

GdaConnectionEvent *gda_msql_make_error(int sock) {
	GdaConnectionEvent *error;

	error=gda_connection_event_new(GDA_CONNECTION_EVENT_ERROR);
	if (msqlErrMsg) {
		gda_connection_event_set_description(error,msqlErrMsg);
		gda_connection_event_set_code(error,-1);
	} else {
		gda_connection_event_set_description(error,"NO DESCRIPTION");
		gda_connection_event_set_code(error,-1);
	}
	gda_connection_event_set_source(error,"gda-mSQL");
	return error;
}


GType gda_msql_type_to_gda(int msql_type) { 
	switch (msql_type) {
	case INT_TYPE :   return G_TYPE_INT;
	case CHAR_TYPE:   return G_TYPE_STRING;
	case REAL_TYPE:   return G_TYPE_DOUBLE;
	case TEXT_TYPE:   return G_TYPE_STRING;
	case DATE_TYPE:   return G_TYPE_DATE;
	case UINT_TYPE:   return G_TYPE_UINT;
	case MONEY_TYPE:  return G_TYPE_FLOAT;
	case TIME_TYPE:   return GDA_TYPE_TIME;
#ifdef HAVE_MSQL3
	case IPV4_TYPE:   return G_TYPE_STRING;
	case INT64_TYPE:  return G_TYPE_INT64;
	case UINT64_TYPE: return G_TYPE_UINT64;
	case INT8_TYPE:   return G_TYPE_CHAR;
	case INT16_TYPE:  return GDA_TYPE_SHORT;
	case UINT8_TYPE:  return G_TYPE_UCHAR;
	case UINT16_TYPE: return GDA_TYPE_USHORT;  
#endif
	default:;
	}
	return G_TYPE_INVALID;
}


gchar *gda_msql_value_to_sql_string(GValue *value) {
	gchar *val_str;
	gchar *ret;
	GType type;

	g_return_val_if_fail(value!=NULL,NULL);
	val_str=gda_value_stringify(value);
	if (!val_str) return NULL;
	type = G_VALUE_TYPE (value);
	if ((type == G_TYPE_DOUBLE) ||
	    (type == G_TYPE_INT64) ||
	    (type == G_TYPE_UINT64) ||
	    (type == GDA_TYPE_NUMERIC) ||
	    (type == G_TYPE_FLOAT) ||
	    (type == GDA_TYPE_SHORT) ||
	    (type == GDA_TYPE_USHORT) ||
	    (type == G_TYPE_CHAR) ||
	    (type == G_TYPE_UCHAR) ||
	    (type == G_TYPE_UINT) ||
	    (type == G_TYPE_INT)) 
		ret=g_strdup(val_str); 
	else 
		ret=g_strdup_printf("\"%s\"",val_str);

	g_free(val_str);
	return ret;
}
