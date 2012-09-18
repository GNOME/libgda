/* -*- Mode: Vala; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * libgdavala
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
using Gee;

namespace GdaData {

    public class Record : Object, DbObject, Comparable<DbRecord>, DbRecord
    {
        private bool _update_meta = false;
        
        protected HashMap<string,DbField> _fields = new HashMap<string,DbField> ();
        protected HashMap<string,DbField> _keys = new HashMap<string,DbField> ();
        /**
         * Derived classes must implement this property to set the table used to get data from.
         */
        public DbTable table { get; set construct; }
        
        /**
         * Returns a Gee.Collection with the data stored by this object.
         */
        public Collection<DbField> fields { owned get { return _fields.values; } }
        
        public Collection<DbField> keys { owned get { return _keys.values; } }
        
        /**
         * Set the connection to be used to get/set data.
         */
        public Connection connection { get; set; }
        
        /**
         * Returns a GLib.Value containing the value stored in the given field.
         */
        public Value? get_value (string field)
        	throws Error
        {
        	var f = this._fields.get (field);
        	return f.value;
        }
        
		/**
         * Set the value to a field with the given @name.
         */
        public void set_field (DbField field) throws Error
        {
        	if (_fields.has_key (field.name)) {
		    	var f = this._fields.get (field.name);
		    	f.value = field.value;
		    	this._fields.set (field.name, f);
        	}
        	else {
        		// FIXME: Get default attributes from table
        		var n = new Field (field.name, DbField.Attribute.NONE); 
        		n.value = field.value;
        		this._fields.set (field.name, n);
        	}
        	
        }
        
        public void set_field_value (string field, Value? val) throws Error
        {
        	var n = new Field (field, DbField.Attribute.NONE); 
    		n.value = val;
    		this.set_field (n);
        }
        
        public DbField get_field (string name) throws Error
        {
        	return _fields.get (name);
        }
        
        public void set_key (DbField field)
        {
        	if (_keys.has_key (field.name)) {
		    	var f = _keys.get (field.name);
		    	f.value = field.value;
		    	_keys.set (field.name, f);
        	}
        	else {
        		// FIXME: Get default attributes from table
        		var n = new Field (field.name, DbField.Attribute.NONE); 
        		n.value = field.value;
        		_keys.set (field.name, n);
        	}
        }
        
        public void set_key_value (string key, Value? val) throws Error
        {
        	var n = new Field (key, DbField.Attribute.NONE); 
    		n.value = val;
    		this.set_key (n);
        }
        
        public DbField get_key (string name) throws Error
        {
        	return _keys.get (name);
        }
        
        // DbObject interface
        
        public bool update_meta { get { return _update_meta; } set { _update_meta = value; } }
        /**
         * Saves any modficiation made to in memory representation of the data directly to
         * the database.
         */
        public void save ()  throws Error
        {
        	if (fields.size <= 0)
        		throw new DbObjectError.SAVE ("No fields has been set");
        	if (keys.size <= 0)
        		throw new DbObjectError.SAVE ("No Keys has been set");
			var q = new SqlBuilder (SqlStatementType.UPDATE);
			q.set_table (table.name);
			foreach (DbField f in fields) {
				q.add_field_value_as_gvalue (f.column_name, f.value);
			}
			SqlBuilderId cond = -1;
			foreach (DbField f in keys) {
				var f_id = q.add_id (f.name);
				var e_id = q.add_expr_value (null, f.value);
				var c_id = q.add_cond (SqlOperatorType.EQ, f_id, e_id, 0);
				if (cond == -1) 
					cond = c_id;
				else
					cond = q.add_cond (SqlOperatorType.AND, cond, c_id, 0);
			}
			q.set_where (cond);
//			stdout.printf ("DEBUG: UPDATE statement to execute: \n"+ 
//							(q.get_statement()).to_sql_extended (this.connection, null, 
//																	StatementSqlFlag.PRETTY, null)
//							+ "\n");
			var i = this.connection.statement_execute_non_select (q.get_statement (), null, null);
			if (i != 1) {
				throw new DbObjectError.SAVE ("Have been saved more or less rows than expected");
			}
        }
        
        /**
         * Updates values stored in database.
         */
        public void update ()  throws Error
        {
        	if (update_meta) {
        		connection.update_meta_store (null);
        		update_meta = false;
        	}
        	if (keys.size <= 0)
        		throw new DbObjectError.UPDATE ("No Keys has been set");
			var q = new SqlBuilder (SqlStatementType.SELECT);
        	q.select_add_target (table.name, null);
        	q.select_add_field ("*", null, null);
			SqlBuilderId cond = -1;
        	foreach (DbField f in keys) {
				var f_id = q.add_id (f.name);
				var e_id = q.add_expr_value (null, f.value);
				var c_id = q.add_cond (SqlOperatorType.EQ, f_id, e_id, 0);
				if (cond == -1) {
					cond = c_id;
				}
				else {
					cond = q.add_cond (SqlOperatorType.AND, cond, c_id, 0);
				}
			}
			q.set_where (cond);
//			stdout.printf ("DEBUG: SELECT statement to execute: \n"+ 
//							(q.get_statement()).to_sql_extended (this.connection, null, 
//																	StatementSqlFlag.PRETTY, null)
//							+ "\n");
			var m = this.connection.statement_execute_select (q.get_statement (), null);
			if (m.get_n_rows () != 1) {
				throw new DbObjectError.UPDATE ("Returning number of rows are more than 1");
			}
			
			for (int c = 0; c < m.get_n_columns (); c++) {
				var f = new Field (m.get_column_name (c), 
											(DbField.Attribute) m.get_attributes_at (c, 0));
				f.value = m.get_value_at (c,0);
				this.set_field (f);
			}
        }
        
        /**
         * Append a new row to the defined table and returns its ID. If defaults is set to true,
         * default value for each field is set.
         */
        public void append () throws Error
        {
        	if (fields.size <= 0)
        		throw new DbObjectError.APPEND ("No fields has been set");
			var sql = new SqlBuilder (SqlStatementType.INSERT);
			sql.set_table (table.name);
			// FIXME: MetaData is required
			foreach (DbField f in _fields.values) {
				sql.add_field_value_as_gvalue (f.column_name, f.value);
			}
//			stdout.printf ("DEBUG: INSERT statement to execute: \n"+ 
//				(sql.get_statement()).to_sql_extended (this.connection, null, 
//														StatementSqlFlag.PRETTY, null)
//				+ "\n");
			var i = this.connection.statement_execute_non_select (sql.get_statement (), null, null);
			if (i != 1) {
				throw new DbObjectError.UPDATE ("Have been updated more or less rows than expected");
			}
		}
		
		public void drop (bool cascade) throws Error 
		{
			if (keys.size <= 0)
        		throw new DbObjectError.DROP ("No keys has been set");
			var sql = new SqlBuilder (SqlStatementType.DELETE);
			sql.set_table (table.name);
			SqlBuilderId cond = -1;
        	foreach (DbField f in keys) {
				var fid = sql.add_id (f.column_name);
				var vid = sql.add_expr_value (null, f.value);
				var c_id = sql.add_cond (SqlOperatorType.EQ, fid, vid, 0);
				if (cond == -1) {
					cond = c_id;
				}
				else {
					cond = sql.add_cond (SqlOperatorType.AND, cond, c_id, 0);
				}
			}
			sql.set_where (cond);
			stdout.printf ("DEBUG: DELETE statement to execute: \n"+ 
				(sql.get_statement()).to_sql_extended (this.connection, null, 
														StatementSqlFlag.PRETTY, null)
				+ "\n");
			var i = this.connection.statement_execute_non_select (sql.get_statement (), null, null);
			if (i != 1) {
				throw new DbObjectError.DROP ("Have been dropped more or less rows than expected");
			}
		}
        // 
        public string to_string ()
        {
        	string r = "";
			foreach (DbField f in this.fields) {
				r += "|" + f.name;
			}
			r+="\n";
			foreach (DbField f in this.fields) {
				r += "|" + Gda.value_stringify (f.value);
			}
			return r;
        }
        // Comparable Interface
        /**
         * Compare two DbRecord keys. If record have more than one key allways return -1.
         *
         * @Returns: 0 if keys are equal or -1 if they are different.
         */
        public int compare_to (DbRecord object)
        {
        	int r = 0;
        	try {
		    	foreach (DbField f in fields) {
		    		var fl = object.get_field (f.name);
		    		if (Gda.value_compare (f.value, fl.value) != 0)
		    			return -1;
		    	}
		    }
		    catch (Error e) { GLib.warning (e.message); }
        	return r;
        }
    }
}
