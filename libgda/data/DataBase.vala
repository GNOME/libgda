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
	public class DataBase : Object, DbObject, DbNamedObject, DbCollection
	{
		public HashMap<string,DbSchema> _schemas = new HashMap<string,DbSchema> ();
		// DbObject Interface
		public Connection connection { get; set; }
		public void update () throws Error
		{
			connection.update_meta_store (null);
			var store = connection.get_meta_store ();
			var msch =  store.extract ("SELECT * FROM _schemata", null);
			int r;
			for ( r = 0; r < msch.get_n_rows (); r++) {
				var schema = new Schema ();
				schema.connection = this.connection;
				schema.name = (string) msch.get_value_at (msch.get_column_index ("schema_name"),r);
				_schemas.set (schema.name, (DbSchema) schema);
			}
		}
		public void save () throws Error {}
		public void append () throws Error {}
		// DbNamedObject
		public string name { get; set; }
		// DbCollection Interface
		public Collection<DbSchema> schemas { owned get { return _schemas.values; }}
	}
}
