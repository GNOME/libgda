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

	class DbRecord : GdaData.Object {
		private static string dbtable = "user";
		
		/**
		 * On derived classes you must implement this property.
		 * Is intended that implementors set table and don't change it in the object live time
		 * in order to keep a good reference to the data the object refers to.
		 */
		public override string table { 
			get { return this.dbtable; }
		}
		
		/**
		 * Wrapping database fields.
		 * You can define properties that use database stored values.
		 * Fields are not limited on the ones defined on the class,
		 * you can always access any other in the database using
		 * get_value(string) function.
		 */
		public string functions {
			get {
					return (string) this.get_value ("functions");
			}
			set {
				try {
					this.set_value ("functions", value);
				}
				catch {}
			}
		}
		
		public string name {
			get {
				return (string) this.get_value ("name");
			}
			
			set {
				try {
					this.set_value ("name", value);
				}
				catch {}
			}
		}
		
		/**
		 * This function is a wrapper to set the id field
		 * and id value used to retrieve a record from the
		 * database.
		 * 
		 * In this example 'user' table have two keys, id and
		 * name (declared as UNIQUE), then we can use the last
		 * to identify any record.
		 */
		public void open (Value name) 
			throws Error
		{
			this.set_id ("name", name);
		}
		
	}

	class App : GLib.Object {
		public Gda.Connection connection;
		
		App ()
		{
			try {
				GLib.FileUtils.unlink("dataobject.db");
				stdout.printf("Creating Database...\n");
				this.connection = Connection.open_from_string("SQLite", 
									"DB_DIR=.;DB_NAME=dataobject", null, 
									Gda.ConnectionOptions.NONE);
				stdout.printf("Creating table 'user'...\n");
				this.connection.execute_non_select_command("CREATE TABLE user (id int PRIMARY KEY AUTOINCREMENT, name string UNIQUE, functions string, security_number int)");
				this.connection.execute_non_select_command("INSERT INTO user (id, name, functions) VALUES (1, \"Martin Stewart\", \"Programmer, QA\", 2334556");
				this.connection.execute_non_select_command("INSERT INTO user (id, name, functions) VALUES (2, \"Jane Castle\", \"Accountant\", 3002884");
				
				this.connection.update_meta_store(null);
			}
			catch (Error e) {
				stdout.printf ("Can't create temporary database...\nERROR: " + e.message + "\n");
			}
		}
		
		public void modify_record (string name)
			throws Error
		{
			var rcd = new DbRecord ();
			rcd.open (name);
			
			stdout.printf ("Modifing user: " + rcd.name);
			foreach (Value v in rcd.record) {
				stdout.printf ("Field Value: " + Gda.value_stringify (v));
			}
			
			// Changing functions
			rcd.functions += ", Hardware Maintenance";
			
			// Updating Name
			rcd.name += " PhD.";
			
			// Changing non class property value in the record
			// You must know the field name refer to
			string v = (string) rcd.get_value ("security_number");
			stdout.printf ("Taken value from a field in the DB: " + Gda.value_stringify (v));
		}
			
		public static int main (string[] args) {
			stdout.printf ("Gda.DataObject Example...\n");
			var app = new App ();
			try {
				app.modify_record ("Martin Stewart");
				app.modify_record ("Jane Castle");
				return 0;
			}
			catch (Error e) {
				stdout.printf ("Can't modify record\nERROR: " + e.message + "\n");
			}
			
			return 0;
		}
	}
}
