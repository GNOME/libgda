/* GDA common libary
 * Copyright (C) 1998,1999 Michael Lausch
 * Copyright (C) 2000,2001 Rodrigo Moya
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

G_BEGIN_DECLS

typedef struct _GdaField        GdaField;
typedef struct _GdaFieldClass   GdaFieldClass;
typedef struct _GdaFieldPrivate GdaFieldPrivate;

#define GDA_TYPE_FIELD            (gda_field_get_type())
#define GDA_FIELD(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_FIELD, GdaField))
#define GDA_FIELD_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_FIELD, GdaFieldClass))
#define GDA_IS_FIELD(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_FIELD))
#define GDA_IS_FIELD_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_FIELD))

struct _GdaField {
	GObject *object;
	GdaFieldPrivate *priv;
};

struct _GdaFieldClass {
	GObjectClass parent_class;
};

GType                    gda_field_get_type (void);

GdaField                *gda_field_new (void);
void                     gda_field_free (GdaField *field);

glong                    gda_field_get_actual_size (GdaField *field);
void                     gda_field_set_actual_size (GdaField *field, glong size);
glong                    gda_field_get_defined_size (GdaField *field);
void                     gda_field_set_defined_size (GdaField *field, glong size);
const gchar             *gda_field_get_name (GdaField *field);
void                     gda_field_set_name (GdaField *field, const gchar *name);
glong                    gda_field_get_scale (GdaField *field);
void                     gda_field_set_scale (GdaField *field, glong scale);
GNOME_Database_ValueType gda_field_get_gdatype (GdaField *field);
void                     gda_field_set_gdatype (GdaField *field,
						GNOME_Database_ValueType type);

gboolean                 gda_field_is_null (GdaField *field);
GdaValue                *gda_field_get_value (GdaField *field);
void                     gda_field_set_value (GdaField *field, GdaValue *value);

long long                gda_field_get_bigint_value (GdaField *field);
void                     gda_field_set_bigint_value (GdaField *field, long long value);
gconstpointer            gda_field_get_binary_value (GdaField *field);
void                     gda_field_set_binary_value (GdaField *field, gconstpointer value, glong size);
gboolean                 gda_field_get_boolean_value (GdaField *field);
void                     gda_field_set_boolean_value (GdaField *field, gboolean value);
GDate                   *gda_field_get_date_value (GdaField *field);
void                     gda_field_set_date_value (GdaField *field, GDate *date);
gdouble                  gda_field_get_double_value (GdaField *field);
void                     gda_field_set_double_value (GdaField *field, gdouble value);
gint                     gda_field_get_integer_value (GdaField *field);
void                     gda_field_set_integer_value (GdaField *field, gint value);
void                     gda_field_set_null_value (GdaField *field);
gfloat                   gda_field_get_single_value (GdaField *field);
void                     gda_field_set_single_value (GdaField *field, gfloat value);
gshort                   gda_field_get_smallint_value (GdaField *field);
void                     gda_field_set_smallint_value (GdaField *field, gshort value);
const gchar             *gda_field_get_string_value (GdaField *field);
void                     gda_field_set_string_value (GdaField *field, const gchar *value);
GTime                    gda_field_get_time_value (GdaField *field);
void                     gda_field_set_time_value (GdaField *field, GTime value);
time_t                   gda_field_get_timestamp_value (GdaField *field);
void                     gda_field_set_timestamp_value (GdaField *field, time_t value);
gchar                    gda_field_get_tinyint_value (GdaField *field);
void                     gda_field_set_tinyint_value (GdaField *field, gchar value);

void                     gda_field_copy_to_corba (GdaField *field, GNOME_Database_Field *corba_field);
void                     gda_field_copy_to_corba_attributes (GdaField *field,
							     GNOME_Database_FieldAttributes *attrs);
gchar                   *gda_field_stringify (GdaField *field);

G_END_DECLS

#endif
