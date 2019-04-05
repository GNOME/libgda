/*
 * Copyright (C) 2008 - 2013 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2019 Daniel Espinosa <esodan@gmail.com>
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

#include "gda-postgres-parser.h"
#include "postgres_token_types.h"
#include <string.h>

/* 
 * Main static functions 
 */
static void gda_postgres_parser_class_init (GdaPostgresParserClass *klass);
static void gda_postgres_parser_init (GdaPostgresParser *stmt);

G_DEFINE_TYPE(GdaPostgresParser, gda_postgres_parser, GDA_TYPE_SQL_PARSER)

/*
 * The interface to the LEMON-generated parser
 */
void *gda_lemon_postgres_parserAlloc (void*(*)(size_t));
void gda_lemon_postgres_parserFree (void*, void(*)(void*));
void gda_lemon_postgres_parserTrace (void*, char *);
void gda_lemon_postgres_parser (void*, int, GValue *, GdaSqlParserIface *);

static void
gda_postgres_parser_class_init (GdaPostgresParserClass * klass)
{
	GdaSqlParserClass *pclass = GDA_SQL_PARSER_CLASS (klass);

	pclass->parser_alloc = gda_lemon_postgres_parserAlloc;
	pclass->parser_free = gda_lemon_postgres_parserFree;
	pclass->parser_trace = gda_lemon_postgres_parserTrace;
	pclass->parser_parse = gda_lemon_postgres_parser;
	pclass->parser_tokens_trans = postgres_parser_tokens;
}

static void
gda_postgres_parser_init (G_GNUC_UNUSED GdaPostgresParser *parser)
{
}
