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
 	
 	public class DataModelIterable : GLib.Object, Gee.Traversable <Value?>, Gee.Iterable <Value?>
 	{
 		private Gda.DataModel model;
 		
 		public DataModelIterable (Gda.DataModel model) {
 			this.model = model;
 		}
 		
 		// Iterable Interface
 		
 		public Type element_type { 
 			get { return typeof (GLib.Value); } 
 		}
 		
 		public Iterator<Value?> iterator ()
 		{
 			return new DataModelIterator (this.model);
 		}
 		
 		// Traversable Interface
 		
		public Gee.Iterator<Value?> chop (int offset, int length = -1)
 		{
 			var iter = new DataModelIterator (this.model);
 			return iter.chop (offset, length);
 		}
 		
		public Gee.Iterator<Value?> filter (owned Gee.Predicate<Value?> f)
		{
			var iter = new DataModelIterator (this.model);
 			return iter.filter (f);
		}
		
		public new void @foreach (Gee.ForallFunc<Value?> f)
		{
			for (int i = 0; i < this.model.get_n_rows (); i++) {
				for (int j = 0; j < this.model.get_n_columns (); j++) {
					Value v = this.model.get_value_at (i, j);
					f(v);
				}				
			}
		}
		
		public Gee.Iterator<A> stream<A> (owned Gee.StreamFunc<Value?,A> f)
		{
			var iter = new DataModelIterator (this.model);
 			return iter.stream<A> (f);
		}
 	}
 	
 	/**
 	 * Iterator that implements [@link [Gee.Iterator] and [@link [Gee.Traversable]]
 	 */
 	public class DataModelIterator : GLib.Object, Gee.Traversable <Value?>, Gee.Iterator <Value?>
 	{
 		private Gda.DataModelIter iter;
 		private int _current_pos;
 		/* Used for Traversable to skip a number of elements in this case rows and columns */
 		private int pos_init;
 		private int maxpos;
 		private Gee.HashMap <int, int> elements;
 		private bool filtered;	
 		
 		
 		public int current_column {
 			get { return this._current_pos - this.iter.get_row () * this.iter.data_model.get_n_columns (); }
 		}
 		
 		public int current_row {
 			get { return this.iter.get_row (); }
 		}
 		
 		public DataModelIterator (Gda.DataModel model)
 		{
 			this.iter = model.create_iter ();
 			this._current_pos = -1;
 			this.pos_init = 0;
 			this.maxpos = this.iter.data_model.get_n_columns () * this.iter.data_model.get_n_rows ();
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
 			
 			this.pos_init = i;
 			this._current_pos = this.pos_init - 1;
 			this.maxpos = maxpos;
 			this.filtered = false;
 			this.iter.move_to_row (this.pos_init / this.iter.data_model.get_n_columns ());
 		}
 		
 		private DataModelIterator.filtered_elements (Gda.DataModel model, Gee.HashMap <int, int> elements)
 		{
 			this.iter = model.create_iter ();
 			this._current_pos = -1;
 			this.pos_init = 0;
 			this.maxpos = this.iter.data_model.get_n_columns () * this.iter.data_model.get_n_rows ();
 			this.filtered = true;
 			this.elements = elements;
 		}
 		
 		// Iterator Interface
 		
 		public bool valid 
 		{ 
 			get { return this.iter.is_valid (); }
 		}
 		
 		public bool read_only {	get { return true; } }
 		
 		public bool next () {
 			if (!this.filtered) 
 			{
	 			if (this._current_pos == -1)
	 				this._current_pos = this.pos_init;
	 			else
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
 				bool ret = false;
 				while (this.has_next ()) {
 					if (this.elements.has_key (++this._current_pos)) {
 						int r = this.elements.get (this._current_pos);
 						ret = true;
 						this.iter.move_to_row (r);
 						break;
 					}
 				}
 				
 				if (this._current_pos > this.maxpos)
 					this.iter.invalidate_contents ();
 				return ret;
 			}
 		}
 		
 		public bool has_next () {
 			int pos = this._current_pos + 1;
			if (pos < this.maxpos && pos >= this.pos_init)
				return true;
			
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
 		public Value? @get ()
 		{
 			return this.iter.get_value_at (this._current_pos - 
 										this.iter.get_row () * 
 											this.iter.data_model.get_n_columns ());
 		}
 		
 		public void remove () {}
 		
 		/* Traversable  Interface */
 		public Gee.Iterator<Value?> chop (int offset, int length = -1) 
 			requires ( offset >= 0)
 		{
 			int maxpos = this.maxpos;
 			int pos_init = this.pos_init + offset;
 			if (length > -1) {
 				maxpos = this.pos_init + offset + length;
 				if (maxpos > this.maxpos)
 					maxpos = this.maxpos;
 			}
 			return new DataModelIterator.with_offset (this.iter.data_model, pos_init, maxpos);
 		}
 		
		public Gee.Iterator<Value?> filter (owned Gee.Predicate<Value?> f)
		{
			var elements = new Gee.HashMap <int,int> ();
			for (int i = this.pos_init; i < this.maxpos; i++) {
				int row = i / this.iter.data_model.get_n_columns ();
				int col = i - row * this.iter.data_model.get_n_columns ();
				try {
					Value v = this.iter.data_model.get_value_at (col, row);
					if (f (v))
						elements.set (i, row);
				}
				catch (Error e) {
					stdout.printf ("ERROR***DataModelIterator: %s\n", e.message);
					continue;
				}
			}
			return new DataModelIterator.filtered_elements (this.iter.data_model, elements);
		}
		
		public new void @foreach (Gee.ForallFunc<Value?> f)
		{
			for (int i = this.pos_init; i < this.maxpos; i++) {
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
		public Gee.Iterator<A> stream<A> (owned Gee.StreamFunc<Value?, A> f)
		{
			var l = new Gee.ArrayList<A> ();
			for (int i = this.pos_init; i < this.maxpos; i++) {
				int row = i / this.iter.data_model.get_n_columns ();
				int col = i - row * this.iter.data_model.get_n_columns ();
				Value v = this.iter.data_model.get_value_at (col, row);
				var g = new Gee.Lazy<Value?>.from_value (v);
				Gee.Lazy<A> s;
				var r = f (Gee.Traversable.Stream.CONTINUE, g, out s);
				if (r == Gee.Traversable.Stream.END)
					break;
				if (r == Gee.Traversable.Stream.YIELD) {
					l.add (s);
				}
			}
						
			return l.iterator<A> ();
		}
 	}
 }
 
