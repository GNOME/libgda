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
#include "gdaError.h"

using namespace gda;

Error::Error() {
	_gda_error = gda_error_new();
}

Error::Error(GdaError *e) {
	_gda_error = e;
}

Error::~Error() {
	if (_gda_error) gda_error_free(_gda_error);
}

GdaError *Error::getCStruct() {
	return _gda_error;
}

void Error::setCStruct(GdaError *e) {
	_gda_error = e;
}


const gchar* Error::description() {
	return gda_error_get_description(_gda_error);
}

const glong Error::number() {
	return gda_error_get_number(_gda_error);
}

const gchar* Error::source() {
	return gda_error_get_source(_gda_error);
}

const gchar* Error::helpurl() {
	return gda_error_get_help_url(_gda_error);
}

const gchar* Error::sqlstate() {
	return gda_error_get_sqlstate(_gda_error);
}

const gchar* Error::nativeMsg() {
	return gda_error_get_native(_gda_error);
}

const gchar* Error::realcommand() {
	return gda_error_get_real_command(_gda_error);
}

							     
  
  
