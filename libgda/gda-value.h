/*
 * Copyright (C) 2001 - 2003 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2002 - 2003 Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 * Copyright (C) 2002 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2003 Akira TAGOH <tagoh@gnome-db.org>
 * Copyright (C) 2003 Danilo Schoeneberg <dschoene@src.gnome.org>
 * Copyright (C) 2003 Laurent Sansonetti <laurent@datarescue.be>
 * Copyright (C) 2004 - 2007 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2004 Paisa  Seeluangsawat <paisa@users.sf.net>
 * Copyright (C) 2008 Przemysław Grzegorczyk <pgrzegorczyk@gmail.com>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef __GDA_VALUE_H__
#define __GDA_VALUE_H__

#include <time.h>
#include <glib.h>
#include <glib-object.h>
#include <libxml/tree.h>
#include <libgda/gda-decl.h>

G_BEGIN_DECLS

#define GDA_TIMEZONE_INVALID (2*12*60*60)

/* Definition of the GType's values used in GValue*/
#define GDA_TYPE_NULL (gda_null_get_type())
#define GDA_TYPE_DEFAULT (gda_default_get_type())
#define	GDA_TYPE_BINARY (gda_binary_get_type())
#define GDA_TYPE_BLOB (gda_blob_get_type())
#define	GDA_TYPE_GEOMETRIC_POINT (gda_geometricpoint_get_type())
#define	GDA_TYPE_NUMERIC (gda_numeric_get_type())
#define	GDA_TYPE_SHORT (gda_short_get_type()) 
#define	GDA_TYPE_USHORT (gda_ushort_get_type())
#define GDA_TYPE_TIME (gda_time_get_type())
#define GDA_TYPE_TIMESTAMP (gda_timestamp_get_type())


/* Definition of the GDA_VALUE_HOLDS macros */
#define GDA_VALUE_HOLDS_NULL(value)            G_VALUE_HOLDS(value, GDA_TYPE_NULL)
#define GDA_VALUE_HOLDS_DEFAULT(value)         G_VALUE_HOLDS(value, GDA_TYPE_DEFAULT)
#define GDA_VALUE_HOLDS_BINARY(value)          G_VALUE_HOLDS(value, GDA_TYPE_BINARY)
#define GDA_VALUE_HOLDS_BLOB(value)            G_VALUE_HOLDS(value, GDA_TYPE_BLOB)
#define GDA_VALUE_HOLDS_GEOMETRIC_POINT(value) G_VALUE_HOLDS(value, GDA_TYPE_GEOMETRIC_POINT)
#define GDA_VALUE_HOLDS_NUMERIC(value)         G_VALUE_HOLDS(value, GDA_TYPE_NUMERIC)
#define GDA_VALUE_HOLDS_SHORT(value)           G_VALUE_HOLDS(value, GDA_TYPE_SHORT)
#define GDA_VALUE_HOLDS_USHORT(value)          G_VALUE_HOLDS(value, GDA_TYPE_USHORT)
#define GDA_VALUE_HOLDS_TIME(value)            G_VALUE_HOLDS(value, GDA_TYPE_TIME)
#define GDA_VALUE_HOLDS_TIMESTAMP(value)       G_VALUE_HOLDS(value, GDA_TYPE_TIMESTAMP)

/**
 * GdaGeometricPoint:
 * @x:
 * @y:
 */
typedef struct {
	gdouble x;
	gdouble y;
} GdaGeometricPoint;

/**
 * GdaNumeric:
 * @number: a string representing a number
 * @precision: precision to use when @number is converted (not implemented jet)
 * @width: (not implemented jet)
 *
 * Holds numbers represented as strings.
 *
 * This struct must be considered as opaque. Any access to its members must use its
 * accessors added since version 5.0.2.
 *
 * Set value func: gda_value_set_numeric
 * Get value func: gda_value_get_numeric
 */
typedef struct {
	gchar*   GSEAL(number);
	glong    GSEAL(precision);
	glong    GSEAL(width);
	
	/*< private >*/
	gpointer reserved; /* reserved for future usage with GMP (http://gmplib.org/) */
} GdaNumeric;

/**
 * GdaTime:
 * @hour: 
 * @minute: 
 * @second: 
 * @fraction: 
 * @timezone: 
 */
typedef struct {
	gushort hour;
	gushort minute;
	gushort second;
	gulong  fraction;
	glong   timezone;	/* # of seconds to the east UTC */
} GdaTime;

/**
 * GdaTimestamp:
 * @year: representation of the date
 * @month: month representation of the date, as a number between 1 and 12
 * @day: day representation of the date, as a number between 1 and 31
 * @hour: 
 * @minute: 
 * @second: 
 * @fraction: 
 * @timezone:
 */
typedef struct {
	gshort  year;
	gushort month;
	gushort day;
	gushort hour;
	gushort minute;
	gushort second;
	gulong  fraction;
	glong   timezone;	/* # of seconds to the east UTC */
} GdaTimestamp;

/**
 * GdaBinary:
 * @data:
 * @binary_length:
 */
typedef struct {
	guchar *data;
	glong   binary_length;
} GdaBinary;

/**
 * GdaBlob
 * @data: data buffer, as a #GdaBinary
 * @op: (allow-none): a pointer to a #GdaBlopOp, or %NULL
 *
 * Represents some binary data, accessed through a #GdaBlobOp object.
 * @op is generally set up by database providers when giving access to an existing BLOB in
 * a database, but can be modified if needed using gda_blob_set_op().
 */
typedef struct {
	GdaBinary  data;
	GdaBlobOp *op;
} GdaBlob;

#define gda_value_isa(value, type) (G_VALUE_HOLDS(value, type))

/**
 * SECTION:gda-value
 * @short_description: Assorted functions for dealing with #GValue values
 * @title: A single Value
 * @stability: Stable
 * @see_also: #GValue and #GdaBlobOp
 *
 * &LIBGDA; manages each individual value within an opaque #GValue structure. Any GValue type can be used,
 * and &LIBGDA; adds a few more data types usually found in DBMS such as NUMERIC, TIME, TIMESTAMP, GEOMETRIC POINT, BINARY and BLOB.
 *
 * Libgda makes a distinction between binary and blob types
 * <itemizedlist>
 *   <listitem><para>binary data can be inserted into an SQL statement using a
 *	(DBMS dependent) syntax, such as "X'ABCD'" syntax for SQLite or the binary strings syntax for PostgreSQL. Binary data
 *	is manipulated using a #GdaBinary structure (which is basically a bytes buffer and a length attribute).
 *   </para></listitem>
 *   <listitem><para>blob data are a special feature that some DBMS have which requires some non SQL code to manipulate them.
 *	Usually only a reference is stored in each table containing a blob, and the actual blob data resides somewhere on the disk
 *	(while still being managed transparently by the database). For example PotsgreSQL stores blobs as files on the disk and
 *	references them using object identifiers (Oid). Blob data
 *	is manipulated using a #GdaBlob structure which encapsulates a #GdaBinary structure and adds a reference to a
 *	#GdaBlobOp object used to read and write data from and to the blob.
 *   </para></listitem>
 * </itemizedlist>
 * Please note that is distinction between binary data and blobs is Libgda only and does not reflect the DBMS's documentations; 
 * for instance MySQL has several BLOB types but Libgda interprets them as binary types.
 *
 * Each provider or connection can be queried about its blob support using the gda_server_provider_supports_feature() or
 * gda_connection_supports_feature() methods.
 *
 * There are two special value types which are:
 * <itemizedlist>
 *   <listitem><para>the GDA_TYPE_NULL value which never changes and acutally represents an SQL NULL value</para></listitem>
 *   <listitem><para>the GDA_TYPE_DEFAULT value which represents a default value which &LIBGDA; does not interpret (such as when a table column's default value is a function call)</para></listitem>
 * </itemizedlist>
 */

GValue                           *gda_value_new (GType type);

GValue                           *gda_value_new_null (void);
GValue                           *gda_value_new_default (const gchar *default_val);
GValue                           *gda_value_new_binary (const guchar *val, glong size);
GValue                           *gda_value_new_blob (const guchar *val, glong size);
GValue                           *gda_value_new_blob_from_file (const gchar *filename);
GValue                           *gda_value_new_timestamp_from_timet (time_t val);

GValue                           *gda_value_new_from_string (const gchar *as_string, GType type);
GValue                           *gda_value_new_from_xml (const xmlNodePtr node);

void                              gda_value_free (GValue *value);
void                              gda_value_reset_with_type (GValue *value, GType type);

gboolean                          gda_value_is_null (const GValue *value); 
gboolean                          gda_value_is_number (const GValue *value); 
GValue                           *gda_value_copy (const GValue *value);

const GdaBinary         *gda_value_get_binary (const GValue *value);
void                              gda_value_set_binary (GValue *value, const GdaBinary *binary);
void                              gda_value_take_binary (GValue *value, GdaBinary *binary);

const GdaBlob           *gda_value_get_blob (const GValue *value);
void                              gda_value_set_blob (GValue *value, const GdaBlob *blob);
void                              gda_value_take_blob (GValue *value, GdaBlob *blob);

const GdaGeometricPoint *gda_value_get_geometric_point (const GValue *value);
void                              gda_value_set_geometric_point (GValue *value, const GdaGeometricPoint *val);
void                              gda_value_set_null (GValue *value);
const GdaNumeric        *gda_value_get_numeric (const GValue *value);
void                              gda_value_set_numeric (GValue *value, const GdaNumeric *val);
gshort                            gda_value_get_short (const GValue *value);
void                              gda_value_set_short (GValue *value, const gshort val);
gushort                           gda_value_get_ushort (const GValue *value);
void                              gda_value_set_ushort (GValue *value, const gushort val);
const GdaTime           *gda_value_get_time (const GValue *value);
void                              gda_value_set_time (GValue *value, const GdaTime *val);
const GdaTimestamp      *gda_value_get_timestamp (const GValue *value);
void                              gda_value_set_timestamp (GValue *value, const GdaTimestamp *val);




gboolean                          gda_value_set_from_string (GValue *value, 
						             const gchar *as_string,
						             GType type);
gboolean                          gda_value_set_from_value (GValue *value, const GValue *from);

gint                              gda_value_differ (const GValue *value1, const GValue *value2);
gint                              gda_value_compare (const GValue *value1, const GValue *value2);
gchar                            *gda_value_stringify (const GValue *value);
xmlNodePtr                        gda_value_to_xml (const GValue *value);

gchar                            *gda_binary_to_string (const GdaBinary *bin, guint maxlen);
GdaBinary                        *gda_string_to_binary (const gchar *str);

gchar                            *gda_blob_to_string (GdaBlob *blob, guint maxlen);
GdaBlob                          *gda_string_to_blob (const gchar *str);

/* Custom data types */

GType                             gda_null_get_type (void) G_GNUC_CONST;
GType                             gda_default_get_type (void) G_GNUC_CONST;
GType                             gda_numeric_get_type (void) G_GNUC_CONST;
GdaNumeric*                       gda_numeric_new (void);
GdaNumeric*                       gda_numeric_copy (GdaNumeric *src);
void                              gda_numeric_set_from_string (GdaNumeric *numeric, const gchar* str);
void                              gda_numeric_set_double (GdaNumeric *numeric, gdouble number);
gdouble                           gda_numeric_get_double (GdaNumeric *numeric);
void                              gda_numeric_set_precision (GdaNumeric *numeric, glong precision);
glong                             gda_numeric_get_precision (GdaNumeric *numeric);
void                              gda_numeric_set_width (GdaNumeric *numeric, glong width);
glong                             gda_numeric_get_width (GdaNumeric *numeric);
gchar*                            gda_numeric_get_string (GdaNumeric *numeric);
void                              gda_numeric_free (GdaNumeric *numeric);

GType                             gda_time_get_type (void) G_GNUC_CONST;
gpointer                          gda_time_copy (gpointer boxed);
void                              gda_time_free (gpointer boxed);
gboolean                          gda_time_valid (const GdaTime *time);


GType                             gda_timestamp_get_type (void) G_GNUC_CONST;
gpointer                          gda_timestamp_copy (gpointer boxed);
void                              gda_timestamp_free (gpointer boxed);
gboolean                          gda_timestamp_valid (const GdaTimestamp *timestamp);


GType                             gda_geometricpoint_get_type (void) G_GNUC_CONST;
gpointer                          gda_geometricpoint_copy (gpointer boxed);
void                              gda_geometricpoint_free (gpointer boxed);

GType                             gda_binary_get_type (void) G_GNUC_CONST;
gpointer                          gda_binary_copy (gpointer boxed);
void                              gda_binary_free (gpointer boxed);

GType                             gda_blob_get_type (void) G_GNUC_CONST;
gpointer                          gda_blob_copy (gpointer boxed);
void                              gda_blob_free (gpointer boxed);
void                              gda_blob_set_op (GdaBlob *blob, GdaBlobOp *op);

GType                             gda_short_get_type (void) G_GNUC_CONST;
GType                             gda_ushort_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif
