/* -*- Mode: Vala; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * libgdavala
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

using Gda;
using Gee;

namespace GdaData {

    public abstract class Object<G> : GLib.Object {
        
        protected HashMap<string,Field<G>> _model;
        /**
         * Derived classes must implement this property to set the table used to get data from.
         */
        public abstract string table { get; }
        /**
         * Returns a Gda.DataModel with the data stored by this object.
         */
        public Collection<Field<G>> fields { owned get { return _model.values; } }
        /**
         * Set the connection to be used to get/set data.
         */
        public Connection connection { get; set; }
        /**
         * Returns a GLib.Value containing the value stored in the given field.
         */
        public G get_value (string field)
        	throws Error
        {
        	var f = this._model.get (field);
        	return f.value;
        }
		/**
         * Set the value to a field with the given @name.
         */
        public void set_value (string field, G v)
        	throws Error
        {
        	var f = _model.get (field);
        	f.value = v;
        	this._model.set (field, f);
        }
        /**
         * Saves any modficiation made to in memory representation of the data directly to
         * the database.
         */
        public abstract void save () throws Error;
        /**
         * Updates values stored in database.
         */
        public abstract void update () throws Error;
        /**
         * Append a new row to the defined table and returns its ID. If defaults is set to true,
         * default value for each field is set.
         */
        public abstract void append (out G id) throws Error;
        
        public abstract string to_string ();
    }
}
