/* GDA client libary
 * Copyright (C) 1998,1999 Michael Lausch
 * Copyright (C) 2000-2001 Rodrigo Moya
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

#ifndef __gda_field_h__
#define __gda_field_h__ 1

#include <glib.h>
#include <gtk/gtkobject.h>
#include <GDA.h>
#include <gda-common-defs.h>

G_BEGIN_DECLS

typedef struct _GdaField      GdaField;
typedef struct _GdaFieldClass GdaFieldClass;

#define GDA_TYPE_FIELD            (gda_field_get_type())
#define GDA_FIELD(obj)            GTK_CHECK_CAST(obj, GDA_TYPE_FIELD, GdaField)
#define GDA_FIELD_CLASS(klass)    GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_FIELD, GdaFieldClass)
#define GDA_IS_FIELD(obj)         GTK_CHECK_TYPE(obj, GDA_TYPE_FIELD)
#define GDA_IS_FIELD_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_FIELD))

struct _GdaField {
	GtkObject object;

	GDA_FieldAttributes *attributes;
	gint actual_length;
	GDA_FieldValue *real_value;
	GDA_FieldValue *shadow_value;
	GDA_FieldValue *original_value;
};

struct _GdaFieldClass {
	GtkObjectClass parent_class;
};

#define gda_field_is_null(f)             (f->real_value ? (f)->real_value->_d : 1)

#define gda_field_get_tinyint_value(f)   ((f)->real_value->_u.v._u.c)
#define gda_field_get_bigint_value(f)    ((f)->real_value->_u.v._u.ll)
#define gda_field_get_boolean_value(f)   ((f)->real_value->_u.v._u.b)
GDate  *gda_field_get_date_value         (GdaField *field);
#define gda_field_get_time_value(f)      ((f)->real_value->_u.v._u.dbt)
time_t  gda_field_get_timestamp_value    (GdaField *field);
#define gda_field_get_double_value(f)    ((f)->real_value->_u.v._u.dp)
#define gda_field_get_integer_value(f)   ((f)->real_value->_u.v._u.i)
#define gda_field_get_binary_value(f)    ((f)->real_value->_u.v._u.lvb._buffer)
#define gda_field_get_string_value(f)    ((f)->real_value->_u.v._u.lvc)
#define gda_field_get_single_value(fld)  ((fld)->real_value->_u.v._u.f)
#define gda_field_get_smallint_value(f)  ((f)->real_value->_u.v._u.si)
#define gda_field_get_ubigint_value(f)  ((f)->real_value->_u.v._u.ull)
#define gda_field_get_usmallint_value(f) ((f)->real_value->_u.v._u.us)

guint         gda_field_get_type (void);
GdaField     *gda_field_new (void);
void          gda_field_free (GdaField *field);

gint          gda_field_actual_size (GdaField * f);
#define       gda_field_get_defined_size(f) (f->attributes->definedSize)
#define       gda_field_get_name(f)         (f->attributes->name)
#define       gda_field_get_scale(f)        (f->attributes->scale)
#define       gda_field_get_gdatype(f)      (f->attributes->gdaType)
#define       gda_field_get_ctype(f)        (f->attributes->cType)
#define       gda_field_get_nativetype(f)   (f->attributes->nativeType)

gchar        *gda_fieldtype_2_string (gchar * bfr, gint length,
				      GDA_ValueType type);
GDA_ValueType gda_string_2_fieldtype (gchar * type);
gchar        *gda_stringify_value (gchar * bfr, gint length, GdaField * f);

G_END_DECLS

#endif
