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

using namespace gda;

Batch::Batch() {
	_gda_batch = gda_batch_new();
}

Batch::Batch(GdaBatch *a) {
	_gda_batch = a;
}

Batch::~Batch() {
	if (_gda_batch) gda_batch_free(_gda_batch);
}

void Batch::setCStruct(GdaBatch *job) {
	_gda_batch = job;
}

GdaBatch *Batch::getCStruct() {
	return _gda_batch;
}

gboolean Batch::loadFile(const gchar *filename, gboolean clean) {
	return gda_batch_load_file(_gda_batch,filename,clean);
}

void Batch::addCommand(const gchar *cmd) {
	gda_batch_add_command(_gda_batch,cmd);
}

void Batch::clear() {
	gda_batch_clear(_gda_batch);
}

gboolean Batch::start() {
	return gda_batch_start(_gda_batch);
}

void Batch::stop() {
	gda_batch_stop(_gda_batch);
}

gboolean Batch::isRunning() {
	return gda_batch_is_running(_gda_batch);
}

Connection* Batch::getConnection() {
	return cnc;
}

void Batch::setConnection(Connection *a) {
	cnc = a;
}

gboolean Batch::getTransactionMode() {
	return gda_batch_get_transaction_mode(_gda_batch);
}

void Batch::setTransactionMode(gboolean mode) {
	gda_batch_set_transaction_mode(_gda_batch,mode);
}


