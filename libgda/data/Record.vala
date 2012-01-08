/* -*- Mode: Vala; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * libgdavala
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
using Gee;

namespace GdaData {

    public class Record : Object, DbObject, DbRecord<Value?>, Comparable<DbRecord>
    {
        protected HashMap<string,DbField<Value?>> _fields = new HashMap<string,DbField<Value?>> ();
        protected HashMap<string,DbField<Value?>> _keys = new HashMap<string,DbField<Value?>> ();
        /**
         * Derived classes must implement this property to set the table used to get data from.
         */
        public DbTable table { get; set construct; }
        /**
         * Returns a Gee.Collection with the data stored by this object.
         */
        public Collection<DbField<Value?>> fields { owned get { return _fields.values; } }
        public Collection<DbField<Value?>> keys { owned get { return _keys.values; } }
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
        public void set_field (DbField<Value?> field) throws Error
        {
        	if (_fields.has_key (field.name)) {
		    	var f = this._fields.get (field.name);
		    	f.value = field.value;
		    	this._fields.set (field.name, f);
        	}
        	else {
        		// FIXME: Get default attributes from table
        		var n = new Field<Value?> (field.name, DbField.Attribute.NONE); 
        		n.connection = connection;
        		n.value = field.value;
        		this._fields.set (field.name, n);
        	}
        	
        }
        public DbField<Value?> get_field (string name) throws Error
        {
        	return _fields.get (name);
        }
        public void set_key (DbField<Value?> field)
        {
        	if (_keys.has_key (field.name)) {
		    	var f = _keys.get (field.name);
		    	f.value = field.value;
		    	_keys.set (field.name, f);
        	}
        	else {
        		// FIXME: Get default attributes from table
        		var n = new Field<Value?> (field.name, DbField.Attribute.NONE); 
        		n.value = field.value;
        		_keys.set (field.name, n);
        	}
        }
        public DbField<Value?> get_key (string name) throws Error
        {
        	return _keys.get (name);
        }
        /**
         * Saves any modficiation made to in memory representation of the data directly to
         * the database.
         */
        public void save ()  throws Error
        {
        	var q = new SqlBuilder (SqlStatementType.UPDATE);
			q.set_table (table.name);
			foreach (DbField<Value?> f in fields) {
				q.add_field_value_as_gvalue (f.column_name, f.value);
			}
			SqlBuilderId cond = -1;
			foreach (DbField<Value?> f in fields) {
				var f_id = q.add_id (f.name);
				var e_id = q.add_expr_value (null, f.value);
				var c_id = q.add_cond (SqlOperatorType.EQ, f_id, e_id, 0);
				if (cond < 0) 
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
				throw new DbObjectError.APPEND ("Have been saved more or less rows than expected");
			}
        }
        /**
         * Updates values stored in database.
         */
        public void update ()  throws Error
        {
        	var q = new SqlBuilder (SqlStatementType.SELECT);
        	q.select_add_target (table.name, null);
        	SqlBuilderId cond = -1;
			foreach (DbField<Value?> f in fields) {
				var f_id = q.add_id (f.name);
				var e_id = q.add_expr_value (null, f.value);
				var c_id = q.add_cond (SqlOperatorType.EQ, f_id, e_id, 0);
				if (cond < 0) 
					cond = c_id;
				else
					cond = q.add_cond (SqlOperatorType.AND, cond, c_id, 0);
			}
			q.set_where (cond);
			q.select_add_field ("*", null, null);
//			stdout.printf ("DEBUG: UPDATE statement to execute: \n"+ 
//							(q.get_statement()).to_sql_extended (this.connection, null, 
//																	StatementSqlFlag.PRETTY, null)
//							+ "\n");
			var i = this.connection.statement_execute_non_select (q.get_statement (), null, null);
			if (i != 1) {
				throw new DbObjectError.UPDATE ("Have been updated more or less rows than expected");
			}
        }
        /**
         * Append a new row to the defined table and returns its ID. If defaults is set to true,
         * default value for each field is set.
         */
        public bool append () throws Error
        {
        	var sql = new SqlBuilder (SqlStatementType.INSERT);
			sql.set_table (table.name);
			// FIXME: MetaData is required
			foreach (DbField<Value?> f in _fields.values) {
				sql.add_field_value_as_gvalue (f.column_name, f.value);
			}
//			stdout.printf ("DEBUG: INSERT statement to execute: \n"+ 
//				(sql.get_statement()).to_sql_extended (this.connection, null, 
//														StatementSqlFlag.PRETTY, null)
//				+ "\n");
			var i = this.connection.statement_execute_non_select (sql.get_statement (), null, null);
			if (i != 1) {
				throw new DbObjectError.UPDATE ("Have been added more or less rows than expected");
			}
			return true;
		}
        // 
        public string to_string ()
        {
        	string r = "";
			foreach (DbField<Value?> f in this.fields) {
				r += "|" + f.name;
			}
			r+="\n";
			foreach (DbField<Value?> f in this.fields) {
				r += "|" + Gda.value_stringify (f.value);
			}
			r+="\n";
			return r;
        }
        // Comparable Interface
        /**
         * Compare two DbRecord keys. If record have more than one key allways return -1.
         *
         * @Returns: 0 if keys are equal or -1 if they are different.
         */
        public int compare_to (DbRecord<Value?> object)
        {
        	int r = 0;
        	foreach (DbField<Value?> f in fields) {
        		var fl = object.get_field (f.name);
        		if (Gda.value_compare (f.value, fl.value) != 0)
        			return -1;
        	}
        	return r;
        }
    }
}
