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
	public class RecordCollection : AbstractCollection<DbRecord<Value?>>, DbRecordCollection<Value?>
	{
		private DataModel _model;
		private DbTable<Value?> _table;
		
		public DbTable<Value?> table { get { return table; } }
		
		public DataModel model { get { return _model; } }
		
		public Connection connection { get; set; }
		
		public RecordCollection (DataModel m, DbTable<Value?> table)
		{
			_model = m;
			_table = table;
		}
		// AbstractCollection Implementation
		public override bool add (DbRecord<Value?> item) 
		{ 
			try {
				int r = _model.append_row ();
				foreach (DbField<Value?> f in item.fields) {
					_model.set_value_at (_model.get_column_index (f.name), r, f.value);
				}
				return true;
			} catch {}
			return false; 
		}
		public override void clear ()
		{ 
			var iter = _model.create_iter ();
			while (iter.move_next ()) {
				_model.remove_row (iter.get_row ());
			}
			((DataProxy) _model).apply_all_changes ();
		}
		public override bool contains (DbRecord<Value?> item)
		{
			var iter = _model.create_iter ();
			while (iter.move_next ()) {
				bool found = true;
				foreach (DbField<Value?> k in item.keys) {
					Value id = iter.get_value_at (iter.data_model.get_column_index (k.name));
					Value v = k.value;
					if (Gda.value_compare (id,v) != 0)
						found = false;
				}
				if (found) return true;
			}
			return false;
		}
		public override Gee.Iterator<DbRecord<Value?>> iterator () 
		{ 
			var iter = _model.create_iter ();
			return new RecordCollectionIterator (iter, _table); 
		}
		public override bool remove (DbRecord<Value?> item)
		{
			var iter = _model.create_iter ();
			while (iter.move_next ()) {
				bool found = true;
				foreach (DbField<Value?> k in item.keys) {
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
		public override Iterator<DbRecord<Value?>> chop (int offset, int length = -1)
		{
			return this.iterator().chop (offset, length);
		}
		public override Gee.Iterator<DbRecord<Value?>> filter (owned Gee.Predicate<DbRecord<Value?>> f)
		{
			return this.iterator().filter (f);			
		}
		public override Iterator<A> stream<A> (owned StreamFunc<DbRecord<Value?>, A> f)
		{
			return this.iterator().stream<A> (f);
		}
		// 
		public string to_string ()
		{
			return _model.dump_as_string ();
		}
	}
	
	public class RecordCollectionIterator : Object, Traversable<DbRecord<Value?>>, Iterator<DbRecord<Value?>>
	{
		private DataModelIter _iter;
		private DbTable<Value?> _table;
		private int _length = -1;
		private HashMap<int,int> _elements = new HashMap<int,int>();
		private int filter_pos = -1;
		
		public RecordCollectionIterator (DataModelIter iter, DbTable<Value?> table)
		{
			_iter = iter;
			_table = table;
			_length = _iter.data_model.get_n_rows ();
		}
		
		private RecordCollectionIterator.filtered_elements (DataModelIter iter, DbTable table, 
															HashMap<int,int> elements)
		{
			_iter = iter;
			_table = table;
			_length = _iter.data_model.get_n_rows ();
			_elements = elements;
		}
		private RecordCollectionIterator.with_lenght (DataModelIter iter, DbTable table, int length)
		{
			_iter = iter;
			_table = table;
			if (length >= 0 && _iter.is_valid ())
				_length = _iter.current_row + length;
			if (_length > _iter.data_model.get_n_rows ())
				_length = _iter.data_model.get_n_rows ();
		}
		
		// Traversable Interface
		public Gee.Iterator<DbRecord<Value?>> chop (int offset, int length = -1) 
			requires (offset <= _iter.data_model.get_n_rows ())
			requires (offset + length <= _iter.data_model.get_n_rows ())
		{
			var iter = _iter.data_model.create_iter ();
			iter.move_to_row (offset);
			return new RecordCollectionIterator.with_lenght (iter, _table, length);
		}
		public Gee.Iterator<DbRecord<Value?>> filter (owned Gee.Predicate<DbRecord<Value?>> f) 
		{
			var elements = new Gee.HashMap <int,int> ();
			while (_iter.move_next ()) {
				var r = this.get ();
				if (f(r)) {
					stdout.printf ("SELECTED row: " + _iter.current_row.to_string () 
									+ "Values:\n" + this.get().to_string ());
					elements.set (++filter_pos, _iter.current_row);
				}
			}
			var iter = _iter.data_model.create_iter ();
			return new RecordCollectionIterator.filtered_elements (iter, _table, elements);
		}
		public new void @foreach (Gee.ForallFunc<DbRecord<Value?>> f) 
		{
			while (_iter.move_next ()) {
				var r = this.get ();
				f(r);
			}
		}
		public Gee.Iterator<A> stream<A> (owned Gee.StreamFunc<DbRecord<Value?>,A> f) 
		{
			return stream_impl<DbRecord<Value?>, A> (this, f);
		}
		
		// Iterator Interface
		public new DbRecord<Value?> @get ()
		{
			var r = new Record ();
			r.connection = _table.connection;
			r.table = _table;
			for (int c = 0; c < _iter.data_model.get_n_columns (); c++) {
				r.set_field_value (_iter.data_model.get_column_name (c), _iter.get_value_at (c));
			}
			return r;
		}
		public bool has_next () 
		{
			return (_iter.current_row + 1 <= _iter.data_model.get_n_rows ()) ||
					(_iter.current_row + 1 <= _length) ? true : false;
		}
		public bool next () 
		{ 
			bool ret = false;
			if (_elements.size > 0 && ++filter_pos < _elements.size) {
				_iter.move_to_row (_elements.get (filter_pos));
			}
			else {
				if (this.has_next ())
					ret = _iter.move_next ();
				else {
					ret = false;
					_iter.invalidate_contents ();
				}
			}
			return ret;
		}
		public void remove () { _iter.data_model.remove_row (_iter.current_row); }
		public bool read_only 
		{ 
			get { 
				if (_iter.data_model is DataProxy)
					return ((DataProxy)_iter.data_model).is_read_only ();
				else
					return false;
			}
		}
		public bool valid { get { return _iter.is_valid (); } }
	}
}
