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
#include "gdaErrorList.h"

// Okay, this class doesn't exist in the C version - it is a list of errors
// from a CORBA exception. C uses the functions gda_errors_from_exception
// and gda_error_list_free, but I decided that a class which contained
// a list of gdaErrors would make more sense under C++. You *can* do it
// the other way.. -- cdw

using namespace gda;

ErrorList::ErrorList(CORBA_Environment* ev) {
	_errors = gda_error_list_from_exception(ev);
}

ErrorList::ErrorList(GList *errorList) {
	_errors = errorList;
}

ErrorList::~ErrorList() {
	if (_errors) gda_error_list_free(_errors);
}

GList* ErrorList::getCStruct() {
	return _errors;
}

GList *ErrorList::errors() {
	return _errors;
}

void ErrorList::setCStruct(GList *errorList) {
	_errors = errorList;
}


  
  
