/* GDA library
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *	Michael Lausch <michael@lausch.at>
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *	Juan-Mariano de Goyeneche <jmseyas@dit.upm.es> (BLOB issues) 
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

#include <time.h>
#include <glib/glist.h>
#include <glib/gmacros.h>
#include <glib-object.h>
#include <libxml/tree.h>
#include <libgda/gda-blob.h>

G_BEGIN_DECLS

#define TIMEZONE_INVALID (2*12*60*60)

typedef enum {
	GDA_VALUE_TYPE_NULL,
	GDA_VALUE_TYPE_BIGINT,
	GDA_VALUE_TYPE_BIGUINT,
	GDA_VALUE_TYPE_BINARY,
	GDA_VALUE_TYPE_BLOB,
	GDA_VALUE_TYPE_BOOLEAN,
	GDA_VALUE_TYPE_DATE,
	GDA_VALUE_TYPE_DOUBLE,
	GDA_VALUE_TYPE_GEOMETRIC_POINT,
	GDA_VALUE_TYPE_GOBJECT,
	GDA_VALUE_TYPE_INTEGER,
	GDA_VALUE_TYPE_LIST,
	GDA_VALUE_TYPE_MONEY,
	GDA_VALUE_TYPE_NUMERIC,
	GDA_VALUE_TYPE_SINGLE,
	GDA_VALUE_TYPE_SMALLINT,
	GDA_VALUE_TYPE_SMALLUINT,
	GDA_VALUE_TYPE_STRING,
	GDA_VALUE_TYPE_TIME,
	GDA_VALUE_TYPE_TIMESTAMP,
	GDA_VALUE_TYPE_TINYINT,
	GDA_VALUE_TYPE_TINYUINT,
	GDA_VALUE_TYPE_TYPE,
        GDA_VALUE_TYPE_UINTEGER,
	GDA_VALUE_TYPE_UNKNOWN
} GdaValueType;

typedef struct {
	gshort year;
	gushort month;
	gushort day;
} GdaDate;

typedef struct {
	gdouble x;
	gdouble y;
} GdaGeometricPoint;

typedef struct {
	gchar *currency;
	gdouble amount;
} GdaMoney;

typedef struct {
	gchar *number;
	glong precision;
	glong width;
} GdaNumeric;

typedef struct {
	gushort hour;
	gushort minute;
	gushort second;
	glong timezone;	/* # of seconds to the east UTC */
} GdaTime;

typedef struct {
	gshort year;
	gushort month;
	gushort day;
	gushort hour;
	gushort minute;
	gushort second;
	gulong fraction;
	glong timezone;	/* # of seconds to the east UTC */
} GdaTimestamp;

typedef GList GdaValueList;
typedef struct {
	GdaValueType type;
	union {
		gint64 v_bigint;
 	        guint64 v_biguint;
		gpointer v_binary;
		GdaBlob v_blob;
		gboolean v_boolean;
		GdaDate v_date;
		gdouble v_double;
		GdaGeometricPoint v_point;
		GObject *v_gobj;
		gint v_integer;
		GdaValueList *v_list;
		GdaMoney v_money;
		GdaNumeric v_numeric;
		gfloat v_single;
		gshort v_smallint;
 	        gushort v_smalluint;
		gchar *v_string;
		GdaTime v_time;
		GdaTimestamp v_timestamp;
		gchar v_tinyint;
 	        guchar v_tinyuint;
		GdaValueType v_type;
		guint v_uinteger;
	} value;
	glong binary_length;
} GdaValue;

/* Note: gda_value_get_type is already defined */
#define GDA_TYPE_VALUE (gda_value_get_gtype())

GType         gda_value_get_gtype (void);
GdaValue     *gda_value_new_null (void);
GdaValue     *gda_value_new_bigint (gint64 val);
GdaValue     *gda_value_new_biguint(guint64 val);
GdaValue     *gda_value_new_binary (gconstpointer val, glong size);
GdaValue     *gda_value_new_blob (const GdaBlob *val);
GdaValue     *gda_value_new_boolean (gboolean val);
GdaValue     *gda_value_new_date (const GdaDate *val);
GdaValue     *gda_value_new_double (gdouble val);
GdaValue     *gda_value_new_geometric_point (const GdaGeometricPoint *val);
GdaValue     *gda_value_new_gobject (const GObject *val);
GdaValue     *gda_value_new_integer (gint val);
GdaValue     *gda_value_new_list (const GdaValueList *val);
GdaValue     *gda_value_new_money (const GdaMoney *val);
GdaValue     *gda_value_new_numeric (const GdaNumeric *val);
GdaValue     *gda_value_new_single (gfloat val);
GdaValue     *gda_value_new_smallint (gshort val);
GdaValue     *gda_value_new_smalluint (gushort val);
GdaValue     *gda_value_new_string (const gchar *val);
GdaValue     *gda_value_new_time (const GdaTime *val);
GdaValue     *gda_value_new_timestamp (const GdaTimestamp *val);
GdaValue     *gda_value_new_timestamp_from_timet (time_t val);
GdaValue     *gda_value_new_tinyint (gchar val);
GdaValue     *gda_value_new_tinyuint (guchar val);
GdaValue     *gda_value_new_type (GdaValueType val);
GdaValue     *gda_value_new_uinteger(guint val);
GdaValue     *gda_value_new_from_string (const gchar *as_string,
					 GdaValueType type);
GdaValue     *gda_value_new_from_xml (const xmlNodePtr node);

void          gda_value_free (GdaValue *value);

GdaValueType  gda_value_get_type (const GdaValue *value);
#define       gda_value_isa(value,type) (gda_value_get_type (value) == type)
gboolean      gda_value_is_null (const GdaValue *value);
gboolean      gda_value_is_number (const GdaValue *value);
GdaValue     *gda_value_copy (const GdaValue *value);

gint64        gda_value_get_bigint (const GdaValue *value);
void          gda_value_set_bigint (GdaValue *value, gint64 val);
guint64       gda_value_get_biguint (const GdaValue *value);
void          gda_value_set_biguint (GdaValue *value, guint64 val);
G_CONST_RETURN gpointer gda_value_get_binary (const GdaValue *value, glong *size);
void          gda_value_set_binary (GdaValue *value, gconstpointer val, glong size);
G_CONST_RETURN GdaBlob *gda_value_get_blob (const GdaValue *value);
void          gda_value_set_blob (GdaValue *value, const GdaBlob *val);
gboolean      gda_value_get_boolean (const GdaValue *value);
void          gda_value_set_boolean (GdaValue *value, gboolean val);
G_CONST_RETURN GdaDate *gda_value_get_date (const GdaValue *value);
void          gda_value_set_date (GdaValue *value, const GdaDate *val);
gdouble       gda_value_get_double (const GdaValue *value);
void          gda_value_set_double (GdaValue *value, gdouble val);
G_CONST_RETURN GdaGeometricPoint *gda_value_get_geometric_point (const GdaValue *value);
void          gda_value_set_geometric_point (GdaValue *value, const GdaGeometricPoint *val);
G_CONST_RETURN GObject *gda_value_get_gobject (const GdaValue *value);
void          gda_value_set_gobject (GdaValue *value, const GObject *val);
gint          gda_value_get_integer (const GdaValue *value);
void          gda_value_set_integer (GdaValue *value, gint val);
G_CONST_RETURN GdaValueList *gda_value_get_list (const GdaValue *value);
void          gda_value_set_list (GdaValue *value, const GdaValueList *val);
void          gda_value_set_null (GdaValue *value);
G_CONST_RETURN GdaMoney *gda_value_get_money (const GdaValue *value);
void          gda_value_set_money (GdaValue *value, const GdaMoney *val);
G_CONST_RETURN GdaNumeric *gda_value_get_numeric (const GdaValue *value);
void          gda_value_set_numeric (GdaValue *value, const GdaNumeric *val);
gfloat        gda_value_get_single (const GdaValue *value);
void          gda_value_set_single (GdaValue *value, gfloat val);
gshort        gda_value_get_smallint (const GdaValue *value);
void          gda_value_set_smallint (GdaValue *value, gshort val);
gushort       gda_value_get_smalluint (const GdaValue *value);
void          gda_value_set_smalluint (GdaValue *value, gushort val);
G_CONST_RETURN gchar  *gda_value_get_string (const GdaValue *value);
void          gda_value_set_string (GdaValue *value, const gchar *val);
G_CONST_RETURN GdaTime *gda_value_get_time (const GdaValue *value);
void          gda_value_set_time (GdaValue *value, const GdaTime *val);
G_CONST_RETURN GdaTimestamp *gda_value_get_timestamp (const GdaValue *value);
void          gda_value_set_timestamp (GdaValue *value, const GdaTimestamp *val);
gchar         gda_value_get_tinyint (const GdaValue *value);
void          gda_value_set_tinyint (GdaValue *value, gchar val);
guchar        gda_value_get_tinyuint (const GdaValue *value);
void          gda_value_set_tinyuint (GdaValue *value, guchar val);
guint         gda_value_get_uinteger (const GdaValue *value);
void          gda_value_set_uinteger (GdaValue *value, guint val);
GdaValueType  gda_value_get_vtype (const GdaValue *value);
void          gda_value_set_vtype (GdaValue *value, GdaValueType type);
gboolean      gda_value_set_from_string (GdaValue *value, 
					 const gchar *as_string,
					 GdaValueType type);
gboolean      gda_value_set_from_value (GdaValue *value, const GdaValue *from);


gint          gda_value_compare (const GdaValue *value1,
				 const GdaValue *value2);
gchar        *gda_value_stringify (const GdaValue *value);
xmlNodePtr    gda_value_to_xml (const GdaValue *value);

G_END_DECLS

#endif
