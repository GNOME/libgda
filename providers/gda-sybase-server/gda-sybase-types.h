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

#if !defined(__gda_sybase_types_h__)
#  define __gda_sybase_types_h__

#if defined(HAVE_CONFIG_H)
#  include <config.h>
#endif

#include "gda-sybase.h"

/*
 * Per-object specific structures
 */

typedef struct _sybase_Types
{
	gchar *name;
	CS_INT sql_type;
	GDA_ValueType gda_type;
}
sybase_Types;

#define GDA_SYBASE_TYPE_CNT 23
extern const sybase_Types gda_sybase_type_list[GDA_SYBASE_TYPE_CNT];

const gshort sybase_get_c_type (const GDA_ValueType);
const GDA_ValueType sybase_get_gda_type (const CS_INT);
const CS_INT sybase_get_sql_type (const GDA_ValueType);
const gchar *sybase_get_type_name (const CS_INT);

void gda_sybase_field_set_datetime (GdaServerField *, CS_DATETIME *);
void gda_sybase_field_set_datetime4 (GdaServerField *, CS_DATETIME4 *);
void gda_sybase_field_set_general (GdaServerField *,
				   sybase_Field *, sybase_Connection *);


#endif
