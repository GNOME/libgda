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

namespace gda
{

	class Recordset
	{
	      public:
		Recordset ();
		Recordset (GdaRecordset * rst, Connection * cnc);	// convenience functions!
		Recordset (GdaRecordset * rst, GdaConnection * cnc);	// convenience functions!
		~Recordset ();

		GdaRecordset *getCStruct ();
		void setCStruct (GdaRecordset * rst);

		void setName (gchar * name);
		void getName (gchar * name);
		void close ();
		Field *field (gchar * name);
		// FIXME: possibly add a fieldText() func?
		Field *field (gint idx);
		gint bof ();
		gint eof ();
		gulong move (gint count, gpointer bookmark);
		gulong moveFirst ();
		gulong moveLast ();
		gulong moveNext ();
		gulong movePrev ();
		gint rowsize ();
		gulong affectedRows ();
		gint open (Command * cmd, GDA_CursorType cursor_type, GDA_LockType lock_type, gulong options);	// FIXME: defaults
		gint open (gchar * txt, GDA_CursorType cursor_type,
			   GDA_LockType lock_type, gulong options);
		gint open (Command * cmd, Connection * cnc, GDA_CursorType cursor_type, GDA_LockType lock_type, gulong options);	// FIXME: defaults
		gint open (gchar * txt, Connection * cnc,
			   GDA_CursorType cursor_type, GDA_LockType lock_type,
			   gulong options);
		gint setConnection (Connection * cnc);
		Connection *getConnection ();
		gint addField (GdaField * field);
		GDA_CursorLocation getCursorloc ();
		void setCursorloc (GDA_CursorLocation loc);
		GDA_CursorType getCursortype ();
		void setCursortype (GDA_CursorType type);

	      private:
		  Connection * cnc;
		GdaRecordset *_gda_recordset;
	};

};

#endif // __gda_bindings_cpp_gdaRecordsetH
