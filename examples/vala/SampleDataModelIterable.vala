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
	
	class App : GLib.Object {
		private Gda.Connection connection;
		private RecordCollection itermodel;
		
		public void init ()
			throws Error
		{
			GLib.FileUtils.unlink("datamodeliterator.db");
			stdout.printf("Creating Database...\n");
			this.connection = Connection.open_from_string("SQLite", "DB_DIR=.;DB_NAME=datamodeliterator", null, Gda.ConnectionOptions.NONE);
			stdout.printf("Creating table 'user'...\n");
			this.connection.execute_non_select_command("CREATE TABLE user (id int PRIMARY KEY, name string, city string)");
			this.connection.execute_non_select_command("INSERT INTO user (id, name, city) VALUES (1, \"Daniel\", \"Mexico\")");
			this.connection.execute_non_select_command("INSERT INTO user (id, name, city) VALUES (2, \"Jhon\", \"USA\")");
			this.connection.execute_non_select_command("INSERT INTO user (id, name, city) VALUES (3, \"James\", \"Germany\")");
			this.connection.execute_non_select_command("INSERT INTO user (id, name, city) VALUES (4, \"Jack\", \"United Kindom\")");
			this.connection.execute_non_select_command("INSERT INTO user (id, name, city) VALUES (5, \"Elsy\", \"España\")");
			this.connection.execute_non_select_command("INSERT INTO user (id, name, city) VALUES (6, \"Mayo\", \"Mexico\")");
			
			
			stdout.printf("Creating table 'company'...\n");
			this.connection.execute_non_select_command("CREATE TABLE company (id int PRIMARY KEY, name string, responsability string)");
			this.connection.execute_non_select_command("INSERT INTO company (id, name, responsability) VALUES (1, \"Telcsa\", \"Programing\")");
			this.connection.execute_non_select_command("INSERT INTO company (id, name, responsability) VALUES (2, \"Viasa\", \"Accessories\")");
			this.connection.update_meta_store(null);
			var model = this.connection.execute_select_command ("SELECT * FROM user");
			var t = new Table ();
			t.connection = connection;
			t.name = "user";
			this.itermodel = new RecordCollection (model, t);
		}
		
		public void iterating ()
			throws Error
		{
			stdout.printf ("Iterating over all Records in DataModel using foreach\n");
			foreach (DbRecord r in itermodel) {
				stdout.printf (r.to_string ());
			}
			stdout.printf ("\nIterating over all Records in DataModel using a Gee.Iterator\n");
			var iter = itermodel.iterator ();
			while (iter.next ())
			{
				stdout.printf (iter.get ().to_string () + "\n");
			}
			stdout.printf ("\n");
		}
		
		public void choping ()
		{
			stdout.printf ("Choping to get the 2nd DbRecord...\n");
			var iter = itermodel.chop (1);
			while (iter.next ()) {
				stdout.printf (iter.get ().to_string () + "\n");
			}
			stdout.printf ("Choping to get the 4th to 5th DbRecords...\n");
			var iter2 = itermodel.chop (3,4);
			while (iter2.next ()) {
				var r2 = iter2.get ();
				stdout.printf (iter2.get ().to_string ());
			}
			stdout.printf ("\n");
		}
		
		public void filtering ()
		{
			stdout.printf ("Filtering Records: Any field type STRING with a letter 'x'...\n");
			
			var iter = itermodel.filter (
			(g) => {
				bool ret = false;
				foreach (DbField fl in g.fields) {
					string t = Gda.value_stringify (fl.value);
					stdout.printf ("Value to check: " + t);
					if (t.contains ("x")) {
						ret = true;
						stdout.printf ("...SELECTED\n");
						break;
					}
					else {
						ret = false;
						stdout.printf ("...REJECTED\n");
					}
				}
				if (ret) stdout.printf ("SELECTED ROW: \n" + g.to_string () + "\n");
				return ret;
			}
			);
			stdout.printf ("Printing Filtered Values...\n");
			while (iter.next ()) {
				stdout.printf (iter.get().to_string ());
			}
			stdout.printf ("\n");
		}
				
		public static int main (string[] args) {
			stdout.printf ("Demo for Gda.DataModelIterator...\n");
			var app = new App ();
			try {
				app.init ();
			}
			catch (Error e) { 
				stdout.printf ("ERROR: Db was initialiced incorrectly\n" + e.message + "\n");
				return 1;
			}
			app.iterating ();
			app.choping ();
			app.filtering ();
			return 0;
		}
	}
}
