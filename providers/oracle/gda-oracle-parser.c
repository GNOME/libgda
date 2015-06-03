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

#include "gda-oracle-parser.h"
#include "oracle_token_types.h"
#include <string.h>

/* 
 * Main static functions 
 */
static void gda_oracle_parser_class_init (GdaOracleParserClass *klass);
static void gda_oracle_parser_init (GdaOracleParser *stmt);

GType
gda_oracle_parser_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GMutex registering;
		static const GTypeInfo info = {
			sizeof (GdaOracleParserClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_oracle_parser_class_init,
			NULL,
			NULL,
			sizeof (GdaOracleParser),
			0,
			(GInstanceInitFunc) gda_oracle_parser_init
		};
		
		g_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GDA_TYPE_SQL_PARSER, "GdaOracleParser", &info, 0);
		g_mutex_unlock (&registering);
	}
	return type;
}

/*
 * The interface to the LEMON-generated parser
 */
void *gda_lemon_oracle_parserAlloc (void*(*)(size_t));
void gda_lemon_oracle_parserFree (void*, void(*)(void*));
void gda_lemon_oracle_parserTrace (void*, char *);
void gda_lemon_oracle_parser (void*, int, GValue *, GdaSqlParserIface *);

static void
gda_oracle_parser_class_init (GdaOracleParserClass * klass)
{
	GdaSqlParserClass *pclass = GDA_SQL_PARSER_CLASS (klass);

	pclass->parser_alloc = gda_lemon_oracle_parserAlloc;
	pclass->parser_free = gda_lemon_oracle_parserFree;
	pclass->parser_trace = gda_lemon_oracle_parserTrace;
	pclass->parser_parse = gda_lemon_oracle_parser;
	pclass->parser_tokens_trans = oracle_parser_tokens;
}

static void
gda_oracle_parser_init (GdaOracleParser *parser)
{
}
