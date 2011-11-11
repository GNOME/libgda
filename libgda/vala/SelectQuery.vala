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
using Gee;

namespace Gda {
    [Compact]
    private class SelectQuery {
        
        public Gee.ArrayList<string> _fields;
        private SqlBuilder _sql;
         
        public string table { set; get; }
        public Connection connection { set; get; }
        
        SelectQuery()
        {
        	this.fields.add("*");
        	this._cond = 0;
        	this.table = "";
        }
        
        public void add_field (string field)
        {
        	if (this._fields.get(0) == "*")
        	{
        		this._fields.clear();
        	}
	        
	        this.fields.add(field);
        }
        
        public SqlBuilder build (void)
        {
        	this._sql = new SqlBuilder(Gda.SqlStatementType.SELECT);
        	
        	foreach string f in this._fields {
	        	this._sql.select_add_field (f, null, null);
	        }            
			this._sql.select_add_target(this.table, null);
        }
        
        public void set_fields_to_all (void) 
        {
        	this._fields.clear();
        	this._fields.add("*");
        }
        
        public void set_condition (string field, Value v, SqlOperatorType op) 
        {
        	var f_id = this._sql.add_id (c.field);
			var e_id = this._sql.add_expr_value (null, c.v);
			var c_id = this._sql.add_cond(c.op, f_id, e_id, 0);
			this._sql.set_where(c_id);
        }
        
        public DataModel execute (void) 
            throws Error 
            requires (this.connection.is_opened())
        {
            /* Build Select Query */
            var b = this.build();
		    var s = b.get_statement();
		    return this.connection.statement_execute_select(s, null);
        }
    }
}
