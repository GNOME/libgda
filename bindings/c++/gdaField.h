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

#ifndef __gda_bindings_cpp_gdaFieldH
#define __gda_bindings_cpp_gdaFieldH

namespace gda {

class Field {

	friend class Recordset;

	      public:
		Field ();
		Field (const Field& field);
		Field (GdaField * f);
		~Field ();

		Field& operator=(const Field& field);
		

		//Value realValue ();
		//Value origValue ();
		// What's shadowValue for? FIXME

		// this function does not exist in C implementation; function returns
		// true if contained GdaField object is not NULL
		//
		bool isValid ();

		bool isNull ();
		GDA_ValueType typeCode ();
		string typeCodeString ();

		Value getValue ();
		gchar getTinyInt ();
		glong getBigInt ();
		bool getBoolean ();
		GDate getDate ();		//###
	//	GDA_DbDate getDBDate ();	//###
	//	GDA_DbTime getDBTime ();	//###
	//	GDA_DbTimestamp getDBTStamp ();	//###
		time_t getTime ();
		time_t getTimestamp ();
		gdouble getDouble ();
		glong getInteger ();
		VarBinString getBinary ();
	//	GDA_VarBinString getVarLenString();
	//	GDA_VarBinString getFixLenString();
	//	string getLongVarChar ();
		string getString ();
		gfloat getSingle ();
		gint getSmallInt ();
	//	gulong getUBigInt ();

		guint getUSmallInt ();

		static string fieldType2String (GDA_ValueType type);
		static GDA_ValueType string2FieldType (const string& type);

		string stringifyValue ();
	//	gchar *getText
		//      gchar *getNewString();
	//	gchar *putInString (gchar *bfr, gint maxLength);

		gint actualSize ();
		glong definedSize ();
		string name ();
		glong scale ();
		GDA_ValueType gdaType ();
		glong cType ();
		glong nativeType ();

	      private:
		Field (GdaField* f, GdaRecordset* recordset);

		// manual operations on contained C object not allowed; sorry folks!
		//
		GdaField *getCStruct (bool refn = true) const;
		void setCStruct (GdaField *f);

		void ref () const;
		void unref ();

		void allocBuffers ();
		void detachBuffers ();
		void freeBuffers ();

		  GdaField * _gda_field;
		GdaRecordset* _gda_recordset;
	};

};

#endif
