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
#include "gdaIncludes.h"
#include "gdaHelpers.h"

using namespace gda;

VarBinString::VarBinString ()
{
	_GDA_VarBinString = NULL;
}

VarBinString::VarBinString (const GDA_VarBinString& varBinString)
{
	this->operator=(varBinString);
}

VarBinString::VarBinString (const VarBinString& varBinString)
{
	this->operator=(varBinString);
}

VarBinString::~VarBinString ()
{
	freeBuffers ();
}

VarBinString&
VarBinString::operator=(const GDA_VarBinString& varBinString)
{
	freeBuffers ();
	if (varBinString._length != 0 && varBinString._buffer != NULL) {
		_GDA_VarBinString = GDA_VarBinString__alloc ();
		_GDA_VarBinString->_buffer =
			CORBA_sequence_CORBA_octet_allocbuf (varBinString._length);
		memcpy (
			_GDA_VarBinString->_buffer,
			varBinString._buffer,
			varBinString._length);
		_GDA_VarBinString->_length = varBinString._length;
	}

	return *this;
}

VarBinString&
VarBinString::operator=(const VarBinString& varBinString)
{
	if (NULL == varBinString._GDA_VarBinString) {
		freeBuffers ();
	}
	else {
		this->operator=(*(varBinString._GDA_VarBinString));
	}

	return *this;
}

CORBA_octet&
VarBinString::operator[](unsigned int index)
{
	g_assert (_GDA_VarBinString != NULL);
	g_assert (index >= 0);
	g_assert (index < length ());

	return _GDA_VarBinString->_buffer [index];
}

unsigned int
VarBinString::length ()
{
	if (NULL == _GDA_VarBinString) {
		return 0;
	}
	else {
		return _GDA_VarBinString->_length;
	}
}

void
VarBinString::freeBuffers ()
{
	if (_GDA_VarBinString != NULL &&
		_GDA_VarBinString->_length > 0 &&
		_GDA_VarBinString->_buffer != NULL) {
		CORBA_free (_GDA_VarBinString);
		_GDA_VarBinString = NULL;
	}
}


Value::Value ()
{
	_gda_value = GDA_Value__alloc ();;
}

Value::Value (const Value& value)
{
	_gda_value = GDA_Value__alloc ();
	copyValue (value._gda_value, _gda_value);
}

Value::Value (GDA_FieldValue *fv)
{
	_gda_value = GDA_Value__alloc ();

	if (fv != NULL && fv->_d) {
		copyValue (&(fv->_u.v), _gda_value);
	}
}

Value::Value (GDA_Value *v)
{
	_gda_value = v;

	if (NULL == _gda_value) {
		_gda_value = GDA_Value__alloc ();
	}
}

Value::~Value()
{
	CORBA_free (_gda_value);
}

Value&
Value::operator=(const Value& value)
{
	_gda_value = GDA_Value__alloc ();
	copyValue (value._gda_value, _gda_value);

	return *this;
}

gchar
Value::getTinyInt ()
{
	return _gda_value->_u.c;
}

glong
Value::getBigInt ()
{
	return _gda_value->_u.ll;
}

bool
Value::getBoolean ()
{
	return _gda_value->_u.b;
}

GDA_Date
Value::getDate ()
{
	return _gda_value->_u.d;
}

GDA_DbDate
Value::getDBDate ()
{
	return _gda_value->_u.dbd;
}

GDA_DbTime
Value::getDBTime ()
{
	return _gda_value->_u.dbt;
}

GDA_DbTimestamp
Value::getDBTStamp ()
{
	return _gda_value->_u.dbtstamp;
}

gdouble
Value::getDouble ()
{
	return _gda_value->_u.dp;
}

glong
Value::getInteger ()
{
	return _gda_value->_u.i;
}

//GDA_VarBinString
//Value::getVarLenString ()
//{
//	return _gda_fieldvalue._u.v._u.lvb;
//}

//GDA_VarBinString
//Value::getFixLenString ()
//{
//	return _gda_fieldvalue._u.v._u.fb;
//}

string
Value::getLongVarChar ()
{
	return gda_return_string (g_strdup (_gda_value->_u.lvc));
}

gfloat
Value::getFloat ()
{
	return _gda_value->_u.f;
}

gint
Value::getSmallInt ()
{
	return _gda_value->_u.si;
}

gulong
Value::getULongLongInt ()
{
	return _gda_value->_u.ull;
}

guint
Value::getUSmallInt ()
{
	return _gda_value->_u.us;
}

void
Value::set (gchar a)
{
	_gda_value->_u.c = a;
}

void Value::set (glong a)
{
	_gda_value->_u.ll = a;
}

void
Value::set (bool a)
{
	_gda_value->_u.b = a;
}

void
Value::set (GDA_Date a)
{
	_gda_value->_u.d = a;
}

void
Value::set (GDA_DbDate a)
{
	_gda_value->_u.dbd = a;
}

void
Value::set (GDA_DbTime a)
{
	_gda_value->_u.dbt = a;
}

void
Value::set (GDA_DbTimestamp a)
{
	_gda_value->_u.dbtstamp = a;
}

void
Value::set (gdouble a)
{
	_gda_value->_u.dp = a;
}

//void
//Value::set (glong a)
//{
//	_gda_fieldvalue._u.v._u.i = a;
//}

//void
//Value::set (GDA_VarBinString a)
//{
//	_gda_fieldvalue._u.v._u.lvb = a;
//}

// void Value::set(GDA_VarBinString a) {
// 	_gda_fieldvalue._u.v._u.fb = a;
// }

void
Value::set (const string& a)
{
	_gda_value->_u.lvc = g_strdup (a.c_str ());
}

void
Value::set (gfloat a)
{
	_gda_value->_u.f = a;
}

//void Value::set(gint a)
//{
//	_gda_fieldvalue._u.v._u.ull = a;
// }

void
Value::set (gulong a)
{
	_gda_value->_u.us = a;
}

void
Value::set (guint a)
{
	_gda_value->_u.c = a;
}

// based upon GDA-common.c GDA_Value__free ()
//
void
Value::copyValue (const GDA_Value* src, GDA_Value* dst)
{
	if (NULL == src || NULL == dst) {
		g_warning ("gda::Value::copyValue received NULL pointer");
	}
	else {
		memset (dst, 0, sizeof (GDA_Value));
		switch (src->_d)
		{
			case GDA_TypeTinyint:
			case GDA_TypeBigint:
			case GDA_TypeBoolean:
			case GDA_TypeDate:
			case GDA_TypeDbDate:
			case GDA_TypeDbTime:
			case GDA_TypeDbTimestamp:
			case GDA_TypeDouble:
			case GDA_TypeInteger:
				memmove (dst, src, sizeof (GDA_Value));
				break;

			case GDA_TypeBinary:
			case GDA_TypeVarbin:
			case GDA_TypeVarwchar:
			case GDA_TypeLongvarwchar:
			case GDA_TypeLongvarbin:
			case GDA_TypeUnknown:
				//dst->_u.lvb = CORBA_sequence_CORBA_octet__alloc ();
				dst->_u.lvb._length = src->_u.lvb._length;
				dst->_u.lvb._buffer = CORBA_sequence_CORBA_octet_allocbuf (src->_u.lvb._length);
				memmove (dst->_u.lvb._buffer, src->_u.lvb._buffer, dst->_u.lvb._length);
				break;

			case GDA_TypeFixbin:
			case GDA_TypeFixwchar:
			case GDA_TypeFixchar:
				//dst->_u.fb = CORBA_sequence_CORBA_octet__alloc ();
				dst->_u.fb._length = src->_u.fb._length;
				dst->_u.fb._buffer = CORBA_sequence_CORBA_octet_allocbuf (src->_u.fb._length);
				memmove (dst->_u.fb._buffer, src->_u.fb._buffer, dst->_u.fb._length);
				break;

			case GDA_TypeCurrency:
			case GDA_TypeDecimal:
			case GDA_TypeNumeric:
			case GDA_TypeVarchar:
			case GDA_TypeLongvarchar:
				dst->_u.lvc = CORBA_string_dup (src->_u.lvc);
				break;

			case GDA_TypeSingle:
			case GDA_TypeSmallint:
			case GDA_TypeUBigint:
			case GDA_TypeUSmallint:
				memmove (dst, src, sizeof (GDA_Value));
				break;

			default:
				g_error ("gda::Value::copyValue: Unknown GDA Value type.");
				break;
}

		dst->_d = src->_d;
	}
}

