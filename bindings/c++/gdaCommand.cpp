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

#include "gdaCommand.h"

gdaCommand::gdaCommand() {
	_gda_command = gda_command_new();
}

gdaCommand::~gdaCommand() {
	if (_gda_command) gda_command_free(_gda_command);
}

Gda_Command *gdaCommand::getCStruct() {
	return _gda_command;
}

void gdaCommand::setCStruct(Gda_Command *cmd) {
	_gda_command = cmd;
}
		
gdaConnection *gdaCommand::getConnection() {
	return cnc;
}

gint gdaCommand::setConnection(gdaConnection *a) {
	cnc = a;
	return gda_command_set_connection(_gda_command,cnc->getCStruct());
}

gchar* gdaCommand::getText() {
	return gda_command_get_text(_gda_command);
}

void gdaCommand::setText(gchar *text) {
	gda_command_set_text(_gda_command,text);
}

gulong gdaCommand::getCmdType() {
	return gda_command_get_cmd_type(_gda_command);
}

void gdaCommand::setCmdType(gulong flags) {
	gda_command_set_cmd_type(_gda_command,flags);
}

gdaRecordset* gdaCommand::execute(gulong* reccount, gulong flags) {
	gda_command_execute(_gda_command,reccount,flags);
}

//void gdaCommand::createParameter(gchar* name, GDA_ParameterDirection inout, gdaValue *value) {
// FIXME If we don't use GDA_Value, how do use use the c function gda_command_create_parameter?
//	GDA_Value v(value->getCValue());
//	gda_command_create_parameter(_gda_command,name,inout,&v);
//}

glong gdaCommand::getTimeout() {
	return gda_command_get_timeout(_gda_command);
}

void gdaCommand::setTimeout(glong timeout) {
	gda_command_set_timeout(_gda_command,timeout);
}
