/* GDA library
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *	Michael Lausch <michael@lausch.at>
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *	Juan-Mariano de Goyeneche <jmseyas@dit.upm.es> (BLOB issues) 
 *      Daniel Espinosa Ortiz <esodan@gmail.com> (Port to GValue)
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

/* Definition of the GType's values used in GdaValue*/

#define G_VALUE_TYPE_NULL G_TYPE_NONE
#define G_VALUE_TYPE_BIGINT G_TYPE_INT64
#define	G_VALUE_TYPE_BIGUINT G_TYPE_UINT64
#define	G_VALUE_TYPE_BINARY (gda_binary_get_type())
// G_VALUE_TYPE_BLOB to be defined in gda-blob.h
#define	G_VALUE_TYPE_BOOLEAN G_TYPE_BOOLEAN
#define	G_VALUE_TYPE_DATE (gda_date_get_type())
#define	G_VALUE_TYPE_DOUBLE G_TYPE_DOUBLE
#define	G_VALUE_TYPE_GEOMETRIC_POINT (gda_geometricpoint_get_type())
#define	G_VALUE_TYPE_GOBJECT G_TYPE_OBJECT
#define	G_VALUE_TYPE_INTEGER G_TYPE_INT
#define G_VALUE_TYPE_UINTEGER G_TYPE_UINT
#define	G_VALUE_TYPE_LIST (gda_value_list_get_type())
#define	G_VALUE_TYPE_MONEY (gda_money_get_type())
#define	G_VALUE_TYPE_NUMERIC (gda_numeric_get_type())
#define	G_VALUE_TYPE_SINGLE G_TYPE_FLOAT
#define	G_VALUE_TYPE_SMALLINT (gda_smallint_get_type())
#define	G_VALUE_TYPE_SMALLUINT (gda_smalluint_get_type())
#define	G_VALUE_TYPE_STRING G_TYPE_STRING
#define	G_VALUE_TYPE_TINYINT G_TYPE_CHAR
#define	G_VALUE_TYPE_TINYUINT G_TYPE_UCHAR
#define G_VALUE_TYPE_TIME (gda_time_get_type())
#define G_VALUE_TYPE_TIMESTAMP (gda_timestamp_get_type())
#define	G_VALUE_TYPE_UNKNOWN G_TYPE_INVALID
#define	G_VALUE_TYPE_TYPE (gda_gdatype_get_type())

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

typedef struct {
	gpointer data;
	glong binary_length;
} GdaBinary;

typedef GList GdaValueList;

typedef GValue GdaValue;

#define GDA_TYPE_VALUE G_TYPE_VALUE
#define GDA_VALUE_TYPE(value) (gda_value_get_type(value))
#define gda_value_isa(value, type) (gda_value_get_type (value) == type)

GdaValueType                      gda_value_get_type(GdaValue *value);

GdaValue                         *gda_value_new_null (void);
GdaValue                         *gda_value_new_bigint (gint64 val);
GdaValue                         *gda_value_new_biguint(guint64 val);
GdaValue                         *gda_value_new_binary (gconstpointer val, glong size);
GdaValue                         *gda_value_new_blob (const GdaBlob *val);
GdaValue                         *gda_value_new_boolean (gboolean val);
GdaValue                         *gda_value_new_date (const GdaDate *val);
GdaValue                         *gda_value_new_double (gdouble val);
GdaValue                         *gda_value_new_geometric_point (const GdaGeometricPoint *val);
GdaValue                         *gda_value_new_gobject (const GObject *val);
GdaValue                         *gda_value_new_integer (gint val);
GdaValue                         *gda_value_new_list (const GdaValueList *val);
GdaValue                         *gda_value_new_money (const GdaMoney *val);
GdaValue                         *gda_value_new_numeric (const GdaNumeric *val);
GdaValue                         *gda_value_new_single (gfloat val);
GdaValue                         *gda_value_new_smallint (gshort val);
GdaValue                         *gda_value_new_smalluint (gushort val);
GdaValue                         *gda_value_new_string (const gchar *val);
GdaValue                         *gda_value_new_time (const GdaTime *val);
GdaValue                         *gda_value_new_timestamp (const GdaTimestamp *val);
GdaValue                         *gda_value_new_timestamp_from_timet (time_t val);
GdaValue                         *gda_value_new_tinyint (gchar val);
GdaValue                         *gda_value_new_tinyuint (guchar val);
GdaValue                         *gda_value_new_uinteger(guint val);
GdaValue                         *gda_value_new_gdatype (GdaValueType val);

GdaValue                         *gda_value_new_from_string (const gchar *as_string, GdaValueType type);
GdaValue                         *gda_value_new_from_xml (const xmlNodePtr node);

void                              gda_value_free (GdaValue *value);
void                              gda_value_reset_with_type (GdaValue *value, GdaValueType type);


gboolean                          gda_value_is_null (GdaValue *value);
gboolean                          gda_value_is_number (GdaValue *value);
GdaValue                         *gda_value_copy (GdaValue *value);

gint64                            gda_value_get_bigint (GdaValue *value);
void                              gda_value_set_bigint (GdaValue *value, gint64 val);
guint64                           gda_value_get_biguint (GdaValue *value);
void                              gda_value_set_biguint (GdaValue *value, guint64 val);
G_CONST_RETURN GdaBinary         *gda_value_get_binary (GdaValue *value, glong *size);
void                              gda_value_set_binary (GdaValue *value, gconstpointer val, glong size);
G_CONST_RETURN GdaBlob           *gda_value_get_blob (GdaValue *value);
void                              gda_value_set_blob (GdaValue *value, const GdaBlob *val);
gboolean                          gda_value_get_boolean (GdaValue *value);
void                              gda_value_set_boolean (GdaValue *value, gboolean val);
G_CONST_RETURN GdaDate           *gda_value_get_date (GdaValue *value);
void                              gda_value_set_date (GdaValue *value, const GdaDate *val);
gdouble                           gda_value_get_double (GdaValue *value);
void                              gda_value_set_double (GdaValue *value, gdouble val);
G_CONST_RETURN GdaGeometricPoint *gda_value_get_geometric_point (GdaValue *value);
void                              gda_value_set_geometric_point (GdaValue *value, const GdaGeometricPoint *val);
G_CONST_RETURN GObject           *gda_value_get_gobject (GdaValue *value);
void                              gda_value_set_gobject (GdaValue *value, const GObject *val);
gint                              gda_value_get_integer (GdaValue *value);
void                              gda_value_set_integer (GdaValue *value, gint val);
G_CONST_RETURN GdaValueList      *gda_value_get_list (GdaValue *value);
void                              gda_value_set_list (GdaValue *value, const GdaValueList *val);
void                              gda_value_set_null (GdaValue *value);
G_CONST_RETURN GdaMoney          *gda_value_get_money (GdaValue *value);
void                              gda_value_set_money (GdaValue *value, const GdaMoney *val);
G_CONST_RETURN GdaNumeric        *gda_value_get_numeric (GdaValue *value);
void                              gda_value_set_numeric (GdaValue *value, const GdaNumeric *val);
gfloat                            gda_value_get_single (GdaValue *value);
void                              gda_value_set_single (GdaValue *value, gfloat val);
gshort                            gda_value_get_smallint (GdaValue *value);
void                              gda_value_set_smallint (GdaValue *value, gshort val);
gushort                           gda_value_get_smalluint (GdaValue *value);
void                              gda_value_set_smalluint (GdaValue *value, gushort val);
G_CONST_RETURN gchar             *gda_value_get_string (GdaValue *value);
void                              gda_value_set_string (GdaValue *value, const gchar *val);
G_CONST_RETURN GdaTime           *gda_value_get_time (GdaValue *value);
void                              gda_value_set_time (GdaValue *value, const GdaTime *val);
G_CONST_RETURN GdaTimestamp      *gda_value_get_timestamp (GdaValue *value);
void                              gda_value_set_timestamp (GdaValue *value, const GdaTimestamp *val);
gchar                             gda_value_get_tinyint (GdaValue *value);
void                              gda_value_set_tinyint (GdaValue *value, gchar val);
guchar                            gda_value_get_tinyuint (GdaValue *value);
void                              gda_value_set_tinyuint (GdaValue *value, guchar val);
guint                             gda_value_get_uinteger (GdaValue *value);
void                              gda_value_set_uinteger (GdaValue *value, guint val);
void                              gda_value_set_gdatype (GValue *value, GdaValueType val);
GdaValueType                      gda_value_get_gdatype (GValue *value);


void                              gda_value_set_type (GdaValue *value, GType type);
gboolean                          gda_value_set_from_string (GdaValue *value, 
						                                     const gchar *as_string,
						                                     GdaValueType type);
gboolean                          gda_value_set_from_value (GdaValue *value, const GdaValue *from);


gint                              gda_value_compare (GdaValue *value1, GdaValue *value2);
gint                              gda_value_compare_ext (GdaValue *value1, GdaValue *value2);
gchar                            *gda_value_stringify (GdaValue *value);
xmlNodePtr                        gda_value_to_xml (GdaValue *value);

/* Custom data types */
GType                             gda_money_get_type (void);
gpointer                          gda_money_copy (gpointer boxed);
void                              gda_money_free (gpointer boxed);

GType                             gda_numeric_get_type (void);
gpointer                          gda_numeric_copy (gpointer boxed);
void                              gda_numeric_free (gpointer boxed);

GType                             gda_time_get_type(void);
gpointer                          gda_time_copy (gpointer boxed);
void                              gda_time_free (gpointer boxed);

GType                             gda_timestamp_get_type(void);
gpointer                          gda_timestamp_copy (gpointer boxed);
void                              gda_timestamp_free (gpointer boxed);

GType                             gda_date_get_type(void);
gpointer                          gda_date_copy (gpointer boxed);
void                              gda_date_free (gpointer boxed);

GType                             gda_geometricpoint_get_type(void);
gpointer                          gda_geometricpoint_copy (gpointer boxed);
void                              gda_geometricpoint_free (gpointer boxed);

GType                             gda_binary_get_type(void);
gpointer                          gda_binary_copy (gpointer boxed);
void                              gda_binary_free (gpointer boxed);

GType                             gda_value_list_get_type (void);
GType                             gda_smallint_get_type (void);
GType                             gda_smalluint_get_type (void);
GType                             gda_gdatype_get_type (void);

G_END_DECLS

#endif
