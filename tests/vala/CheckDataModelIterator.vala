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
			stdout.printf (">>> NEW TEST: GdaData.RecordCollection & RecordCollectionIterator API tests\n"
						 	+ ">>> Testing Iterable, Traversable Interfaces\n");
			int fails = 0;
			var model = this.connection.execute_select_command ("SELECT * FROM user");
			var t = new Table<Value?> ();
			t.connection = this.connection;
			t.name = "user";
			var itermodel = new RecordCollection (model, t);

			stdout.printf ("Iterating over all Records in DataModel\n");
			foreach (DbRecord r in itermodel) {
				stdout.printf (r.to_string ());
			}
			
			stdout.printf ("Choping to get first record id= 1\n");
			var iter = itermodel.chop (0,0);
			iter.next ();
			var v = iter.get ().get_field ("id").value;
			if (v.type () == typeof (int))
				stdout.printf ("ID=%i\n", (int) v);
			else {
				fails++;
				stdout.printf ("FAIL:'%i': Expected type = '%s'," +
							   " but Value is type = '%s'" +
							   " with value = %s\n",
							   fails, typeof (int).name (), v.type_name (), 
							   Gda.value_stringify (v));			
			}
			
			stdout.printf ("Choping to get 5th Value = 'Jhon'\n");
			var iter2 = itermodel.chop (1,0);
			iter2.next ();
			var v2 = iter2.get ().get_value ("name");
			if (v2.type () == typeof (string))
				stdout.printf ("Name = %s;\n", (string) v2);
			else {
				fails++;
				stdout.printf ("FAIL:'%i': Expected type = '%s'," +
							   " but Value is type = '%s'" +
							   " with value = %s\n", 
							   fails, typeof (string).name (), v2.type_name (), 
							   Gda.value_stringify (v2));			
			}
			// FIXME: test filter and stream
			return fails;
		}
		
		public int t2 ()
		{
			stdout.printf (">>> NEW TEST: GdaData.RecordCollection & RecordCollectionIterator API tests\n"
							+ ">>> Testing Collection Interface\n");
			int fails = 0;
			var model = this.connection.execute_select_command ("SELECT * FROM user");
			((DataSelect) model).compute_modification_statements ();
			var pxy = (Gda.DataModel) Gda.DataProxy.new (model);
			var t = new Table<Value?>();
			t.connection = this.connection;
			t.name = "user";
			var itermodel = new RecordCollection (pxy,t);
			var row = new Record ();
			row.table = t;
			var f = new Field<Value?>("id", DbField.Attribute.NONE);
			f.value = 10;
			row.set_field (f);
			f.name = "name";
			f.value = "Samanta";
			row.set_field (f);
			f.name = "city";
			f.value = "San Francisco";
			row.set_field (f);
			if ( itermodel.add (row)) {
				stdout.printf("New contents in DataModel:\n" + itermodel.to_string ());
			}
			else
				fails++;
			
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
