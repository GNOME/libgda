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

#include "gdaIncludes.h"

class gdaErrorList {
	public:
		gdaErrorList(CORBA_Environment* ev);
		gdaErrorList(GList *errorList);
		~gdaErrorList();
		
		GList *getCStruct();
		void setCStruct(GList *errorList);
		GList* errors();

	private:
		GList* _errors;
};
							     
#endif  
  
