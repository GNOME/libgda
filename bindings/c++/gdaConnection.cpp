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

gdaConnection::gdaConnection() {
	_gda_connection = NULL;
}

gdaConnection::gdaConnection(GdaConnection *a) {
	_gda_connection = a;
}

gdaConnection::gdaConnection(CORBA_ORB orb) {
	_gda_connection = gda_connection_new(orb);
}

gdaConnection::~gdaConnection() {
	if (_gda_connection) gda_connection_free(_gda_connection);
}

GdaConnection* gdaConnection::getCStruct() {
	return _gda_connection;
}

void gdaConnection::setCStruct(GdaConnection *cnc) {
	_gda_connection = cnc;
}
		
void gdaConnection::setProvider(gchar* name) {
	gda_connection_set_provider(_gda_connection,name);
}

const gchar* gdaConnection::getProvider() {
	return gda_connection_get_provider(_gda_connection);
}

gboolean gdaConnection::supports(GDA_Connection_Feature feature) {
	return gda_connection_supports(_gda_connection,feature);
}

void gdaConnection::setDefaultDB(gchar* dsn) {
	gda_connection_set_default_db(_gda_connection,dsn);
}

gint gdaConnection::open(gchar* dsn, gchar* user,gchar* pwd ) {
	return gda_connection_open(_gda_connection,dsn,user,pwd);
}

void gdaConnection::close() {
	gda_connection_close(_gda_connection);
}

gdaErrorList* gdaConnection::getErrors() {
	gdaErrorList *a = NULL;
	a = new gdaErrorList(gda_connection_get_errors(_gda_connection));
	return a;
}

gint gdaConnection::beginTransaction() {
	return gda_connection_begin_transaction(_gda_connection);
}

gint gdaConnection::commitTransaction() {
	return gda_connection_commit_transaction(_gda_connection);
}

gint gdaConnection::rollbackTransaction() {
	return gda_connection_rollback_transaction(_gda_connection);
}

gdaRecordset* gdaConnection::execute(gchar* txt, gulong* reccount, gulong flags) {
	GdaRecordset *b = NULL;
	gdaRecordset *a = NULL;
	b = gda_connection_execute(_gda_connection,txt,reccount,flags);
	a = new gdaRecordset(b,this);
	return a;
}

gint gdaConnection::startLogging(gchar* filename) {
	return gda_connection_start_logging(_gda_connection,filename);
}

gint gdaConnection::stopLogging() {
	return gda_connection_stop_logging(_gda_connection);
}

void gdaConnection::addSingleError(gdaError* error) {
	gda_connection_add_single_error(_gda_connection,error->getCStruct());
}

void gdaConnection::addErrorlist(gdaErrorList* list) {
	gda_connection_add_errorlist(_gda_connection,list->errors());
}

gboolean gdaConnection::isOpen() {
	return gda_connection_is_open(_gda_connection);
}

gchar* gdaConnection::getDSN() {
	return gda_connection_get_dsn(_gda_connection);
}

gchar* gdaConnection::getUser() {
	return gda_connection_get_user(_gda_connection);
}

glong gdaConnection::getFlags() {
	return gda_connection_get_flags(_gda_connection);
}

void gdaConnection::setFlags(glong flags) {
	gda_connection_set_flags(_gda_connection,flags);
}

glong gdaConnection::getCmdTimeout() {
	return gda_connection_get_cmd_timeout(_gda_connection);
}

void gdaConnection::setCmdTimeout(glong cmd_timeout) {
	gda_connection_set_cmd_timeout(_gda_connection,cmd_timeout);
}

glong gdaConnection::getConnectTimeout() {
	return gda_connection_get_connect_timeout(_gda_connection);
}

void gdaConnection::setConnectTimeout(glong timeout) {
	gda_connection_set_connect_timeout(_gda_connection,timeout);
}

GDA_CursorLocation gdaConnection::getCursorLocation() {
	return gda_connection_get_cursor_location(_gda_connection);
}

void gdaConnection::setCursorLocation(GDA_CursorLocation cursor) {
	gda_connection_set_cursor_location(_gda_connection,cursor);
}

gchar* gdaConnection::getVersion() {
	return gda_connection_get_version(_gda_connection);
}

