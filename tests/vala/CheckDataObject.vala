/* -*- Mode: Vala; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * libgda
 * Copyright (C) Daniel Espinosa Ortiz 2008 <esodan@gmail.com>
 * 
 * libgda is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * libgda is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

using Gda;
using GdaData;

namespace Check {
	class Tests : GdaData.Object {
		Tests()
		{
			try {
				this.connection = Connection.open_sqlite(null, "check_dataobject", true);
				this.connection.execute_non_select_command("CREATE TABLE user (int id, string name, string city)");
				this.connection.execute_non_select_command("INSERT INTO user (id, name, city) VALUES (1, \"Daniel\", \"Mexico\")");
				this.connection.execute_non_select_command("INSERT INTO user (id, name, city) VALUES (2, \"Jhon\", \"USA\")");
			
				this.connection.execute_non_select_command("CREATE TABLE company (int id, string name, string responsability)");
				this.connection.execute_non_select_command("INSERT INTO company (id, name, city) VALUES (1, \"Telcsa\", \"Programing\")");
				this.connection.execute_non_select_command("INSERT INTO company (id, name, city) VALUES (2, \"Viasa\", \"Accessories\")");
			}
			catch (Error e) {
				stdout.printf ("Couln't create temporary database...\n");
			}
		}
		
		public int t1()
			throws Error
		{
			int fails = 0;
			this._table = "user";
			Value v = 1;
			this.set_id ("id", v);
			var f = this.get_field_id();
			if ( f != "id")
				fails++;
			
			var i = this.get_value_id();
			if (i != 1 )
			    fails++;
			    
			var vdb = this.get_value ("name");
			if ( vdb != "Daniel")
				fails++;
			
			return fails;
		}
		
		public static int main (string[] args) {
			stdout.printf ("Checking Gda.DataObject implementation\n");
			int failures = 0;
			var app = new Tests ();
			failures += app.t1 ();
			return failures != 0 ? 1 : 0;
		}
	}
}
