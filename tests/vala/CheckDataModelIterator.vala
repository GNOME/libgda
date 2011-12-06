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
	
	class Tests : GLib.Object {
		private Gda.Connection connection;
		
		Tests()
		{
			try {
				GLib.FileUtils.unlink("datamodeliterator.db");
				stdout.printf("Creating Database...\n");
				this.connection = Connection.open_from_string("SQLite", "DB_DIR=.;DB_NAME=datamodeliterator", null, Gda.ConnectionOptions.NONE);
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
			stdout.printf (">>> NEW TEST: Gda.DataModelIterator API tests\n");
			int fails = 0;
			var model = this.connection.execute_select_command ("SELECT * FROM user");
			var itermodel = new DataModelIterable (model);

			stdout.printf ("Iterating over all Values in DataModel\n");
			foreach (Value v in itermodel) {
				stdout.printf ("%s\t%s\n", v.type_name (), Gda.value_stringify (v));
			}
			
			stdout.printf ("Choping to get first Value = 1\n");
			var iter = itermodel.chop (0,0);
			iter.next ();
			Value v = iter.get ();
			if (v.type () == typeof (int))
				stdout.printf ("ID=%i; Row = %i Col = %i\n", (int) v, 
							  ((GdaData.DataModelIterator) iter).current_row, 
							  ((GdaData.DataModelIterator) iter).current_column);
			else {
				fails++;
				stdout.printf ("FAIL:'%i': Expected type = '%s'," +
							   " but Value is type = '%s'" +
							   " with value = %s\n" + 
							   " : Current Row = %i Col = %i\n", 
							   fails, typeof (int).name (), v.type_name (), 
							   Gda.value_stringify (v),
							   ((GdaData.DataModelIterator) iter).current_row, 
							   ((GdaData.DataModelIterator) iter).current_column);			
			}
			
			stdout.printf ("Choping to get 5th Value = 'Jhon'\n");
			var iter2 = itermodel.chop (4,0);
			stdout.printf ("Row = %i Col = %i\n", 
							   ((GdaData.DataModelIterator) iter2).current_row, 
							   ((GdaData.DataModelIterator) iter2).current_column);
			iter2.next ();
			Value v2 = iter2.get ();
			if (v2.type () == typeof (string))
				stdout.printf ("Name = %s; Row = %i Col = %i\n", (string) v2,
							   ((GdaData.DataModelIterator) iter2).current_row, 
							   ((GdaData.DataModelIterator) iter2).current_column);
			else {
				fails++;
				stdout.printf ("FAIL:'%i': Expected type = '%s'," +
							   " but Value is type = '%s'" +
							   " with value = %s" + 
							   " : Current Row = %i Col = %i\n", 
							   fails, typeof (string).name (), v2.type_name (), 
							   Gda.value_stringify (v2),
							   ((GdaData.DataModelIterator) iter2).current_row, 
							   ((GdaData.DataModelIterator) iter2).current_column);			
			}
			
			stdout.printf ("Choping to get 2nd to 5th Values...\n");
			var iter3 = itermodel.chop (1,4);
			stdout.printf ("TYPE\t\tVALUE\n");
			while (iter3.next ()) {
				Value v3 = iter3.get ();
				stdout.printf ("%s\t\t%s\n", typeof (int).name (), Gda.value_stringify (v3));
				if (((GdaData.DataModelIterator) iter3).current_row == 0 
					&& ((GdaData.DataModelIterator) iter3).current_column == 1) {
						string name1 = (string) v3;
						if (name1 != "Daniel") {
							fails++;
							break;
						}
				}
				if (((GdaData.DataModelIterator) iter3).current_row == 0 
					&& ((GdaData.DataModelIterator) iter3).current_column == 2) {
						string name1 = (string) v3;
						if (name1 != "Mexico") {
							fails++;
							break;
						}
				}
				if (((GdaData.DataModelIterator) iter3).current_row == 1 
					&& ((GdaData.DataModelIterator) iter3).current_column == 0) {
						int id1 = (int) v3;
						if (id1 != 2) {
							fails++;
							break;
						}
				}
				if (((GdaData.DataModelIterator) iter3).current_row == 1 
					&& ((GdaData.DataModelIterator) iter3).current_column == 1) {
						string name1 = (string) v3;
						if (name1 != "Jhon") {
							fails++;
							break;
						}
				}
			}
						
			return fails;
		}
		
		public static int main (string[] args) {
			stdout.printf ("Checking Gda.DataModelIterator implementation...\n");
			int failures = 0;
			var app = new Tests ();
			failures += app.t1 ();
			return failures != 0 ? 1 : 0;
		}
	}
}
