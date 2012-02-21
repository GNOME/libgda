/* -*- Mode: Vala; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * libgdadata Unit Tests
 * Copyright (C) Daniel Espinosa Ortiz 2012 <esodan@gmail.com>
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
				this.connection = Connection.open_from_string("SQLite", "DB_DIR=.;DB_NAME=table", null, Gda.ConnectionOptions.NONE);
				stdout.printf("Creating table 'company'...\n");
				this.connection.execute_non_select_command("CREATE TABLE company (id int PRIMARY KEY, name string, responsability string)");
				this.connection.execute_non_select_command("INSERT INTO company (id, name, responsability) VALUES (1, \"Telcsa\", \"Programing\")");
				this.connection.execute_non_select_command("INSERT INTO company (id, name, responsability) VALUES (2, \"Viasa\", \"Accessories\")");
				
				stdout.printf("Creating table 'customer'...\n");
				this.connection.execute_non_select_command("CREATE TABLE customer (id integer PRIMARY KEY AUTOINCREMENT, name string UNIQUE,"+
				                                           " city string DEFAULT \"New Yield\","+
				                                           " company integer REFERENCES company (id) ON DELETE SET NULL ON UPDATE CASCADE)");
				this.connection.execute_non_select_command("INSERT INTO customer (id, name, city, company) VALUES (1, \"Daniel\", \"Mexico\", 1)");
				this.connection.execute_non_select_command("INSERT INTO customer (id, name, city) VALUES (2, \"Jhon\", \"USA\")");
				this.connection.execute_non_select_command("INSERT INTO customer (id, name) VALUES (3, \"Jack\")");
				stdout.printf("Creating table 'salary'...\n");
				this.connection.execute_non_select_command("CREATE TABLE salary (id integer PRIMARY KEY AUTOINCREMENT,"+
				                                           " customer integer REFERENCES customer (id) ON DELETE CASCADE ON UPDATE CASCADE,"+
				                                           " income float DEFAULT 10.0)");
				this.connection.execute_non_select_command("INSERT INTO salary (id, customer, income) VALUES (1,1,55.0)");
				this.connection.execute_non_select_command("INSERT INTO salary (id, customer, income) VALUES (2,2,65.0)");
				this.connection.execute_non_select_command("INSERT INTO salary (customer) VALUES (3)");
			}
			catch (Error e) {
				stdout.printf ("Couln't create temporary database...\nERROR: %s\n", e.message);
			}
		}
		
		public void init ()
			throws Error
		{
			stdout.printf("Creating new table\n");
			table = new Table ();
			stdout.printf("Setting connection\n");
			table.connection = this.connection;
			stdout.printf("Setting name\n");
			table.name = "customer";
		}
		public int update ()
			throws Error
		{
			stdout.printf("\n\n\n>>>>>>>>>>>>>>> NEW TEST: GdaData.DbTable -- Update\n");
			int fails = 0;
			stdout.printf(">>>>>> Updating meta information\n");
			table.update ();
			if (fails > 0)
				stdout.printf (">>>>>>>> FAIL <<<<<<<<<<<\n");
			else
				stdout.printf (">>>>>>>> TEST PASS <<<<<<<<<<<\n");
			return fails;
		}
		
		public int fields ()
			throws Error
		{
			stdout.printf("\n\n\n>>>>>>>>>>>>>>> NEW TEST: Gda.DbTable - Fields...\n");
			int fails = 0;
			var f = new Gee.HashMap<string,int> ();
			f.set ("id", 0);
			f.set ("name", 0);
			f.set ("city", 0);
			f.set ("company",0);
			foreach (DbFieldInfo fi in table.fields)
			{
				if (! f.keys.contains (fi.name))
				{
					fails++;
					stdout.printf (">>>>>>>> Check Fields names:  FAIL\n");
					break;
				}
			}
			var f2 = new Gee.HashMap<string,int> ();
			f2.set ("id",0);
			f2.set ("name",0);
			foreach (DbFieldInfo fi2 in table.primary_keys)
			{
				if (! f2.keys.contains (fi2.name))
				{
					stdout.printf (">>>>>>>> Check Primary Keys Fields:  FAIL\n");
					fails++;
					break;
				}
			}
			
			var f3 = new Gee.HashMap<string,int> ();
			f3.set ("company",0);
			foreach (DbTable t in table.depends)
			{
				if (! f3.keys.contains (t.name))
				{
					stdout.printf (">>>>>>>> Check Table Depends:  FAIL\n");
					fails++;
					break;
				}
			}
			
			var f4 = new Gee.HashMap<string,int> ();
			f4.set ("salary",0);
			foreach (DbTable t2 in table.referenced)
			{
				if (! f4.keys.contains (t2.name))
				{
					stdout.printf (">>>>>>>> Check Table Referenced by:  FAIL\n");
					fails++;
					break;
				}
			}
			
			// Test for default values
			int found = 0;
			foreach (DbFieldInfo fi3 in table.fields) {
				if (DbFieldInfo.Attribute.HAVE_DEFAULT in fi3.attributes
						&& fi3.name == "city") {
					found++;
					if (GLib.strcmp ((string)fi3.default_value,"\"New Yield\"") != 0) {
						fails++;
						stdout.printf (">>>>>>>> Default Value No Match. Holded \'"+
						               (string) fi3.default_value + "\' But Expected \"New Yield\" : FAIL\n");
					}
					break;
				}
			}
			
			if (found == 0) {
				stdout.printf (">>>>>>>> Check Default Values: FAIL\n");
				fails++;
			}
			if (fails > 0)
				stdout.printf (">>>>>>>> FAIL <<<<<<<<<<<\n");
			else
				stdout.printf (">>>>>>>> TEST PASS <<<<<<<<<<<\n");
			return fails;
		}
		
		public static int main (string[] args) {
			stdout.printf ("\n\n\n>>>>>>>>>>>>>>>> NEW TEST: Checking GdaData.DbRecord implementation... <<<<<<<<<< \n");
			int failures = 0;
			var app = new Tests ();
			try {
				app.init ();
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
