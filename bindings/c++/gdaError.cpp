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

gdaError::gdaError() {
	_gda_error = gda_error_new();
}

gdaError::gdaError(GdaError *e) {
	_gda_error = e;
}

gdaError::~gdaError() {
	if (_gda_error) gda_error_free(_gda_error);
}

GdaError *gdaError::getCStruct() {
	return _gda_error;
}

void gdaError::setCStruct(GdaError *e) {
	_gda_error = e;
}


const gchar* gdaError::description() {
	return gda_error_description(_gda_error);
}

const glong gdaError::number() {
	return gda_error_number(_gda_error);
}

const gchar* gdaError::source() {
	return gda_error_source(_gda_error);
}

const gchar* gdaError::helpurl() {
	return gda_error_helpurl(_gda_error);
}

const gchar* gdaError::sqlstate() {
	return gda_error_sqlstate(_gda_error);
}

const gchar* gdaError::nativeMsg() {
	return gda_error_nativeMsg(_gda_error);
}

const gchar* gdaError::realcommand() {
	return gda_error_realcommand(_gda_error);
}

							     
  
  
