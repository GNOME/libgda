/* GDA library
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *      Michael Lausch <michael@lausch.at>
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

#if !defined(__gda_row_h__)
#  define __gda_row_h__

#include <libgda/gda-value.h>
#include <libgda/GNOME_Database.h>
#include <glib/gmacros.h>

G_BEGIN_DECLS

typedef GNOME_Database_Row             GdaRow;
typedef GNOME_Database_RowAttributes   GdaRowAttributes;
typedef GNOME_Database_FieldAttributes GdaFieldAttributes;
typedef GNOME_Database_Field           GdaField;
typedef enum {
	GDA_TYPE_NULL = GNOME_Database_TYPE_NULL,
	GDA_TYPE_BIGINT = GNOME_Database_TYPE_BIGINT,
	GDA_TYPE_BINARY = GNOME_Database_TYPE_BINARY,
	GDA_TYPE_BOOLEAN = GNOME_Database_TYPE_BOOLEAN,
	GDA_TYPE_DATE = GNOME_Database_TYPE_DATE,
	GDA_TYPE_DOUBLE = GNOME_Database_TYPE_DOUBLE,
	GDA_TYPE_GEOMETRIC_POINT = GNOME_Database_TYPE_GEOMETRIC_POINT,
	GDA_TYPE_INTEGER = GNOME_Database_TYPE_INTEGER,
	GDA_TYPE_LIST = GNOME_Database_TYPE_LIST,
	GDA_TYPE_SINGLE = GNOME_Database_TYPE_SINGLE,
	GDA_TYPE_SMALLINT = GNOME_Database_TYPE_SMALLINT,
	GDA_TYPE_STRING = GNOME_Database_TYPE_STRING,
	GDA_TYPE_TIME = GNOME_Database_TYPE_TIME,
	GDA_TYPE_TIMESTAMP = GNOME_Database_TYPE_TIMESTAMP,
	GDA_TYPE_TINYINT = GNOME_Database_TYPE_TINYINT,
	GDA_TYPE_UNKNOWN = GNOME_Database_TYPE_UNKNOWN
}
GdaType;

GdaRow             *gda_row_new (gint count);
void                gda_row_free (GdaRow *row);
GdaField           *gda_row_get_field (GdaRow *row, gint num);

GdaRowAttributes   *gda_row_attributes_new (gint count);
void                gda_row_attributes_free (GdaRowAttributes *attrs);
GdaFieldAttributes *gda_row_attributes_get_field (GdaRowAttributes *attrs, gint num);

glong               gda_field_attributes_get_defined_size (GdaFieldAttributes *fa);
void                gda_field_attributes_set_defined_size (GdaFieldAttributes *fa, glong size);
const gchar        *gda_field_attributes_get_name (GdaFieldAttributes *fa);
void                gda_field_attributes_set_name (GdaFieldAttributes *fa, const gchar *name);
glong               gda_field_attributes_get_scale (GdaFieldAttributes *fa);
void                gda_field_attributes_set_scale (GdaFieldAttributes *fa, glong scale);
GdaType             gda_field_attributes_get_gdatype (GdaFieldAttributes *fa);
void                gda_field_attributes_set_gdatype (GdaFieldAttributes *fa, GdaType type);

glong               gda_field_get_actual_size (GdaField *field);
void                gda_field_set_actual_size (GdaField *field, glong size);
glong               gda_field_get_defined_size (GdaField *field);
void                gda_field_set_defined_size (GdaField *field, glong size);
const gchar        *gda_field_get_name (GdaField *field);
void                gda_field_set_name (GdaField *field, const gchar *name);
glong               gda_field_get_scale (GdaField *field);
void                gda_field_set_scale (GdaField *field, glong scale);
GdaType             gda_field_get_gdatype (GdaField *field);
void                gda_field_set_gdatype (GdaField *field, GdaType type);
gboolean            gda_field_is_null (GdaField *field);
GdaValue           *gda_field_get_value (GdaField *field);
void                gda_field_set_value (GdaField *field, const GdaValue *value);
gint64              gda_field_get_bigint_value (GdaField *field);
void                gda_field_set_bigint_value (GdaField *field, gint64 value);
gconstpointer       gda_field_get_binary_value (GdaField *field);
void                gda_field_set_binary_value (GdaField *field, gconstpointer value, glong size);
gboolean            gda_field_get_boolean_value (GdaField *field);
void                gda_field_set_boolean_value (GdaField *field, gboolean value);
const GdaDate      *gda_field_get_date_value (GdaField *field);
void                gda_field_set_date_value (GdaField *field, GdaDate *date);
gdouble             gda_field_get_double_value (GdaField *field);
void                gda_field_set_double_value (GdaField *field, gdouble value);
const GdaGeometricPoint  *gda_field_get_geometric_point_value (GdaField *field);
void                gda_field_set_geometric_point_value (GdaField *field, GdaGeometricPoint *value);
gint                gda_field_get_integer_value (GdaField *field);
void                gda_field_set_integer_value (GdaField *field, gint value);
const GdaValueList *gda_field_get_list_value (GdaField *field);
void                gda_field_set_list_value (GdaField *field, GdaValueList *value);
void                gda_field_set_null_value (GdaField *field);
gfloat              gda_field_get_single_value (GdaField *field);
void                gda_field_set_single_value (GdaField *field, gfloat value);
gshort              gda_field_get_smallint_value (GdaField *field);
void                gda_field_set_smallint_value (GdaField *field, gshort value);
const gchar        *gda_field_get_string_value (GdaField *field);
void                gda_field_set_string_value (GdaField *field, const gchar *value);
const GdaTime      *gda_field_get_time_value (GdaField *field);
void                gda_field_set_time_value (GdaField *field, GdaTime *value);
const GdaTimestamp  *gda_field_get_timestamp_value (GdaField *field);
void                gda_field_set_timestamp_value (GdaField *field, GdaTimestamp *value);
gchar               gda_field_get_tinyint_value (GdaField *field);
void                gda_field_set_tinyint_value (GdaField *field, gchar value);

gchar              *gda_field_stringify (const GdaField *field);

G_END_DECLS

#endif
