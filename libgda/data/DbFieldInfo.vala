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
	public interface DbFieldInfo : Object
	{
		public abstract int                    ordinal           { get; set; }
		public abstract Type                   value_type        { get; set; }
		public abstract DbFieldInfo.Attribute  attributes        { get; set; }
		public abstract Value?                 default_value     { get; set; }
		public abstract string                 name              { get; set; }
		public abstract string                 desc              { get; set; }
		
		// Numeric and Datetime attributes
		public abstract int                    precision         { get; set; }
		public abstract int                    scale             { get; set; }
		
		// Check clause expression
//		public abstract DbSqlExpression        check_expression  { get; set; }

		// Constrains
		public abstract ForeignKey  fkey   { get; set; }

		public virtual string to_string () {
			string fk = "";
			if (fkey != null)
				fk = fkey.to_string ();
			return @"([$ordinal],[$name],[$(value_type.name ())],[%s],[$(attributes)],[%s],[$fk])"
							.printf (default_value == null ? "" : Gda.value_stringify (default_value),
							         desc == null ? "" : desc);
		}

		[Flags]
		public enum Attribute {
			NONE,
			PRIMARY_KEY,
			UNIQUE,
			FOREIGN_KEY,
			CHECK,
			HAVE_DEFAULT,
			CAN_BE_NULL,
			AUTO_INCREMENT;

			public static Attribute[] items () {
				return {
						NONE,
						PRIMARY_KEY,
						UNIQUE,
						FOREIGN_KEY,
						CHECK,
						HAVE_DEFAULT,
						CAN_BE_NULL,
						AUTO_INCREMENT
						};
			}

			public string to_string () {
				string str = "";
				if (NONE in this)
					str += "NONE ";
				if (PRIMARY_KEY in this)
					str += "PRIMARY_KEY ";
				if (UNIQUE in this)
					str += "UNIQUE ";
				if (FOREIGN_KEY in this)
					str += "FOREIGN_KEY ";
				if (CHECK in this)
					str += "CHECK ";
				if (HAVE_DEFAULT in this)
					str += "HAVE_DEFAULT ";
				if (CAN_BE_NULL in this)
					str += "CAN_BE_NULL ";
				if (AUTO_INCREMENT in this)
					str += "AUTO_INCREMENT ";
				if (str == "")
					return "NONE";
				return str;
			}

			public static Attribute from_string (string str)
			{
				if (str == "NONE")
					return NONE;
				if (str == "PRIMARY_KEY")
					return PRIMARY_KEY;
				if (str == "PRIMARY KEY")
					return PRIMARY_KEY;
				if (str == "UNIQUE")
					return UNIQUE;
				if (str == "FOREIGN_KEY")
					return FOREIGN_KEY;
				if (str == "FOREIGN KEY")
					return FOREIGN_KEY;
				if (str == "CHECK")
					return CHECK;
				if (str == "HAVE_DEFAULT")
					return HAVE_DEFAULT;
				if (str == "CAN_BE_NULL")
					return CAN_BE_NULL;
				if (str == "AUTO_INCREMENT")
					return AUTO_INCREMENT;			
				return Attribute.NONE;
			}
		}
		
		
		
		public class ForeignKey {
			public string                 name        { get; set; }
			public string                 refname     { get; set; }
			public DbTable                reftable    { get; set; }
			public ArrayList<string>      refcol      { get; set; }
			public Match                  match       { get; set; }
			public Rule                   update_rule { get; set; }
			public Rule                   delete_rule { get; set; }
			
			public ForeignKey ()
			{
				refcol = new ArrayList<string> ();
			}

			public bool equal (ForeignKey fkey)
			{
				if (fkey.name != name)
					return false;
				if (fkey.refname != refname)
					return false;
				if (fkey.reftable != reftable)
					return false;
				if (fkey.match != match)
					return false;
				if (fkey.update_rule != update_rule)
					return false;
				if (fkey.delete_rule != delete_rule)
					return false;
				foreach (string rc in fkey.refcol) {
					if (!refcol.contains (rc))
						return false;
				}
				return true;
			}

			public string to_string ()
			{
				string s = "";
				foreach (string str in refcol) {
					s += str + ",";
				}
				return @"{[$(name)],[$(refname)],[$(reftable.name)],[$s],[$(match)],[$(update_rule)],[$(delete_rule)]}";
			}

			public enum Match {
				NONE,
				FULL,
				PARTIAL;

				public static Match[] items () {
					return {
							NONE,
							FULL,
							PARTIAL
							};
				}

				public string to_string ()
				{
					switch (this) {
						case FULL:
							return "FULL";
						case PARTIAL:
							return "PARTIAL";
					}
					return "NONE";
				}

				public static Match from_string (string str)
				{
					if (str == "FULL")
						return FULL;
					if (str == "PARTIAL")
						return PARTIAL;

					return Match.NONE;
				}
			}
			public enum Rule {
				NONE,
				CASCADE,
				SET_NULL,
				SET_DEFAULT,
				RESTRICT,
				NO_ACTION;

				public static Rule[] items () {
					return {
							NONE,
							CASCADE,
							SET_NULL,
							SET_DEFAULT,
							RESTRICT,
							NO_ACTION
							};
				}

				public string to_string ()
				{
					switch (this) {
						case CASCADE:
							return "CASCADE";
						case SET_NULL:
							return "SET_NULL";
						case SET_DEFAULT:
							return "SET_DEFAULT";
						case RESTRICT:
							return "RESTRICT";
						case NO_ACTION:
							return "NO_ACTION";
					}
					return "NONE";
				}

				public static Rule from_string (string? str)
				{
					if (str == "CASCADE")
						return Rule.CASCADE;
					if (str == "SET_NULL")
						return Rule.SET_NULL;
					if (str == "SET NULL")
						return Rule.SET_NULL;
					if (str == "SET_DEFAULT")
						return Rule.SET_DEFAULT;
					if (str == "SET DEFAULT")
						return Rule.SET_DEFAULT;
					if (str == "RESTRICT")
						return Rule.RESTRICT;
					if (str == "NO_ACTION")
						return Rule.NO_ACTION;
					if (str == "NO ACTION")
						return Rule.NO_ACTION;

					return Rule.NONE;
				}
			} 
		}
	}
}
