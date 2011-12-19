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

[CCode (gir_namespace = "GdaData", gir_version = "5.0")]
[CCode (cheader_filename="libgdadata.h")]
namespace GdaData {

    public abstract class Object : GLib.Object {
        
        private string? _field_id;
        private Value? _id_value;
        private DataModelIterable _model;
        
        public abstract string table { get; }
        
        public DataModelIterable record {
        	get {
        		return this._model;
        	}
        }
        
        public Connection connection { get; set; }
        
        public string get_field_id ()
        {
        	return this._field_id;
        }
        
        public Value get_value_id ()
        {
        	return this._id_value;
        }
        
        /* 
         * Set a condition to get only one row data from the table in the database.
         */
        public void set_id (string field, Value v)
        	throws Error
        	requires (this.table != "")
        {
        	this._field_id = field;
        	this._id_value = v;
        	var q = this.sql ();
        	var s = q.get_statement ();
        	var m = this.connection.statement_execute_select (s, null);
        	((DataSelect) m).compute_modification_statements ();
        	this._model= new DataModelIterable ((DataProxy) DataProxy.new (m));
        }
        
        public unowned Value? get_value (string field)
        	throws Error
        {
        	return this._model.get_value_at (this._model.get_column_index (field), 0);
        }
        
        public void set_value (string field, Value v)
        	throws Error
        {
        	this._model.set_value_at (this._model.get_column_index (field), 0, v);
        }
        
        public void save ()
        	throws Error
        {
        	((DataProxy) this._model).apply_all_changes ();
        }
        
        public void update ()
        	throws Error
        	requires (this.table != "")
        {
        	set_id (this._field_id, this._id_value);
        }
        
        public SqlBuilder sql ()
        	requires (this.table != null || this.table != "")
        	requires (this._field_id != null || this._field_id != "")
        	requires (this._id_value != null)
        {
        	var q = new SqlBuilder (SqlStatementType.SELECT);
        	q.select_add_target (this.table, null);
        	var f_id = q.add_id (this._field_id);
			var e_id = q.add_expr_value (null, this._id_value);
			var c_id = q.add_cond (SqlOperatorType.EQ, f_id, e_id, 0);
			q.set_where (c_id);
			q.select_add_field ("*", null, null);
			return q;			
        }
    }
}
