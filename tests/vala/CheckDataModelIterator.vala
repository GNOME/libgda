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
	
	class Tests : GLib.Object {
		private Gda.Connection connection;
		private DbRecordCollection itermodel;
		
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
				this.connection.execute_non_select_command("INSERT INTO user (id, name, city) VALUES (3, \"James\", \"Germany\")");
				this.connection.execute_non_select_command("INSERT INTO user (id, name, city) VALUES (4, \"Jack\", \"United Kindom\")");
				this.connection.execute_non_select_command("INSERT INTO user (id, name, city) VALUES (5, \"Elsy\", \"España\")");
				this.connection.execute_non_select_command("INSERT INTO user (id, name, city) VALUES (6, \"Mayo\", \"Mexico\")");
				
				this.connection.update_meta_store(null);
			}
			catch (Error e) {
				stdout.printf ("Couln't create temporary database...\nERROR: %s\n", e.message);
			}
		}
		
		public int iterating ()
			throws Error
		{
			int fails = 0;
			stdout.printf (">>> TESTING: Iterating over all Records in DataModel using foreach...\n");
			int i = 0;
			stdout.printf ("Iterating using a Gee.Iterator...\n");
			i = 0;
			var iter = itermodel.iterator ();
			if (iter == null)
				stdout.printf ("----- FAIL "+ (++fails).to_string () + "\n");
			while (iter.next ()) {
				string t2 = iter.get ().to_string ();
				if (t2 == null) {
					fails++;
					break;
				}
				else {
					stdout.printf (t2 + "\n");
					i++;
				}
			}
			if (fails > 0 || i != 6)
				stdout.printf ("----- FAIL "+ fails.to_string () + "\n");
			else
				stdout.printf ("+++++ PASS\n");
			stdout.printf ("Iterating using foreach instruction...\n");
			i = 0;
			foreach (DbRecord r in itermodel) {
				string t = r.to_string ();
				if (t == null) {
					fails++;
					break;
				}
				i++;
				stdout.printf (t + "\n");
			}
			if (fails > 0 || i != 6)
				stdout.printf ("----- FAIL "+ fails.to_string () + "\n");
			else
				stdout.printf ("+++++ PASS\n");
			return fails;
		}
		
		public int choping ()
		{
			int fails = 0;
			stdout.printf (">>> TESTING: Chopping...\n");
			stdout.printf (" to get from the 2nd DbRecord to the 6th...\n");
			var iter = itermodel.chop (1);
			int i = 0;
			while(iter.next()) {
				stdout.printf (iter.get().to_string () + "\n");
				i++;
				var name = (string) iter.get().get_value ("name");
				switch (i) {
				case 1:
					if (name != "Jhon")
						fails++;
					break;
				case 2:
					if (name != "James")
						fails++;
					break;
				case 3:
					if (name != "Jack")
						fails++;
					break;
				case 4:
					if (name != "Elsy")
						fails++;
					break;
				case 5:
					if (name != "Mayo")
						fails++;
					break;
				}
			}
			if (fails != 0 || i != 5)
					stdout.printf ("----- FAIL: " + (++fails).to_string () + "\n");
				else
					stdout.printf ("+++++ PASS\n");

			stdout.printf ("Choping to get the 4th to 5th DbRecords...\n");
			var iter2 = itermodel.chop (3,2);
			i = 0;
			while (iter2.next ()) {
				i++;
				stdout.printf (iter2.get ().to_string () + "\n");
				var name2 = (string) iter2.get().get_value ("name");
				if (i==1) {
					if (name2 != "Jack") {
						fails++;
						stdout.printf ("----- FAIL: " + (++fails).to_string () + "\n");
						break;
					}
				}
				if (i==2) {
					if (name2 != "Elsy") {
						fails++;
						stdout.printf ("----- FAIL: " + (++fails).to_string () + "\n");
						break;
					}
				}
			}
			if (i!=2) {
				fails++;
				stdout.printf ("----- FAIL: " + (++fails).to_string () + "\n");
			}
			else
			stdout.printf ("+++++ PASS\n");
			
			stdout.printf ("Choping offset = 7 must fail...\n");
			var iter3 = itermodel.chop (7);
			if (iter3.has_next ())
				stdout.printf ("----- FAIL: " + (++fails).to_string () + "\n");
			else {
				if (iter3.next ())
					stdout.printf ("----- FAIL: " + (++fails).to_string () + "\n");
				if (fails == 0)
					stdout.printf ("+++++ PASS\n");
			}
			
			stdout.printf ("Choping offset = 6 length = 0 must fail...\n");
			var iter4 = itermodel.chop (6,0);
			if (iter4.has_next ())
				stdout.printf ("----- FAIL: " + (++fails).to_string () + "\n");
			else {
				if (iter4.next ())
					stdout.printf ("----- FAIL: " + (++fails).to_string () + "\n");
				if (fails == 0)
					stdout.printf ("+++++ PASS\n");
			}
			
			stdout.printf ("Choping offset = 5 length = 1...\n");
			var iter5 = itermodel.chop (5,1);
			if (!iter5.next ())
				stdout.printf ("----- FAIL: " + (++fails).to_string () + "\n");
			else {
				if (iter5.next ())
					stdout.printf ("----- FAIL: " + (++fails).to_string () + "\n");
				if (fails == 0)
					stdout.printf ("+++++ PASS\n");
			}
			if (fails == 0)
				stdout.printf ("+++++ PASS: " + "\n");
			return fails;
		}
		
		public int filtering ()
		{
			int fails = 0;			
			stdout.printf (">>> TESTING: Filtering Records: Any field type STRING with a letter 'J'...\n");
			
			var iter = itermodel.filter (
			(g) => {
				bool ret = false;
				foreach (DbField fl in g.fields) {
					string t = Gda.value_stringify (fl.value);
					stdout.printf ("Value to check: " + t);
					if (t.contains ("J")) { 
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
			stdout.printf ("\nPrinting Filtered Values...\n");
			int i = 0;
			while (iter.next ()) {
				stdout.printf ("Row"+(++i).to_string()+":\n" + iter.get ().to_string () + "\n");
			}
			if(i != 3) {
				stdout.printf ("----- FAIL" + (++fails).to_string () + "\n");
			}
			else
				stdout.printf ("+++++ PASS\n");
			return fails;
		}
		public int streaming ()
		{
			int fails = 0;
			stdout.printf (">>> TESTING: Streaming Values: First DbRecord's field type STRING will be YIELDED...\n");
			var iter = itermodel.stream<string> (
				(state, g, out lazy) =>	{
					switch(state) {
					case Gee.Traversable.Stream.YIELD:
						lazy = null;
						return Gee.Traversable.Stream.CONTINUE;
						break;
					case Gee.Traversable.Stream.CONTINUE:
						var r = g.value;
						foreach (DbField f in r.fields) {
							if (f.value.type () == typeof (string)) {
								stdout.printf ("Field (%s) =  %s\n", f.name, Gda.value_stringify (f.value));
								string ts = Gda.value_stringify (f.value);
								lazy = new Gee.Lazy<string> (() => {return ts.dup ();});
							}
						}
						return Gee.Traversable.Stream.YIELD;
						break;
					}
					return Gee.Traversable.Stream.END;
				}
			);
			stdout.printf ("Printing Streamed Values...\n");
			var l = new Gee.ArrayList<string> ();
			while (iter.next ()) {
				string v = iter.get ();
				l.add (v);
				stdout.printf (v + "\n");
			}
			
			if (!l.contains ("Daniel"))
				++fails;
			if (!l.contains ("Jhon"))
				++fails;
			if (!l.contains ("Jack"))
				++fails;
			if (!l.contains ("Elsy"))
				++fails;
			if (!l.contains ("Mayo"))
				++fails;
			
			if (fails > 0) {
				stdout.printf ("----- FAIL" + (++fails).to_string () + "\n");
			}
			else
				stdout.printf ("+++++ PASS\n");
			return fails;
		}
		public int InitIter()
			throws Error
		{
			stdout.printf (">>> INITIALIZING: DbRecordCollection/RecordCollection\n");
			int fails = 0;
			var model = this.connection.execute_select_command ("SELECT * FROM user ORDER BY id");
			stdout.printf ("Setting up Table...");
			var t = new Table ();
			t.connection = this.connection;
			t.name = "user";
			stdout.printf ("+++++ PASS\n");
			stdout.printf ("Setting up DbRecordCollection...");
			itermodel = new RecordCollection (model, t);
			stdout.printf ("+++++ PASS\n");
			stdout.printf ("DataModel rows:\n" + model.dump_as_string ());
			return fails;
		}
		
		public static int main (string[] args) {
			stdout.printf ("Checking Gda.DataModelIterator implementation...\n");
			int failures = 0;
			var app = new Tests ();
			failures += app.InitIter ();
			failures += app.iterating ();
			//failures += app.streaming ();
			// failures += app.filtering ();
			// failures += app.choping ();
			return failures != 0 ? 1 : 0;
		}
	}
}
