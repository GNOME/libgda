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
	public interface DbTable : DbObject, DbNamedObject
	{
		public abstract DbCatalog                        catalog    { get; set; }
		public abstract DbSchema                         schema     { get; set; }
		public abstract TableType                        table_type { get; set; }
		public abstract Collection<DbRecord>             records    { owned get; }
		public abstract Collection<DbTable>              fk_depends { owned get; }
		public abstract Collection<DbTable>              fk         { owned get; }
		public abstract HashMap<string,DbFieldInfo>      fields     { get; }
//		public abstract Iterator<DbFieldInfo>            pk_fields  { get; } // FIXME: Implement
		
		public enum TableType {
			NONE,
			BASE_TABLE,
			VIEW, 
			LOCAL_TEMPORARY,
			SYSTEM_TABLE,
			GLOBAL_TEMPORARY,
			ALIAS,
			SYNONYM
		}
		
		public static TableType type_from_string (string str)
		{
			if (str == "BASE TABLE")
				return TableType.BASE_TABLE;
			if (str == "VIEW")
				return TableType.VIEW;
			if (str == "LOCAL TEMPORARY")
				return TableType.LOCAL_TEMPORARY;
			if (str == "SYSTEM TABLE")
				return TableType.SYSTEM_TABLE;
			if (str == "GLOBAL TEMPORARY")
				return TableType.GLOBAL_TEMPORARY;
			if (str == "ALIAS")
				return TableType.ALIAS;
			if (str == "SYNONYM")
				return TableType.SYNONYM;
			
			return TableType.NONE;
		}
	}
}
