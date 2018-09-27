/*
 * Copyright (C) 2008 - 2014 Vivien Malerba <malerba@gnome-db.org>
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


#ifndef __GDA_SQL_PARSER_PRIV_H_
#define __GDA_SQL_PARSER_PRIV_H_

#include <glib-object.h>
#include "gda-sql-parser.h"
#include "gda-statement-struct-pspec.h"

G_BEGIN_DECLS

typedef struct {
	gint      token_type;
        gchar    *next_token_start;
	gchar    *last_token_start;
	gchar     delimiter;
        gboolean  in_param_spec;
        gint      block_level;
	gboolean  ignore_semi; /* ignore any SEMI untill the next END statement where block_level==0 */

	/* Padding for future expansion */
	gpointer _gda_reserved1;
	gpointer _gda_reserved2;
} TokenizerContext;

typedef struct {
	GRecMutex  mutex;
	gchar     *sql;
	GSList    *parsed_statements;

	/* parser */
	void     *lemon_delimiter;
	void     *lemon_parser;
	GArray   *passed_tokens; /* array of token types (gint), spaces omitted, which have been given to the parser */

	/* tokenizer contexts */
	TokenizerContext *context;
	GSList           *pushed_contexts;

        /* error reporting */
	GdaSqlParserError  error_type;
        gchar             *error_msg;
        gint               error_line; /* (starts at 1) */
        gint               error_col; /* (starts at 1) */
        gint               error_pos; /* absolute count from start of message (starts at 1) */

        /* modes */
        GdaSqlParserMode     mode;
	GdaSqlParserFlavour flavour;

	/* Padding for future expansion */
	gpointer _gda_reserved1;
	gpointer _gda_reserved2;
} GdaSqlParserPrivate;

G_END_DECLS

#endif
