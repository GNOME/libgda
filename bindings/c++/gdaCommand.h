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

#ifndef __gda_bindings_cpp_gdaCommandH
#define __gda_bindings_cpp_gdaCommandH

#include "gdaIncludes.h"

class gdaCommand {
	public:
		gdaCommand();
		gdaCommand(Gda_Command *cmd);
		~gdaCommand();

		Gda_Command *getCStruct();
		void setCStruct(Gda_Command *cmd);
		
		gdaConnection *getConnection();
		gint setConnection(gdaConnection *a);
		gchar *getText();
		void setText(gchar *text);
		gulong getCmdType();
		void setCmdType(gulong flags);
		gdaRecordset *execute(gulong* reccount, gulong flags);
		void createParameter(gchar* name, GDA_ParameterDirection inout, gdaValue *value);
		glong getTimeout();
		void setTimeout(glong timeout);

	private:
		gdaConnection *cnc;
		Gda_Command* _gda_command;
};

#endif
