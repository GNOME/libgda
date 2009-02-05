/* gda-sql-parser-private.h
 *
 * Copyright (C) 2007 Vivien Malerba
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


#ifndef __GDA_SQL_PARSER_PRIV_H_
#define __GDA_SQL_PARSER_PRIV_H_

#include <glib-object.h>
#include "gda-sql-parser.h"
#include "gda-statement-struct-pspec.h"
#include <libgda/gda-mutex.h>

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

struct _GdaSqlParserPrivate {
	GdaMutex *mutex;
 	gchar    *sql;
	GSList   *parsed_statements;

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
};

G_END_DECLS

#endif
