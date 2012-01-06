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

    public abstract class Record<G> : Object {
        
        protected HashMap<string,DbField<G>> _model = new HashMap<string,DbField<G>> ();
        protected HashMap<string,DbField<G>> _keys = new HashMap<string,DbField<G>> ();
        /**
         * Derived classes must implement this property to set the table used to get data from.
         */
        public DbTable table { get; set construct; }
        /**
         * Returns a Gee.Collection with the data stored by this object.
         */
        public Collection<DbField<G>> fields { owned get { return _model.values; } }
        public Collection<DbField<G>> keys { owned get { return _keys.values; } }
        /**
         * Set the connection to be used to get/set data.
         */
        public Connection connection { get; set; }
        /**
         * Returns a GLib.Value containing the value stored in the given field.
         */
        public G get_value (string field)
        	throws Error
        {
        	var f = this._model.get (field);
        	return f.value;
        }
		/**
         * Set the value to a field with the given @name.
         */
        public void set_value (string field, G v)
        	throws Error
        {
        	if (_model.has_key (field)) {
		    	var f = this._model.get (field);
		    	f.value = v;
		    	this._model.set (field, f);
        	}
        	else {
        		// FIXME: Get default attributes from table
        		var n = new Field<G> (field, DbField.Attribute.NONE); 
        		n.value = v;
        		this._model.set (field, n);
        	}
        	
        }
        
        public void set_key (string field, G v)
        {
        	if (keys.has_key (field)) {
		    	var f = keys.get (field);
		    	f.value = v;
		    	keys.set (field, f);
        	}
        	else {
        		// FIXME: Get default attributes from table
        		var n = new Field<G> (field, DbField.Attribute.NONE); 
        		n.value = v;
        		keys.set (field, n);
        	}
        }
        /**
         * Saves any modficiation made to in memory representation of the data directly to
         * the database.
         */
        public void save ()  throws Error
        {
        	var q = new SqlBuilder (SqlStatementType.UPDATE);
			q.set_table (table.name);
			foreach (DbField<G> f in fields) {
				Value v = f.value;
				q.add_field_value_as_gvalue (f.column_name, v);
			}
			SqlBuilderId cond = -1;
			foreach (DbField<G> f in fields) {
				var f_id = q.add_id (f.name);
				Value v = f.value;
				var e_id = q.add_expr_value (null, v);
				var c_id = q.add_cond (SqlOperatorType.EQ, f_id, e_id, 0);
				if (cond < 0) 
					cond = c_id;
				else
					cond = sql.add_cond (SqlOperatorType.AND, cond, c_id, 0);
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
			foreach (DbField<G> f in fields) {
				var f_id = sql.add_id (f.name);
				Value v = f.value;
				var e_id = sql.add_expr_value (null, v);
				var c_id = sql.add_cond (SqlOperatorType.EQ, f_id, e_id, 0);
				if (cond < 0) 
					cond = c_id;
				else
					cond = sql.add_cond (SqlOperatorType.AND, cond, c_id, 0);
			}
			q.set_where (cond);
			q.select_add_field ("*", null, null);
//			stdout.printf ("DEBUG: UPDATE statement to execute: \n"+ 
//							(q.get_statement()).to_sql_extended (this.connection, null, 
//																	StatementSqlFlag.PRETTY, null)
//							+ "\n");
			var i = this.connection.statement_execute_non_select (sql.get_statement (), null, null);
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
			sql.set_table (this.table);
			// FIXME: MetaData is required
			foreach (Field<G> f in _model.values) {
				Value v = f.value;
				sql.add_field_value_as_gvalue (f.column_name, v);
			}
//			stdout.printf ("DEBUG: INSERT statement to execute: \n"+ 
//				(sql.get_statement()).to_sql_extended (this.connection, null, 
//														StatementSqlFlag.PRETTY, null)
//				+ "\n");
			Set last_inserted;
			var i = this.connection.statement_execute_non_select (sql.get_statement (), null, null);
			if (i != 1) {
				throw new DbObjectError.UPDATE ("Have been added more or less rows than expected");
			}
			return true;
		}
        
        public string to_string ()
        {
        	string r = "";
			foreach (Field<G> f in this.fields) {
				r += "|" + f.name;
			}
			r+="\n";
			foreach (Field<Value?> f in this.fields) {
				Value v = f.value;
				r += "|" + Gda.value_stringify (v);
			}
			r+="\n";
			return r;
        }
    }
}
