/* GDA Server Library
 * Copyright (C) 2000 Rodrigo Moya
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

#if !defined(__gda_server_field_h__)
#  define __gda_server_field_h__

#include <GDA.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct {
	gchar*     name;
	gulong     sql_type;
	gshort     c_type;
	gshort     nullable;
	GDA_Value* value;
	guchar     precision;
	gshort     num_scale;
	glong      defined_length;
	glong      actual_length;
	gint       malloced;
	
	gpointer   user_data;
} GdaServerField;

GdaServerField* gda_server_field_new  (void);
void            gda_server_field_set_name (GdaServerField *field, const gchar *name);
gulong          gda_server_field_get_sql_type (GdaServerField *field);
void            gda_server_field_set_sql_type (GdaServerField *field, gulong sql_type);
void            gda_server_field_set_defined_length (GdaServerField *field, glong length);
void            gda_server_field_set_actual_length (GdaServerField *field, glong length);
void            gda_server_field_set_scale (GdaServerField *field, gshort scale);
gpointer        gda_server_field_get_user_data (GdaServerField *field);
void            gda_server_field_set_user_data (GdaServerField *field, gpointer user_data);
void            gda_server_field_free (GdaServerField *field);

void            gda_server_field_set_boolean (GdaServerField *field, gboolean val);
void            gda_server_field_set_date (GdaServerField *field, GDate *val);
void            gda_server_field_set_time (GdaServerField *field, GTime val);
void            gda_server_field_set_timestamp (GdaServerField *field, GDate *dat, GTime tim);
void            gda_server_field_set_smallint (GdaServerField *field, gshort val);
void            gda_server_field_set_integer (GdaServerField *field, gint val);
void            gda_server_field_set_longvarchar (GdaServerField *field, gchar *val);
void            gda_server_field_set_char (GdaServerField *field, gchar *val);
void            gda_server_field_set_varchar (GdaServerField *field, gchar *val);
void            gda_server_field_set_single (GdaServerField *field, gfloat val);
void            gda_server_field_set_double (GdaServerField *field, gdouble val);
void            gda_server_field_set_varbin (GdaServerField *field, gpointer val, glong size);

#if defined(__cplusplus)
}
#endif

#endif
