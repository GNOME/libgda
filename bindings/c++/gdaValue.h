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

#include "gdaIncludes.h"

namespace gda
{

	class Value
	{
	      public:
		Value ();
		Value (GDA_FieldValue * fv);
		~Value ();

		GDA_FieldValue *getCStruct ();
		//      GDA_Value getCValue();
		void setCStruct (GDA_FieldValue * fv);

		gchar getTinyint ();
		glong getBigint ();
		bool getBoolean ();
		GDA_Date getDate ();
		GDA_DbDate getDBdate ();
		GDA_DbTime getDBtime ();
		GDA_DbTimestamp getDBtstamp ();
		gdouble getDouble ();
		glong getInteger ();
		GDA_VarBinString getVarLenString ();
		GDA_VarBinString getFixLenString ();
		gchar *getLongVarChar ();
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
		void set (GDA_VarBinString a);
		// void set(GDA_VarBinString a);  // GDA_VarBinString fb; 
		void set (gchar * a);
		void set (gfloat a);
		// void set(gint a); // CORBA_short
		void set (gulong a);
		void set (guint a);

	      private:
		  GDA_FieldValue * _gda_fieldvalue;
	};

};

#endif
