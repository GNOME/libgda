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

namespace gda {

class Recordset;
class Value;

class Command {

	friend class Recordset;

	      public:
		Command ();
        Command (const Command& cmd);
		Command (GdaCommand * cmd);
		~Command ();

        Command& operator=(const Command& cmd);

		Connection getConnection ();
		gint setConnection (const Connection& cnc);
		string getText ();
		void setText (const string& text);
		GDA_CommandType getCmdType ();
		void setCmdType (GDA_CommandType type);
		Recordset execute (gulong& reccount, gulong flags);
		void createParameter (
			const string& name,
				      GDA_ParameterDirection inout,
			const Value& value);
//		glong getTimeout ();
//		void setTimeout (glong timeout);

	      private:
		GdaCommand *getCStruct (bool refn = true) const;
		void setCStruct (GdaCommand *cmd);

		void ref () const;
		void unref ();

		GdaCommand *_gda_command;
		Connection _connection;
		vector<Value> _parameters_values;
	};

};

#endif // __gda_bindings_cpp_gdaCommandH

