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

#ifndef __gda_bindings_cpp_gdaErrorH
#define __gda_bindings_cpp_gdaErrorH

namespace gda {

class Connection;
class ErrorList;

class Error {

	friend class Connection;
	friend class ErrorList;

	      public:
		Error ();
		Error (const Error& error);
		Error (GdaError * e);
		~Error ();

		Error& operator=(const Error& error);
		

		string description ();
		glong number ();
		string source ();
		string helpurl ();
		string sqlstate();
		string nativeMsg();
		string realcommand();

	      private:
		// manual operations on contained C object not allowed; sorry folks!
		//
		GdaError *getCStruct (bool refn = true) const;
		void setCStruct (GdaError *e);

		void ref () const;
		void unref ();

		  GdaError * _gda_error;
	};

};

#endif // __gda_bindings_cpp_gdaErrorH
