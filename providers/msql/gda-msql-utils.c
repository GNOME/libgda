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
}

/*-------------------------------------------------------------------------*/

GdaValueType gda_msql_type_to_gda(int msql_type) {
  int msql_mapping[9][2] = {
    {INT_TYPE,GDA_VALUE_TYPE_INTEGER},
    {CHAR_TYPE,GDA_VALUE_TYPE_STRING},
    {REAL_TYPE,GDA_VALUE_TYPE_DOUBLE},
    {TEXT_TYPE,GDA_VALUE_TYPE_STRING},
    {DATE_TYPE,GDA_VALUE_TYPE_DATE},
    {UINT_TYPE,GDA_VALUE_TYPE_INTEGER},
    {MONEY_TYPE,GDA_VALUE_TYPE_DOUBLE},
    {TIME_TYPE,GDA_VALUE_TYPE_TIME},
    {8,GDA_VALUE_TYPE_UNKNOWN}
  };
  int t_idx;

  t_idx=((msql_type>0) && (msql_type<=LAST_REAL_TYPE)) ? msql_type-1 : 8;
  return msql_mapping[t_idx][1];
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
    case GDA_VALUE_TYPE_NUMERIC:
    case GDA_VALUE_TYPE_SINGLE:
    case GDA_VALUE_TYPE_SMALLINT:
    case GDA_VALUE_TYPE_TINYINT:
    case GDA_VALUE_TYPE_INTEGER: ret=g_strdup(val_str); break;
    default: ret=g_strdup_printf("\"%s\"",val_str);
  }
  g_free(val_str);
  return ret;
}
