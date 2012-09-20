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

namespace GdaData {
	public class RecordCollection : AbstractCollection<DbRecord>, DbRecordCollection
	{
		private DataModel _model;
		private DbTable   _table;
		
		public DbTable    table { get { return table; } }
		
		public DataModel  model { get { return _model; } }
		
		public Connection connection { get; set; }
		
		public RecordCollection (DataModel m, DbTable table)
		{
			_model = m;
			_table = table;
		}
		// AbstractCollection Implementation
		public override bool add (DbRecord item) 
		{ 
			try {
				int r = _model.append_row ();
				foreach (DbField f in item.fields) {
					_model.set_value_at (_model.get_column_index (f.name), r, f.value);
				}
				return true;
			} 
			catch (Error e) { GLib.warning (e.message); }
			return false; 
		}
		public override void clear ()
		{ 
			try {
				var iter = _model.create_iter ();
				while (iter.move_next ()) {
					_model.remove_row (iter.get_row ());
				}
				((DataProxy) _model).apply_all_changes ();
			}
			catch (Error e) { GLib.warning (e.message); }
		}
		public override bool contains (DbRecord item)
		{
			bool found = true;
			var iter = _model.create_iter ();
			while (iter.move_next ()) {
				foreach (DbField k in item.keys) {
					try {
						Value id = iter.get_value_at (iter.data_model.get_column_index (k.name));
						Value v = k.value;
						if (Gda.value_compare (id,v) != 0)
							found = false;
					}
					catch (Error e) { 
						GLib.warning (e.message); 
						found = false;
					}
				}
				if (found) break;
			}
			return found;
		}
		public override Gee.Iterator<DbRecord> iterator () 
		{ 
			Gda.DataModelIter iter;
			iter = _model.create_iter ();
			return new RecordCollectionIterator (iter, _table); 
		}
		public override bool remove (DbRecord item)
		{
			var iter = _model.create_iter ();
			while (iter.move_next ()) {
				bool found = true;
				foreach (DbField k in item.keys) {
					try {
						Value id = iter.get_value_at (iter.data_model.get_column_index (k.name));
						Value v = k.value;
						if (Gda.value_compare (id,v) != 0)
							found = false;
					}
					catch (Error e) { 
						GLib.warning (e.message);
						found = false;
					}
				}
				if (found) {
					try {
						_model.remove_row (iter.get_row ());
						((DataProxy)_model).apply_all_changes ();
						return true;
					} catch {}
				}
			}
			return false;
		}
		public override bool read_only { 
			get {
				var f = _model.get_access_flags ();
				if ( (f & Gda.DataModelAccessFlags.INSERT
					& Gda.DataModelAccessFlags.UPDATE
					& Gda.DataModelAccessFlags.DELETE) != 0 )
					return true;
				else
					return false;
			}
		}
		public override int size { 
			get {
				return _model.get_n_rows ();
			} 
		}
		// 
		public string to_string ()
		{
			return _model.dump_as_string ();
		}
	}
}
