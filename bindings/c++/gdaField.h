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

#include "gdaIncludes.h"

namespace gda
{

	class Field
	{
	      public:
		Field ();
		Field (GdaField * f);
		~Field ();

		GdaField *getCStruct ();
		void setCStruct (GdaField * f);

		Value *realValue ();
		Value *origValue ();
		// What's shadowValue for? FIXME

		bool isNull ();
		GDA_ValueType typeCode ();
		gchar *typeCodeString ();

		gchar getTinyint ();
		glong getBigint ();
		bool getBoolean ();
		GDate *getDate ();
		time_t getTime ();
		time_t getTimestamp ();
		gdouble getDouble ();
		glong getInteger ();
		gchar *getString ();
		gfloat getSingle ();
		gint getSmallInt ();
		gulong getUBigInt ();
		guint getUSmallInt ();

		//      gchar *getText();
		//      gchar *getNewString();
		gchar *putInString (gchar * bfr, gint maxLength);

		gint actualSize ();
		glong definedSize ();
		gchar *name ();
		glong scale ();
		GDA_ValueType gdaType ();
		glong cType ();
		glong nativeType ();

	      private:
		  GdaField * _gda_field;
	};

};

#endif
