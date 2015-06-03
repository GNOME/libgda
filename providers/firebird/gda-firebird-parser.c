/*
 * Copyright (C) 2008 - 2013 Vivien Malerba <malerba@gnome-db.org>
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
		static GMutex registering;
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
		
		g_mutex_lock (&registering);
		if (type == 0) {
#ifdef FIREBIRD_EMBED
			type = g_type_register_static (GDA_TYPE_SQL_PARSER, "GdaFirebirdParserEmbed", &info, 0);
#else
			type = g_type_register_static (GDA_TYPE_SQL_PARSER, "GdaFirebirdParser", &info, 0);
#endif
		}
		g_mutex_unlock (&registering);
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
