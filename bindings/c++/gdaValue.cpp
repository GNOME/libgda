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

#include "gdaValue.h"

gdaValue::gdaValue() {
	_gda_fieldvalue = NULL;
}

gdaValue::gdaValue(GDA_FieldValue *fv) {
	_gda_fieldvalue = fv;
}

gdaValue::~gdaValue() {
	// no free function!
}
		
GDA_FieldValue *gdaValue::getCStruct() {
	return _gda_fieldvalue;
}

//GDA_Value gdaValue::getCValue() {
//	return _gda_fieldvalue->_u.v;
//}

void gdaValue::setCStruct(GDA_FieldValue *v) {
	_gda_fieldvalue = v;
}

gchar gdaValue::getTinyint() {
	return _gda_fieldvalue->_u.v._u.c;
}

glong gdaValue::getBigint() {
	return _gda_fieldvalue->_u.v._u.ll;
}

bool gdaValue::getBoolean() {
	return _gda_fieldvalue->_u.v._u.b;
}

GDA_Date gdaValue::getDate() {
	return _gda_fieldvalue->_u.v._u.d;
}

GDA_DbDate gdaValue::getDBdate() {
	return _gda_fieldvalue->_u.v._u.dbd;
}

GDA_DbTime gdaValue::getDBtime() {
	return _gda_fieldvalue->_u.v._u.dbt;
}

GDA_DbTimestamp gdaValue::getDBtstamp() {
	return _gda_fieldvalue->_u.v._u.dbtstamp;
}

gdouble gdaValue::getDouble() {
	return _gda_fieldvalue->_u.v._u.dp;
}

glong gdaValue::getInteger() {
	return _gda_fieldvalue->_u.v._u.i;
}

GDA_VarBinString gdaValue::getVarLenString() {
	return _gda_fieldvalue->_u.v._u.lvb;
}

GDA_VarBinString gdaValue::getFixLenString() {
	return _gda_fieldvalue->_u.v._u.fb;
}

gchar *gdaValue::getLongVarChar() {
	return g_strdup(_gda_fieldvalue->_u.v._u.lvc);
}

gfloat gdaValue::getFloat() {
	return _gda_fieldvalue->_u.v._u.f;
}

gint gdaValue::getSmallInt() {
	return _gda_fieldvalue->_u.v._u.si;
}

gulong gdaValue::getULongLongInt() {
	return _gda_fieldvalue->_u.v._u.ull;
}

guint gdaValue::getUSmallInt() {
	return _gda_fieldvalue->_u.v._u.us;
}

void gdaValue::set(gchar a) {
	_gda_fieldvalue->_u.v._u.c = a;
}

//void gdaValue::set(glong a) {
//	_gda_fieldvalue->_u.v._u.ll = a;
//}

void gdaValue::set(bool a) {
	_gda_fieldvalue->_u.v._u.b = a;
}

void gdaValue::set(GDA_Date a) {
	_gda_fieldvalue->_u.v._u.d = a;
}

void gdaValue::set(GDA_DbDate a) {
	_gda_fieldvalue->_u.v._u.dbd = a;
}

void gdaValue::set(GDA_DbTime a) {
	_gda_fieldvalue->_u.v._u.dbt = a;
}

void gdaValue::set(GDA_DbTimestamp a) {
	_gda_fieldvalue->_u.v._u.dbtstamp = a;
}

void gdaValue::set(gdouble a) {
	_gda_fieldvalue->_u.v._u.dp = a;
}

void gdaValue::set(glong a) {
	_gda_fieldvalue->_u.v._u.i = a;
}

void gdaValue::set(GDA_VarBinString a) {
	_gda_fieldvalue->_u.v._u.lvb = a;
}

// void gdaValue::set(GDA_VarBinString a) {
// 	_gda_fieldvalue->_u.v._u.fb = a;
// }

void gdaValue::set(gchar *a) {
	_gda_fieldvalue->_u.v._u.lvc = g_strdup(a);
}

void gdaValue::set(gfloat a) {
	_gda_fieldvalue->_u.v._u.f = a;
}

// void gdaValue::set(gint a) {
// 	_gda_fieldvalue->_u.v._u.ull = a;
// }

void gdaValue::set(gulong a) {
	_gda_fieldvalue->_u.v._u.us = a;
}

void gdaValue::set(guint a) {
	_gda_fieldvalue->_u.v._u.c = a;
}

