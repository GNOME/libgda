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

namespace GdaData {

	public abstract class ObjectSingleId : GdaData.Object<Value?> 
	{
		private Value? _id_value;
        
        /**
         * Field's name used as ID in the row
         */
        public abstract string field_id { get; }
        /* 
         * ID Field's default index, returned when an INSERT statement is used.
         */
        public abstract int field_id_index { get; }
        
        public Value get_id ()
        {
        	return this._id_value;
        }
        
        /* 
         * Set a condition to get only one row data from the table in the database.
         */
        public void set_id (Value v)
        	throws Error
        	requires (this.table != "")
        {
        	this._id_value = v;
        	var q = this.sql ();
        	var s = q.get_statement ();
        	var m = this.connection.statement_execute_select (s, null);
        	for (int r = 0; r < m.get_n_rows (); r++) {
        		for (int c = 0; c < m.get_n_columns (); c++) {
        			Gda.ValueAttribute attr = m.get_attributes_at (c, r);
        			string col_name = m.get_column_name (c);
        			var f = new Field<Value?> (col_name, (DbField.Attribute) attr);
        			f.value = m.get_value_at (c, r);
        			this._model.set (f.name, f);
        		}
        	}
        }
        
        /**
         * Returns a #SqlBuilder object with the query used to select the data in the used
         * that points this object to.
         */
        public SqlBuilder sql ()
        	requires (this.table != null || this.table != "")
        	requires (this.field_id != null || this.field_id != "")
        	requires (this._id_value != null)
        {
        	var q = new SqlBuilder (SqlStatementType.SELECT);
        	q.select_add_target (this.table, null);
        	var f_id = q.add_id (this.field_id);
			var e_id = q.add_expr_value (null, this._id_value);
			var c_id = q.add_cond (SqlOperatorType.EQ, f_id, e_id, 0);
			q.set_where (c_id);
			q.select_add_field ("*", null, null);
			return q;			
        }
        
        public override void save () throws Error
        {
        	var sql = new SqlBuilder (SqlStatementType.UPDATE);
			sql.set_table (this.table);
			foreach (Field<Value?> f in _model.values) {
				sql.add_field_value_as_gvalue (f.column_name, f.value);
			}
			var f_id = sql.add_id (this.field_id);
			var e_id = sql.add_expr_value (null, this._id_value);
			var c_id = sql.add_cond (SqlOperatorType.EQ, f_id, e_id, 0);
			sql.set_where (c_id);
//			stdout.printf ("DEBUG: UPDATE statement to execute: \n"+ 
//							(sql.get_statement()).to_sql_extended (this.connection, null, 
//																	StatementSqlFlag.PRETTY, null)
//							+ "\n");
			var i = this.connection.statement_execute_non_select (sql.get_statement (), null, null);
			if (i != 1) {
				throw new DbObjectError.APPEND ("Have been saved more or less rows than expected");
			}
        }
        public override bool append () throws Error
        {
        	var sql = new SqlBuilder (SqlStatementType.INSERT);
			sql.set_table (this.table);
			// FIXME: MetaData is required
			foreach (Field<Value?> f in _model.values) {
				sql.add_field_value_as_gvalue (f.column_name, f.value);
			}
//			stdout.printf ("DEBUG: INSERT statement to execute: \n"+ 
//				(sql.get_statement()).to_sql_extended (this.connection, null, 
//														StatementSqlFlag.PRETTY, null)
//				+ "\n");
			Set last_inserted;
			var i = this.connection.statement_execute_non_select (sql.get_statement (), null, null);
			if (i != 1) {
				throw new DbObjectError.APPEND ("Have been added more or less rows than expected");
			}
			return true;
        }
		public override void update ()
        	throws Error
        	requires (this.table != "")
        {
        	set_id (this._id_value);
        }
        public override string to_string () 
        {
        	string r = "";
			foreach (Field<Value?> f in this.fields) {
				r += "|" + f.name;
			}
			r+="\n";
			foreach (Field<Value?> f in this.fields) {
				r += "|" + Gda.value_stringify (f.value);
			}
			r+="\n";
			return r;
        }
	}
}
