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

#ifndef __gda_bindings_cpp_gdaErrorListH
#define __gda_bindings_cpp_gdaErrorListH

namespace gda
{

class Error;

	class ErrorList
: public vector<Error>
	{

	friend class Connection;

	      public:
		ErrorList ();
		ErrorList (CORBA_Environment * ev);
		ErrorList (GList * errorList);
		ErrorList (const ErrorList& errorList);
		~ErrorList ();

		ErrorList& operator=(const ErrorList& errorList);

	      private:
		ErrorList glist2vector (GList* errorList, bool freeList = false);
		GList* vector2glist (ErrorList& errorList);

		GList* errors ();
	};

};

#endif // __gda_bindings_cpp_gdaErrorListH
