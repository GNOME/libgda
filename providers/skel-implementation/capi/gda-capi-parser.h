/* GDA Capi provider
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


#ifndef __GDA_CAPI_PARSER_H_
#define __GDA_CAPI_PARSER_H_

#include <sql-parser/gda-sql-parser.h>

G_BEGIN_DECLS

#define GDA_TYPE_CAPI_PARSER          (gda_capi_parser_get_type())
#define GDA_CAPI_PARSER(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_capi_parser_get_type(), GdaCapiParser)
#define GDA_CAPI_PARSER_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_capi_parser_get_type (), GdaCapiParserClass)
#define GDA_IS_CAPI_PARSER(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_capi_parser_get_type ())

typedef struct _GdaCapiParser GdaCapiParser;
typedef struct _GdaCapiParserClass GdaCapiParserClass;
typedef struct _GdaCapiParserPrivate GdaCapiParserPrivate;

/* struct for the object's data */
struct _GdaCapiParser
{
	GdaSqlParser          object;
	GdaCapiParserPrivate *priv;
};

/* struct for the object's class */
struct _GdaCapiParserClass
{
	GdaSqlParserClass      parent_class;
};

GType gda_capi_parser_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif
