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
#include "gdaRecordset.h"

gdaRecordset::gdaRecordset() {
	_gda_recordset = (Gda_Recordset *) gda_recordset_new();
	cnc = NULL;
}

gdaRecordset::gdaRecordset(Gda_Recordset *rst, gdaConnection *cnca) {
	_gda_recordset = rst;
	cnc = cnca;
}

gdaRecordset::gdaRecordset(Gda_Recordset *rst, Gda_Connection *cnca) {
	_gda_recordset = rst;
	cnc = new gdaConnection();
	cnc->setCStruct(cnca);
}

gdaRecordset::~gdaRecordset() {
	if (_gda_recordset) gda_recordset_free(_gda_recordset);
}

Gda_Recordset* gdaRecordset::getCStruct() {
	return _gda_recordset;
}

void gdaRecordset::setCStruct(Gda_Recordset *rst) {
	_gda_recordset = rst;
}

void gdaRecordset::setName(gchar* name) {
	gda_recordset_set_name(_gda_recordset,name);
}

void gdaRecordset::getName(gchar* name) {
	gda_recordset_get_name(_gda_recordset,name);
}

void gdaRecordset::close() {
	gda_recordset_close(_gda_recordset);
}

gdaField* gdaRecordset::field(gchar* name) {
	gdaField *a = NULL;
	a = new gdaField(gda_recordset_field_name(_gda_recordset,name));
	return a;
}

gdaField* gdaRecordset::field(gint idx) {
	gdaField *a = NULL;
	a = new gdaField(gda_recordset_field_idx(_gda_recordset,idx));
	return a;
}

gint gdaRecordset::bof() {
	return gda_recordset_bof(_gda_recordset);
}

gint gdaRecordset::eof() {
	return gda_recordset_eof(_gda_recordset);
}

gulong gdaRecordset::move (gint count, gpointer bookmark) {
	return gda_recordset_move(_gda_recordset,count,bookmark);
}

gulong gdaRecordset::moveFirst() {
	return gda_recordset_move_first(_gda_recordset);
}

gulong gdaRecordset::moveLast() {
	return gda_recordset_move_last(_gda_recordset);
}

gulong gdaRecordset::moveNext() {
	return gda_recordset_move_next(_gda_recordset);
}

gulong gdaRecordset::movePrev() {
	return gda_recordset_move_prev(_gda_recordset);
}

gint gdaRecordset::rowsize() {
	return gda_recordset_rowsize(_gda_recordset);
}

gulong gdaRecordset::affectedRows() {
	return gda_recordset_affected_rows(_gda_recordset);
}

gint gdaRecordset::open(gdaCommand* cmd, GDA_CursorType cursor_type, GDA_LockType lock_type, gulong options) {
	return gda_recordset_open(_gda_recordset,cmd->getCStruct(),cursor_type,lock_type,options);
}

gint gdaRecordset::open(gchar* txt, GDA_CursorType cursor_type, GDA_LockType lock_type, gulong options) {
	return gda_recordset_open_txt(_gda_recordset,txt,cursor_type,lock_type,options);
}

gint gdaRecordset::open(gdaCommand* cmd, gdaConnection *cnac, GDA_CursorType cursor_type, GDA_LockType lock_type, gulong options) {
	if (cnac) setConnection(cnac);
	return gda_recordset_open(_gda_recordset,cmd->getCStruct(),cursor_type,lock_type,options);
}

gint gdaRecordset::open(gchar* txt, gdaConnection *cnac, GDA_CursorType cursor_type, GDA_LockType lock_type, gulong options) {
	if (cnac) setConnection(cnac);
	return gda_recordset_open_txt(_gda_recordset,txt,cursor_type,lock_type,options);
}

gint gdaRecordset::setConnection(gdaConnection *a) {
	cnc = a;
	return gda_recordset_set_connection(_gda_recordset,cnc->getCStruct());
}

gdaConnection* gdaRecordset::getConnection() {
	return cnc;
}

gint gdaRecordset::addField(Gda_Field* field) {
	return gda_recordset_add_field(_gda_recordset,field);
}

GDA_CursorLocation gdaRecordset::getCursorloc() {
	return gda_recordset_get_cursorloc(_gda_recordset);
}

void gdaRecordset::setCursorloc(GDA_CursorLocation loc) {
	gda_recordset_set_cursorloc(_gda_recordset,loc);
}

GDA_CursorType gdaRecordset::getCursortype() {
	return gda_recordset_get_cursortype(_gda_recordset);
}

void gdaRecordset::setCursortype(GDA_CursorType type) {
	gda_recordset_set_cursortype(_gda_recordset,type);
}


