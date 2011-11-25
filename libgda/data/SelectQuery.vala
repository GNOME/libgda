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
using Gee;
using Gda;

namespace GdaData {
    
    private class SelectQuery : Object {
        
        private Gee.ArrayList<string> _fields;
        private SqlBuilder _sql;
        private string _table;
        
        public string table { 
        	get {
        		return this._table;
        	} 
        	set { 
        		this._table = value;
        		this._sql.select_add_target (value, null); 
        	}
        }
        
        public Connection connection { set; get; }
        
        public SelectQuery()
        {
        	this._fields.add ("*");
        	this.table = "";
        }
        
        public void add_field (string field)
        {
        	if (this._fields.get (0) == "*")
        	{
        		this._fields.clear ();
        	}
	        
	        this._fields.add (field);
        }
        
        public SqlBuilder build ()
        	requires (this.table != "")
        {
        	this._sql = new SqlBuilder (Gda.SqlStatementType.SELECT);
        	
        	foreach (string f in this._fields) {
	        	this._sql.select_add_field (f, null, null);
	        }            
			return this._sql;
        }
        
        public void set_fields_to_all () 
        {
        	this._fields.clear ();
        	this._fields.add ("*");
        }
        
        public void set_condition (string field, Value v, SqlOperatorType op) 
        {
        	var f_id = this._sql.add_id (field);
			var e_id = this._sql.add_expr_value (null, v);
			var c_id = this._sql.add_cond (op, f_id, e_id, 0);
			this._sql.set_where (c_id);
        }
        
        public DataModel execute () 
            throws Error 
            requires (this.connection.is_opened())
        {
            /* Build Select Query */
            var b = this.build ();
		    var s = b.get_statement ();
		    return this.connection.statement_execute_select (s, null);
        }
    }
}
