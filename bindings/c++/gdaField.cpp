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

Field::Field() {
	_gda_field = NULL;
}

Field::Field(GdaField *f) {
	_gda_field = f;
}

Field::~Field() {
//	if (_gda_field) gda_field_free(_gda_field);
// FIXME What?! no gda_field_free?!?!
}

GdaField *Field::getCStruct() {
	return _gda_field;
}

void Field::setCStruct(GdaField *f) {
	_gda_field = f;
}

bool Field::isNull() {
	return gda_field_isnull(_gda_field);
}

Value *Field::realValue() {
	Value *v = new gdaValue(_gda_field->real_value);
	return v;
}

Value *Field::origValue() {
	Value *v = new gdaValue(_gda_field->original_value);
	return v;
}

GDA_ValueType Field::typeCode() {
	return _gda_field->real_value->_u.v._d;
}

gchar *Field::typeCodeString() {
	// Convenience function to get the string of the field's value type
	// without resorting to the C commands...
	gchar *a = 0;
	GDA_ValueType t;

	// Create a string, then get the type, then put the type into the
	// string and return the string.
	a = (gchar *) g_malloc(256);
	t = typeCode();
	gda_fieldtype_2_string(a,256,t);
	return a;
}
		
gchar Field::getTinyint() {
	return gda_field_tinyint(_gda_field);
}

glong Field::getBigint() {
	return gda_field_bigint(_gda_field);
}

bool Field::getBoolean() {
	return gda_field_boolean(_gda_field);
}

GDA_Date Field::getDate() {
	return gda_field_date(_gda_field);
}

GDA_DbDate Field::getDBdate() {
	return gda_field_dbdate(_gda_field);
}

GDA_DbTime Field::getDBtime() {
	return gda_field_dbtime(_gda_field);
}

GDA_DbTimestamp Field::getDBtstamp() {
	return gda_field_timestamp(_gda_field);
}

gdouble Field::getDouble() {
	return gda_field_double(_gda_field);
}

glong Field::getInteger() {
	return gda_field_integer(_gda_field);
}

// GDA_VarBinString Field::getVarLenString() {
// 	return gda_field_varbin(_gda_field);
// }

// GDA_VarBinString Field::getFixLenString() {
// 	return gda_field_fixbin(_gda_field);
// }

gchar *Field::getLongVarChar() {
	return gda_field_longvarchar(_gda_field);
}

gfloat Field::getFloat() {
	return gda_field_single(_gda_field);
	// return _gda_field->real_value->_u.v._u.f;
}

gint Field::getSmallInt() {
	return gda_field_smallint(_gda_field);
}

gulong Field::getULongLongInt() {
	return gda_field_ubingint(_gda_field);
}

guint Field::getUSmallInt() {
	return gda_field_usmallint(_gda_field);
}

//gchar *Field::getText() {
//	return getNewString();
//}
//
//gchar *Field::getNewString() {
//	// Find the size of the value, then make a string that big
//	// then put it into the string.
//	// FIXME: Can we use getLongVarChar instead of this? -- cdw
//	gint s = 0;
//	gchar *a = 0;
//	s = gda_field_actual_size(_gda_field);
//	a = (gchar *) g_malloc(s+1);
//	gda_stringify_value(a,s,_gda_field);
//	return a;
//}

gchar *Field::putInString(gchar *bfr, gint maxLength) {
	return gda_stringify_value(bfr,maxLength,_gda_field);
}

gint Field::actualSize() {
	return gda_field_actual_size(_gda_field);
}

glong Field::definedSize() {
	return gda_field_defined_size(_gda_field);
}

gchar *Field::name() {
	return gda_field_name(_gda_field);
}

glong Field::scale() {
	return gda_field_scale(_gda_field);
}

GDA_ValueType Field::type() {
	return gda_field_type(_gda_field);
}

glong Field::cType() {
	return gda_field_cType(_gda_field);
}

glong Field::nativeType() {
	return gda_field_nativeType(_gda_field);
}

