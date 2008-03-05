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

#include "gda-capi-parser.h"
#include "capi_token_types.h"
#include <string.h>

/* 
 * Main static functions 
 */
static void gda_capi_parser_class_init (GdaCapiParserClass *klass);
static void gda_capi_parser_init (GdaCapiParser *stmt);

GType
gda_capi_parser_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdaCapiParserClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_capi_parser_class_init,
			NULL,
			NULL,
			sizeof (GdaCapiParser),
			0,
			(GInstanceInitFunc) gda_capi_parser_init
		};
		
		type = g_type_register_static (GDA_TYPE_SQL_PARSER, "GdaCapiParser", &info, 0);
	}
	return type;
}

/*
 * The interface to the LEMON-generated parser
 */
void *gda_lemon_capi_parserAlloc (void*(*)(size_t));
void gda_lemon_capi_parserFree (void*, void(*)(void*));
void gda_lemon_capi_parserTrace (void*, char *);
void gda_lemon_capi_parser (void*, int, GValue *, GdaSqlParserIface *);

static void
gda_capi_parser_class_init (GdaCapiParserClass * klass)
{
	GdaSqlParserClass *pclass = GDA_SQL_PARSER_CLASS (klass);

	pclass->parser_alloc = gda_lemon_capi_parserAlloc;
	pclass->parser_free = gda_lemon_capi_parserFree;
	pclass->parser_trace = gda_lemon_capi_parserTrace;
	pclass->parser_parse = gda_lemon_capi_parser;
	pclass->parser_tokens_trans = capi_parser_tokens;
}

static void
gda_capi_parser_init (GdaCapiParser *parser)
{
}
