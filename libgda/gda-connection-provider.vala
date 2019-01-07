/*
 * libgdadata
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
  public abstract signal void closing ();

  public abstract void close ();
  public abstract void close_no_warning ();
  public abstract bool open () throws GLib.Error;

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
 * A result from a select query representing a table with
 * rows and columns.
 */
public interface Gda.TableModel : GLib.Object, GLib.ListModel, Gda.Result {
  /**
   * Model to iterate or access to {link Gda.DbRow} objects
   */
  public abstract GLib.ListModel rows { get; }
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
  public abstract GLib.Type data_type { get; }
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
public interface Gda.Parameters : GLib.Object {
  public abstract void set_value (string name, GLib.Value val);
  public abstract GLib.Value? get_value (string name);
}
