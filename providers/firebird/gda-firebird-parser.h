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


#ifndef __GDA_FIREBIRD_PARSER_H_
#define __GDA_FIREBIRD_PARSER_H_

#include <sql-parser/gda-sql-parser.h>

G_BEGIN_DECLS

#define GDA_TYPE_FIREBIRD_PARSER            (gda_firebird_parser_get_type())
#define GDA_FIREBIRD_PARSER(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_FIREBIRD_PARSER, GdaFirebirdParser))
#define GDA_FIREBIRD_PARSER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_FIREBIRD_PARSER, GdaFirebirdParserClass))
#define GDA_IS_FIREBIRD_PARSER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_FIREBIRD_PARSER))
#define GDA_IS_FIREBIRD_PARSER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_FIREBIRD_PARSER))

typedef struct _GdaFirebirdParser GdaFirebirdParser;
typedef struct _GdaFirebirdParserClass GdaFirebirdParserClass;
typedef struct _GdaFirebirdParserPrivate GdaFirebirdParserPrivate;

/* struct for the object's data */
struct _GdaFirebirdParser
{
	GdaSqlParser          object;
	GdaFirebirdParserPrivate *priv;
};

/* struct for the object's class */
struct _GdaFirebirdParserClass
{
	GdaSqlParserClass      parent_class;
};

GType gda_firebird_parser_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif
