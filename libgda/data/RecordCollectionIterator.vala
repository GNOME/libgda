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
	public class RecordCollectionIterator : Object, Traversable<DbRecord>, Iterator<DbRecord>
	{
		private DataModelIter _iter;
		private DbTable _table;
		
		public RecordCollectionIterator (DataModelIter iter, DbTable table)
		{
			_iter = iter;
			_table = table;
		}
		
		public new bool @foreach (Gee.ForallFunc<DbRecord> f) 
		{
			var r = this.get ();
			bool ret = f(r);
			if (ret && this.next ())
				return true;
			else
				return false; 
		}
		
		// Iterator Interface
		public new DbRecord @get ()
		{
			var r = new Record ();
			r.connection = _table.connection;
			r.table = _table;
			try {
				for (int c = 0; c < _iter.data_model.get_n_columns (); c++) {
					r.set_field_value (_iter.data_model.get_column_name (c), _iter.get_value_at (c));
				}
			}
			catch (Error e) { GLib.message (e.message); }
			return r;
		}
		public bool has_next () 
		{
			return _iter.current_row + 1 <= _iter.data_model.get_n_rows () ? true : false;
		}
		public bool next () 
		{ 
			if (this.has_next ())
				return _iter.move_next ();
			else
				return false;
		}
		public void remove () 
		{ 
			try { _iter.data_model.remove_row (_iter.current_row); }
			catch (Error e) { GLib.message (e.message); }
		}
		public bool read_only 
		{ 
			get { 
				if (_iter.data_model is DataProxy)
					return ((DataProxy)_iter.data_model).is_read_only ();
				else
					return false;
			}
		}
		public bool valid { 
			get { 
				return _iter.is_valid ();
			}
		}
	}
}
