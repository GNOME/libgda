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

Error::Error ()
{
	_gda_error = NULL;
	setCStruct (gda_error_new ());
}

Error::Error (const Error& error)
{
	_gda_error = NULL;
	setCStruct (error.getCStruct ());
}

Error::Error (GdaError *e)
{
	_gda_error = NULL;
	setCStruct (e);
}

Error::~Error ()
{
	unref ();
	//if (_gda_error) gda_error_free (_gda_error);
}

Error&
Error::operator=(const Error& error)
{
	setCStruct (error.getCStruct ());

	return *this;
}


string
Error::description ()
{
	return gda_return_string (g_strdup (gda_error_get_description (_gda_error)));
}

glong
Error::number ()
{
	return gda_error_get_number (_gda_error);
}

string
Error::source ()
{
	return gda_return_string (g_strdup (gda_error_get_source (_gda_error)));
}

string
Error::helpurl ()
{
	return gda_return_string (g_strdup (gda_error_get_help_url (_gda_error)));
}

string
Error::sqlstate ()
{
	return gda_return_string (g_strdup (gda_error_get_sqlstate (_gda_error)));
}

string
Error::nativeMsg ()
{
	return gda_return_string (g_strdup (gda_error_get_native (_gda_error)));
}

string
Error::realcommand ()
{
	return gda_return_string (g_strdup (gda_error_get_real_command (_gda_error)));
}

GdaError*
Error::getCStruct (bool refn = true) const
{
	if (refn) ref ();
	return _gda_error;
}

void
Error::setCStruct (GdaError *e)
{
	unref ();
	_gda_error = e;
}

void
Error::ref () const
{
	if (NULL == _gda_error) {
		g_warning ("gda::Error::ref () received NULL pointer");
}
	else {
#ifdef HAVE_GOBJECT
		g_object_ref (G_OBJECT (_gda_error));
#else
		gtk_object_ref (GTK_OBJECT (_gda_error));
#endif
	}
}

void
Error::unref ()
{
	if (_gda_error != NULL) {
		gda_error_free (_gda_error);
	}
}

