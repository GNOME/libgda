/* -*- Mode: Vala; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * libgdadata Unit Tests
 * Copyright (C) Daniel Espinosa Ortiz 2011 <esodan@gmail.com>
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
	class Record : RecordSingleId
	{
		public static string t = "user";
		public override string table { 
			get { return this.t; }
		}
		public override string field_id {
			get { return "id";}
		}
		public override int field_id_index {
			get { return 0;}
		}
	}
	class Tests :  GLib.Object {
		public Gda.Connection connection { get; set; }
		
		Tests()
		{
			try {
				GLib.FileUtils.unlink("dataobject.db");
				stdout.printf("Creating Database...\n");
				this.connection = Connection.open_from_string("SQLite", "DB_DIR=.;DB_NAME=dataobject", null, Gda.ConnectionOptions.NONE);
				stdout.printf("Creating table 'user'...\n");
				this.connection.execute_non_select_command("CREATE TABLE user (id integer PRIMARY KEY AUTOINCREMENT, name string, city string)");
				this.connection.execute_non_select_command("INSERT INTO user (id, name, city) VALUES (1, \"Daniel\", \"Mexico\")");
				this.connection.execute_non_select_command("INSERT INTO user (id, name, city) VALUES (2, \"Jhon\", \"USA\")");
				
				stdout.printf("Creating table 'company'...\n");
				this.connection.execute_non_select_command("CREATE TABLE company (id int PRIMARY KEY, name string, responsability string)");
				this.connection.execute_non_select_command("INSERT INTO company (id, name, responsability) VALUES (1, \"Telcsa\", \"Programing\")");
				this.connection.execute_non_select_command("INSERT INTO company (id, name, responsability) VALUES (2, \"Viasa\", \"Accessories\")");
				this.connection.update_meta_store(null);
			}
			catch (Error e) {
				stdout.printf ("Couln't create temporary database...\nERROR: %s\n", e.message);
			}
		}
		
		public int t1()
			throws Error
		{
			stdout.printf(">>> NEW TEST: Gda.DataObject API tests\n");
			int fails = 0;
			var r = new Check.Record ();
			r.connection = this.connection;
			stdout.printf("Setting ID to 1\n");
			try {
				r.set_id (1);
			}
			catch (Error e) {
				fails++;
				stdout.printf ("Couln't set ID...\nFAILS: %i\nERROR: %s\n", fails, e.message);
			}
			
			stdout.printf("DataObject points to, in table "+ r.table + ":\n", r.table);
			stdout.printf("%s\n", r.to_string());
			
			stdout.printf("Getting ID value...\n");
			var i = (int) r.get_id ();
			if (i != 1 ){
				fails++;
				stdout.printf("FAILS: %i\n", fails);
			}
			
			stdout.printf("Getting value at 'name'...\n");
			var vdb = (string) r.get_value ("name");
			if (vdb == null ){
				fails++;
				stdout.printf("FAILS: %i\n", fails);
			}
			else
				if ( (string)vdb != "Daniel"){
					fails++;
					stdout.printf("FAILS: %i\n", fails);
				}
			
			stdout.printf("Setting value at 'name'...\n");
			Value n = "Daniel Espinosa";
			r.set_value ("name", n);
			stdout.printf("DataObject points to in memory modified value, in table '%s':\n", r.table);
			stdout.printf("%s\n", r.to_string());
			
			stdout.printf("Saving changes...\n");
			try {
				r.save();
				stdout.printf("DataObject points to modified value, in table '%s':\n", r.table);
				stdout.printf("%s\n", r.to_string());
			}
			catch (Error e) {
				fails++;
				stdout.printf ("Couln't SAVE...\nFAILS: %i\nERROR: %s\n", fails, e.message);
			}
			
			try {
				stdout.printf ("Simulating external database update\n");
				this.connection.execute_non_select_command("UPDATE user SET name = \"Jhon Strauss\", city =\"New Jersey\"");
			}
			catch (Error e) {
				fails++;
				stdout.printf ("Couln't manual update table '%s'...\nFAILS: %i\nERROR: %s\n", r.table, fails, e.message);
			}
			
			stdout.printf("Updating values from database...\n");
			try {
				r.update();
				stdout.printf("DataObject points to actual stored values, in table '%s':\n", r.table);
				stdout.printf("%s\n", r.to_string());
			}
			catch (Error e) {
				fails++;
				stdout.printf ("Couln't UPDATE...\nFAILS: %i\nERROR: %s\n", fails, e.message);
			}
			
			stdout.printf("No Common Operation: Setting a new Table... \n");
			r.t = "company";
			stdout.printf("Updating values from database using a new table 'company'...\n");
			try {
				r.update();
				stdout.printf("DataObject points to actual stored values, in table '%s':\n", r.table);
				stdout.printf("%s\n", r.to_string());
			}
			catch (Error e) {
				fails++;
				stdout.printf ("Couln't UPDATE...\nFAILS: %i\nERROR: %s\n", fails, e.message);
			}
			
			stdout.printf("Setting ID to 2\n");
			try {
				r.set_id (2);
				stdout.printf("DataObject points to actual stored values, in table '%s':\n", r.table);
				stdout.printf("%s\n", r.to_string());
			}
			catch (Error e) {
				fails++;
				stdout.printf ("Couln't set ID...\nFAILS: %i\nERROR: %s\n", fails, e.message);
			}
			r.t = "user"; // Reset to default
			return fails;
		}
		
		public int t2()
			throws Error
		{
			stdout.printf(">>> NEW TEST: Gda.DataObject Adding new objects to DB\n");
			int fails = 0;
			try {
				var n = new Check.Record ();
				n.connection = this.connection;
				n.set_value ("id", 3);
				n.set_value ("name", "GdaDataNewName");
				n.set_value ("city", "GdaDataNewCity");
				stdout.printf("DataObject points to actual stored values, in table '%s':\n", n.table);
				stdout.printf("%s\n", n.to_string());
				n.append ();
				var m = n.connection.execute_select_command ("SELECT * FROM user");
				if (m.get_n_rows () != 3) fails++;
				stdout.printf ("All records:\n" + m.dump_as_string () + "\n");
			}
			catch (Error e) {
				fails++;
				stdout.printf ("Couln't set ID...\nFAILS: %i\nERROR: %s\n", fails, e.message);
			}
			return fails;
		}
		
		public static int main (string[] args) {
			stdout.printf ("Checking Gda.DataObject implementation...\n");
			int failures = 0;
			var app = new Tests ();
			try {
				failures += app.t1 ();
				failures += app.t2 ();
			}
			catch (Error e) { stdout.printf ("ERROR: " + e.message); }
			return failures != 0 ? 1 : 0;
		}
	}
}
