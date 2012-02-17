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
	public class FieldInfo : Object, DbFieldInfo
	{
		private DbFieldInfo.Attribute      _attr;
		private Value?                 _default_value;
		private string                 _name;
		private string                 _desc;
		private int                    _precision = -1;
		private int                    _scale = -1;
		private DbFieldInfo.ForeignKey _fk;
		
		public DbFieldInfo.Attribute  attributes {
			get { return _attr; } set { _attr = value; }
		}
		
		public Value? default_value { 
			get { return _default_value; }
			set { _default_value = value; }
		}
		
		public string name { 
			get { return _name; }
			set { _name = value; }
		}
		
		public string desc { get { return _desc; } set { _desc = value; } }
		
		public int precision   { get { return _precision; } set { _precision = value; } }
		public int scale       { get { return _scale; } set { _scale = value; } }
		
		public DbFieldInfo.ForeignKey  fkey   { get  { return _fk; } set { _fk = value; } }
	}
}
