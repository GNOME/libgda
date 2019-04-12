/*
 * gda-connection-provider.vala
 * Copyright (C) Daniel Espinosa 2018 <esodan@gmail.com>
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

/**
 * An interface to be implemented by database providers
 */
public interface Gda.ConnectionModel : GLib.Object {
  public abstract ConnectionModelParams cnc_params { get; set; }
  public abstract bool is_opened { get; }

  public abstract signal void closed ();
  public abstract signal void opened ();
  public abstract signal void opening ();
  public abstract signal void closing ();

  public abstract void close ();
  public abstract void close_no_warning ();
  public abstract bool open () throws GLib.Error;

  public abstract Gda.Query? create_query (string sql) throws GLib.Error;
  public abstract Gda.QueryBuilder? create_query_builder (string sql) throws GLib.Error;
  public abstract Gda.MetaCatalog? create_meta_catalog ();
  public abstract Gda.CreateDatabaseBuilder? builder_create_data_base (string name);
  public abstract Gda.DropDatabaseBuilder? builder_drop_data_base (string name);
  public abstract Gda.CreateTableBuilder? builder_create_table (string name, string? schema = null);
  public abstract Gda.DropTableBuilder? builder_drop_table (string name, string? schema = null);
}
/**
 * A class to hold and parse connection string
 */
public class Gda.ConnectionModelParams : GLib.Object {
  public string user { get; set; }
  public string pasword { get; set; }
  public string cnc_string { get; set; }
}
/**
 * An interface to be represent any query to be executed by
 * providers
 */
public interface Gda.Query : GLib.Object {
  public abstract string name { get; }
  public abstract string sql { get; }
  public abstract Gda.ConnectionModel connection { get; }
  public abstract async Gda.Result execute () throws GLib.Error;
  public abstract async void cancel ();
}

/**
 * Represent any result after execute a query
 */
public interface Gda.Result : GLib.Object {}

/**
 * Represent a table in a database.
 */
public interface Gda.TableModel : GLib.Object, Gda.MetaTable {}
/**
 * a Table allowing to change its value
 */
public interface Gda.ReadonlyTableModel : GLib.Object, Gda.TableModel
{
  /**
   * Get the value at give row and column number
   */
  public abstract GLib.Value get_value (int row, string column);
  /**
   * Get the value at give row and column number
   */
  public abstract GLib.Value get_value_at (int row, int column);
  /**
   * Model to iterate or access to {link Gda.RowModel} objects
   */
  public abstract GLib.ListModel rows { get; }
}
/**
 * A table as a result of a {@link Query} execution
 */
public interface Gda.ResultTable : GLib.Object,
                                  Gda.MetaTable,
                                  Gda.TableModel,
                                  Gda.ReadonlyTableModel,
                                  Gda.Result
{}
/**
 * a Table allowing to change its value
 */
public interface Gda.WritableTableModel : GLib.Object, Gda.TableModel, Gda.ReadonlyTableModel {
  /**
   * Get the value at give row and column number
   */
  public abstract void set_value (int row, string column, GLib.Value value);
  /**
   * Get the value at give row and column number
   */
  public abstract void set_value_at (int row, int column, GLib.Value value);
  /**
   * Creates a new {@link Gda.RowModel} to be used by {@link insert_row}
   */
  public abstract RowModel create_row ();
  /**
   * Insert a {@link Gda.RowModel} into the table model.
   */
  public abstract void insert_row (RowModel new_row);
}
/**
 * Is a result from a query inserting rows, with the
 * number of rows and last row inserted
 */
public interface Gda.Inserted : GLib.Object, Gda.Result {
  public abstract int number { get; }
  public abstract Gda.RowModel last_insertd { get; }
}

/**
 * Is a result from a query with a number of afected rows
 */
public interface Gda.AfectedRows : GLib.Object, Gda.Result {
  public abstract int number { get; }
}
/**
 * Represent a row in a table model.
 */
public interface Gda.RowModel : GLib.Object, GLib.ListModel {
  public abstract int n_columns { get; }
  public abstract Gda.ColumnModel get_column (string name);
}

/**
 * Represent a column in a row of a table model
 */
public interface Gda.ColumnModel : GLib.Object {
  public abstract string name { get; }
  public abstract int index { get; }
  public abstract GLib.Type data_type { get; }
  public abstract GLib.Value? value { get; set; }
  public abstract ColumnAttributes attributes { get; set; }
  public abstract ForeignKey? foreign_key { get; set; }
}
/**
 * Attributes for Columns
 */
[Flags]
public enum Gda.ColumnAttributes {
  NONE,
  PRIMARY_KEY,
  UNIQUE,
  FOREIGN_KEY,
  CHECK,
  HAVE_DEFAULT,
  CAN_BE_NULL,
  AUTO_INCREMENT;

  public static ColumnAttributes[] items () {
	  return {
			  NONE,
			  PRIMARY_KEY,
			  UNIQUE,
			  FOREIGN_KEY,
			  CHECK,
			  HAVE_DEFAULT,
			  CAN_BE_NULL,
			  AUTO_INCREMENT
			  };
  }

  public string to_string () {
	  string str = "";
	  if (NONE in this)
		  str += "NONE ";
	  if (PRIMARY_KEY in this)
		  str += "PRIMARY_KEY ";
	  if (UNIQUE in this)
		  str += "UNIQUE ";
	  if (FOREIGN_KEY in this)
		  str += "FOREIGN_KEY ";
	  if (CHECK in this)
		  str += "CHECK ";
	  if (HAVE_DEFAULT in this)
		  str += "HAVE_DEFAULT ";
	  if (CAN_BE_NULL in this)
		  str += "CAN_BE_NULL ";
	  if (AUTO_INCREMENT in this)
		  str += "AUTO_INCREMENT ";
	  if (str == "")
		  return "NONE";
	  return str;
  }

  public static ColumnAttributes from_string (string str)
  {
	  if (str == "NONE")
		  return NONE;
	  if (str == "PRIMARY_KEY")
		  return PRIMARY_KEY;
	  if (str == "PRIMARY KEY")
		  return PRIMARY_KEY;
	  if (str == "UNIQUE")
		  return UNIQUE;
	  if (str == "FOREIGN_KEY")
		  return FOREIGN_KEY;
	  if (str == "FOREIGN KEY")
		  return FOREIGN_KEY;
	  if (str == "CHECK")
		  return CHECK;
	  if (str == "HAVE_DEFAULT")
		  return HAVE_DEFAULT;
	  if (str == "CAN_BE_NULL")
		  return CAN_BE_NULL;
	  if (str == "AUTO_INCREMENT")
		  return AUTO_INCREMENT;
	  return NONE;
  }
}
/**
 * Foreign Key description for a Column
 */
public interface Gda.ForeignKey : GLib.Object {
  public abstract string                 name        { get; set; }
  public abstract string                 refname     { get; set; }
  public abstract TableModel             reftable    { get; set; }
  public abstract GLib.ListModel         refcol      { get; set; }
  public abstract Match                  match       { get; set; }
  public abstract Rule                   update_rule { get; set; }
  public abstract Rule                   delete_rule { get; set; }

  public bool equal (ForeignKey fkey)
  {
	  if (fkey.name != name)
		  return false;
	  if (fkey.refname != refname)
		  return false;
	  if (fkey.reftable != reftable)
		  return false;
	  if (fkey.match != match)
		  return false;
	  if (fkey.update_rule != update_rule)
		  return false;
	  if (fkey.delete_rule != delete_rule)
		  return false;
	  for (uint i = 0; i < refcol.get_n_items (); i++) {
	    var rc = refcol.get_item (i) as ReferencedColumn;
	    if (rc == null) {
	      continue;
	    }
	    bool found = false;
	    for (uint j = 0; j < fkey.refcol.get_n_items (); j++) {
	      var rrc = fkey.refcol.get_item (i) as ReferencedColumn;
	      if (rrc == null) {
	        continue;
	      }
	      if (rrc.name == rc.name) {
	        found = true;
	        break;
	      }
      }
      if (!found) {
        return false;
      }
	  }
	  return true;
  }

  public string to_string ()
  {
	  string s = "";
	  for (uint i = 0; i < refcol.get_n_items (); i++) {
	    var rc = refcol.get_item (i) as ReferencedColumn;
	    if (rc == null) {
	      continue;
	    }
		  s += rc.name + ",";
	  }
	  string r = "{[";
	  if (name != null)
		  r += name;
	  r +="],[";
	  if (refname != null)
		  r += refname;
	  r +="],[";
	  r += match.to_string ();
	  r +="],[";
	  r += update_rule.to_string ();
	  r +="],[";
	  r += delete_rule.to_string ();
	  r +="]}";
	  return r;
  }

  public enum Match {
	  NONE,
	  FULL,
	  PARTIAL;

	  public static Match[] items () {
		  return {
				  NONE,
				  FULL,
				  PARTIAL
				  };
	  }

	  public string to_string ()
	  {
		  switch (this) {
			  case FULL:
				  return "FULL";
			  case PARTIAL:
				  return "PARTIAL";
		  }
		  return "NONE";
	  }

	  public static Match from_string (string str)
	  {
		  if (str == "FULL")
			  return FULL;
		  if (str == "PARTIAL")
			  return PARTIAL;

		  return Match.NONE;
	  }
  }
  public enum Rule {
	  NONE,
	  CASCADE,
	  SET_NULL,
	  SET_DEFAULT,
	  RESTRICT,
	  NO_ACTION;

	  public static Rule[] items () {
		  return {
				  NONE,
				  CASCADE,
				  SET_NULL,
				  SET_DEFAULT,
				  RESTRICT,
				  NO_ACTION
				  };
	  }

	  public string to_string ()
	  {
		  switch (this) {
			  case CASCADE:
				  return "CASCADE";
			  case SET_NULL:
				  return "SET_NULL";
			  case SET_DEFAULT:
				  return "SET_DEFAULT";
			  case RESTRICT:
				  return "RESTRICT";
			  case NO_ACTION:
				  return "NO_ACTION";
		  }
		  return "NONE";
	  }

	  public static Rule from_string (string? str)
	  {
		  if (str == "CASCADE")
			  return Rule.CASCADE;
		  if (str == "SET_NULL")
			  return Rule.SET_NULL;
		  if (str == "SET NULL")
			  return Rule.SET_NULL;
		  if (str == "SET_DEFAULT")
			  return Rule.SET_DEFAULT;
		  if (str == "SET DEFAULT")
			  return Rule.SET_DEFAULT;
		  if (str == "RESTRICT")
			  return Rule.RESTRICT;
		  if (str == "NO_ACTION")
			  return Rule.NO_ACTION;
		  if (str == "NO ACTION")
			  return Rule.NO_ACTION;

		  return Rule.NONE;
	  }
  }
}

/**
 * A reference to a column in a referenced table
 */
public interface Gda.ReferencedColumn : GLib.Object
{
  public abstract string table_name { set; }
  public abstract string name { get; }
}

/**
 * Represented a prepared query. Values required by
 * query can be set by using paramenters property.
 */
public interface Gda.PreparedQuery : GLib.Object, Gda.Query {
  public abstract string name { get; }
  public abstract Gda.Parameters parameters { get; }
}
/**
 * Set parameters for a prepared query.
 */
public interface Gda.Parameters : GLib.Object, GLib.ListModel {
  public abstract void set_value (string name, GLib.Value val);
  public abstract GLib.Value? get_value (string name);
}
/**
 * A catalog of meta data for a connection.
 */
public interface Gda.MetaCatalog : GLib.Object {
  public abstract ConnectionModel connection { get; construct set; }
  public abstract GLib.ListModel get_tables ();
  public abstract GLib.ListModel get_views ();
}
/**
 * Meta data describing a table.
 */
public interface Gda.MetaTable : GLib.Object {
  public abstract string name { get; set; }
  public abstract string schema { get; set; }
  public abstract string catalog { get; set; }
  public abstract GLib.ListModel columns { get; }
  public abstract Gda.MetaColumn get_colum (string name);
}
/**
 * Meta data describing a column.
 */
public interface Gda.MetaColumn : GLib.Object {
  /**
   * Column's name
   */
  public abstract string name { get; set; }
  /**
   * A {@link GLib.Type} with the traslated colum's type
   */
  public abstract GLib.Type column_type { get; }
  /**
   * Specific data type name from the connection server
   */
  public abstract string column_type_name { get; }
}
/**
 *
 */
public interface Gda.QueryBuilder : GLib.Object {
  /**
   * Builder's SQL representation
   */
  public abstract string sql { get; set; }
  /**
   * Builder's name
   */
  public abstract string name { get; set; }
  /**
   * Connection specific parameters required
   */
  public abstract Gda.Parameters parameters { get; }

  public abstract bool add_savepoint (string? name) throws GLib.Error;
  public abstract bool delete_savepoint (string? name) throws GLib.Error;
  public abstract bool rollback_savepoint (string? name) throws GLib.Error;
  public abstract bool begin_transaction (string? name) throws GLib.Error;
  public abstract bool commit_transaction (string? name) throws GLib.Error;
  public abstract bool rollback_transaction (string? name) throws GLib.Error;

  public abstract Gda.Query parse_string (string sql) throws GLib.Error;
  public abstract Gda.PreparedQuery? prepare_string (string name, string sql) throws GLib.Error;
  public abstract Gda.PreparedQuery? get_prepared_query (string name);
}
/**
 * Create Data Base Builder
 */
public interface Gda.CreateDatabaseBuilder : GLib.Object, QueryBuilder {
  public abstract string database_name { get; set; }
}
/**
 * Drop Base Builder
 */
public interface Gda.DropDatabaseBuilder : GLib.Object, QueryBuilder {
  public abstract string database_name { get; set; }
}
/**
 * Create Table Builder
 */
public interface Gda.CreateTableBuilder : GLib.Object, QueryBuilder {
  /**
   * Table's name to create
   */
  public abstract string table_name { get; set; }
  /**
   * List of columns to add
   */
  public abstract GLib.ListModel columns { get; set; }
  /**
   * List of constraints to add to the table
   */
  public abstract GLib.ListModel contraints { get; set; }
}
/**
 * Represent a constraint for a table
 */
public interface Gda.TableConstraint {
  /**
   * Table this constraint apply to
   */
  public abstract TableModel table { get; }
  /**
   * String representation of the constraint.
   */
  public abstract string definition { get; set; }
}
/**
 * Create Table Builder
 */
public interface Gda.DropTableBuilder : GLib.Object, QueryBuilder {
  /**
   * Table's name to drop
   */
  public abstract string table_name { get; set; }
  /**
   * Constrols if all dependencies will be dropped in cascade
   */
  public abstract bool cascade { get; set; }
}
