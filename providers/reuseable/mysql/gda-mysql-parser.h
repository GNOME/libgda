/* GDA Mysql provider
 *
 * Copyright (C) 2008 The GNOME Foundation
 *
 * AUTHORS:
 *      TO_ADD: your name and email
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


#ifndef __GDA_MYSQL_PARSER_H_
#define __GDA_MYSQL_PARSER_H_

#include <sql-parser/gda-sql-parser.h>

G_BEGIN_DECLS

#define GDA_TYPE_MYSQL_PARSER            (gda_mysql_parser_get_type())
#define GDA_MYSQL_PARSER(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_MYSQL_PARSER, GdaMysqlParser))
#define GDA_MYSQL_PARSER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_MYSQL_PARSER, GdaMysqlParserClass))
#define GDA_IS_MYSQL_PARSER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_MYSQL_PARSER))
#define GDA_IS_MYSQL_PARSER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_MYSQL_PARSER))

typedef struct _GdaMysqlParser GdaMysqlParser;
typedef struct _GdaMysqlParserClass GdaMysqlParserClass;
typedef struct _GdaMysqlParserPrivate GdaMysqlParserPrivate;

/* struct for the object's data */
struct _GdaMysqlParser
{
	GdaSqlParser          object;
	GdaMysqlParserPrivate *priv;
};

/* struct for the object's class */
struct _GdaMysqlParserClass
{
	GdaSqlParserClass      parent_class;
};

GType gda_mysql_parser_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif
