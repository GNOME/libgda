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
	public interface Table : Object
	{
		// DbObject Interface
		public Connection connection { get; set; }
		public void update ()
		{
			
		}
		// DbNamedObject Interface
		public string name { get; set; }
		
		// DbTable Interface
		public Collection<FieldInfo<G>> info_fields {}
		public DbSchema schema { get; set; }
//		public Collection<DbRecord> records { get; }
//		public Collection<DbTable> fk_depends { get; }
//		public Collection<DbTable> fk { get; }
//		public Iterator<DbRecord> iterator ();
	}
}
