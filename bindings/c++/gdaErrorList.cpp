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

// Okay, this class doesn't exist in the C version - it is a list of errors
// from a CORBA exception. C uses the functions gda_errors_from_exception
// and gda_error_list_free, but I decided that a class which contained
// a list of gdaErrors would make more sense under C++. You *can* do it
// the other way.. -- cdw

using namespace gda;

ErrorList::ErrorList ()
{
}

ErrorList::ErrorList (CORBA_Environment * ev)
{
	*this = glist2vector (gda_error_list_from_exception (ev));
}

ErrorList::ErrorList (GList * errorList)
{
	*this = glist2vector (errorList, true);
}

ErrorList::ErrorList (const ErrorList& errorList)
{
	operator=(errorList);
}

ErrorList::~ErrorList ()
{
}

ErrorList&
ErrorList::operator=(const ErrorList& errorList)
{
	dynamic_cast<vector<Error>* >(this)->operator=(errorList);

	return *this;
}

GList *
ErrorList::errors ()
{
	return vector2glist (*this);
}

ErrorList
ErrorList::glist2vector (GList* errorList, bool freeList)
{
	GList* node;
	ErrorList ret;
	Error error;

	if (NULL != errorList) {
		for (node = g_list_first (errorList);
			node != NULL;
			node = g_list_next (node))
		{
			error.setCStruct (static_cast<GdaError*>(node->data));;

			ret.insert (ret.end (), error);
			if (false == freeList) {
				error.ref ();
			}
		}

		// this ain't mistake - the ownership of list's elements is taken
		// by Error objects stored in ret vector
		if (true == freeList) {
			g_list_free (errorList);
		}
}

	return ret;
}

GList*
ErrorList::vector2glist (ErrorList& errorList)
{
	GList* ret = g_list_alloc ();
	GdaError* error;

	for (int i = 0; i < errorList.size (); i++) {
		error = errorList [i].getCStruct ();
		g_list_append (ret, error);
	}

	return ret;
}
