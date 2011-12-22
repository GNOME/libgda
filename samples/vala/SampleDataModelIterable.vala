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
		private DataModelIterable itermodel;
		
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
			
			stdout.printf("Creating table 'company'...\n");
			this.connection.execute_non_select_command("CREATE TABLE company (id int PRIMARY KEY, name string, responsability string)");
			this.connection.execute_non_select_command("INSERT INTO company (id, name, responsability) VALUES (1, \"Telcsa\", \"Programing\")");
			this.connection.execute_non_select_command("INSERT INTO company (id, name, responsability) VALUES (2, \"Viasa\", \"Accessories\")");
			this.connection.update_meta_store(null);
			var model = this.connection.execute_select_command ("SELECT * FROM user");
			this.itermodel = new DataModelIterable (model);

		}
		
		public void iterating ()
			throws Error
		{
			stdout.printf ("Iterating over all Values in DataModel\n");
			stdout.printf ("VALUE\tTYPE\n");
			foreach (Value v in itermodel) {
				stdout.printf ("%s\t%s\n", v.type_name (), Gda.value_stringify (v));
			}
		}
		
		public void choping ()
		{
			stdout.printf ("Choping to get the 5th Value\n");
			var iter = itermodel.chop (4,0);
			iter.next ();
			Value v = iter.get ();
			stdout.printf ("Value=%s; Row = %i Col = %i\n", Gda.value_stringify (v), 
							  ((GdaData.DataModelIterator) iter).current_row, 
							  ((GdaData.DataModelIterator) iter).current_column);
			
			stdout.printf ("Choping to get 2nd to 5th Values...\n");
			var iter2 = itermodel.chop (1,4);
			stdout.printf ("TYPE\t\tVALUE\n");
			while (iter2.next ()) {
				Value v2 = iter2.get ();
				stdout.printf ("%s\t\t%s\n", v2.type_name (), Gda.value_stringify (v2));
			}
		}
		
		public void filtering ()
		{
			stdout.printf ("Filtering Values: Any STRING with a letter 'n'...\n");
			Gee.Predicate <Value?> f = (g) => {
						bool ret = false;
						if (g.type () == typeof (string)) {
							string t = (string) g;
							if (t.contains ("n"))
								ret = true;
							else
								ret = false;
						}
						else
							ret = false;
						stdout.printf ("To be Included?: %s, %s\n", 
										Gda.value_stringify (g), ret == true ? "TRUE" : "FALSE");
						return ret;
					};
			
			var iter3 = itermodel.filter (f);
			stdout.printf ("Printing Filtered Values...\n");
			stdout.printf ("TYPE\tVALUE\n");
			while (iter3.next ()) {
				Value v3 = iter3.get ();
				stdout.printf (v3.type_name (), Gda.value_stringify (v3) );
			}
			
		}
		public void streaming ()
		{
			/* FIXME: This code doesn't return YIELDED strings maybe becasue Lazy is not correctly built. 
				In theory stream() function is working correctly*/
			stdout.printf ("Streaming Values: Any STRING will be YIELDED...\n");
			Gee.StreamFunc<Value?,string> s = (state, g, lazy) =>	{
						if (state == Gee.Traversable.Stream.CONTINUE) {
							Value vs = g.get ();
							stdout.printf ("Value to YIELD: %s\n", Gda.value_stringify (vs));
							string ts = "YIELDED Value = " + Gda.value_stringify (vs) + "\n";
							lazy = new Gee.Lazy<string>.from_value (ts.dup ());
							stdout.printf (ts);
							return Gee.Traversable.Stream.YIELD;
						}
						return Gee.Traversable.Stream.END;
					};
			
			var iter5 = itermodel.stream<string> (s);
			stdout.printf ("Printing Streamed Values...\n");
			while (iter5.next ()) {
				string sv = iter5.get ();
				stdout.printf ("%s\n", sv);
			}
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
			app.streaming ();
			return 0;
		}
	}
}
