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
	public interface DbRecord : DbObject, Comparable<DbRecord>
	{
		public abstract DbTable                   table  { get; set construct; }
		public abstract Collection<DbField>       fields { owned get; }
        public abstract Collection<DbField>       keys   { owned get; }
        /**
         * Returns a GLib.Value containing the value stored in the given field.
         */
        public abstract Value?                    get_value        (string name) throws Error;
        public abstract void                      set_field        (DbField field) throws Error;
        public abstract void                      set_field_value  (string field, Value? val) throws Error;
        public abstract DbField                   get_field        (string name) throws Error;
        public abstract void                      set_key          (DbField field) throws Error;
        public abstract void                      set_key_value    (string field, Value? val) throws Error;
        public abstract DbField                   get_key          (string name) throws Error;
        public abstract string                    to_string        ();
	}
}
