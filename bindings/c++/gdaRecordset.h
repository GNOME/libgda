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

#ifndef __gda_bindings_cpp_gdaRecordsetH
#define __gda_bindings_cpp_gdaRecordsetH

#include "gdaIncludes.h"

class gdaRecordset {
	public:
		gdaRecordset();
		gdaRecordset(Gda_Recordset *rst, gdaConnection *cnc); // convenience functions!
		gdaRecordset(Gda_Recordset *rst, Gda_Connection *cnc); // convenience functions!
		~gdaRecordset();

		Gda_Recordset* getCStruct();
		void setCStruct(Gda_Recordset *rst);

		void setName(gchar* name);
		void getName(gchar* name);
		void close();
		gdaField* field(gchar* name);
		// FIXME: possibly add a fieldText() func?
		gdaField* field(gint idx);
		gint bof();
		gint eof();
		gulong move (gint count, gpointer bookmark);
		gulong moveFirst();
		gulong moveLast();
		gulong moveNext();
		gulong movePrev();
		gint rowsize();
		gulong affectedRows();
		gint open(gdaCommand* cmd, GDA_CursorType cursor_type, GDA_LockType lock_type, gulong options); // FIXME: defaults
		gint open(gchar* txt, GDA_CursorType cursor_type, GDA_LockType lock_type, gulong options);
		gint open(gdaCommand* cmd, gdaConnection *cnc, GDA_CursorType cursor_type, GDA_LockType lock_type, gulong options); // FIXME: defaults
		gint open(gchar* txt, gdaConnection *cnc, GDA_CursorType cursor_type, GDA_LockType lock_type, gulong options);
		gint setConnection(gdaConnection *cnc);
		gdaConnection *getConnection();
		gint addField(Gda_Field* field);
		GDA_CursorLocation getCursorloc();
		void setCursorloc(GDA_CursorLocation loc );
		GDA_CursorType getCursortype();
		void setCursortype(GDA_CursorType type);

	private:
		gdaConnection *cnc;
		Gda_Recordset* _gda_recordset;
};

#endif // __gda_bindings_cpp_gdaRecordsetH

