/*
 * Copyright (C) 2008 - 2012 Vivien Malerba <malerba@gnome-db.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */


#ifndef __GDA_ORACLE_PARSER_H_
#define __GDA_ORACLE_PARSER_H_

#include <sql-parser/gda-sql-parser.h>

G_BEGIN_DECLS

#define GDA_TYPE_ORACLE_PARSER            (gda_oracle_parser_get_type())
#define GDA_ORACLE_PARSER(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_ORACLE_PARSER, GdaOracleParser))
#define GDA_ORACLE_PARSER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_ORACLE_PARSER, GdaOracleParserClass))
#define GDA_IS_ORACLE_PARSER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_ORACLE_PARSER))
#define GDA_IS_ORACLE_PARSER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_ORACLE_PARSER))

typedef struct _GdaOracleParser GdaOracleParser;
typedef struct _GdaOracleParserClass GdaOracleParserClass;
typedef struct _GdaOracleParserPrivate GdaOracleParserPrivate;

/* struct for the object's data */
struct _GdaOracleParser
{
	GdaSqlParser          object;
	GdaOracleParserPrivate *priv;
};

/* struct for the object's class */
struct _GdaOracleParserClass
{
	GdaSqlParserClass      parent_class;
};

GType gda_oracle_parser_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif
