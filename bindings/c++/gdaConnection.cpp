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
#include "gdaIncludes.h"
#include "gdaHelpers.h"

using namespace gda;

Connection::Connection ()
{
	_gda_connection = NULL;
}

Connection::Connection (GdaConnection *cnc)
{
	_gda_connection = NULL;
	setCStruct (cnc);
}

Connection::Connection (const Connection& cnc)
{
	_gda_connection = NULL;
	setCStruct (cnc.getCStruct ());
}

Connection::Connection (CORBA_ORB orb)
{
	_gda_connection = NULL;
	setCStruct (gda_connection_new (orb));
}

Connection::~Connection()
{
	unref ();
}

Connection&
Connection::operator=(const Connection& cnc)
{
	setCStruct (cnc.getCStruct ());
	return *this;
}

void
Connection::setProvider (const string& name)
{
	gda_connection_set_provider (
		_gda_connection, const_cast<gchar*>(name.c_str ()));
}

string
Connection::getProvider ()
{
	return gda_return_string (
		(gchar*)gda_connection_get_provider (_gda_connection));
}

bool
Connection::supports (GDA_Connection_Feature feature)
{
	return gda_connection_supports (_gda_connection, feature);
}

void
Connection::setDefaultDB (const string& dsn)
{
	gda_connection_set_default_db (
		_gda_connection, const_cast<gchar*>(dsn.c_str ()));
}

gint
Connection::open (const string& dsn, const string& user, const string& pwd)
{
	return gda_connection_open (
		_gda_connection,
		const_cast<gchar*>(dsn.c_str ()),
		const_cast<gchar*>(user.c_str ()),
		const_cast<gchar*>(pwd.c_str ()));
}

void
Connection::close ()
{
	gda_connection_close (_gda_connection);
}

Recordset
Connection::openSchema (GDA_Connection_QType t, ...)
{
	va_list ap;
	GDA_Connection_ConstraintType constraint_type;
	gint                          index;
	Recordset					empty;

	g_return_val_if_fail(isOpen (), empty);
	g_return_val_if_fail(_gda_connection->connection != NULL, empty);

	vector<GDA_Connection_ConstraintType> types;
	vector<string> values;

	va_start (ap, t);
	while (1) {
		constraint_type = static_cast<GDA_Connection_ConstraintType>(va_arg(ap, int));
		if (constraint_type == GDA_Connection_no_CONSTRAINT)
		  break;

		types.insert (types.end (), constraint_type);
		values.insert (values.end (), va_arg(ap, const char*));
	}

	GdaConstraint_Element* elements = g_new0 (GdaConstraint_Element, types.size () + 1);

	index = 0;
	while (index < types.size ()) {
		elements [index].type = GDA_Connection_QType(types [index]); // something wrong here: conversion betwee GDA_Connection_ConstraintType and GDA_Connection_QType
		elements [index].value = const_cast<gchar*>(values [index].c_str ());
	}

	elements [index].type = GDA_Connection_QType (GDA_Connection_no_CONSTRAINT); // something wrong here: conversion betwee GDA_Connection_ConstraintType and GDA_Connection_QType

	return openSchemaArray (t, elements);
}

Recordset
Connection::openSchemaArray (GDA_Connection_QType t, GdaConstraint_Element* elems)
{
	Recordset recordset (gda_connection_open_schema_array (_gda_connection, t, elems));

	return recordset;
}

glong
Connection::modifySchema (GDA_Connection_QType t, ...)
{
	g_return_val_if_fail(isOpen (), -1);
	g_return_val_if_fail(_gda_connection->connection != NULL, -1);
}

ErrorList
Connection::getErrors ()
{
	ErrorList errorList (gda_connection_get_errors (_gda_connection));

	return errorList;
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

Recordset
Connection::execute (const string& txt, gulong& reccount, gulong flags)
{
	GdaRecordset *gdaRst = NULL;
	//Recordset *rst = NULL;
	gdaRst = gda_connection_execute (
		_gda_connection, const_cast<gchar*>(txt.c_str ()), &reccount, flags);
	if (gdaRst != NULL) {
  		ref (); // the same connection gobject is inside returned recordset
	}
	
	Recordset rst (gdaRst);

	return rst;
}

gint
Connection::startLogging (const string& filename)
{
	return gda_connection_start_logging (
		_gda_connection, const_cast<gchar*>(filename.c_str ()));
}

gint
Connection::stopLogging ()
{
	return gda_connection_stop_logging (_gda_connection);
}

void
Connection::addSingleError (Error& error)
{
	gda_connection_add_single_error (_gda_connection, error.getCStruct ());
}

void
Connection::addErrorlist (ErrorList& list)
{
	gda_connection_add_error_list (_gda_connection, list.errors ());
}

bool
Connection::isOpen ()
{
	return gda_connection_is_open (_gda_connection);
}

string
Connection::getDSN ()
{
	return gda_return_string (gda_connection_get_dsn (_gda_connection));
}

string
Connection::getUser ()
{
	return gda_return_string (gda_connection_get_user (_gda_connection));
}

/*
glong
Connection::getFlags ()
{
	return gda_connection_get_flags (_gda_connection);
}

void
Connection::setFlags (glong flags)
{
	gda_connection_set_flags (_gda_connection, flags);
}

glong
Connection::getCmdTimeout ()
{
	return gda_connection_get_cmd_timeout (_gda_connection);
}

void
Connection::setCmdTimeout (glong cmdTimeout)
{
	gda_connection_set_cmd_timeout (_gda_connection, cmdTimeout);
}

glong
Connection::getConnectTimeout ()
{
	return gda_connection_get_connect_timeout (_gda_connection);
}

void
Connection::setConnectTimeout (glong timeout)
{
	gda_connection_set_connect_timeout (_gda_connection, timeout);
}

GDA_CursorLocation
Connection::getCursorLocation ()
{
	return gda_connection_get_cursor_location (_gda_connection);
}

void
Connection::setCursorLocation (GDA_CursorLocation cursor)
{
	gda_connection_set_cursor_location (_gda_connection, cursor);
}
*/

string
Connection::getVersion ()
{
	return gda_connection_get_version (_gda_connection);
}

GdaConnection* Connection::getCStruct (bool refn = true) const
{
	if (refn) ref ();
	return _gda_connection;
}

void Connection::setCStruct (GdaConnection *cnc)
{
	unref ();
	_gda_connection = cnc;
}


void
Connection::ref () const
{
	if (NULL == _gda_connection) {
		g_warning ("gda::Connection::ref () received NULL pointer");
	}
	else {
#ifdef HAVE_GOBJECT
		g_object_ref (G_OBJECT (_gda_connection));
#else
		gtk_object_ref (GTK_OBJECT (_gda_connection));
#endif
	}
}

void
Connection::unref ()
{
	if (_gda_connection != NULL) {
		gda_connection_free (_gda_connection);
	}
}

