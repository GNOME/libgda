/* 
 * $Id$
 *
 * GNOME DB sybase Provider
 * Copyright (C) 2000 Rodrigo Moya
 * Copyright (C) 2000 Stephan Heinze
 * Copyright (C) 2000 Holger Thon
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

// $Log$
// Revision 1.2  2001/04/07 08:49:31  rodrigo
// 2001-04-07  Rodrigo Moya <rodrigo@gnome-db.org>
//
// 	* objects renaming (Gda_* to Gda*) to conform to the GNOME
// 	naming standards
//
// Revision 1.1.1.1  2000/08/10 09:32:39  rodrigo
// First version of libgda separated from GNOME-DB
//
// Revision 1.2  2000/08/04 11:36:30  rodrigo
// Things I forgot in the last commit
//
// Revision 1.1  2000/08/04 10:20:37  rodrigo
// New Sybase provider
//
//


#include "gda-sybase.h"

// NULL means we have no corresponding server type name
// illegal type should always be last type
const sybase_Types gda_sybase_type_list[GDA_SYBASE_TYPE_CNT] = {
  // Binary types
  { "binary",        CS_BINARY_TYPE,      GDA_TypeVarbin  },     //   1
  { NULL,            CS_LONGBINARY_TYPE,  GDA_TypeLongvarbin  }, //   2
  { "varbinary",     CS_VARBINARY_TYPE,   GDA_TypeVarbin  },     //   3
  // Boolean types
  { "boolean",       CS_BIT_TYPE,         GDA_TypeBoolean },     //   4
  // Character types
  { "char",          CS_CHAR_TYPE,        GDA_TypeVarchar },     //   5
  { NULL,            CS_LONGCHAR_TYPE,    GDA_TypeLongvarchar }, //   6
  { "varchar",       CS_VARCHAR_TYPE,     GDA_TypeVarchar },     //   7
  // Datetime types
  { "datetime",      CS_DATETIME_TYPE,    GDA_TypeDbTimestamp }, //   8
  { "smalldatetime", CS_DATETIME4_TYPE,   GDA_TypeDbTimestamp }, //   9
  // Numeric types
  { "tinyint",       CS_TINYINT_TYPE,     GDA_TypeSmallint },    //  10
  { "smallint",      CS_SMALLINT_TYPE,    GDA_TypeSmallint },    //  11
  { "int",           CS_INT_TYPE,         GDA_TypeInteger },     //  12
  { "decimal",       CS_DECIMAL_TYPE,     GDA_TypeDecimal },     //  13
  { "numeric",       CS_NUMERIC_TYPE,     GDA_TypeNumeric },     //  14
  { "float",         CS_FLOAT_TYPE,       GDA_TypeDouble },      //  15
  { "real",          CS_REAL_TYPE,        GDA_TypeSingle },      //  16
  { "text",          CS_TEXT_TYPE,        GDA_TypeLongvarchar }, //  17
  { "image",         CS_IMAGE_TYPE,       GDA_TypeVarbin },      //  18

  // Security types
  { "boundary",      CS_BOUNDARY_TYPE,    GDA_TypeVarbin },      //  19
  { "sensitivity",   CS_SENSITIVITY_TYPE, GDA_TypeVarbin },      //  20

// Types unsupported by gda-srv are reported as varchar and at the end
// of this list
	// Money types
  { "money",         CS_MONEY_TYPE,       GDA_TypeVarchar },     //  21
  { "smallmoney",    CS_MONEY4_TYPE,      GDA_TypeVarchar },     //  22
  //	{ "money",         CS_MONEY_TYPE,       GDA_TypeCurrency },    //  21
  //	{ "smallmoney",    CS_MONEY4_TYPE,      GDA_TypeCurrency },    //  22
	
	
  { NULL,            CS_ILLEGAL_TYPE,     GDA_TypeNull    }      //  23
};

const gshort
sybase_get_c_type(const GDA_ValueType gda_type)
{
  // FIXME: implement c types
  return -1;
}

const GDA_ValueType
sybase_get_gda_type(const CS_INT sql_type)
{
  gint i = 0;
	
  while ((i < GDA_SYBASE_TYPE_CNT)) {
    if (gda_sybase_type_list[i].sql_type == sql_type) {
      return gda_sybase_type_list[i].gda_type;
    }
    i++;
  }
	
  return gda_sybase_type_list[GDA_SYBASE_TYPE_CNT - 1].gda_type;
}

const CS_INT
sybase_get_sql_type(const GDA_ValueType gda_type)
{
  gint i = 0;

  while ((i < GDA_SYBASE_TYPE_CNT)) {
    if (gda_sybase_type_list[i].gda_type == gda_type) {
      return gda_sybase_type_list[i].gda_type;
    }
    i++;
  }

  return gda_sybase_type_list[GDA_SYBASE_TYPE_CNT - 1].sql_type;
}

const gchar *
sybase_get_type_name(const CS_INT sql_type)
{
  gint i = 0;

  while ((i < GDA_SYBASE_TYPE_CNT)) {
    if (gda_sybase_type_list[i].sql_type == sql_type) {
      return gda_sybase_type_list[i].name;
    }
    i++;
  }

  return gda_sybase_type_list[GDA_SYBASE_TYPE_CNT - 1].name;
}

void
gda_sybase_field_set_datetime(GdaServerField *field, CS_DATETIME *dt)
{
  GDate  *date;
  GTime  time;

  date = g_date_new_dmy(1, 1, 1900);
  g_return_if_fail(date != NULL);
  g_date_add_days(date, (guint) dt->dtdays);
  time = (GTime) dt->dttime / 300;
  gda_server_field_set_timestamp(field, date, time);
  g_date_free(date);
}

void
gda_sybase_field_set_datetime4(GdaServerField *field, CS_DATETIME4 *dt)
{
  GDate *date;
  GTime time;

  date = g_date_new_dmy(1, 1, 1900);
  g_return_if_fail(date != NULL);
  g_date_add_days(date, (guint) dt->days);
  time = (GTime) dt->minutes * 60;
  gda_server_field_set_timestamp(field, date, time);
  g_date_free(date);
}

#define SYBASE_CONVBUF_SIZE 1024

void
gda_sybase_field_set_general(GdaServerField       *field,
                             sybase_Field          *sfield,
                             sybase_Connection     *scnc)
{
  CS_DATAFMT           destfmt;
  CS_RETCODE           ret = CS_SUCCEED;
  CS_INT               len = 0;
  gchar                tmpdata[SYBASE_CONVBUF_SIZE];
	
  g_return_if_fail(field != NULL);
  g_return_if_fail(sfield != NULL);
  g_return_if_fail(sfield->data != NULL);
  g_return_if_fail(sfield->fmt != NULL);
  g_return_if_fail(scnc->ctx != NULL);

  if (sfield->indicator == CS_NULLDATA) {
    gda_server_field_set_actual_length(field, 0);
    return;
  }

  cs_will_convert(scnc->ctx, sfield->fmt->datatype, CS_CHAR_TYPE, &ret);
  if (ret != CS_TRUE) {
    gda_server_field_set_actual_length(field, 0);
    gda_log_message("cslib cannot convert type %d",
		    sfield->fmt->datatype);
    return;
  }
	
  destfmt.maxlength = SYBASE_CONVBUF_SIZE;
  destfmt.datatype  = CS_CHAR_TYPE;
  destfmt.format    = CS_FMT_NULLTERM;
  destfmt.locale    = NULL;
	
  if (cs_convert(scnc->ctx, sfield->fmt, sfield->data, &destfmt,
		 &tmpdata, &len) != CS_SUCCEED) {
    gda_server_field_set_actual_length(field, 0);
    return;
  }
	
  gda_server_field_set_varchar(field, (gchar *) &tmpdata);
  gda_server_field_set_actual_length(field, len - 1);
}
