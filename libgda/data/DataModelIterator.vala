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
 	
 	/**
 	 * Iterator that implements [@link [Gee.Iterator] and [@link [Gee.Traversable]]
 	 */
 	class DataModelIterator : Object, Gee.Iterator <Value>, Gee.Traversable < Value >
 	{
 		private Gda.DataModelIter iter;
 		private int _current_pos;
 		/* Used for Traversable to skip a number of elements in this case rows and columns */
 		private int pos_init;
 		private int maxpos;
 		private Gee.HashMap <int, int> elements;
 		private bool filtered;	
 		
 		
 		public int current_col {
 			get { return this._current_pos - this.iter.get_row () * this.iter.data_model.get_n_columns (); }
 		}
 		
 		public DataModelIterator (Gda.DataModel model)
 		{
 			this.iter = model.create_iter ();
 			this._current_pos = 0;
 			this.pos_init = 0;
 			this.maxpos = this.iter.data_model.get_n_columns () * this.iter.data_model.get_n_rows () - 1;
 			this.filtered = false;
 		}
 		
 		private DataModelIterator.with_offset (Gda.DataModel model, int pos_init, int maxpos)
 		{
 			this.iter = model.create_iter ();
 			int i = pos_init;
 			if (i > maxpos)
 				i = maxpos;
 			if (maxpos > (model.get_n_columns () *  model.get_n_rows () - 1))
 				this.iter.invalidate_contents ();
 			
 			this._current_pos = i;
 			this.pos_init = i;
 			this.maxpos = maxpos;
 			this.filtered = false;
 		}
 		
 		private DataModelIterator.filtered_elements (Gda.DataModel model, Gee.HashMap <int, int> elements)
 		{
 			this.iter = model.create_iter ();
 			this._current_pos = 0;
 			this.pos_init = 0;
 			this.maxpos = this.iter.data_model.get_n_columns () * this.iter.data_model.get_n_rows () - 1;
 			this.filtered = true;
 			this.elements = elements;
 		}
 		
 		public bool valid 
 		{ 
 			get { return this.iter.is_valid (); }
 		}
 		
 		public bool read_only {	get { return true; } }
 		
 		public bool next () {
 			if (!this.filtered) 
 			{
	 			this._current_pos++;
	 			if (this._current_pos > this.maxpos) {
	 				this.iter.invalidate_contents ();
	 				return false;
	 			}
	 			else
	 				return this.iter.move_to_row (this._current_pos / this.iter.data_model.get_n_columns ());
 			}
 			else
 			{
 				while (true) {
 					if (this.elements.has_key (++this._current_pos))
 						break;
 				}
 				return this.iter.move_to_row (this._current_pos / this.iter.data_model.get_n_columns ());
 			}
 		}
 		
 		public bool has_next () {
 			if (this.iter.is_valid ()) {
 				int pos = this._current_pos;
 				if (++pos > this.maxpos)
 					return false;
 				else
 					return true;
 			}
 			return false;
 		}
 		
 		/**
		 * {@inheritDoc}.
		 * 
		 * {@inheritDoc}<< BR >>
		 * << BR >>
		 * ''Implementation:'' Gda.DataModel have read only GLib.Value no modification is allowed.
		 * This function returns a copy of the Value stored in the DataModel.
		 */
 		public Value @get ()
 		{
 			return this.iter.get_value_at (this._current_pos - 
 										this.iter.get_row () * 
 											this.iter.data_model.get_n_columns ());
 		}
 		
 		public void remove () {}
 		
 		/* Traversable */
 		public Gee.Iterator<Value?> chop (int offset, int length = -1) 
 			requires ( offset >= 0)
 		{
 			int elements = this.iter.data_model.get_n_columns () * this.iter.data_model.get_n_rows ();
 			int maxpos = elements;
 			if (length > -1) {
 				maxpos = offset + length;
 				if (maxpos > elements)
 					maxpos = elements;
 			}
 			return new DataModelIterator.with_offset (this.iter.data_model, offset, maxpos);
 		}
 		
		public Gee.Iterator<Value?> filter (owned Gee.Predicate<Value?> f)
		{
			var elements = new Gee.HashMap <int,int> ();
			for (int i = 0; i < this.maxpos; i++) {
				int row = i / this.iter.data_model.get_n_columns ();
				int col = i - row * this.iter.data_model.get_n_columns ();
				Value v = this.iter.data_model.get_value_at (row, col);
				if (f (v))
					elements.set (row, col);
			}
			return new DataModelIterator.filtered_elements (this.iter.data_model, elements);
		}
		
		public new void @foreach (Gee.ForallFunc<Value?> f)
		{
			for (int i = 0; i < this.maxpos; i++) {
				int row = i / this.iter.data_model.get_n_columns ();
				int col = i - row * this.iter.data_model.get_n_columns ();
				Value v = this.iter.data_model.get_value_at (row, col);
				f(v);
			}
		}
		
		/**
		 * {@inheritDoc}
		 * 
		 * {@inheritDoc}<< BR >>
		 * << BR >>
		 * ''Implementation:'' This function returns an iterator that can iterate over YIELDED values
		 * by {@link [StreamFunc].
		 */
		public Gee.Iterator<Value?> stream<Value> (owned Gee.StreamFunc<Value?,Value?> f)
		{
			var l = new Gee.HashMap<int,int> ();
			for (int i = 0; i < this.maxpos; i++) {
				int row = i / this.iter.data_model.get_n_columns ();
				int col = i - row * this.iter.data_model.get_n_columns ();
				Value v = this.iter.data_model.get_value_at (row, col);
				var g = new Gee.Lazy<Value>.from_value (v);
				Gee.Lazy<Value> s;
				var r = f (Gee.Traversable.Stream.CONTINUE, g, out s);
				if (r == Gee.Traversable.Stream.END)
					break;
				if (r == Stream.YIELD)
					l.set (this.iter.get_row (), this._current_pos - 
													this.iter.get_row () * 
															this.iter.data_model.get_n_columns ());
			}
						
			return new DataModelIterator.filtered_elements (this.iter.data_model, l);
		}
 	}
 }
 
