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
	public interface SqlExpression<A,G> : GLib.Object
	{
		public abstract Gee.HashMap<A,G> fields { get; }
		public abstract Gee.HashMap<A,G> values { get; }
		
		public abstract bool verify (DbCollection db);
		
		enum DbExpressionOperator {
			AND, OR, EQ, IS, LIKE, BETWEEN,
			GT, LT, GEQ, LEQ, DIFF, REGEXP,
			REGEXP_CI, NOT_REGEXP, NOT_REGEXP_CI,
			SIMILAR, ISNULL, ISNOTNULL, NOT, IN,
			NOTIN, CONCAT, PLUS, MINUS, STAR, DIV,
			REM, BITAND, BITOR, BITNOT, ILIKE
		}
		
		struct DbExpressionRelation {
			A id;
			DbExpressionOperator value;
		}
		
	}
}
