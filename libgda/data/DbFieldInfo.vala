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
		
		public static Attribute attribute_from_string (string str)
		{
			if (str == "NONE")
				return Attribute.NONE;
			if (str == "PRIMARY_KEY")
				return Attribute.PRIMARY_KEY;
			if (str == "PRIMARY KEY")
				return Attribute.PRIMARY_KEY;
			if (str == "UNIQUE")
				return Attribute.UNIQUE;
			if (str == "FOREIGN_KEY")
				return Attribute.FOREIGN_KEY;
			if (str == "FOREIGN KEY")
				return Attribute.FOREIGN_KEY;
			if (str == "CHECK")
				return Attribute.CHECK;
			if (str == "HAVE_DEFAULT")
				return Attribute.HAVE_DEFAULT;
			if (str == "CAN_BE_NULL")
				return Attribute.CAN_BE_NULL;
			if (str == "AUTO_INCREMENT")
				return Attribute.AUTO_INCREMENT;
			
			return Attribute.NONE;
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
			AUTO_INCREMENT
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
			
			public static Match match_from_string (string str)
			{
				if (str == "FULL")
					return Match.FULL;
				if (str == "PARTIAL")
					return Match.PARTIAL;
				
				return Match.NONE;
			}
			
			public static Rule rule_from_string (string? str)
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
			
			public static string rule_to_string (Rule r)
			{
				if (r == Rule.CASCADE)
					return "CASCADE";
				if (r == Rule.SET_NULL)
					return "SET NULL";
				if (r == Rule.SET_DEFAULT)
					return "SET DEFAULT";
				if (r == Rule.RESTRICT)
					return "RESTRICT";
				if (r == Rule.NO_ACTION)
					return "NO ACTION";
	
				return "NONE";
			}
			
			public enum Match {
				FULL,
				PARTIAL,
				NONE
			}
			public enum Rule {
				NONE,
				CASCADE,
				SET_NULL,
				SET_DEFAULT,
				RESTRICT,
				NO_ACTION
			} 
		}
	}
}
