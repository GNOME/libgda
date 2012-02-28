/* -*- Mode: Vala; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * libgdavala
 * Copyright (C) Daniel Espinosa Ortiz 2012 <esodan@gmail.com>
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
	public class Catalog : Object, DbObject, DbNamedObject, DbCatalog
	{
		// DbObject interface
		// DbObject Interface
		public Connection connection { get; set; }
		public void update () throws Error {}
		public void save () throws Error {}
		public void append () throws Error {}
		public void drop (bool cascade) throws Error {}
		// DbNamedObject Interface
		public string name { get; set; }
		// DbCatalog interface
		public Collection<DbSchema> shemas { get; set; }
	}
}
