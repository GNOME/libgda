/* GDA C++ bindings
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

using namespace gda;

Recordset::Recordset ()
{
	_gda_recordset = (GdaRecordset *) gda_recordset_new ();
	cnc = NULL;
}

Recordset::Recordset (GdaRecordset * rst, Connection * cnca)
{
	_gda_recordset = rst;
	cnc = cnca;
}

Recordset::Recordset (GdaRecordset * rst, GdaConnection * cnca)
{
	_gda_recordset = rst;
	cnc = new Connection ();
	cnc->setCStruct (cnca);
}

Recordset::~Recordset ()
{
	if (_gda_recordset)
		gda_recordset_free (_gda_recordset);
}

GdaRecordset *
Recordset::getCStruct ()
{
	return _gda_recordset;
}

void
Recordset::setCStruct (GdaRecordset * rst)
{
	_gda_recordset = rst;
}

void
Recordset::setName (gchar * name)
{
	gda_recordset_set_name (_gda_recordset, name);
}

void
Recordset::getName (gchar * name)
{
	gda_recordset_get_name (_gda_recordset, name);
}

void
Recordset::close ()
{
	gda_recordset_close (_gda_recordset);
}

Field *
Recordset::field (gchar * name)
{
	Field *a = NULL;
	a = new Field (gda_recordset_field_name (_gda_recordset, name));
	return a;
}

Field *
Recordset::field (gint idx)
{
	Field *a = NULL;
	a = new Field (gda_recordset_field_idx (_gda_recordset, idx));
	return a;
}

gint
Recordset::bof ()
{
	return gda_recordset_bof (_gda_recordset);
}

gint
Recordset::eof ()
{
	return gda_recordset_eof (_gda_recordset);
}

gulong
Recordset::move (gint count, gpointer bookmark)
{
	return gda_recordset_move (_gda_recordset, count, bookmark);
}

gulong
Recordset::moveFirst ()
{
	return gda_recordset_move_first (_gda_recordset);
}

gulong
Recordset::moveLast ()
{
	return gda_recordset_move_last (_gda_recordset);
}

gulong
Recordset::moveNext ()
{
	return gda_recordset_move_next (_gda_recordset);
}

gulong
Recordset::movePrev ()
{
	return gda_recordset_move_prev (_gda_recordset);
}

gint
Recordset::rowsize ()
{
	return gda_recordset_rowsize (_gda_recordset);
}

gulong
Recordset::affectedRows ()
{
	return gda_recordset_affected_rows (_gda_recordset);
}

gint
Recordset::open (Command * cmd, GDA_CursorType cursor_type,
		 GDA_LockType lock_type, gulong options)
{
	return gda_recordset_open (_gda_recordset, cmd->getCStruct (),
				   cursor_type, lock_type, options);
}

gint
Recordset::open (gchar * txt, GDA_CursorType cursor_type,
		 GDA_LockType lock_type, gulong options)
{
	return gda_recordset_open_txt (_gda_recordset, txt, cursor_type,
				       lock_type, options);
}

gint
Recordset::open (Command * cmd, Connection * cnac, GDA_CursorType cursor_type,
		 GDA_LockType lock_type, gulong options)
{
	if (cnac)
		setConnection (cnac);
	return gda_recordset_open (_gda_recordset, cmd->getCStruct (),
				   cursor_type, lock_type, options);
}

gint
Recordset::open (gchar * txt, Connection * cnac, GDA_CursorType cursor_type,
		 GDA_LockType lock_type, gulong options)
{
	if (cnac)
		setConnection (cnac);
	return gda_recordset_open_txt (_gda_recordset, txt, cursor_type,
				       lock_type, options);
}

gint
Recordset::setConnection (Connection * a)
{
	cnc = a;
	return gda_recordset_set_connection (_gda_recordset,
					     cnc->getCStruct ());
}

Connection *
Recordset::getConnection ()
{
	return cnc;
}

gint
Recordset::addField (GdaField * field)
{
	return gda_recordset_add_field (_gda_recordset, field);
}

GDA_CursorLocation
Recordset::getCursorloc ()
{
	return gda_recordset_get_cursorloc (_gda_recordset);
}

void
Recordset::setCursorloc (GDA_CursorLocation loc)
{
	gda_recordset_set_cursorloc (_gda_recordset, loc);
}

GDA_CursorType
Recordset::getCursortype ()
{
	return gda_recordset_get_cursortype (_gda_recordset);
}

void
Recordset::setCursortype (GDA_CursorType type)
{
	gda_recordset_set_cursortype (_gda_recordset, type);
}

GList* Recordset::getRow ()
{
	return gda_recordset_get_row (_gda_recordset);
}

gchar* Recordset::getRowAsString ()
{
	return gda_recordset_get_row_as_string (_gda_recordset);
}
