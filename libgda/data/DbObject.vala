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
	public interface DbObject : Object 
	{
		public abstract Connection   connection { get; set; }
		public abstract void         append () throws Error;
		public abstract void         update () throws Error;
		public abstract void         save () throws Error;
	}
	
	public interface DbNamedObject : Object, DbObject 
	{
		public abstract string       name { get; set; }
	}
	
	public errordomain DbObjectError {
    	APPEND,
    	UPDATE,
    	SAVE
    }

}
