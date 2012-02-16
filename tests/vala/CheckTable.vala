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
	class Tests :  GLib.Object {
		public Gda.Connection connection { get; set; }
		public DbTable table;
		
		Tests()
		{
			try {
				GLib.FileUtils.unlink("table.db");
				stdout.printf("Creating Database...\n");
				this.connection = Connection.open_from_string("SQLite", "DB_DIR=.;DB_NAME=dataobject", null, Gda.ConnectionOptions.NONE);
				stdout.printf("Creating table 'user'...\n");
				this.connection.execute_non_select_command("CREATE TABLE user (id integer PRIMARY KEY AUTOINCREMENT, name string UNIQUE, city string)");
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
		
		public int update ()
			throws Error
		{
			stdout.printf(">>> NEW TEST: GdaData.DbTable -- Update\n");
			int fails = 0;
			stdout.printf("Creating new table\n");
			table = new Table ();
			stdout.printf("Setting connection\n");
			t.connection = this.connection;
			stdout.printf("Setting name\n");
			t.name = "user";
			stdout.printf(">>> Updating meta information\n");
			t.update ();
			return fails;
		}
		
		public int fields ()
			throws Error
		{
			stdout.printf(">>> NEW TEST: Gda.DbTable - Fields...\n");
			int fails = 0;
			var f = new Gee.HashSet<string> ();
			f.add ("id");
			f.add ("name");
			f.add ("city");
			foreach (DbFieldInfo fi in table.fields)
			{
				if (! f.contains (fi.name))
				{
					fails++;
					break;
				}
			}
			var f2 = new Gee.HashSet<string> ();
			f2.add ("id");
			f2.add ("name");
			foreach (DbFieldInfo fi2 in table.primary_keys)
			{
				if (! f2.contains (fi2.name))
				{
					fails++;
					break;
				}
			}
			
			//FIXME: Add test for default values
			
			return fails;
		}
		
		public static int main (string[] args) {
			stdout.printf ("Checking GdaData.DbRecord implementation...\n");
			int failures = 0;
			var app = new Tests ();
			try {
				failures += app.update ();
				failures += app.fields ();
				//failures += app.records ();
				//failures += app.depends ();
				//failures += app.references ();
				//failures += app.check_expression ();
			}
			catch (Error e) 
			{ 
				stdout.printf ("ERROR: " + e.message); 
				return 1;
			}
			return failures != 0 ? 1 : 0;
		}
	}
}
