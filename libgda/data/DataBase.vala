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
	public class DataBase : Object 
	{
		public HashMap<string,DbSchema> schemas_container;
		// DbObject Interface
		public Connection connection { get; set; }
		public void update () throws Error
		{
			connection.update_meta_store (null);
			var store = connection.get_meta_store ();
			var mstruct = Gda.MetaStruct.new (store, Gda.MetaStructFeature.ALL);
			var msch =  store.extract ("SELECT * FROM _schemata");
			int c, r;
			for ( r = 0; r < msch.get_n_rows (); r++) {
				var schema = new Schema ();
				schema.connection = this.connection;
				schema.name = (string) msch.get_value_at (msch.get_column_index ("schema_name"),r);
				_schemas.set (schema.name, (DbSchema) schema);
			}
		}
		public void save () {}
		public void append () {}
		// DbCollection Interface
		public abstract Collection<DbSchema> schemas { get { _schemas.values; }}
	}
}
