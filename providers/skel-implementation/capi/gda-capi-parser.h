/*
 *
 * Copyright (C) YEAR The GNOME Foundation
 *
 * AUTHORS:
 *      TO_ADD: your name and email
 *      Vivien Malerba <malerba@gnome-db.org>
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
 * write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */


#ifndef __GDA_CAPI_PARSER_H_
#define __GDA_CAPI_PARSER_H_

#include <sql-parser/gda-sql-parser.h>

G_BEGIN_DECLS

#define GDA_TYPE_CAPI_PARSER            (gda_capi_parser_get_type())
G_DECLARE_DERIVABLE_TYPE (GdaCapiParser, gda_capi_parser, GDA, CAPI_PARSER, GdaSqlParser)
/* struct for the object's class */
struct _GdaCapiParserClass
{
	GdaSqlParserClass      parent_class;
};

GType gda_capi_parser_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif
