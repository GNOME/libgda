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
	class Tests : GdaData.Object {
		Tests()
		{
			try {
				GLib.FileUtils.unlink("dataobject.db");
				stdout.printf("Creating Database...\n");
				this.connection = Connection.open_from_string("SQLite", "DB_DIR=.;DB_NAME=dataobject", null, Gda.ConnectionOptions.NONE);
				stdout.printf("Creating table 'user'...\n");
				this.connection.execute_non_select_command("CREATE TABLE user (id int PRIMARY KEY, name string, city string)");
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
			this._table = "user";
			Value v = 1;
			stdout.printf("Setting ID to %i\n", (int) v);
			try {
				this.set_id ("id", v);
				var f = this.get_field_id();
				if ( f != "id") {
					fails++;
					stdout.printf("FAILS: %i\n", fails);
				}
			}
			catch (Error e) {
				fails++;
				stdout.printf ("Couln't set ID...\nFAILS: %i\nERROR: %s\n", fails, e.message);
			}
			
			stdout.printf("DataObject points to, in table '%s':\n", this._table);
			stdout.printf("%s\n", this._model.dump_as_string());
			
			stdout.printf("Getting ID value...\n");
			var i = (int) this.get_value_id();
			if (i != 1 ){
				fails++;
				stdout.printf("FAILS: %i\n", fails);
			}
			
			stdout.printf("Getting value at 'name'...\n");
			var vdb = (string) this.get_value ("name");
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
			this.set_value ("name", n);
			stdout.printf("DataObject points to in memory modified value, in table '%s':\n", this._table);
			stdout.printf("%s\n", this._model.dump_as_string());
			
			stdout.printf("Saving changes...\n");
			try {
				this.save();
				stdout.printf("DataObject points to modified value, in table '%s':\n", this._table);
				stdout.printf("%s\n", this._model.dump_as_string());
			}
			catch (Error e) {
				fails++;
				stdout.printf ("Couln't SAVE...\nFAILS: %i\nERROR: %s\n", fails, e.message);
			}
			
			try {
				this.connection.execute_non_select_command("UPDATE user SET name = \"Jhon Strauss\", city =\"New Jersey\"");
			}
			catch (Error e) {
				fails++;
				stdout.printf ("Couln't manual update table '%s'...\nFAILS: %i\nERROR: %s\n", this._table, fails, e.message);
			}
			
			stdout.printf("Updating values from database...\n");
			try {
				this.update();
				stdout.printf("DataObject points to actual stored values, in table '%s':\n", this._table);
				stdout.printf("%s\n", this._model.dump_as_string());
			}
			catch (Error e) {
				fails++;
				stdout.printf ("Couln't UPDATE...\nFAILS: %i\nERROR: %s\n", fails, e.message);
			}
			
			stdout.printf("Setting a new Table... \n");
			this._table = "company";
			stdout.printf("Updating values from database using a new table 'company'...\n");
			try {
				this.update();
				stdout.printf("DataObject points to actual stored values, in table '%s':\n", this._table);
				stdout.printf("%s\n", this._model.dump_as_string());
			}
			catch (Error e) {
				fails++;
				stdout.printf ("Couln't UPDATE...\nFAILS: %i\nERROR: %s\n", fails, e.message);
			}
			
			v = 2;
			stdout.printf("Setting ID to %i\n", (int) v);
			try {
				this.set_id ("id", v);
				stdout.printf("DataObject points to actual stored values, in table '%s':\n", this._table);
				stdout.printf("%s\n", this._model.dump_as_string());
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
			failures += app.t1 ();
			return failures != 0 ? 1 : 0;
		}
	}
}
