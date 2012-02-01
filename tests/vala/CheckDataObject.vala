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
using Gee;

namespace Check {
	class Record : GdaData.Record
	{
		public static string t = "user";
		
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
			stdout.printf(">>> NEW TEST: GdaData.DbRecord API tests\n");
			int fails = 0;
			stdout.printf("Creating new record\n");
			var r = new Check.Record ();
			stdout.printf("Setting connection\n");
			r.connection = this.connection;
			stdout.printf("Setting up DbTable\n");
			var t = new Table ();
			stdout.printf("Setting DbTable name\n");
			t.name = "user";
			stdout.printf("Setting DbTable connection\n");
			t.connection = this.connection;
			stdout.printf(">>> Setting table to record\n");
			r.table = t;
			stdout.printf(">>> Setting up Key 'id'\n");
			var k = new Field ("id", DbField.Attribute.NONE);
			stdout.printf("Setting record ID to 1...");
			try {
				k.value = 1;
				r.set_key (k);
				foreach (DbField kv in r.keys) {
					stdout.printf ("KEY: " + kv.name + " VALUE: " + Gda.value_stringify(kv.value) + "\n");
				}
				r.update ();
				foreach (DbField kv in r.fields) {
					stdout.printf ("FIELD: " + kv.name + " VALUE: " + Gda.value_stringify(kv.value) + "\n");
				}
				stdout.printf("PASS\n");
			}
			catch (Error e) {
				fails++;
				stdout.printf ("FAIL: %i\nCouln't set ID...ERROR: %s\n", fails, e.message);
			}
			
			stdout.printf("DbRecord points to, in table "+ r.table.name + ":\n", r.table);
			stdout.printf("%s\n", r.to_string());
			
			stdout.printf("Getting ID value...");
			var i = (int) (r.get_key ("id")).value;
			if (i != 1 ){
				fails++;
				stdout.printf("FAIL: %i\n", fails);
			}
			else
				stdout.printf("PASS\n");
			
			stdout.printf("Getting value at 'name'...");
			var vdb = (string) (r.get_field ("name")).value;
			if (vdb == null ){
				fails++;
				stdout.printf("FAIL: %i\n", fails);
			}
			else 
				if ( vdb != "Daniel"){
					fails++;
					stdout.printf("FAIL: %i\nERROR: Value not match. Expected 'Daniel' but value is %s:\n", 
									fails, vdb);
				}
				else
					stdout.printf("PASS\n");
			
			stdout.printf("Setting value at 'name'...");
			try {
				var f = r.get_field ("name");
				f.value = "Daniel Espinosa";
				r.set_field (f);
				stdout.printf("DataObject points to in memory modified value, in table '%s':\n", r.table.name);
				stdout.printf("%s\n", r.to_string());
				stdout.printf("PASS\n");
			}
			catch (Error e) {
				fails++;
				stdout.printf ("FAIL: %i\nCouln't modify record...ERROR: %s\n", fails, e.message);
			}
			stdout.printf("Saving changes...");
			try {
				r.save();
				stdout.printf("DataObject points to modified value, in table '%s':\n", r.table.name);
				stdout.printf("%s\n", r.to_string());
				stdout.printf("PASS\n");
			}
			catch (Error e) {
				fails++;
				stdout.printf ("FAIL: %i\nCouln't SAVE...ERROR: %s\n", fails, e.message);
			}
			
			try {
				stdout.printf ("Simulating external database update...");
				this.connection.execute_non_select_command("UPDATE user SET name = \"Jhon Strauss\", city =\"New Jersey\"");
				stdout.printf("PASS\n");
			}
			catch (Error e) {
				fails++;
				stdout.printf ("FAIL: %i\nCouln't manual update table '%s'...ERROR: %s\n", 
								fails, r.table.name,  e.message);
			}
			
			stdout.printf("Updating values from database...");
			try {
				r.update();
				stdout.printf("DataObject points to actual stored values, in table '%s':\n", r.table.name);
				stdout.printf("%s\n", r.to_string());
				
			}
			catch (Error e) {
				fails++;
				stdout.printf ("FAIL: %i\nCouln't UPDATE...ERROR: %s\n", fails, e.message);
			}
			
			stdout.printf("Setting a new Table... \n");
			var t2 = new Table ();
			t2.name = "company";
			t2.connection = this.connection;
			r.table = t2;
			stdout.printf("Updating values from database using a new table '" + r.table.name + "'...");
			try {
				r.update();
				stdout.printf("DataObject points to actual stored values, in table '%s':\n", r.table.name);
				stdout.printf("%s\n", r.to_string());
				stdout.printf("PASS\n");
			}
			catch (Error e) {
				fails++;
				stdout.printf ("FAIL: %i\nCouln't UPDATE...ERROR: %s\n", fails, e.message);
			}
			
			stdout.printf("Setting ID to 2...");
			try {
				k.value = 2;
				r.set_key (k);
				stdout.printf("DataObject points to actual stored values, in table '%s':\n", r.table.name);
				stdout.printf("%s\n", r.to_string());
				stdout.printf("PASS\n");
			}
			catch (Error e) {
				fails++;
				stdout.printf ("FAIL: %i\nCouln't set ID...ERROR: %s\n", fails, e.message);
			}
			r.t = "user"; // Reset to default
			return fails;
		}
		
		public int t2()
			throws Error
		{
			stdout.printf(">>> NEW TEST: Gda.DbRecord - Adding new objects to DB...\n");
			int fails = 0;
			try {
				var n = new Check.Record ();
				n.connection = this.connection;
				var t = new Table ();
				t.name = "user";
				t.connection = this.connection;
				n.table = t;
				var f = new HashMap<string,Value?> ();
				f.set ("id", 3);
				f.set ("name", "GdaDataNewName");
				f.set ("city","GdaDataNewCity");
				foreach (string k in f.keys) {
					n.set_field_value (k, f.get (k));
				}
				stdout.printf("DbRecord in memory values, to added to table '%s':\n", n.table.name);
				stdout.printf("%s\n", n.to_string());
				n.append ();
				var m = n.connection.execute_select_command ("SELECT * FROM user");
				if (m.get_n_rows () != 3) fails++;
				stdout.printf ("All records:\n" + m.dump_as_string () + "\n");
				stdout.printf("PASS\n");
			}
			catch (Error e) {
				fails++;
				stdout.printf ("FAIL: %i\nCouln't set add new record...-ERROR: %s\n", fails, e.message);
			}
			return fails;
		}
		
		public static int main (string[] args) {
			stdout.printf ("Checking GdaData.DbRecord implementation...\n");
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
