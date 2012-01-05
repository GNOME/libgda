/* -*- Mode: Vala; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * libgdadata
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

namespace GdaData
{
	public class Field<G> : Object, DbField<G>
	{
		private G                 val;
		private string            _name;
		private string            _column_name;
		private DbField.Attribute _attributes;
		
		public G @value { 
			get { return val; } 
			set { val = value; }
		}
		public string name { 
			get { return _name; } 
			set { _name = value; }
		}
		public string column_name { 
			get { return _column_name; }
		}
		public DbField.Attribute attributes { 
			get { return _attributes; }
		}
		public string to_string () { return (string) val; }
		public Field (string col_name, DbField.Attribute attr) 
		{
			_column_name = col_name;
			_attributes = attr;
			_name = col_name;
		}
	}
}
