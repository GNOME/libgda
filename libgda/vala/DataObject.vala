/* -*- Mode: Vala; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * libgda
 * Copyright (C) Daniel Espinosa Ortiz 2008 <esodan@gmail.com>
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

namespace Gda {
    public abstract class DataObject : Object {
        
        private DataModel? _model;
        private string _field_id;
        private Value _id_value;
        
        protected _table;
        
        public Connection _cnn { get; set; };
        
        public SqlBuilder sql {
            get {
                var q = new SelectQuery();
                q.connection = this._cnn;
                q.table = this._table;
                q.set_condition(this._field_id, this._id_value, SqlOperatorType.EQ);
                return q.build();
            }
        }
        
        public string get_field_id ()
        {
        	return this._field_id;
        }
        
        public Value get_value_id ()
        {
        	return this._id_value;
        }
        
        public void set_id (string field, Value cond)
        	throws Error
        {
        	this._field_id = field;
        	this._id_value = cond;
        	var q = this.sql;
        	var m = this._cnn.statement_execute_select(q.get_statement(), null);
        	((DataSelect) m).compute_modification_statements();
        	this._model = new DataProxy(m);
        }
        
        public Value? get_value (string field)
        {
        	return this._model.get_value_at(this._model.get_column_index(field), 0);
        }
        
        public void set_value (string field, Value v)
        {
        	this._model.set_value_at(this._model.get_column_index(field), 0);
        }
        
        public void save ()
        	throws Error
        {
        	((DataProxy) this._model).apply_all_changes();
        }
    }
}
