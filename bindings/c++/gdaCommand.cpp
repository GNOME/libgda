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
#include "gdaCommand.h"

using namespace gda;

Command::Command() {
	_gda_command = gda_command_new();
}

Command::~Command() {
	if (_gda_command) gda_command_free(_gda_command);
}

GdaCommand *Command::getCStruct() {
	return _gda_command;
}

void Command::setCStruct(GdaCommand *cmd) {
	_gda_command = cmd;
}
		
Connection *Command::getConnection() {
	return cnc;
}

gint Command::setConnection(Connection *a) {
	cnc = a;
	return gda_command_set_connection(_gda_command,cnc->getCStruct());
}

gchar* Command::getText() {
	return gda_command_get_text(_gda_command);
}

void Command::setText(gchar *text) {
	gda_command_set_text(_gda_command,text);
}

GDA_CommandType Command::getCmdType() {
	return gda_command_get_cmd_type(_gda_command);
}

void Command::setCmdType(GDA_CommandType type) {
	gda_command_set_cmd_type(_gda_command, type);
}

Recordset* Command::execute(gulong* reccount, gulong flags) {
	GdaRecordset *pGdaRecordset = NULL;
	pGdaRecordset = gda_command_execute(_gda_command, reccount, flags);

	Recordset *pRecordset = NULL;
	if (pGdaRecordset != NULL) {
		pRecordset = new Recordset (pGdaRecordset, cnc);
	} else {
		pRecordset = new Recordset ();
	}

	return pRecordset;
}

//void Command::createParameter(gchar* name, GDA_ParameterDirection inout, Value *value) {
// FIXME If we don't use GDA_Value, how do use use the c function gda_command_create_parameter?
//	GDA_Value v(value->getCValue());
//	gda_command_create_parameter(_gda_command,name,inout,&v);
//}

glong Command::getTimeout() {
	return gda_command_get_timeout(_gda_command);
}

void Command::setTimeout(glong timeout) {
	gda_command_set_timeout(_gda_command,timeout);
}
