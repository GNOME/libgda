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

Recordset::Recordset ()
{
	_gda_recordset = NULL;
	setCStruct (gda_recordset_new ());
}

Recordset::Recordset (const Recordset& rst)
{
	_gda_recordset = NULL;
	setCStruct (rst.getCStruct ());

	_cnc = rst._cnc;
}

// we take ownership of C object; not of contained C object
//
Recordset::Recordset (GdaRecordset* rst)
{
	_gda_recordset = NULL;
	setCStruct (rst);

	_cnc.setCStruct (gda_recordset_get_connection (rst));
	_cnc.ref ();
}

Recordset::~Recordset ()
{
	if (_gda_recordset) gda_recordset_free (_gda_recordset);
}

Recordset&
Recordset::operator=(const Recordset& rst)
{
	setCStruct (rst.getCStruct ());
	return *this;
}

bool
Recordset::isValid ()
{
	return _gda_recordset != NULL;
}

void
Recordset::setName (const string& name)
{
	//gda_recordset_set_name(_gda_recordset, const_cast<gchar*>(name.c_str ()));
}

string
Recordset::getName ()
{
    //gchar* name;
	//gda_recordset_get_name (_gda_recordset, name);
	string name;
    return name; //gda_return_string (name);
}

void
Recordset::close ()
{
	gda_recordset_close (_gda_recordset);
}

Field
Recordset::field (const string& name)
{
	Field field (gda_recordset_field_name (_gda_recordset, const_cast<gchar*>(name.c_str ())), _gda_recordset);
	
	return field;
}

Field
Recordset::field (gint idx)
{
	Field field (gda_recordset_field_idx (_gda_recordset, idx), _gda_recordset);
	return field;
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
Recordset::open (const Command& cmd, GDA_CursorType cursor_type, GDA_LockType lock_type, gulong options)
{
	_cnc = cmd._connection;

	// Not neccessary. Connection is copied from GdaCommand object in
	// gda_recordset_open ()
	//
	//###gda_recordset_set_connection (_gda_recordset, _cnc.getCStruct (false));

	return gda_recordset_open (_gda_recordset, cmd.getCStruct (false), cursor_type, lock_type, options);
}

gint
Recordset::open (const string& txt, GDA_CursorType cursor_type, GDA_LockType lock_type, gulong options)
{
	return gda_recordset_open_txt (_gda_recordset, const_cast<gchar*>(txt.c_str ()), cursor_type, lock_type, options);
}

gint
Recordset::setConnection (const Connection& cnc)
{
	return gda_recordset_set_connection (_gda_recordset, cnc.getCStruct (false));
	_cnc = cnc;
}

Connection
Recordset::getConnection ()
{
	return _cnc;
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

// should be gda_recordset_get_row (), but:
// 1. it generates memory leaks
// 2. there is no wrapper for GList around
//
vector<string>
Recordset::getRow ()
{
	Field field;
	vector<string> ret;

	for (gint i = 0; i < rowsize (); i++) {
		field = this->field (i);
		ret.insert (ret.end (), field.stringifyValue ());
	}

	return ret;
}

// should be gda_recordset_get_row (), but
// it generates memory leaks
//
string
Recordset::getRowAsString ()
{
	Field field;
	string ret;

	for (gint i = 0; i < rowsize (); i++) {
		field = this->field (i);
		ret = ret + field.stringifyValue ();
	}

	return ret;
}

GdaRecordset*
Recordset::getCStruct (bool refn = true) const
{
	if (refn) ref ();
	return _gda_recordset;
}

void
Recordset::setCStruct (GdaRecordset *rst)
{
	_gda_recordset = rst;
}

void
Recordset::ref () const
{
	if (NULL == _gda_recordset) {
		g_warning ("gda::Recordset::ref () received NULL pointer");
	}
	else {
#ifdef HAVE_GOBJECT
		g_object_ref (G_OBJECT (_gda_recordset));
		GdaConnection* cnc = gda_recordset_get_connection (_gda_recordset);
		if (NULL != cnc) {
			g_object_ref (G_OBJECT (cnc));
		}
#else
		gtk_object_ref (GTK_OBJECT (_gda_recordset));
		GdaConnection* cnc = gda_recordset_get_connection (_gda_recordset);
		if (NULL != cnc) {
			gtk_object_ref (GTK_OBJECT (cnc));
		}
#endif
	}
}

void
Recordset::unref ()
{
	if (_gda_recordset != NULL) {
		GdaConnection* cnc = gda_recordset_get_connection (_gda_recordset);
		if (cnc != NULL) {
			gda_connection_free (cnc);
		}

		gda_recordset_free (_gda_recordset);
	}
}

