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

namespace Sample {

	class Record : GdaData.Record {
		/**
		 * Wrapping database fields.
		 * You can define properties that use database stored values.
		 * Fields are not limited on the ones defined on the class,
		 * you can always access any other in the database using
		 * get_value(string) function.
		 */
		public string functions {
			owned get {
					return (string) this.get_value ("functions");
			}
			set {
				try {
					this.set_field_value ("functions", value);
				}
				catch {}
			}
		}
		
		public string name {
			owned get {
				return (string) this.get_value ("name");
			}
			
			set {
				try {
					this.set_field_value ("name", value);
				}
				catch {}
			}
		}
		
		/**
		 * This will create a default table to 'user'.
		 */
		public Record ()
		{
			var t = new Table ();
			t.name = "user";
			t.connection = this.connection;
			this.table = t;
		}
	}

	class App : Object {
		public Gda.Connection connection;
		
		public void init ()
			throws Error
		{
			GLib.FileUtils.unlink("dataobject.db");
			stdout.printf("Creating Database...\n");
			this.connection = Connection.open_from_string("SQLite", 
								"DB_DIR=.;DB_NAME=dataobject", null, 
								Gda.ConnectionOptions.NONE);
			stdout.printf("Creating table 'user'...\n");
			this.connection.execute_non_select_command("CREATE TABLE user (name string PRIMARY KEY, functions string, security_number integer)");
			this.connection.execute_non_select_command("INSERT INTO user (name, functions, security_number) VALUES ( \"Martin Stewart\", \"Programmer, QA\", 2334556)");
			this.connection.execute_non_select_command("INSERT INTO user (name, functions, security_number) VALUES (\"Jane Castle\", \"Accountant\", 3002884)");
			
			this.connection.update_meta_store(null);
		}
		
		public void demo_modify_record (string name)
			throws Error
		{
			stdout.printf (">>> DEMO: Modifying Records\n");
			var rcd = new Sample.Record ();
			rcd.connection = this.connection;
			try { 
				rcd.set_key_value ("name", name);
				rcd.update ();
			}
			catch (Error e) { stdout.printf ("ERROR: Record no opened\n" + e.message + "\n"); }
			
			stdout.printf ("Initial Values for: " + rcd.name + "\n" + rcd.to_string () + "\n");
			
			stdout.printf ("Modifying user: " + rcd.name + "\n");
			// Changing functions
			rcd.functions += ", Hardware Maintenance";
			
			// Updating Name
			rcd.name += " PhD.";
			
			// Changing non class property value in the record
			// You must know the field name refer to
			var v = rcd.get_value ("security_number");
			stdout.printf ("Initial value for field 'security_number' in the DB: " + Gda.value_stringify (v) + "\n");
			rcd.set_field_value ("security_number", 1002335);
			
			try { rcd.save (); }
			catch (Error e) { stdout.printf ("ERROR: Can't save modifycations'\n" + e.message + "\n"); }
			stdout.printf ("Modified Values: " + rcd.name + "\n" + rcd.to_string () + "\n");
		}
		
		public void simulate_external_modifications ()
				throws Error
		{
			stdout.printf (">>> DEMO: Updating Records modified externally\n");
			var rcd = new Record ();
			rcd.connection = this.connection;
			rcd.set_key_value ("name", "Jane Castle PhD.");
			stdout.printf ("Initial Values for: " + rcd.name + "\n" + rcd.to_string () + "\n");
			this.connection.execute_non_select_command("UPDATE user SET functions = \"Secretary\" WHERE name = \"Jane Castle PhD.\"");
			rcd.update ();
			stdout.printf ("Updated Values for: " + rcd.name + "\n" + rcd.to_string () + "\n");
		}
		
		public void append_objects (string name, string functions)
			throws Error
		{
			stdout.printf("DEMO: Appending Objects...\n");
			var ob = new Sample.Record ();
			ob.connection = this.connection;
			ob.name = "Jacob Sanders PhD.";
			ob.append ();
			var m = this.connection.execute_select_command ("SELECT * FROM user");
			stdout.printf ("Appended Values:\n" + m.dump_as_string () + "\n");
		}
			
		public static int main (string[] args) {
			stdout.printf ("Gda.DataObject Example...\n");
			var app = new App ();
			
			try {
				app.init ();
				try {
					/* These will open and modify records with the given name */
					app.demo_modify_record ("Martin Stewart");
					app.demo_modify_record ("Jane Castle");
				}
				catch (Error e) { stdout.printf ("Can't modify record\nERROR: " + e.message + "\n"); }
				
				try {
					/* Simulating an 'external' application modifying DB records and how to update */
					app.simulate_external_modifications ();
				}
				catch (Error e) { stdout.printf ("Can't update record\nERROR: " + e.message + "\n"); }
				
				try {
					/* Append a new object with the same */
					app.append_objects ("Jack MacJason", "Maintener");
				}
				catch (Error e) { stdout.printf ("Can't append record\nERROR: " + e.message + "\n"); }
				
				return 0;
			}
			catch (Error e) 
			{
				stdout.printf ("Error on Initializing DB\nERROR: " + e.message + "\n");
			}
			return 1;
		}
	}
}
