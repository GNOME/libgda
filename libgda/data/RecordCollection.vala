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
			try {
				var iter = _model.create_iter ();
				while (iter.move_next ()) {
					foreach (DbField k in item.keys) {
						Value id = iter.get_value_at (iter.data_model.get_column_index (k.name));
						Value v = k.value;
						if (Gda.value_compare (id,v) != 0)
							found = false;
					}
					if (found) break;
				}
			}
			catch (Error e) { 
				GLib.warning (e.message); 
				found = false;
			}
			return found;
		}
		public override Gee.Iterator<DbRecord> iterator () 
		{ 
			Gda.DataModelIter iter;
			try {
				iter = _model.create_iter ();
			}
			catch (Error e) { GLib.warning (e.message); }
			return new RecordCollectionIterator (iter, _table); 
		}
		public override bool remove (DbRecord item)
		{
			try {
				var iter = _model.create_iter ();
				while (iter.move_next ()) {
					bool found = true;
					foreach (DbField k in item.keys) {
						Value id = iter.get_value_at (iter.data_model.get_column_index (k.name));
						Value v = k.value;
						if (Gda.value_compare (id,v) != 0)
							found = false;
					}
					if (found) {
						try {
							_model.remove_row (iter.get_row ());
							((DataProxy)_model).apply_all_changes ();
							return true;
						} catch {}
					}
				}
			}
			catch (Error e) { GLib.warning (e.message); }
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
		// Traversable Interface
		public override Iterator<DbRecord> chop (int offset, int length = -1)
		{
			return this.iterator().chop (offset, length);
		}
		public override Gee.Iterator<DbRecord> filter (owned Gee.Predicate<DbRecord<Value?>> f)
		{
			return this.iterator().filter (f);			
		}
		public override Iterator<A> stream<A> (owned StreamFunc<DbRecord, A> f)
		{
			return this.iterator().stream<A> (f);
		}
		// 
		public string to_string ()
		{
			return _model.dump_as_string ();
		}
	}
	
	public class RecordCollectionIterator : Object, Traversable<DbRecord>, Iterator<DbRecord>
	{
		private DataModelIter _iter;
		private DbTable _table;
		private int _length = -1;
		private int init_row = -1;
		private HashMap<int,int> _elements;
		private int filter_pos = -1;
		
		public RecordCollectionIterator (DataModelIter iter, DbTable table)
		{
			_iter = iter;
			_table = table;
			_elements = new HashMap<int,int>();
		}
		
		private RecordCollectionIterator.filtered (DataModelIter iter, DbTable table, 
											int init, int offset, int length, 
											HashMap<int,int> elements, int filter_pos)
		{
			_iter = iter;
			_table = table;
			_elements = elements;
			this.filter_pos = filter_pos;
			if (init < 0 && offset >= 0) init = 0;
			if (init >= 0 && init < _iter.data_model.get_n_rows ()
				&& init + offset >= 0
				&& init + offset < _iter.data_model.get_n_rows ())
			{
				_iter.move_to_row (init + offset);
				init_row = _iter.current_row;
				if (length >= 0 && init + length < _iter.data_model.get_n_rows ())
				{
					_length = length;
				}
			}
			else
				init_row = -10; // Invalidate all the interator
			if (_elements.size > 0)
				init_row = -1; // Validate interator due to filtered values
		}
		
		// Traversable Interface
		public Gee.Iterator<DbRecord> chop (int offset, int length = -1) 
		{
			var iter = _iter.data_model.create_iter ();
			return new RecordCollectionIterator.filtered (iter, _table, _iter.current_row, 
									offset, length, _elements, filter_pos);
		}
		public Gee.Iterator<DbRecord> filter (owned Gee.Predicate<DbRecord> f) 
		{
			var elements = new Gee.HashMap <int,int> ();
			int pos = -1;
			while (this.next ()) {
				var r = this.get ();
				if (f(r)) {
					elements.set (++pos, _iter.current_row);
				}
			}
			var iter = _iter.data_model.create_iter ();
			return new RecordCollectionIterator.filtered (iter, _table, -1, 0, -1, elements, -1);
		}
		public new void @foreach (Gee.ForallFunc<DbRecord> f) 
		{
			while (this.next ()) {
				var r = this.get ();
				f(r);
			}
		}
		public Gee.Iterator<A> stream<A> (owned Gee.StreamFunc<DbRecord,A> f) 
		{
			return stream_impl<DbRecord, A> (this, f);
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
			if ((_length >= 0) && (_iter.current_row + 1 < _length + init_row)
					&& (_iter.current_row + 1 > _iter.data_model.get_n_rows ())) 
				return false;
			if (_elements.size > 0)
				return filter_pos + 1 < _elements.size ? true : false;
			
			return _iter.current_row + 1 <= _iter.data_model.get_n_rows () ? true : false;
		}
		public bool next () 
		{ 
			try {
				if (this.has_next ()) {
					if (_elements.size > 0) {
						_iter.move_to_row (_elements.get (++filter_pos));
						return true;
					}
					else
						return _iter.move_next ();
				}
			}
			catch (Error e) { GLib.message (e.message); }
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
				if (init_row == -10)
					return false;
					
				if (_length >= 0)
					return (_iter.current_row >= init_row)
							&& (_iter.current_row < _length + init_row) ? true : false;
				if (_elements.size > 0)
					return filter_pos < _elements.size;
				
				return (_iter.current_row >= 0) && 
						(_iter.current_row < _iter.data_model.get_n_rows ())? true : false; 
			}
		}
	}
}
