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

#include "gdaField.h"

gdaField::gdaField() {
	_gda_field = NULL;
}

gdaField::gdaField(Gda_Field *f) {
	_gda_field = f;
}

gdaField::~gdaField() {
//	if (_gda_field) gda_field_free(_gda_field);
// FIXME What?! no gda_field_free?!?!
}

Gda_Field *gdaField::getCStruct() {
	return _gda_field;
}

void gdaField::setCStruct(Gda_Field *f) {
	_gda_field = f;
}

bool gdaField::isNull() {
	return gda_field_isnull(_gda_field);
}

gdaValue *gdaField::realValue() {
	gdaValue *v = new gdaValue(_gda_field->real_value);
	return v;
}

gdaValue *gdaField::origValue() {
	gdaValue *v = new gdaValue(_gda_field->original_value);
	return v;
}

GDA_ValueType gdaField::typeCode() {
	return _gda_field->real_value->_u.v._d;
}

gchar *gdaField::typeCodeString() {
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
		
gchar gdaField::getTinyint() {
	return gda_field_tinyint(_gda_field);
}

glong gdaField::getBigint() {
	return gda_field_bigint(_gda_field);
}

bool gdaField::getBoolean() {
	return gda_field_boolean(_gda_field);
}

GDA_Date gdaField::getDate() {
	return gda_field_date(_gda_field);
}

GDA_DbDate gdaField::getDBdate() {
	return gda_field_dbdate(_gda_field);
}

GDA_DbTime gdaField::getDBtime() {
	return gda_field_dbtime(_gda_field);
}

GDA_DbTimestamp gdaField::getDBtstamp() {
	return gda_field_timestamp(_gda_field);
}

gdouble gdaField::getDouble() {
	return gda_field_double(_gda_field);
}

glong gdaField::getInteger() {
	return gda_field_integer(_gda_field);
}

// GDA_VarBinString gdaField::getVarLenString() {
// 	return gda_field_varbin(_gda_field);
// }

// GDA_VarBinString gdaField::getFixLenString() {
// 	return gda_field_fixbin(_gda_field);
// }

gchar *gdaField::getLongVarChar() {
	return gda_field_longvarchar(_gda_field);
}

gfloat gdaField::getFloat() {
	return gda_field_single(_gda_field);
	// return _gda_field->real_value->_u.v._u.f;
}

gint gdaField::getSmallInt() {
	return gda_field_smallint(_gda_field);
}

gulong gdaField::getULongLongInt() {
	return gda_field_ubingint(_gda_field);
}

guint gdaField::getUSmallInt() {
	return gda_field_usmallint(_gda_field);
}

//gchar *gdaField::getText() {
//	return getNewString();
//}
//
//gchar *gdaField::getNewString() {
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

gchar *gdaField::putInString(gchar *bfr, gint maxLength) {
	return gda_stringify_value(bfr,maxLength,_gda_field);
}

gint gdaField::actualSize() {
	return gda_field_actual_size(_gda_field);
}

glong gdaField::definedSize() {
	return gda_field_defined_size(_gda_field);
}

gchar *gdaField::name() {
	return gda_field_name(_gda_field);
}

glong gdaField::scale() {
	return gda_field_scale(_gda_field);
}

GDA_ValueType gdaField::type() {
	return gda_field_type(_gda_field);
}

glong gdaField::cType() {
	return gda_field_cType(_gda_field);
}

glong gdaField::nativeType() {
	return gda_field_nativeType(_gda_field);
}

