/* GNOME DB libary
 * Copyright (C) 2000 Chris Wiegand
 * (Based on gda_connection.h and related files in gnome-db)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "config.h"
#include "gdaConnection.h"

using namespace gda;

Connection::Connection ()
{
	_gda_connection = NULL;
}

Connection::Connection (GdaConnection * a)
{
	_gda_connection = a;
}

Connection::Connection (CORBA_ORB orb)
{
	_gda_connection = gda_connection_new (orb);
}

Connection::~Connection ()
{
	if (_gda_connection)
		gda_connection_free (_gda_connection);
}

GdaConnection *
Connection::getCStruct ()
{
	return _gda_connection;
}

void
Connection::setCStruct (GdaConnection * cnc)
{
	_gda_connection = cnc;
}

void
Connection::setProvider (gchar * name)
{
	gda_connection_set_provider (_gda_connection, name);
}

const gchar *
Connection::getProvider ()
{
	return gda_connection_get_provider (_gda_connection);
}

gboolean
Connection::supports (GDA_Connection_Feature feature)
{
	return gda_connection_supports (_gda_connection, feature);
}

void
Connection::setDefaultDB (gchar * dsn)
{
	gda_connection_set_default_db (_gda_connection, dsn);
}

gint
Connection::open (gchar * dsn, gchar * user, gchar * pwd)
{
	return gda_connection_open (_gda_connection, dsn, user, pwd);
}

void
Connection::close ()
{
	gda_connection_close (_gda_connection);
}

ErrorList *
Connection::getErrors ()
{
	ErrorList *a = NULL;
	a = new ErrorList (gda_connection_get_errors (_gda_connection));
	return a;
}

gint
Connection::beginTransaction ()
{
	return gda_connection_begin_transaction (_gda_connection);
}

gint
Connection::commitTransaction ()
{
	return gda_connection_commit_transaction (_gda_connection);
}

gint
Connection::rollbackTransaction ()
{
	return gda_connection_rollback_transaction (_gda_connection);
}

Recordset *
Connection::execute (gchar * txt, gulong * reccount, gulong flags)
{
	GdaRecordset *b = NULL;
	Recordset *a = NULL;
	b = gda_connection_execute (_gda_connection, txt, reccount, flags);
	a = new Recordset (b, this);
	return a;
}

gint
Connection::startLogging (gchar * filename)
{
	return gda_connection_start_logging (_gda_connection, filename);
}

gint
Connection::stopLogging ()
{
	return gda_connection_stop_logging (_gda_connection);
}

void
Connection::addSingleError (Error * error)
{
	gda_connection_add_single_error (_gda_connection,
					 error->getCStruct ());
}

void
Connection::addErrorlist (ErrorList * list)
{
	gda_connection_add_error_list (_gda_connection, list->errors ());
}

gboolean
Connection::isOpen ()
{
	return gda_connection_is_open (_gda_connection);
}

gchar *
Connection::getDSN ()
{
	return gda_connection_get_dsn (_gda_connection);
}

gchar *
Connection::getUser ()
{
	return gda_connection_get_user (_gda_connection);
}

gchar *
Connection::getVersion ()
{
	return gda_connection_get_version (_gda_connection);
}
