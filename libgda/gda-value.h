/*
 * Copyright (C) 2001 - 2003 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2002 - 2003 Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 * Copyright (C) 2002 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2003 Akira TAGOH <tagoh@gnome-db.org>
 * Copyright (C) 2003 Danilo Schoeneberg <dschoene@src.gnome.org>
 * Copyright (C) 2003 Laurent Sansonetti <laurent@datarescue.be>
 * Copyright (C) 2004 - 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2004 Paisa  Seeluangsawat <paisa@users.sf.net>
 * Copyright (C) 2008 Przemysław Grzegorczyk <pgrzegorczyk@gmail.com>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2011 - 2017 Daniel Espinosa <esodan@gmail.com>
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
#define GDA_TYPE_BLOB (gda_blob_get_type())
#define	GDA_TYPE_GEOMETRIC_POINT (gda_geometric_point_get_type())
#define	GDA_TYPE_SHORT (gda_short_get_type()) 
#define	GDA_TYPE_USHORT (gda_ushort_get_type())
#define GDA_TYPE_TIME (gda_time_get_type())
#define GDA_TYPE_TEXT (gda_text_get_type())

/* Definition of the GDA_VALUE_HOLDS macros */
#define GDA_VALUE_HOLDS_NULL(value)            G_VALUE_HOLDS(value, GDA_TYPE_NULL)
#define GDA_VALUE_HOLDS_DEFAULT(value)         G_VALUE_HOLDS(value, GDA_TYPE_DEFAULT)
#define GDA_VALUE_HOLDS_BLOB(value)            G_VALUE_HOLDS(value, GDA_TYPE_BLOB)
#define GDA_VALUE_HOLDS_GEOMETRIC_POINT(value) G_VALUE_HOLDS(value, GDA_TYPE_GEOMETRIC_POINT)
#define GDA_VALUE_HOLDS_SHORT(value)           G_VALUE_HOLDS(value, GDA_TYPE_SHORT)
#define GDA_VALUE_HOLDS_USHORT(value)          G_VALUE_HOLDS(value, GDA_TYPE_USHORT)
#define GDA_VALUE_HOLDS_TIME(value)            G_VALUE_HOLDS(value, GDA_TYPE_TIME)
#define GDA_VALUE_HOLDS_TEXT(value)            G_VALUE_HOLDS(value, GDA_TYPE_TEXT)

/* GdaText */

typedef struct _GdaText GdaText;

GType                             gda_text_get_type (void) G_GNUC_CONST;
GdaText                          *gda_text_new ();
void                              gda_text_free (GdaText *text);
const gchar                      *gda_text_get_string (GdaText *text);
void                              gda_text_set_string (GdaText *text, const gchar *str);
void                              gda_text_take_string (GdaText *text, gchar *str);

/* GdaNumeric */
typedef struct _GdaNumeric GdaNumeric;

#define	GDA_TYPE_NUMERIC (gda_numeric_get_type())
#define GDA_VALUE_HOLDS_NUMERIC(value)         G_VALUE_HOLDS(value, GDA_TYPE_NUMERIC)

GType                             gda_numeric_get_type (void) G_GNUC_CONST;
GdaNumeric*                       gda_numeric_new (void);
GdaNumeric*                       gda_numeric_copy (GdaNumeric *src);
void                              gda_numeric_set_from_string (GdaNumeric *numeric, const gchar* str);
void                              gda_numeric_set_double (GdaNumeric *numeric, gdouble number);
gdouble                           gda_numeric_get_double (const GdaNumeric *numeric);
void                              gda_numeric_set_precision (GdaNumeric *numeric, glong precision);
glong                             gda_numeric_get_precision (const GdaNumeric *numeric);
void                              gda_numeric_set_width (GdaNumeric *numeric, glong width);
glong                             gda_numeric_get_width (const GdaNumeric *numeric);
gchar*                            gda_numeric_get_string (const GdaNumeric *numeric);
void                              gda_numeric_free (GdaNumeric *numeric);

/**
 * GdaTime:
 *
 * Represents a time information.
 */
typedef struct _GDateTime GdaTime;

GType                             gda_time_get_type (void) G_GNUC_CONST;
GdaTime*                          gda_time_new (void);
GdaTime*                          gda_time_new_from_date_time (GDateTime *dt);
GdaTime*                          gda_time_new_from_values (gushort hour, gushort minute, gushort second, gulong fraction, glong timezone);
GdaTime*                          gda_time_copy (const GdaTime* time);
void                              gda_time_free (GdaTime* time);

gushort                           gda_time_get_hour (const GdaTime* time);
void                              gda_time_set_hour (GdaTime* time, gushort hour);
gushort                           gda_time_get_minute (const GdaTime* time);
void                              gda_time_set_minute (GdaTime* time, gushort minute);
gushort                           gda_time_get_second (const GdaTime* time);
void                              gda_time_set_second (GdaTime* time, gushort second);
gulong                            gda_time_get_fraction (const GdaTime *time);
void                              gda_time_set_fraction (GdaTime* time, gulong fraction);
GTimeZone*                        gda_time_get_tz (const GdaTime *time);
glong                             gda_time_get_timezone (const GdaTime *time);

G_DEPRECATED
gboolean                          gda_time_valid (const GdaTime *time);
GdaTime                          *gda_time_to_timezone (GdaTime *time, GTimeZone *ntz);
GdaTime                          *gda_time_to_utc (GdaTime *time);
gchar*                            gda_time_to_string (GdaTime *time);
gchar*                            gda_time_to_string_local (GdaTime *time);
gchar*                            gda_time_to_string_utc (GdaTime *time);

/**
 * GdaBinary: (ref-func gda_binary_new) (unref-func gda_binary_free) (get-value-func gda_value_get_binary) (set-value-func gda_value_set_binary)
 */
typedef struct _GdaBinary GdaBinary;

#define GDA_TYPE_BINARY (gda_binary_get_type ())
#define GDA_VALUE_HOLDS_BINARY(value)          G_VALUE_HOLDS(value, GDA_TYPE_BINARY)

GValue*                           gda_value_new_binary (const guchar *val, glong size);
GdaBinary*                        gda_value_get_binary (const GValue *value);
void                              gda_value_set_binary (GValue *value, GdaBinary *binary);
void                              gda_value_take_binary (GValue *value, GdaBinary *binary);


GType                             gda_binary_get_type (void) G_GNUC_CONST;
GdaBinary*                        gda_binary_new (void);
glong                             gda_binary_get_size (const GdaBinary* binary);
gpointer                          gda_binary_get_data (GdaBinary* binary);
void                              gda_binary_reset_data (GdaBinary* binary);
void                              gda_binary_set_data (GdaBinary *binary, const guchar *val, glong size);
void                              gda_binary_take_data (GdaBinary *binary, guchar *val, glong size);
GdaBinary*                        gda_binary_copy (GdaBinary *src);
void                              gda_binary_free (GdaBinary *binary);

/**
 * GdaBlob:
 * @data: data buffer, as a #GdaBinary
 * @op: (nullable): a pointer to a #GdaBlopOp, or %NULL
 *
 * Represents some binary data, accessed through a #GdaBlobOp object.
 * @op is generally set up by database providers when giving access to an existing BLOB in
 * a database, but can be modified if needed using gda_blob_set_op().
 */
typedef struct _GdaBlob GdaBlob;


GType                             gda_blob_get_type (void) G_GNUC_CONST;
GdaBlob*                          gda_blob_new (void);
GdaBinary*                        gda_blob_get_binary (GdaBlob *blob);
GdaBlobOp*                        gda_blob_get_op (GdaBlob *blob);
GdaBlob*                          gda_blob_copy (GdaBlob *src);
void                              gda_blob_free (GdaBlob *blob);
void                              gda_blob_set_op (GdaBlob *blob, GdaBlobOp *op);


#define gda_value_isa(value, type) (G_VALUE_HOLDS(value, type))

/**
 * SECTION:gda-value
 * @short_description: Assorted functions for dealing with single #GValue values
 * @title: GdaValue
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
GValue                           *gda_value_new_blob (const guchar *val, glong size);
GValue                           *gda_value_new_blob_from_file (const gchar *filename);
GValue                           *gda_value_new_time_from_timet (time_t val);
GValue                           *gda_value_new_date_time_from_timet (time_t val);

GValue                           *gda_value_new_from_string (const gchar *as_string, GType type);
GValue                           *gda_value_new_from_xml (const xmlNodePtr node);

void                              gda_value_free (GValue *value);
void                              gda_value_reset_with_type (GValue *value, GType type);

gboolean                          gda_value_is_null (const GValue *value); 
gboolean                          gda_value_is_number (const GValue *value); 
GValue                           *gda_value_copy (const GValue *value);


const GdaBlob                    *gda_value_get_blob (const GValue *value);
void                              gda_value_set_blob (GValue *value, const GdaBlob *blob);
void                              gda_value_take_blob (GValue *value, GdaBlob *blob);

typedef struct _GdaGeometricPoint GdaGeometricPoint;
GType                             gda_geometric_point_get_type (void) G_GNUC_CONST;
GdaGeometricPoint                *gda_geometric_point_new (void);
gdouble                           gda_geometric_point_get_x (GdaGeometricPoint* gp);
void                              gda_geometric_point_set_x (GdaGeometricPoint* gp, double x);
gdouble                           gda_geometric_point_get_y (GdaGeometricPoint* gp);
void                              gda_geometric_point_set_y (GdaGeometricPoint* gp, double y);
GdaGeometricPoint                *gda_geometric_point_copy (GdaGeometricPoint *gp);
void                              gda_geometric_point_free (GdaGeometricPoint *gp);
const GdaGeometricPoint          *gda_value_get_geometric_point (const GValue *value);
void                              gda_value_set_geometric_point (GValue *value, const GdaGeometricPoint *val);

void                              gda_value_set_null (GValue *value);

const GdaNumeric                 *gda_value_get_numeric (const GValue *value);
void                              gda_value_set_numeric (GValue *value, const GdaNumeric *val);
gshort                            gda_value_get_short (const GValue *value);
void                              gda_value_set_short (GValue *value, const gshort val);
gushort                           gda_value_get_ushort (const GValue *value);
void                              gda_value_set_ushort (GValue *value, const gushort val);
const GdaTime                    *gda_value_get_time (const GValue *value);
void                              gda_value_set_time (GValue *value, const GdaTime *val);
void                              gda_time_set_timezone (GdaTime* time, glong timezone);




gboolean                          gda_value_set_from_string (GValue *value, 
						             const gchar *as_string,
						             GType type);
gboolean                          gda_value_set_from_value (GValue *value, const GValue *from);

gint                              gda_value_differ (const GValue *value1, const GValue *value2);
gint                              gda_value_compare (const GValue *value1, const GValue *value2);
gchar                            *gda_value_stringify (const GValue *value);
xmlNodePtr                        gda_value_to_xml (const GValue *value);
gchar*                            gda_value_to_xml_string (const GValue *value);

gchar                            *gda_binary_to_string (const GdaBinary *bin, guint maxlen);
GdaBinary                        *gda_string_to_binary (const gchar *str);

gchar                            *gda_blob_to_string (GdaBlob *blob, guint maxlen);
GdaBlob                          *gda_string_to_blob (const gchar *str);

/* Custom data types */

GType                             gda_null_get_type (void) G_GNUC_CONST;
GType                             gda_default_get_type (void) G_GNUC_CONST;





/* Timestamp based on GDateTime */

GDateTime                        *gda_date_time_copy (GDateTime *ts);

GType                             gda_short_get_type (void) G_GNUC_CONST;
GType                             gda_ushort_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif
