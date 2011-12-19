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

	class Book : GdaData.Object {
		
		public override string table { 
			get { return "book"; }
			construct {}
		}
		
		public Collection accounts {
			get {
				int id = (int) this.get_value_id ();
				string sql = @"SELECT * FROM accounts WHERE book = $id";
				var accs = this.connection.execute_select (sql);
				return (Collection) new DataModelIterable (accs);
			}
		}
				
		public bool open (string name) 
			throws Error
		{
			string sql = @"SELECT * FROM book WHERE name = $name";
			var model = this.connection.execute_select (sql);
			var id = model.get_value_at (model.get_column_index ("id"), 0);
			this.set_id ("id", id);
		}
		
	}
	
	class Account : GdaData.Object {
		public override string table { 
			get { return "account"; }
			construct {}
		}
		public Book book { get; set; }
		/* Is possible to create properties to easy access to any field in the database row */
		public string name { 
			get {
				return (string) this.get_value ("name");
			} 
			set {
				return this.set_value ("name", value);
			}
		}
		
		public Collection transactions {
			get {
				int id = (int) this.get_value_id ();
				string sql = @"SELECT * FROM transactions WHERE account = $id";
				var accs = this.connection.execute_select (sql);
				return (Collection) new DataModelIterable (accs);
			}
		}
		
		public double balance {
			get {
				string sql_debit = @"SELECT sum (debit) FROM transactions WHERE account = $id";
				string sql_credit = @"SELECT sum (credit) FROM transactions WHERE account = $id";
				var model_credit = this.connection.execute_select (sql_credit);
				var model_debit = this.connection.execute_select (sql_debit);
				var credit = model_credit.get_value_at (0, 0);
				var debit = model_debit.get_value_at (0, 0);
				return (double) credit - (double) debit;
			}
		}
		
		public bool open (string name) {
			Value n = name;
			this.set_id (name, n);
		}
		
	}
	
	class Transaction : GdaData.Object {
		public override string table { 
			get { return "transaction"; }
			construct {}
		}
		public Account account { 
			get {
				var acc = new Account ();
				Value idacc = this.get_value ("account");
				acc.set_id ("id", idacc);
				return acc;
			}
			set {
				Account acc = value;
				var id = acc.get_value ("id");
				this.set_value ("account", id);
			}
		}
		public string description { 
			get {
				this.get_value ("description");
			} 
			set {
				this.set_value ("description", value);
			}
		}
		
		public double debit { 
			get {
				var credit = (bool) this.get_value ("credit");
				if (!credit)
					return (double) this.get_value ("amount");
				else
					return 0;
			} 
			set {
				this.set_value ("amount", value);
				bool credit = false;
				Value v = credit;
				this.set_value ("debit", v);
			}
		}
		
		public double credit { 
			get {
				var credit = (bool) this.get_value ("credit");
				if (credit)
					return (double) this.get_value ("amount");
				else
					return -1;
			} 
			set {
				this.set_value ("amount", value);
				bool credit = true;
				Value v = credit;
				this.set_value ("debit", v);
			}
		}
	}
	
	class App : GLib.Object {
		public Gda.Connection connection;
		
		App ()
		{
			try {
				GLib.FileUtils.unlink("dataobject.db");
				stdout.printf("Creating Database...\n");
				this.connection = Connection.open_from_string("SQLite", "DB_DIR=.;DB_NAME=dataobject", null, Gda.ConnectionOptions.NONE);
				stdout.printf("Creating table 'user'...\n");
				this.connection.execute_non_select_command("CREATE TABLE book (id int PRIMARY KEY AUTOINCREMENT, name string NOT NULL UNIQUE, manager string)");
				
				this.connection.execute_non_select_command("INSERT INTO book (id, name, manager) VALUES (1, \"General Book\", \"Jhon\")");
				
				stdout.printf("Creating table 'company'...\n");
				this.connection.execute_non_select_command("CREATE TABLE account (id int PRIMARY KEY AUTOINCREMENT, name string UNIQUE, description string)");
				this.connection.execute_non_select_command("INSERT INTO account (id, name) VALUES (1, \"Incomes\"");
				this.connection.execute_non_select_command("INSERT INTO account (id, name) VALUES (2, \"Expenses\"");
				this.connection.execute_non_select_command("INSERT INTO account (id, name) VALUES (3, \"Bank\"");
				this.connection.execute_non_select_command("CREATE TABLE transaction (id int PRIMARY KEY AUTOINCREMENT, credit int, debit int, description string, amount double)");
				this.connection.execute_non_select_command("INSERT INTO account (id, credit, debit, description, amount, credit) VALUES (1, 3, 1, \"Salary\", 3100.0, TRUE");
				this.connection.execute_non_select_command("INSERT INTO account (id, credit, debit, description, amount, credit) VALUES (2, 2, 3, \"Expenses of the week\"");
				this.connection.update_meta_store(null);
			}
			catch (Error e) {
				stdout.printf ("Couln't create temporary database...\nERROR: %s\n", e.message);
			}
		}
		
				
		public static int main (string[] args) {
			stdout.printf ("Checking Gda.DataObject implementation...\n");
			int failures = 0;
			var app = new App ();
			app.get_balance ("Incomes");
			app.get_balance ("Expenses");
			app.get_balance ("Bank");
			app.get_operations ("Bank");
			return failures != 0 ? 1 : 0;
		}
	}
}
