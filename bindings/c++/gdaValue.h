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

#ifndef __gda_bindings_cpp_gdaValueH
#define __gda_bindings_cpp_gdaValueH

namespace gda {

class VarBinString {

	public:
		VarBinString ();
		VarBinString (const GDA_VarBinString& varBinString);
		VarBinString (const VarBinString& varBinString);
		~VarBinString ();

		VarBinString& operator=(const GDA_VarBinString& varBinString);
		VarBinString& operator=(const VarBinString& varBinString);

		CORBA_octet& operator[](unsigned int index);

		unsigned int length ();

	private:

		void freeBuffers ();
		GDA_VarBinString* _GDA_VarBinString;
}; // class VarBinString

class Value {
	friend class Command;
	friend class Field;

	      public:
		Value ();
		Value (const Value& value);
		Value (GDA_FieldValue * fv);
		Value (GDA_Value* v);
		~Value ();

		Value& operator=(const Value& value);


		gchar getTinyInt ();
		glong getBigInt ();
		bool getBoolean ();
		GDA_Date getDate ();
		GDA_DbDate getDBDate ();
		GDA_DbTime getDBTime();
		GDA_DbTimestamp getDBTStamp ();
		gdouble getDouble ();
		glong getInteger ();
		//GDA_VarBinString getVarLenString ();
		//GDA_VarBinString getFixLenString ();
		string getLongVarChar ();
		gfloat getFloat ();
		gint getSmallInt ();
		gulong getULongLongInt ();
		guint getUSmallInt ();

		void set (gchar a);
		void set (glong a);
		void set (bool a);
		void set (GDA_Date a);
		void set (GDA_DbDate a);
		void set (GDA_DbTime a);
		void set (GDA_DbTimestamp a);
		void set (gdouble a);
		// void set(glong a);  // CORBA_long
		//void set (GDA_VarBinString a);
		// void set(GDA_VarBinString a);  // GDA_VarBinString fb; 
		void set (const string& a);
		void set (gfloat a);
		//void set (gint16 a); // CORBA_short
		void set (gulong a);
		void set (guint a);

	      private:
		GDA_Value* _gda_value;

		static void copyValue (const GDA_Value* src, GDA_Value* dst);
	};

};

#endif
