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

#if !defined(__gda_sybase_types_h__)
#  define __gda_sybase_types_h__

#if defined(HAVE_CONFIG_H)
#endif
#include <string.h>
#include "gda-sybase.h"
#include "gda-sybase-recordset.h"

G_BEGIN_DECLS

typedef gboolean (*sybase_conv_Func) (GdaSybaseConnectionData *scnc,
                                      GdaSybaseField          *field,
                                      GdaValue                *value);

typedef struct _sybase_Types {
  gchar            *name;
  CS_INT           sql_type;
  GdaValueType     gda_type;
//  sybase_conv_Func conv_sql2gda;
} sybase_Types;

#define GDA_SYBASE_TYPE_CNT 23
extern const sybase_Types gda_sybase_type_list[GDA_SYBASE_TYPE_CNT];

const GdaValueType gda_sybase_get_value_type (const CS_INT sql_type);
const CS_INT gda_sybase_get_sql_type (const GdaValueType gda_type);

void gda_sybase_set_value_by_datetime(GdaValue *value, CS_DATETIME *dt);
void gda_sybase_set_value_by_datetime4(GdaValue *value, CS_DATETIME4 *dt);

const gboolean gda_sybase_set_gda_value (GdaSybaseConnectionData *scnc,
                                         GdaValue *value, 
                                         GdaSybaseField *field);
const gboolean gda_sybase_set_value_general (GdaSybaseConnectionData *scnc,
                                             GdaValue                *value,
                                             GdaSybaseField          *field);



G_END_DECLS

#endif
