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
#include "gdaBatch.h"

gdaBatch::gdaBatch() {
	_gda_batch = gda_batch_new();
}

gdaBatch::gdaBatch(Gda_Batch *a) {
	_gda_batch = a;
}

gdaBatch::~gdaBatch() {
	if (_gda_batch) gda_batch_free(_gda_batch);
}

void gdaBatch::setCStruct(Gda_Batch *job) {
	_gda_batch = job;
}

Gda_Batch *gdaBatch::getCStruct() {
	return _gda_batch;
}

gboolean gdaBatch::loadFile(const gchar *filename, gboolean clean) {
	return gda_batch_load_file(_gda_batch,filename,clean);
}

void gdaBatch::addCommand(const gchar *cmd) {
	gda_batch_add_command(_gda_batch,cmd);
}

void gdaBatch::clear() {
	gda_batch_clear(_gda_batch);
}

gboolean gdaBatch::start() {
	return gda_batch_start(_gda_batch);
}

void gdaBatch::stop() {
	gda_batch_stop(_gda_batch);
}

gboolean gdaBatch::isRunning() {
	return gda_batch_is_running(_gda_batch);
}

gdaConnection* gdaBatch::getConnection() {
	return cnc;
}

void gdaBatch::setConnection(gdaConnection *a) {
	cnc = a;
}

gboolean gdaBatch::getTransactionMode() {
	return gda_batch_get_transaction_mode(_gda_batch);
}

void gdaBatch::setTransactionMode(gboolean mode) {
	gda_batch_set_transaction_mode(_gda_batch,mode);
}


