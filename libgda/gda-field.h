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

#if !defined(__gda_field_h__)
#  define __gda_field_h__

#include <libgda/gda-value.h>
#include <glib/gmacros.h>

G_BEGIN_DECLS

typedef struct {
	gint defined_size;
	gchar *name;
	gchar *caption;
	gint scale;
	GdaValueType gda_type;
	gboolean allow_null;
	gboolean primary_key;
	gboolean unique_key;
	gchar *references;
	gboolean auto_increment;
	glong auto_increment_start;
	glong auto_increment_step;
} GdaFieldAttributes;

typedef struct {
	gint actual_size;
	GdaValue *value;
	GdaFieldAttributes *attributes;
} GdaField;

GdaFieldAttributes *gda_field_attributes_new (void);
void                gda_field_attributes_free (GdaFieldAttributes *fa);
glong               gda_field_attributes_get_defined_size (GdaFieldAttributes *fa);
void                gda_field_attributes_set_defined_size (GdaFieldAttributes *fa, glong size);
const gchar        *gda_field_attributes_get_name (GdaFieldAttributes *fa);
void                gda_field_attributes_set_name (GdaFieldAttributes *fa, const gchar *name);
const gchar        *gda_field_attributes_get_caption (GdaFieldAttributes *fa);
void                gda_field_attributes_set_caption (GdaFieldAttributes *fa, const gchar *caption);
glong               gda_field_attributes_get_scale (GdaFieldAttributes *fa);
void                gda_field_attributes_set_scale (GdaFieldAttributes *fa, glong scale);
GdaValueType        gda_field_attributes_get_gdatype (GdaFieldAttributes *fa);
void                gda_field_attributes_set_gdatype (GdaFieldAttributes *fa,
						      GdaValueType type);
gboolean            gda_field_attributes_get_allow_null (GdaFieldAttributes *fa);
void                gda_field_attributes_set_allow_null (GdaFieldAttributes *fa, gboolean allow);
gboolean            gda_field_attributes_get_primary_key (GdaFieldAttributes *fa);
void                gda_field_attributes_set_primary_key (GdaFieldAttributes *fa, gboolean pk);
gboolean            gda_field_attributes_get_unique_key (GdaFieldAttributes *fa);
void                gda_field_attributes_set_unique_key (GdaFieldAttributes *fa, gboolean uk);
const gchar        *gda_field_attributes_get_references (GdaFieldAttributes *fa);
void                gda_field_attributes_set_references (GdaFieldAttributes *fa, const gchar *ref);

GdaField           *gda_field_new (void);
void                gda_field_free (GdaField *field);
glong               gda_field_get_actual_size (GdaField *field);
void                gda_field_set_actual_size (GdaField *field, glong size);
glong               gda_field_get_defined_size (GdaField *field);
void                gda_field_set_defined_size (GdaField *field, glong size);
const gchar        *gda_field_get_name (GdaField *field);
void                gda_field_set_name (GdaField *field, const gchar *name);
const gchar        *gda_field_get_caption (GdaField *field);
void                gda_field_set_caption (GdaField *field, const gchar *caption);
glong               gda_field_get_scale (GdaField *field);
void                gda_field_set_scale (GdaField *field, glong scale);
GdaValueType        gda_field_get_gdatype (GdaField *field);
void                gda_field_set_gdatype (GdaField *field, GdaValueType type);
gboolean            gda_field_get_allow_null (GdaField *field);
void                gda_field_set_allow_null (GdaField *field, gboolean allow);
gboolean            gda_field_get_primary_key (GdaField *field);
void                gda_field_set_primary_key (GdaField *field, gboolean pk);
gboolean            gda_field_get_unique_key (GdaField *field);
void                gda_field_set_unique_key (GdaField *field, gboolean uk);
const gchar        *gda_field_get_references (GdaField *field);
void                gda_field_set_references (GdaField *field, const gchar *ref);
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
const GdaNumeric   *gda_field_get_numeric_value (GdaField *field);
void                gda_field_set_numeric_value (GdaField *field, GdaNumeric *value);
gfloat              gda_field_get_single_value (GdaField *field);
void                gda_field_set_single_value (GdaField *field, gfloat value);
gshort              gda_field_get_smallint_value (GdaField *field);
void                gda_field_set_smallint_value (GdaField *field, gshort value);
const gchar        *gda_field_get_string_value (GdaField *field);
void                gda_field_set_string_value (GdaField *field, const gchar *value);
const GdaTime      *gda_field_get_time_value (GdaField *field);
void                gda_field_set_time_value (GdaField *field, GdaTime *value);
const GdaTimestamp *gda_field_get_timestamp_value (GdaField *field);
void                gda_field_set_timestamp_value (GdaField *field, GdaTimestamp *value);
gchar               gda_field_get_tinyint_value (GdaField *field);
void                gda_field_set_tinyint_value (GdaField *field, gchar value);

gchar              *gda_field_stringify (GdaField *field);

G_END_DECLS

#endif
