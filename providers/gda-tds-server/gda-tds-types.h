/* 
 * $Id$
 *
 * GNOME DB tds Provider
 * Copyright (C) 2000 Holger Thon
 * Copyright (C) 2000 Stephan Heinze
 * Copyright (C) 2000 Rodrigo Moya
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

#if !defined(__gda_tds_types_h__)
#  define __gda_tds_types_h__

#if defined(HAVE_CONFIG_H)
#  include <config.h>
#endif

#include "gda-tds.h"

/*
 * Per-object specific structures
 */

typedef struct _tds_Types {
  gchar         *name;
  CS_INT        sql_type;
  GDA_ValueType gda_type;
} tds_Types;

#define GDA_TDS_TYPE_CNT 23
extern const tds_Types gda_tds_type_list[GDA_TDS_TYPE_CNT];

const gshort        tds_get_c_type(const GDA_ValueType);
const GDA_ValueType tds_get_gda_type(const CS_INT);
const CS_INT        tds_get_sql_type(const GDA_ValueType);
const gchar         *tds_get_type_name(const CS_INT);

void gda_tds_field_set_datetime(Gda_ServerField *, CS_DATETIME *);
void gda_tds_field_set_datetime4(Gda_ServerField *, CS_DATETIME4 *);
void gda_tds_field_set_general(Gda_ServerField    *,
                               tds_Field          *,
                               tds_Connection     *);


#endif
