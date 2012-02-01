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

namespace GdaData {
	public interface DbFieldInfo : Object
	{
		public abstract DbFieldInfo.Attribute  attributes { get; set; }
		public abstract Value?                 default_value { get; set; }
		public abstract string                 name { get; set; }
		public abstract string                 desc { get; set; }
		
		// Numeric and Datetime attributes
		public abstract int                    precision   { get; set; }
		public abstract int                    scale       { get; set; }
		
		public static Attribute attribute_from_string (string str)
		{
			if (str == "NONE")
				return Attribute.NONE;
			if (str == "PRIMARY_KEY")
				return Attribute.PRIMARY_KEY;
			if (str == "UNIQUE")
				return Attribute.UNIQUE;
			if (str == "FOREIGN_KEY")
				return Attribute.FOREIGN_KEY;
			if (str == "CHECK")
				return Attribute.CHECK;
			if (str == "HAVE_DEFAULT")
				return Attribute.HAVE_DEFAULT;
			if (str == "IS_AUTO_INCREMENT")
				return Attribute.IS_AUTO_INCREMENT;
			
			return Attribute.NONE;
		}
		public enum Attribute {
			NONE,
			PRIMARY_KEY,
			UNIQUE,
			FOREIGN_KEY,
			CHECK,
			HAVE_DEFAULT,
			IS_AUTO_INCREMENT
		}
		
		// Constrains
		public abstract ForeignKey  fkey   { get; set; }
		
		[Compact]
		public class ForeignKey {
			public string    name;
			public string    refname;
			public DbTable   reftable;
			public Match     match;
			public Rule      update_rule;
			public Rule      delete_rule;
			
			public ForeignKey copy () { return new ForeignKey (); }
			
			public static Match match_from_string (string str)
			{
				if (str == "FULL")
					return Match.FULL;
				if (str == "PARTIAL")
					return Match.PARTIAL;
				
				return Match.NONE;
			}
			
			public static Rule rule_from_string (string str)
			{
				if (str == "CASCADE")
					return Rule.CASCADE;
				if (str == "SET NULL")
					return Rule.SET_NULL;
				if (str == "SET DEFAULT")
					return Rule.SET_DEFAULT;
				if (str == "RESTRICT")
					return Rule.RESTRICT;
				if (str == "NO ACTION")
					return Rule.NO_ACTION;
	
				return Rule.NONE;
			}
			public enum Match {
				FULL,
				PARTIAL,
				NONE
			}
			public enum Rule {
				CASCADE,
				SET_NULL,
				SET_DEFAULT,
				RESTRICT,
				NO_ACTION,
				NONE
			} 
		}
	}
}
