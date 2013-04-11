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
	public interface DbField : Object
	{
		public abstract Value?             @value      { get; set; }
		public abstract string             name        { get; set; }
		public abstract string             column_name { get; }
		public abstract DbField.Attribute  attributes  { get; }

		[Flags]
		public enum Attribute {
			NONE,
			IS_NULL,
			CAN_BE_NULL,
			IS_DEFAULT,
			CAN_BE_DEFAULT,
			IS_UNCHANGED,
			ACTIONS_SHOWN,
			DATA_NON_VALID,
			HAS_VALUE_ORIG,
			NO_MODIF,
			UNUSED;

			public static Attribute[] items () {
				return {
						NONE,
						IS_NULL,
						CAN_BE_NULL,
						IS_DEFAULT,
						CAN_BE_DEFAULT,
						IS_UNCHANGED,
						ACTIONS_SHOWN,
						DATA_NON_VALID,
						HAS_VALUE_ORIG,
						NO_MODIF,
						UNUSED
						};
			}

			public string to_string () {
				switch (this) {
					case NONE:
						return "None";
					case IS_NULL:
						return "IsNull";
					case CAN_BE_NULL:
						return "CanBeNull";
					case IS_DEFAULT:
						return "IsDefault";
					case CAN_BE_DEFAULT:
						return "CanBeDefault";
					case IS_UNCHANGED:
						return "IsUnchanged";
					case ACTIONS_SHOWN:
						return "ActionsShown";
					case DATA_NON_VALID:
						return "DataNonValid";
					case HAS_VALUE_ORIG:
						return "HasValueOrig";
					case NO_MODIF:
						return "NoModif";
					case UNUSED:
						return "CanBeNull";
					default:
						assert_not_reached();
				}
			}
		}

		public virtual bool equal (DbField field) {
			if (field.name != name)
				return false;
			if (field.column_name != column_name)
				return false;
			var attributes = Attribute.items ();
			foreach (Attribute att in attributes) {
				if (att in field.attributes && !(att in attributes))
					return false;
			}
			return true;
		}

		public static Value? value_from_string (string as_string, Type type)
		{
			// FIXME: No all basic types have support to parse from string se bug 669278 
			
			if (type == typeof (bool)) {
				if ((as_string[0] == 't') || (as_string[0] == 'T') || as_string == "true")
					return true;
				if ((as_string[0] == 'f') || (as_string[0] == 'F') || as_string == "false")
					return false;
				int i =  int.parse(as_string);
				return i==0 ? false : true;
			}
			if (type == typeof (int64)) 
				return int64.parse (as_string);
			if (type == typeof (uint64))
				return uint64.parse (as_string);
			if (type == typeof (int))
				return int.parse (as_string);
//			if (type == typeof (short))
//				return short.parse (as_string);
//			if (type == typeof (ushort))
//				return ushort.parse (as_string);
			if (type == typeof (long))
				return long.parse (as_string);
//			if (type == typeof (float))
//				return float.parse (as_string);
			if (type == typeof (double))
				return double.parse (as_string);
			if (type == typeof (Gda.Numeric))
			{
				var n = new Gda.Numeric ();
				n.set_from_string (as_string);
				return n;
			}
			
			return as_string;
		}

		public virtual string to_string () {
			return @"([$name],[$column_name],[%s],[$(attributes)])".printf (Gda.value_stringify (value));
		}
	}
}
