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

namespace gda {

class Command {
	public:
		Command();
		Command(GdaCommand *cmd);
		~Command();

		GdaCommand *getCStruct();
		void setCStruct(GdaCommand *cmd);
		
		Connection *getConnection();
		gint setConnection(Connection *a);
		gchar *getText();
		void setText(gchar *text);
		GDA_CommandType getCmdType();
		void setCmdType(GDA_CommandType type);
		Recordset *execute(gulong* reccount, gulong flags);
		void createParameter(gchar* name, GDA_ParameterDirection inout, Value *value);
		glong getTimeout();
		void setTimeout(glong timeout);

	private:
		Connection *cnc;
		GdaCommand* _gda_command;
};

};

#endif
