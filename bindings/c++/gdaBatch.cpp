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

Batch::Batch ()
{
	_gda_batch = NULL;
	setCStruct (gda_batch_new ());
}

Batch::Batch (const Batch& job)
{
	_gda_batch = NULL;
    setCStruct (job.getCStruct ());

	_connection = job._connection;
}

// take ownership of C object (GdaBatch); not of
// contained C object (GdaConnection)
//
Batch::Batch (GdaBatch* job)
{
	_gda_batch = NULL;
	setCStruct (job);

	_connection.setCStruct (gda_batch_get_connection (job));
	_connection.ref ();
}

Batch::~Batch ()
{
	if (_gda_batch)
		gda_batch_free (_gda_batch);
}

Batch&
Batch::operator=(const Batch& batch)
{
    setCStruct (batch.getCStruct ());
	_connection = batch._connection;

	return *this;
}

bool
Batch::loadFile (const string& filename, bool clean)
{
	return gda_batch_load_file (
		_gda_batch, const_cast<gchar*>(filename.c_str ()), clean);
}

void
Batch::addCommand (const string& cmdText)
{
	gda_batch_add_command (
		_gda_batch, const_cast<gchar*>(cmdText.c_str ()));
}

void
Batch::clear ()
{
	gda_batch_clear (_gda_batch);
}

bool
Batch::start ()
{
	return gda_batch_start (_gda_batch);
}

void
Batch::stop ()
{
	gda_batch_stop (_gda_batch);
}

bool
Batch::isRunning ()
{
	return gda_batch_is_running (_gda_batch);
}

Connection
Batch::getConnection ()
{
	return _connection;
}

void
Batch::setConnection (const Connection& cnc)
{
	gda_batch_set_connection (_gda_batch, cnc.getCStruct (false));
	_connection = cnc;
}

bool
Batch::getTransactionMode ()
{
	return gda_batch_get_transaction_mode (_gda_batch);
}

void
Batch::setTransactionMode (bool mode)
{
	gda_batch_set_transaction_mode (_gda_batch, mode);
}

GdaBatch*
Batch::getCStruct (bool refn = true) const
{
	if (refn)
	  ref ();

	return _gda_batch;
}

void
Batch::setCStruct (GdaBatch *batch)
{
    unref ();
	_gda_batch = batch;
}

void
Batch::ref () const
{
	if (NULL == _gda_batch) {
		g_warning ("gda::Batch::ref () received NULL pointer");
	}
	else {
#ifdef HAVE_GOBJECT
		g_object_ref (G_OBJECT (_gda_batch));
#else
		gtk_object_ref (GTK_OBJECT (_gda_batch));
#endif
	}
}

void
Batch::unref ()
{
	if (_gda_batch != NULL) {
		gda_batch_free (_gda_batch);
	}
}

