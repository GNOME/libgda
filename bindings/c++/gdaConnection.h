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

namespace gda {

class Recordset;
class Error;
class ErrorList;

class Connection {

	friend class Batch;
	friend class Command;
	friend class Recordset;

	      public:
		Connection ();
		Connection (const Connection& cnc);
		Connection (GdaConnection *cnc);
		Connection (CORBA_ORB orb);
		~Connection ();

		Connection& operator=(const Connection& cnc);

		void setProvider (const string& name);
		string getProvider ();
		bool supports (GDA_Connection_Feature feature);
		void setDefaultDB (const string& dsn);
		gint open (const string& dsn, const string& user, const string& pwd);
		void close ();
		Recordset openSchema (GDA_Connection_QType t, ...);
		Recordset openSchemaArray (GDA_Connection_QType t, GdaConstraint_Element* constraint);
		glong modifySchema (GDA_Connection_QType t, ...);
		ErrorList getErrors ();
		gint beginTransaction ();
		gint commitTransaction ();
		gint rollbackTransaction ();
		Recordset execute (const string& txt, gulong& reccount, gulong flags);
		gint startLogging (const string& filename);
		gint stopLogging ();

		void addSingleError (Error& xError);
		void addErrorlist (ErrorList& xList);

		bool isOpen ();
		string getDSN ();
		string getUser ();

//		glong getFlags ();
//		void setFlags (glong flags);
//		glong getCmdTimeout ();
//		void setCmdTimeout (glong cmdTimeout);
//		glong getConnectTimeout ();
//		void setConnectTimeout (glong timeout);
//		GDA_CursorLocation getCursorLocation ();
//		void setCursorLocation (GDA_CursorLocation cursor);
		string getVersion ();

	      private:
		GdaConnection* getCStruct (bool ref = true) const;
		void setCStruct (GdaConnection *cnc);

		void ref () const;
		void unref ();

		  GdaConnection * _gda_connection;
	};

};

#endif // __gda_bindings_cpp_gdaConnectionH
