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
		private Value?                  _default_value;
		private DbFieldInfo.ForeignKey  _fkey;
		
		public int                    ordinal           { get; set; }
		public Type                   value_type        { get; set; }
		public DbFieldInfo.Attribute  attributes        { get; set; }
		public Value?                 default_value     
		{ 
			get { return _default_value; }
			set {
				attributes = attributes | DbFieldInfo.Attribute.HAVE_DEFAULT;
				_default_value = value;
			}
		}
		public string                 name              { get; set; }
		public string                 desc              { get; set; }
		
		// Numeric and Datetime attributes
		public int                    precision         { get; set; }
		public int                    scale             { get; set; }
		
		// Constrains
		public DbFieldInfo.ForeignKey  fkey   
		{ 
			get { return _fkey; }
			set {
				attributes = attributes | DbFieldInfo.Attribute.FOREIGN_KEY;
				_fkey = value;
			}
		}
		
		public FieldInfo ()
		{
			ordinal = -1;
		}

	}
}
