/* GNOME DB libary
 * Copyright (C) 2000 Chris Wiegand
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "config.h"
#include "gdaField.h"

using namespace gda;

Field::Field ()
{
	_gda_field = NULL;
}

Field::Field (GdaField * f)
{
	_gda_field = f;
}

Field::~Field ()
{
	if (_gda_field)
		gda_field_free (_gda_field);
}

GdaField *
Field::getCStruct ()
{
	return _gda_field;
}

void
Field::setCStruct (GdaField * f)
{
	_gda_field = f;
}

bool
Field::isNull ()
{
	return gda_field_is_null (_gda_field);
}

Value *
Field::realValue ()
{
	Value *v = new Value (_gda_field->real_value);
	return v;
}

Value *
Field::origValue ()
{
	Value *v = new Value (_gda_field->original_value);
	return v;
}

GDA_ValueType
Field::typeCode ()
{
	return _gda_field->real_value->_u.v._d;
}

gchar *
Field::typeCodeString ()
{
	// Convenience function to get the string of the field's value type
	// without resorting to the C commands...
	gchar *a = 0;
	GDA_ValueType t;

	// Create a string, then get the type, then put the type into the
	// string and return the string.
	a = (gchar *) g_malloc (256);
	t = typeCode ();
	gda_fieldtype_2_string (a, 256, t);
	return a;
}

gchar
Field::getTinyint ()
{
	return gda_field_get_tinyint_value (_gda_field);
}

glong
Field::getBigint ()
{
	return gda_field_get_bigint_value (_gda_field);
}

bool
Field::getBoolean ()
{
	return gda_field_get_boolean_value (_gda_field);
}

GDate *
Field::getDate ()
{
	return gda_field_get_date_value (_gda_field);
}

time_t
Field::getTime ()
{
	//return gda_field_get_time_value (_gda_field);
	return -1;
}

time_t
Field::getTimestamp ()
{
	return gda_field_get_timestamp_value (_gda_field);
}

gdouble
Field::getDouble ()
{
	return gda_field_get_double_value (_gda_field);
}

glong
Field::getInteger ()
{
	return gda_field_get_integer_value (_gda_field);
}

gchar *
Field::getString ()
{
	return gda_field_get_string_value (_gda_field);
}

gfloat
Field::getSingle ()
{
	return gda_field_get_single_value (_gda_field);
}

gint
Field::getSmallInt ()
{
	return gda_field_get_smallint_value (_gda_field);
}

gulong
Field::getUBigInt ()
{
	return gda_field_get_ubingint_value (_gda_field);
}

guint
Field::getUSmallInt ()
{
	return gda_field_get_usmallint_value (_gda_field);
}

//gchar *Field::getText() {
//      return getNewString();
//}
//
//gchar *Field::getNewString() {
//      // Find the size of the value, then make a string that big
//      // then put it into the string.
//      // FIXME: Can we use getLongVarChar instead of this? -- cdw
//      gint s = 0;
//      gchar *a = 0;
//      s = gda_field_actual_size(_gda_field);
//      a = (gchar *) g_malloc(s+1);
//      gda_stringify_value(a,s,_gda_field);
//      return a;
//}

gchar *
Field::putInString (gchar * bfr, gint maxLength)
{
	return gda_stringify_value (bfr, maxLength, _gda_field);
}

gint
Field::actualSize ()
{
	return gda_field_get_actual_size (_gda_field);
}

glong
Field::definedSize ()
{
	return gda_field_get_defined_size (_gda_field);
}

gchar *
Field::name ()
{
	return gda_field_get_name (_gda_field);
}

glong
Field::scale ()
{
	return gda_field_get_scale (_gda_field);
}

GDA_ValueType
Field::gdaType ()
{
	return gda_field_get_gdatype (_gda_field);
}

glong
Field::cType ()
{
	return gda_field_get_ctype (_gda_field);
}

glong
Field::nativeType ()
{
	return gda_field_get_nativetype (_gda_field);
}
