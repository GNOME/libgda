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

Command::Command ()
{
	_gda_command = NULL;
	setCStruct (gda_command_new ());
}

Command::Command (const Command& cmd)
{
	_gda_command = NULL;
    setCStruct (cmd.getCStruct ());

	_connection = cmd._connection;
}

// we take ownership of C object (GdaCommand);
// not of contained C object (GdaConnection)
//
Command::Command (GdaCommand* cmd)
{
	_gda_command = NULL;
    setCStruct (cmd);

	_connection.setCStruct (gda_command_get_connection (cmd));
	_connection.ref ();
}

Command::~Command ()
{
	if (_gda_command)
		gda_command_free (_gda_command);
}

Command&
Command::operator=(const Command& cmd)
{
    setCStruct (cmd.getCStruct ());
	return *this;
}

Connection
Command::getConnection ()
{
	return _connection;
}

gint
Command::setConnection (const Connection& cnc)
{
    gda_command_set_connection (_gda_command, cnc.getCStruct ());

	_connection = cnc;

	return 0;
}

string
Command::getText ()
{
	return gda_return_string (gda_command_get_text (_gda_command));
}

void
Command::setText (const string& text)
{
	gda_command_set_text (_gda_command, const_cast<gchar*>(text.c_str ()));
}

GDA_CommandType
Command::getCmdType ()
{
	return gda_command_get_cmd_type (_gda_command);
}

void
Command::setCmdType (GDA_CommandType type)
{
	gda_command_set_cmd_type (_gda_command, type);
}

Recordset
Command::execute (gulong& reccount, gulong flags)
{
	GdaRecordset *gdaRecordset = NULL;
	gdaRecordset = gda_command_execute (_gda_command, &reccount, flags);

    Recordset recordset (gdaRecordset);

	return recordset;
}

void
Command::createParameter (
	const string& name, GDA_ParameterDirection inout, const Value& value)
{
	_parameters_values.insert (_parameters_values.end (), value);

	gda_command_create_parameter (
		_gda_command,
		const_cast<gchar*>(name.c_str ()),
		inout,
		_parameters_values [_parameters_values.size () - 1]._gda_value);
}
/*
glong
Command::getTimeout ()
{
	return gda_command_get_timeout (_gda_command);
}

void
Command::setTimeout (glong timeout)
{
	gda_command_set_timeout (_gda_command, timeout);
}
*/

GdaCommand*
Command::getCStruct (bool refn = true) const
{
	if (refn)
		ref ();

	return _gda_command;
}


void
Command::setCStruct (GdaCommand *cmd)
{
    unref ();
	_gda_command = cmd;
}


void
Command::ref () const
{
	if (NULL == _gda_command) {
		g_warning ("gda::Command::ref () received NULL pointer");
	}
	else {
#ifdef HAVE_GOBJECT
		g_object_ref (G_OBJECT (_gda_command));
		GdaConnection* cnc = gda_command_get_connection (_gda_command);
		if (NULL != cnc) {
			g_object_ref (G_OBJECT (cnc));
		}
#else
		gtk_object_ref (GTK_OBJECT (_gda_command));
		GdaConnection* cnc = gda_command_get_connection (_gda_command);
		if (NULL != cnc) {
			gtk_object_ref (GTK_OBJECT (cnc));
		}
#endif
	}
	}

void
Command::unref ()
{
	if (_gda_command != NULL) {
		GdaConnection* cnc = gda_command_get_connection (_gda_command);
		if (cnc != NULL) {
			gda_connection_free (cnc);
		}

		gda_command_free (_gda_command);
	}
}

