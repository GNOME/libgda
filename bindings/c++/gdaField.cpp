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

Field::Field ()
{
	_gda_field = NULL;
	_gda_recordset = NULL;

	setCStruct (gda_field_new ());
	allocBuffers ();
}

Field::Field (const Field& field)
{
	_gda_field = NULL;
	_gda_recordset = NULL;

	setCStruct (field.getCStruct ());

	_gda_recordset = field._gda_recordset;
}

Field::Field (GdaField * f)
{
	_gda_field = NULL;
	_gda_recordset = NULL;

	setCStruct (f);
}

Field::Field (GdaField* f, GdaRecordset* recordset)
{
	_gda_field = NULL;
	_gda_recordset = NULL;

	setCStruct (f);

	_gda_recordset = recordset;
}

Field::~Field ()
{
	// since there is no gda_field_free ()
	//
	unref ();

	_gda_field = NULL;
}

Field&
Field::operator=(const Field& field)
{
	// order is meaningful
	//
	setCStruct (field.getCStruct ());
	_gda_recordset = field._gda_recordset;

	return *this;
}

bool
Field::isValid ()
{
	return (NULL != _gda_field);
}

bool
Field::isNull ()
{
	g_assert (isValid ());

	return gda_field_is_null (_gda_field);
}

//Value
//Field::realValue ()
//{
//	g_assert (isValid ());a
//
//	Value v (_gda_field->real_value);
//	v.ref ();
//
//	return v;
//}

//Value
//Field::origValue ()
//{
//	g_assert (isValid ());
//
//	Value v (_gda_field->original_value);
//	v.ref ();
//
//	return v;
//}

GDA_ValueType
Field::typeCode ()
{
	g_assert (isValid ());

	return _gda_field->real_value->_u.v._d;
}

string
Field::typeCodeString ()
{
	g_assert (isValid ());

	return Field::fieldType2String (typeCode ());
}

Value
Field::getValue ()
{
	g_assert (isValid ());

	{
		Value value;

		if (true == isValid () && _gda_field->real_value != NULL && _gda_field->real_value->_d) {
			Value::copyValue (&(_gda_field->real_value->_u.v), value._gda_value);
		}

		return value;
	}
}

gchar
Field::getTinyInt ()
{
	g_assert (isValid ());

	return gda_field_get_tinyint_value (_gda_field);
}

glong
Field::getBigInt ()
{
	g_assert (isValid ());

	return gda_field_get_bigint_value (_gda_field);
}

bool
Field::getBoolean ()
{
	g_assert (isValid ());

	return gda_field_get_boolean_value (_gda_field);
}

GDate
Field::getDate ()
{
	g_assert (isValid ());

	GDate* gdate =  gda_field_get_date_value (_gda_field);
	if (NULL == gdate) {
		gdate = g_date_new ();
}

	GDate date = *gdate;

	g_date_free (gdate);

	return date;
}

/*
GDA_DbDate
Field::getDBDate ()
{
	g_assert (isValid ());

	return gda_field_get_dbdate_value (_gda_field);
}
*/
/*
GDA_DbTime
Field::getDBTime ()
{
	g_assert (isValid ());

	return gda_field_get_dbtime_value (_gda_field);
}
*/
/*
GDA_DbTimestamp
Field::getDBTStamp ()
{
	g_assert (isValid ());

	return gda_field_get_timestamp_value (_gda_field);
}
*/

time_t
Field::getTime ()
{
	g_assert (isValid ());

	//return gda_field_get_time_value (_gda_field);
	return -1;
}

time_t
Field::getTimestamp ()
{
	g_assert (isValid ());

	return gda_field_get_timestamp_value (_gda_field);
}

gdouble
Field::getDouble ()
{
	g_assert (isValid ());

	return gda_field_get_double_value (_gda_field);
}

glong
Field::getInteger ()
{
	g_assert (isValid ());

	return gda_field_get_integer_value (_gda_field);
}

VarBinString
Field::getBinary ()
{
	g_assert (isValid ());

	VarBinString varBinString;
	varBinString = _gda_field->real_value->_u.v._u.lvb;

	return varBinString;
}

/*
GDA_VarBinString Field::getVarLenString ()
{
 g_assert (isValid ());

 return gda_field_varbin (_gda_field);
}

GDA_VarBinString Field::getFixLenString ()
{
 g_assert (isValid ());

 return gda_field_fixbin (_gda_field);
}

string
Field::getLongVarChar ()
{
	g_assert (isValid ());

	return gda_return_string (gda_field_longvarchar (_gda_field));
}
*/

string
Field::getString ()
{
	g_assert (isValid ());

	//return gda_return_string (gda_field_get_string_value (_gda_field));
	return "";
}

gfloat
Field::getSingle ()
{
	g_assert (isValid ());

	return gda_field_get_single_value (_gda_field);
	// return _gda_field->real_value->_u.v._u.f;
}

gint
Field::getSmallInt ()
{
	g_assert (isValid ());

	return gda_field_get_smallint_value (_gda_field);
}

/*
gulong
Field::getUBigInt ()
{
	g_assert (isValid ());

	return gda_field_get_ubingint_value (_gda_field);
}
*/

guint
Field::getUSmallInt ()
{
	g_assert (isValid ());

	return gda_field_get_usmallint_value (_gda_field);
}

string
Field::fieldType2String (GDA_ValueType type)
{
	return gda_return_string (gda_fieldtype_2_string (NULL, 0, type));
}

GDA_ValueType
Field::string2FieldType (const string& type)
{
	return gda_string_2_fieldtype (const_cast<gchar*>(type.c_str ()));
}

string
Field::stringifyValue ()
{
	return gda_return_string (gda_stringify_value (NULL, 0, _gda_field));
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

//gchar*
//Field::putInString (gchar *bfr, gint maxLength)
//{
//	return gda_stringify_value (bfr, maxLength, _gda_field);
//}

gint
Field::actualSize ()
{
	g_assert (isValid ());

	return gda_field_actual_size (_gda_field);
}

glong
Field::definedSize ()
{
	g_assert (isValid ());

	return gda_field_get_defined_size (_gda_field);
}

string
Field::name ()
{
	g_assert (isValid ());

	string name = const_cast<const char*>(gda_field_get_name (_gda_field));

	return name;
}

glong
Field::scale ()
{
	g_assert (isValid ());

	return gda_field_get_scale (_gda_field);
}

GDA_ValueType
Field::gdaType ()
{
	g_assert (isValid ());

	return gda_field_get_gdatype (_gda_field);
}

glong
Field::cType ()
{
	g_assert (isValid ());

	return gda_field_get_ctype (_gda_field);
}

glong
Field::nativeType ()
{
	g_assert (isValid ());

	return gda_field_get_nativetype (_gda_field);
}

GdaField*
Field::getCStruct (bool refn = true) const
{
	if (refn) ref ();
	return _gda_field;
}

void
Field::setCStruct (GdaField *f)
{
	unref ();
	_gda_field = f;
}



void
Field::ref () const
{
	if (NULL == _gda_field) {
		g_warning ("gda::Field::ref () received NULL pointer");
	}
	else {
#ifdef HAVE_GOBJECT
		g_object_ref (G_OBJECT (_gda_field));
#else
		gtk_object_ref (GTK_OBJECT (_gda_field));
#endif
	}
}

void
Field::unref ()
{
	if (_gda_field != NULL) {
		// When field created with Recordset::fieldIdx () or fieldName ()
		// method, the buffers are part of recordset - we leave them alone.
		// Possible time hazard here with ref_count incremented from other
		// thread
		//
		if (_gda_recordset != NULL) {
			if (1 == GTK_OBJECT(_gda_field)->ref_count) {
				detachBuffers ();
			}

			_gda_recordset = NULL;
		}
		else {

			freeBuffers ();
		}

		// since there is no gda_field_free ()
		//
#ifdef HAVE_GOBJECT
		g_object_unref (G_OBJECT(_gda_field));
#else
		gtk_object_unref (GTK_OBJECT (_gda_field));
#endif
	}
}

void
Field::allocBuffers ()
{
	g_assert (_gda_recordset == NULL);

	if (_gda_field != NULL) {
		_gda_field->attributes = GDA_FieldAttributes__alloc ();
		_gda_field->real_value = GDA_FieldValue__alloc ();
		_gda_field->shadow_value = GDA_FieldValue__alloc ();
		_gda_field->original_value = GDA_FieldValue__alloc ();
	}
}

void
Field::detachBuffers ()
{
	g_assert (_gda_recordset != NULL);

	if (_gda_field != NULL) {
	  _gda_field->attributes = NULL;
	  _gda_field->real_value = NULL;
	  _gda_field->shadow_value = NULL;
	  _gda_field->original_value = NULL;
	}
}

void
Field::freeBuffers ()
{
	g_assert (_gda_recordset == NULL);

	CORBA_free (_gda_field->attributes);
	CORBA_free (_gda_field->real_value);
	CORBA_free (_gda_field->shadow_value);
	CORBA_free (_gda_field->original_value);
}
