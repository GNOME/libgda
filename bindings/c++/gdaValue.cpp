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
#include "gdaValue.h"

using namespace gda;

Value::Value ()
{
	_gda_fieldvalue = NULL;
}

Value::Value (GDA_FieldValue * fv)
{
	_gda_fieldvalue = fv;
}

Value::~Value ()
{
	// no free function!
}

GDA_FieldValue *
Value::getCStruct ()
{
	return _gda_fieldvalue;
}

//GDA_Value Value::getCValue() {
//      return _gda_fieldvalue->_u.v;
//}

void
Value::setCStruct (GDA_FieldValue * v)
{
	_gda_fieldvalue = v;
}

gchar
Value::getTinyint ()
{
	return _gda_fieldvalue->_u.v._u.c;
}

glong
Value::getBigint ()
{
	return _gda_fieldvalue->_u.v._u.ll;
}

bool
Value::getBoolean ()
{
	return _gda_fieldvalue->_u.v._u.b;
}

GDA_Date
Value::getDate ()
{
	return _gda_fieldvalue->_u.v._u.d;
}

GDA_DbDate
Value::getDBdate ()
{
	return _gda_fieldvalue->_u.v._u.dbd;
}

GDA_DbTime
Value::getDBtime ()
{
	return _gda_fieldvalue->_u.v._u.dbt;
}

GDA_DbTimestamp
Value::getDBtstamp ()
{
	return _gda_fieldvalue->_u.v._u.dbtstamp;
}

gdouble
Value::getDouble ()
{
	return _gda_fieldvalue->_u.v._u.dp;
}

glong
Value::getInteger ()
{
	return _gda_fieldvalue->_u.v._u.i;
}

GDA_VarBinString
Value::getVarLenString ()
{
	return _gda_fieldvalue->_u.v._u.lvb;
}

GDA_VarBinString
Value::getFixLenString ()
{
	return _gda_fieldvalue->_u.v._u.fb;
}

gchar *
Value::getLongVarChar ()
{
	return g_strdup (_gda_fieldvalue->_u.v._u.lvc);
}

gfloat
Value::getFloat ()
{
	return _gda_fieldvalue->_u.v._u.f;
}

gint
Value::getSmallInt ()
{
	return _gda_fieldvalue->_u.v._u.si;
}

gulong
Value::getULongLongInt ()
{
	return _gda_fieldvalue->_u.v._u.ull;
}

guint
Value::getUSmallInt ()
{
	return _gda_fieldvalue->_u.v._u.us;
}

void
Value::set (gchar a)
{
	_gda_fieldvalue->_u.v._u.c = a;
}

//void Value::set(glong a) {
//      _gda_fieldvalue->_u.v._u.ll = a;
//}

void
Value::set (bool a)
{
	_gda_fieldvalue->_u.v._u.b = a;
}

void
Value::set (GDA_Date a)
{
	_gda_fieldvalue->_u.v._u.d = a;
}

void
Value::set (GDA_DbDate a)
{
	_gda_fieldvalue->_u.v._u.dbd = a;
}

void
Value::set (GDA_DbTime a)
{
	_gda_fieldvalue->_u.v._u.dbt = a;
}

void
Value::set (GDA_DbTimestamp a)
{
	_gda_fieldvalue->_u.v._u.dbtstamp = a;
}

void
Value::set (gdouble a)
{
	_gda_fieldvalue->_u.v._u.dp = a;
}

void
Value::set (glong a)
{
	_gda_fieldvalue->_u.v._u.i = a;
}

void
Value::set (GDA_VarBinString a)
{
	_gda_fieldvalue->_u.v._u.lvb = a;
}

// void Value::set(GDA_VarBinString a) {
//      _gda_fieldvalue->_u.v._u.fb = a;
// }

void
Value::set (gchar * a)
{
	_gda_fieldvalue->_u.v._u.lvc = g_strdup (a);
}

void
Value::set (gfloat a)
{
	_gda_fieldvalue->_u.v._u.f = a;
}

// void Value::set(gint a) {
//      _gda_fieldvalue->_u.v._u.ull = a;
// }

void
Value::set (gulong a)
{
	_gda_fieldvalue->_u.v._u.us = a;
}

void
Value::set (guint a)
{
	_gda_fieldvalue->_u.v._u.c = a;
}
