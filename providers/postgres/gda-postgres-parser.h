/* gda-postgres-parser.h
 *
 * Copyright (C) 2007 Vivien Malerba
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#ifndef __GDA_POSTGRES_PARSER_H_
#define __GDA_POSTGRES_PARSER_H_

#include <sql-parser/gda-sql-parser.h>

G_BEGIN_DECLS

#define GDA_TYPE_POSTGRES_PARSER          (gda_postgres_parser_get_type())
#define GDA_POSTGRES_PARSER(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_postgres_parser_get_type(), GdaPostgresParser)
#define GDA_POSTGRES_PARSER_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_postgres_parser_get_type (), GdaPostgresParserClass)
#define GDA_IS_POSTGRES_PARSER(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_postgres_parser_get_type ())

typedef struct _GdaPostgresParser GdaPostgresParser;
typedef struct _GdaPostgresParserClass GdaPostgresParserClass;
typedef struct _GdaPostgresParserPrivate GdaPostgresParserPrivate;

/* struct for the object's data */
struct _GdaPostgresParser
{
	GdaSqlParser              object;
	GdaPostgresParserPrivate *priv;
};

/* struct for the object's class */
struct _GdaPostgresParserClass
{
	GdaSqlParserClass         parent_class;
};

GType               gda_postgres_parser_get_type               (void) G_GNUC_CONST;

G_END_DECLS

#endif
