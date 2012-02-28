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
	public class Schema : Object, DbObject, DbNamedObject, DbSchema
	{
		public HashMap<string,DbTable> _tables = new HashMap<string,DbTable> ();
		// DbObject Interface
		public Connection connection { get; set; }
		public void update () throws Error
		{
			connection.update_meta_store (null); // FIXME: just update schemas
			var store = connection.get_meta_store ();
			tables.clear ();
			var vals = new HashTable<string,Value?> (str_hash,str_equal);
			Value v = name;
			vals.set ("name", v);
			var mt = store.extract ("SELECT * FROM _tables WHERE schema_name = ##name::string", vals);
			for (int r = 0; r < mt.get_n_rows (); r++) {
				var t = new Table ();
				t.connection = connection;
				t.name = (string) mt.get_value_at (mt.get_column_index ("table_name"), r);
				t.table_type = 
						DbTable.type_from_string((string) mt.get_value_at (mt.get_column_index ("table_type"), r));
				t.schema = this;
				tables.set (t.name, t);
			}
		}
		public void save () throws Error {}
		public void append () throws Error {}
		public void drop (bool cascade) throws Error {}
		// DbNamedObject Interface
		public string name { get; set; }
		// DbSchema Interface
		public DbCatalog           catalog { get; set; }
		public Collection<DbTable> tables { owned get { return _tables.values; } }
	}
}
