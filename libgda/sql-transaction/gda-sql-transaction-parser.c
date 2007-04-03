/* gda-sql-transaction-parser.c
 *
 * Copyright (C) 2006 Vivien Malerba
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

#include <stdio.h>
#include <glib.h>
#include <strings.h>
#include <string.h>

#include "gda-sql-transaction-parser.h"
#include "gda-sql-transaction-tree.h"
#include <glib/gi18n-lib.h>

extern char *trantext;
extern void tran_switch_to_buffer (void *buffer);
extern void *tran_scan_string (const char *string);
extern void tran_delete_buffer (void *buffer);

void tranerror (char *error);

GdaSqlTransaction *tran_result;
GError **tran_error;

int tranparse (void);

/**
 * tranerror:
 * 
 * Internal function for displaying error messages used by the lexer parser.
 */
void
tranerror (char *string)
{
	if (tran_error) {
		if (!strcmp (string, "parse error"))
			g_set_error (tran_error, 0, 0,
				     _("Parse error near `%s'"), trantext);
		if (!strcmp (string, "syntax error"))
			g_set_error (tran_error, 0, 0,
				     _("Syntax error near `%s'"), trantext);
	}
	else
		fprintf (stderr, "SQL Parser error: %s near `%s'\n", string,
			 trantext);
}

/**
 * gda_sql_transaction_parse_with_error:
 * @sqlquery: A SQL query string. ie SELECT * FROM FOO
 * @error: a place where to store an error, or %NULL
 * 
 * Generate in memory a structure of the @sqlquery in an easy
 * to view way.  You can also modify the returned structure and
 * regenerate the sql query using sql_stringify().  The structure
 * contains information on what type of sql statement it is, what
 * tables its getting from, what fields are selected, the where clause,
 * joins etc.
 * 
 * Returns: A generated sql_statement or %NULL on error.
 */
GdaSqlTransaction *
gda_sql_transaction_parse_with_error (const char *sqlquery, GError ** error)
{
	void *buffer;
	GError *local_error = NULL;

	if (sqlquery == NULL) {
		if (error)
			g_set_error (error, 0, 0, _("Empty query to parse"));
		else
			fprintf (stderr,
				 "SQL parse error, you can not specify NULL");
		return NULL;

	}

	tran_error = error;
	if (!tran_error)
		tran_error = &local_error;

	buffer = tran_scan_string (sqlquery);
	tran_switch_to_buffer (buffer);

	if (tranparse () == 0) {
		tran_result->full_query = g_strdup (sqlquery);
		tran_delete_buffer (buffer);
		return tran_result;
	}
	else {
		if (!error && local_error)
			g_error_free (local_error);
		tran_delete_buffer (buffer);
		return NULL;
	}
}

/**
 * gda_sql_transaction_parse:
 * @sqlquery: A SQL query string. ie SELECT * FROM FOO
 *
 * Generate in memory a structure of the @sqlquery in an easy
 * to view way.  You can also modify the returned structure and
 * regenerate the sql query using sql_stringify().  The structure
 * contains information on what type of sql statement it is, what
 * tables its getting from, what fields are selected, the where clause,
 * joins etc.
 *
 * Returns: A generated sql_statement or %NULL on error.
 */
GdaSqlTransaction *
gda_sql_transaction_parse (const char *sqlquery)
{
	return gda_sql_transaction_parse_with_error (sqlquery, NULL);
}
