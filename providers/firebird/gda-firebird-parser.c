/* GDA Firebird provider
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

#include "gda-firebird-parser.h"
#include "firebird_token_types.h"
#include <string.h>

/* 
 * Main static functions 
 */
static void gda_firebird_parser_class_init (GdaFirebirdParserClass *klass);
static void gda_firebird_parser_init (GdaFirebirdParser *stmt);

GType
gda_firebird_parser_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static const GTypeInfo info = {
			sizeof (GdaFirebirdParserClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_firebird_parser_class_init,
			NULL,
			NULL,
			sizeof (GdaFirebirdParser),
			0,
			(GInstanceInitFunc) gda_firebird_parser_init
		};
		
		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GDA_TYPE_SQL_PARSER, "GdaFirebirdParser", &info, 0);
		g_static_mutex_unlock (&registering);
	}
	return type;
}

/*
 * The interface to the LEMON-generated parser
 */
void *gda_lemon_firebird_parserAlloc (void*(*)(size_t));
void gda_lemon_firebird_parserFree (void*, void(*)(void*));
void gda_lemon_firebird_parserTrace (void*, char *);
void gda_lemon_firebird_parser (void*, int, GValue *, GdaSqlParserIface *);

static void
gda_firebird_parser_class_init (GdaFirebirdParserClass * klass)
{
	GdaSqlParserClass *pclass = GDA_SQL_PARSER_CLASS (klass);

	pclass->parser_alloc = gda_lemon_firebird_parserAlloc;
	pclass->parser_free = gda_lemon_firebird_parserFree;
	pclass->parser_trace = gda_lemon_firebird_parserTrace;
	pclass->parser_parse = gda_lemon_firebird_parser;
	pclass->parser_tokens_trans = firebird_parser_tokens;
}

static void
gda_firebird_parser_init (GdaFirebirdParser *parser)
{
}
