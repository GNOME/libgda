/* GDA mSQL Provider
 * Copyright (C) 1998-2002 The GNOME Foundation
 *
 * AUTHORS:
 * 	   Danilo Schoeneberg <dj@starfire-programming.net
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

GdaError *gda_msql_make_error(int sock) {
  GdaError *error;

  error=gda_error_new();
  if (msqlErrMsg) {
    gda_error_set_description(error,msqlErrMsg);
    gda_error_set_number(error,-1);
  } else {
    gda_error_set_description(error,"NO DESCRIPTION");
    gda_error_set_number(error,-1);
  }
  gda_error_set_source(error,"gda-mSQL");
  gda_error_set_sqlstate(error,"Not available");
  return error;
}

/*-------------------------------------------------------------------------*/

GdaValueType gda_msql_type_to_gda(int msql_type) { 
  switch (msql_type) {
    case INT_TYPE :   return GDA_VALUE_TYPE_INTEGER;
    case CHAR_TYPE:   return GDA_VALUE_TYPE_STRING;
    case REAL_TYPE:   return GDA_VALUE_TYPE_DOUBLE;
    case TEXT_TYPE:   return GDA_VALUE_TYPE_STRING;
    case DATE_TYPE:   return GDA_VALUE_TYPE_DATE;
    case UINT_TYPE:   return GDA_VALUE_TYPE_UINTEGER;
    case MONEY_TYPE:  return GDA_VALUE_TYPE_SINGLE;
    case TIME_TYPE:   return GDA_VALUE_TYPE_TIME;
#ifdef HAVE_MSQL3
    case IPV4_TYPE:   return GDA_VALUE_TYPE_STRING;
    case INT64_TYPE:  return GDA_VALUE_TYPE_BIGINT;
    case UINT64_TYPE: return GDA_VALUE_TYPE_BIGUINT;
    case INT8_TYPE:   return GDA_VALUE_TYPE_TINYINT;
    case INT16_TYPE:  return GDA_VALUE_TYPE_SMALLINT;
    case UINT8_TYPE:  return GDA_VALUE_TYPE_TINYUINT;
    case UINT16_TYPE: return GDA_VALUE_TYPE_SMALLUINT;  
#endif
    default:;
  }
  return GDA_VALUE_TYPE_UNKNOWN;
}

/*-------------------------------------------------------------------------*/

gchar *gda_msql_value_to_sql_string(GdaValue *value) {
  gchar *val_str;
  gchar *ret;
  
  g_return_val_if_fail(value!=NULL,NULL);
  val_str=gda_value_stringify(value);
  if (!val_str) return NULL;
  switch (value->type) {
    case GDA_VALUE_TYPE_DOUBLE:
    case GDA_VALUE_TYPE_BIGINT:
    case GDA_VALUE_TYPE_BIGUINT:
    case GDA_VALUE_TYPE_NUMERIC:
    case GDA_VALUE_TYPE_SINGLE:
    case GDA_VALUE_TYPE_SMALLINT:
    case GDA_VALUE_TYPE_SMALLUINT:
    case GDA_VALUE_TYPE_TINYINT:
    case GDA_VALUE_TYPE_TINYUINT:
    case GDA_VALUE_TYPE_UINTEGER:
    case GDA_VALUE_TYPE_INTEGER: ret=g_strdup(val_str); break;
    default: ret=g_strdup_printf("\"%s\"",val_str);
  }
  g_free(val_str);
  return ret;
}
