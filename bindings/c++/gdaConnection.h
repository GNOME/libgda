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

#ifndef __gda_bindings_cpp_gdaConnectionH
#define __gda_bindings_cpp_gdaConnectionH

#include "gdaIncludes.h"

class gdaConnection {
	public:
		gdaConnection();
		gdaConnection(GdaConnection *a);
		gdaConnection(CORBA_ORB orb);
		~gdaConnection();
		
		GdaConnection* getCStruct();
		void setCStruct(GdaConnection *cnc);
		
		void setProvider(gchar* name);
		const gchar* getProvider();
		gboolean supports(GDA_Connection_Feature feature);
		void setDefaultDB(gchar* dsn);
		gint open(gchar* dsn, gchar* user,gchar* pwd);	
		void close();
		gdaErrorList *getErrors();
		gint beginTransaction();
		gint commitTransaction();
		gint rollbackTransaction();
		gdaRecordset* execute(gchar* txt, gulong* reccount, gulong flags);
		gint startLogging(gchar* filename);
		gint stopLogging();
	
		void addSingleError(gdaError* error);
		void addErrorlist(gdaErrorList* list);

		gboolean isOpen();
		gchar* getDSN();
		gchar* getUser();

		glong getFlags();
		void setFlags(glong flags);
		glong getCmdTimeout();
		void setCmdTimeout(glong cmd_timeout);
		glong getConnectTimeout();
		void setConnectTimeout(glong timeout);
		GDA_CursorLocation getCursorLocation();
		void setCursorLocation(GDA_CursorLocation cursor);
		gchar* getVersion();
			
	private:
		GdaConnection* _gda_connection;
};

#endif // __gda_bindings_cpp_gdaConnectionH
