/* GDA common library
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *	Michael Lausch <michael@lausch.at>
 *	Rodrigo Moya <rodrigo@gnome-db.org>
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

#if !defined(__gda_value_h__)
#  define __gda_value_h__

#include <glib/gmacros.h>
#include <libgda/GNOME_Database.h>

G_BEGIN_DECLS

#define TIMEZONE_INVALID (2*12*60*60)

typedef CORBA_any      GdaValue;
typedef CORBA_TypeCode GdaValueType;
typedef GNOME_Database_GeometricPoint GdaGeometricPoint;
typedef GNOME_Database_Date GdaDate;
typedef GNOME_Database_Time GdaTime;
typedef GNOME_Database_Timestamp GdaTimestamp;

#define GDA_VALUE_TYPE_NULL      TC_null
#define GDA_VALUE_TYPE_BIGINT    TC_CORBA_long_long
#define	GDA_VALUE_TYPE_BINARY    TC_null /* FIXME */
#define GDA_VALUE_TYPE_BOOLEAN   TC_CORBA_boolean
#define GDA_VALUE_TYPE_DATE      TC_GNOME_Database_Date
#define GDA_VALUE_TYPE_DOUBLE    TC_CORBA_double
#define GDA_VALUE_TYPE_GEOMETRIC_POINT     TC_GNOME_Database_GeometricPoint
#define GDA_VALUE_TYPE_INTEGER   TC_CORBA_long
#define GDA_VALUE_TYPE_SINGLE    TC_CORBA_float
#define GDA_VALUE_TYPE_SMALLINT  TC_CORBA_short
#define GDA_VALUE_TYPE_STRING    TC_CORBA_string
#define GDA_VALUE_TYPE_TIME      TC_GNOME_Database_Time
#define GDA_VALUE_TYPE_TIMESTAMP TC_GNOME_Database_Timestamp
#define GDA_VALUE_TYPE_TINYINT   TC_CORBA_char

GdaValue     *gda_value_new_null (void);
GdaValue     *gda_value_new_bigint (long long val);
GdaValue     *gda_value_new_binary (gconstpointer val);
GdaValue     *gda_value_new_boolean (gboolean val);
GdaValue     *gda_value_new_date (GdaDate *val);
GdaValue     *gda_value_new_double (gdouble val);
GdaValue     *gda_value_new_geometric_point (GdaGeometricPoint *val);
GdaValue     *gda_value_new_integer (gint val);
GdaValue     *gda_value_new_single (gfloat val);
GdaValue     *gda_value_new_smallint (gshort val);
GdaValue     *gda_value_new_string (const gchar *val);
GdaValue     *gda_value_new_time (GdaTime *val);
GdaValue     *gda_value_new_timestamp (GdaTimestamp *val);
GdaValue     *gda_value_new_tinyint (gchar val);

void          gda_value_free (GdaValue *value);

gboolean      gda_value_isa (const GdaValue *value, GdaValueType type);
gboolean      gda_value_is_null (GdaValue *value);
GdaValue     *gda_value_copy (GdaValue *value);

long long     gda_value_get_bigint (GdaValue *value);
void          gda_value_set_bigint (GdaValue *value, long long val);
gconstpointer gda_value_get_binary (GdaValue *value);
void          gda_value_set_binary (GdaValue *value, gconstpointer val, glong size);
gboolean      gda_value_get_boolean (GdaValue *value);
void          gda_value_set_boolean (GdaValue *value, gboolean val);
const GdaDate *gda_value_get_date (GdaValue *value);
void          gda_value_set_date (GdaValue *value, GdaDate *val);
gdouble       gda_value_get_double (GdaValue *value);
void          gda_value_set_double (GdaValue *value, gdouble val);
const GdaGeometricPoint *gda_value_get_geometric_point (GdaValue *value);
void          gda_value_set_geometric_point (GdaValue *value, GdaGeometricPoint *val);
gint          gda_value_get_integer (GdaValue *value);
void          gda_value_set_integer (GdaValue *value, gint val);
void          gda_value_set_null (GdaValue *value);
gfloat        gda_value_get_single (GdaValue *value);
void          gda_value_set_single (GdaValue *value, gfloat val);
gshort        gda_value_get_smallint (GdaValue *value);
void          gda_value_set_smallint (GdaValue *value, gshort val);
const gchar  *gda_value_get_string (GdaValue *value);
void          gda_value_set_string (GdaValue *value, const gchar *val);
const GdaTime *gda_value_get_time (GdaValue *value);
void          gda_value_set_time (GdaValue *value, GdaTime *val);
const GdaTimestamp *gda_value_get_timestamp (GdaValue *value);
void          gda_value_set_timestamp (GdaValue *value, GdaTimestamp *val);
gchar         gda_value_get_tinyint (GdaValue *value);
void          gda_value_set_tinyint (GdaValue *value, gchar val);

gchar        *gda_value_stringify (const GdaValue *value);

G_END_DECLS

#endif
