/* GDA client libary
 * Copyright (C) 1998,1999 Michael Lausch
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

#ifndef __gda_field_h__
#define __gda_field_h__ 1

#include <glib.h>
#ifdef HAVE_GOBJECT
#  include <glib-object.h>
#else
#  include <gtk/gtkobject.h>
#endif
#include <GDA.h>
#include <gda-common-defs.h>

G_BEGIN_DECLS

typedef struct _GdaField GdaField;
typedef struct _GdaFieldClass GdaFieldClass;

#define GDA_TYPE_FIELD            (gda_field_get_type())

#ifdef HAVE_GOBJECT
#  define GDA_FIELD(obj) \
            G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_FIELD, GdaField)
#  define GDA_FIELD_CLASS(klass) \
            G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_FIELD, GdaFieldClass)
#  define GDA_IS_FIELD(obj) \
            G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_FIELD)
#  define GDA_IS_FIELD_CLASS(klass) \
            G_TYPE_CHECK_CLASS_TYPE (klass, GDA_TYPE_FIELD)
#else
#  define GDA_FIELD(obj)            GTK_CHECK_CAST(obj, GDA_TYPE_FIELD, GdaField)
#  define GDA_FIELD_CLASS(klass)    GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_FIELD, GdaFieldClass)
#  define GDA_IS_FIELD(obj)         GTK_CHECK_TYPE(obj, GDA_TYPE_FIELD)
#  define GDA_IS_FIELD_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_FIELD))
#endif

struct _GdaField {
#ifdef HAVE_GOBJECT
	GObject object;
#else
	GtkObject object;
#endif
	GDA_FieldAttributes *attributes;
	gint actual_length;
	GDA_FieldValue *real_value;
	GDA_FieldValue *shadow_value;
	GDA_FieldValue *original_value;
};

struct _GdaFieldClass {
#ifdef HAVE_GOBJECT
	GObjectClass parent_class;
#else
	GtkObjectClass parent_class;
#endif
};

#define gda_field_isnull(f)         (f->real_value ? (f)->real_value->_d : 1)
#define gda_field_typecode(f)       ((f)->real_value->_u._d)
#define gda_field_tinyint(f)        ((f)->real_value->_u.v._u.c)
#define gda_field_typechar(f)       ((f)->real_value->_u.v._u.c1)
#define gda_field_bigint(f)         ((f)->real_value->_u.v._u.ll)
#define gda_field_bstr(f)           ((f)->real_value->_u.v._u.s)
#define gda_field_boolean(f)        ((f)->real_value->_u.v._u.b)
#define gda_field_date(f)           ((f)->real_value->_u.v._u.d)
#define gda_field_dbdate(f)         ((f)->real_value->_u.v._u.dbd)
#define gda_field_dbtime(f)         ((f)->real_value->_u.v._u.dbt)
#define gda_field_timestamp(f)      ((f)->real_value->_u.v._u.dbtstamp)
#define gda_field_dec(f)            ((f)->real_value->_u.v._u.dec)
#define gda_field_double(f)         ((f)->real_value->_u.v._u.dp)
#define gda_field_integer(f)        ((f)->real_value->_u.v._u.i)
#define gda_field_varbin(f)         ((f)->real_value->_u.v._u.lvb._buffer)
#define gda_field_varbin_length(f)  ((f)->real_value->_u.v._u.lvb._length)
#define gda_field_fixbin(f)         ((f)->real_value->_u.v._u.fb._buffer)
#define gda_field_fixbin_length(f)  ((f)->real_value->_u.v._u.fb._length)
#define gda_field_longvarchar(f)    ((f)->real_value->_u.v._u.lvc)
#define gda_field_single(fld)       ((fld)->real_value->_u.v._u.f)
#define gda_field_smallint(f)       ((f)->real_value->_u.v._u.si)
#define gda_field_ubingint(f)       ((f)->real_value->_u.v._u.ull)
#define gda_field_usmallint(f)      ((f)->real_value->_u.v._u.us)

guint gda_field_get_type (void);
GdaField *gda_field_new (void);
gchar *gda_fieldtype_2_string (gchar * bfr, gint length,
			       GDA_ValueType type);
GDA_ValueType gda_string_2_fieldtype (gchar * type);
gchar *gda_stringify_value (gchar * bfr, gint length, GdaField * f);

gint gda_field_actual_size (GdaField * f);
#define       gda_field_defined_size(f) (f->attributes->definedSize)
#define       gda_field_name(f)         (f->attributes->name)
#define       gda_field_scale(f)        (f->attributes->scale)
#define       gda_field_type(f)         (f->attributes->gdaType)
#define       gda_field_cType(f)        (f->attributes->cType)
#define       gda_field_nativeType(f)   (f->attributes->nativeType)

G_END_DECLS

#endif
