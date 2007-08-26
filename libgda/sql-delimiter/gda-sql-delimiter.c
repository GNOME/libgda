/* gda-sql-delimiter.c
 *
 * Copyright (C) 2004 - 2006 Vivien Malerba
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
#include <glib/gi18n-lib.h>
#include <strings.h>
#include <string.h>

#include "gda-sql-delimiter.h"
#include "gda-delimiter-tree.h"

extern char *gda_delimitertext;
extern int gda_delimiterdebug;
extern void gda_delimiter_switch_to_buffer (void *buffer);
extern void *gda_delimiter_scan_string (const char *string);
extern void gda_delimiter_delete_buffer (void *buffer);

void gda_delimitererror (char *error);

GdaDelimiterStatement  *last_sql_result;
GList                  *all_sql_results;
GError                **gda_sql_error;
static gboolean         error_forced;

int gda_delimiterparse (void);

static void sql_destroy_expr (GdaDelimiterExpr *expr);
static void sql_destroy_param_spec (GdaDelimiterParamSpec *pspec);


/**
 * sqlerror:
 * 
 * Internal function for displaying error messages used by the lexer parser.
 */
void
gda_delimitererror (char *string)
{
	if (gda_sql_error) {
		if (!*gda_sql_error) {
			if (!strcmp (string, "parse error"))
				g_set_error (gda_sql_error, 0, 0, _("Parse error near `%s'"), gda_delimitertext);
			else if (!strcmp (string, "syntax error"))
				g_set_error (gda_sql_error, 0, 0, _("Syntax error near `%s'"), gda_delimitertext);
			else g_set_error (gda_sql_error, 0, 0, string);
		}
	}
	else
		fprintf (stderr, "SQL Parser error: %s near `%s'\n", string, gda_delimitertext);
	error_forced = TRUE;
}



static void
sql_destroy_param_spec (GdaDelimiterParamSpec *pspec)
{
	if (!pspec)
		return;

	g_free (pspec->content);
	g_free (pspec);
}

static void
sql_destroy_expr (GdaDelimiterExpr *expr)
{
	if (expr->sql_text)
		g_free (expr->sql_text);
	if (expr->pspec_list) {
		GList *pspecs = expr->pspec_list;
		while (pspecs) {
			sql_destroy_param_spec ((GdaDelimiterParamSpec *)(pspecs->data));
			pspecs = g_list_next (pspecs);
		}
		g_list_free (expr->pspec_list);
	}

	g_free (expr);
}

static void
sql_destroy_statement (GdaDelimiterStatement *statement)
{
	GList *list;
	list = statement->expr_list;
	while (list) {
		sql_destroy_expr ((GdaDelimiterExpr *)(list->data));
		list = g_list_next (list);
	}
	g_list_free (statement->expr_list);
	g_list_free (statement->params_specs);
	g_free (statement);
}


/**
 * gda_delimiter_destroy:
 * @statement: Sql statement
 * 
 * Free up a GdaDelimiterStatement.
 */
int
gda_delimiter_destroy (GdaDelimiterStatement *statement)
{
	if (!statement)
		return 0;

	sql_destroy_statement (statement);
	return 0;
}

/**
 * gda_delimiter_parse_with_error:
 * @sqlquery: A SQL query string. ie SELECT * FROM FOO
 * @error: a place where to store an error, or %NULL
 * 
 * Generates a list of #GdaDelimiterStatement which is an in memory a structure of the 
 * @sqlquery in an easy to use way. It basically makes chuncks of string and
 * identifies required parameters. There is one #GdaDelimiterStatement for every
 * chunck of string separated by a semicolon.
 * 
 * Returns: A list of generated GdaDelimiterStatement or %NULL on error.
 */
GList *
gda_delimiter_parse_with_error (const char *sqlquery, GError ** error)
{
	void *buffer;

	gda_delimiter_lex_reset ();
	gda_delimiterdebug = 0; /* parser debug active or not */
	last_sql_result = NULL;
	all_sql_results = NULL;
	error_forced = FALSE;
	if (!sqlquery) {
		if (error)
			g_set_error (error, 0, 0, _("Empty query to parse"));
		return NULL;
	}
	
	gda_sql_error = error;
	buffer = gda_delimiter_scan_string (sqlquery);
	gda_delimiter_switch_to_buffer (buffer);
	if (gda_delimiterparse () || error_forced) {
		g_list_foreach (all_sql_results, (GFunc) sql_destroy_statement, NULL);
		g_list_free (all_sql_results);
		all_sql_results = NULL;
		last_sql_result = NULL;
		error_forced = FALSE;
	}
	else {
		GList *list;

		list = all_sql_results;
		while (list) {
			GdaDelimiterStatement *stm = (GdaDelimiterStatement *) list->data;

			if (stm->expr_list) {
				GdaDelimiterExpr *expr;
				
				expr = (GdaDelimiterExpr *) stm->expr_list->data;
				if (expr->sql_text) {
					if (! g_ascii_strcasecmp (expr->sql_text, "select"))
						stm->type = GDA_DELIMITER_SQL_SELECT;
					else if (! g_ascii_strcasecmp (expr->sql_text, "update"))
						stm->type = GDA_DELIMITER_SQL_UPDATE;
					else if (! g_ascii_strcasecmp (expr->sql_text, "insert"))
						stm->type = GDA_DELIMITER_SQL_INSERT;
					else if (! g_ascii_strcasecmp (expr->sql_text, "delete"))
						stm->type = GDA_DELIMITER_SQL_DELETE;
					else if (! g_ascii_strcasecmp (expr->sql_text, "begin"))
						stm->type = GDA_DELIMITER_SQL_BEGIN;
					else if (! g_ascii_strcasecmp (expr->sql_text, "commit"))
						stm->type = GDA_DELIMITER_SQL_COMMIT;
				}
			}
			list = g_list_next (list);
		}
	}
	gda_delimiter_delete_buffer (buffer);

	return all_sql_results;
}

/**
 * gda_delimiter_parse:
 * @sqlquery: A SQL query string. ie SELECT * FROM FOO
 *
 * Generate #GdaDelimiterStatement which is an in memory a structure of the 
 * @sqlquery in an easy to use way. It basically makes chuncks of string and
 * identifies required parameters.
 *
 * Returns: A generated GdaDelimiterStatement or %NULL on error.
 */
GList *
gda_delimiter_parse (const char *sqlquery)
{
	return gda_delimiter_parse_with_error (sqlquery, NULL);
}

/**
 * gda_delimiter_split_sql
 * @sql_text:
 *
 * Splits @sql_text into a NULL-terminated array of SQL statements, like the g_strsplit()
 * function.
 *
 * As a side note, this function returns %NULL if @sql_text is %NULL or if no statement was found.
 *
 * Returns: a newly-allocated NULL-terminated array of strings. Use g_strfreev() to free it.
 */
gchar **
gda_delimiter_split_sql (const char *sql_text)
{
	GList *statements;
	gchar **array = NULL;
	GError *error = NULL;

	if (!sql_text)
		return NULL;

	/*g_print ("%s(): %s#\n", __FUNCTION__, sql_text);*/
	statements = gda_delimiter_parse_with_error (sql_text, &error);

	if (statements) {
		/* no error */
		GList *list;
		gint i, size;

		size = g_list_length (statements);
		array = g_new0 (gchar *, size + 1);

		list = statements;
		i = 0;
		while (list) {
			array [i] = gda_delimiter_to_string ((GdaDelimiterStatement *) list->data);
			list = g_list_next (list);
			i++;
		}
		gda_delimiter_free_list (statements);
	}
	else
		/* an error occured which is caused by an empty statement */
		array = NULL;

	if (error)
		g_error_free (error);

	return array;
}

/**
 * gda_delimiter_free_list
 * @statements: a list of #GdaDelimiterStatement structures
 *
 * Destroys all the #GdaDelimiterStatement structures in @statements, and
 * frees the @statements list
 */
void
gda_delimiter_free_list (GList *statements)
{
	if (!statements)
		return;

	g_list_foreach (statements, (GFunc) sql_destroy_statement, NULL);
	g_list_free (statements);
}

/**
 * gda_delimiter_concat_list
 * @statements: a list of #GdaDelimiterStatement structures
 *
 * Creates one #GdaDelimiterStatement from all the statements in @statements
 *
 * Returns: a new #GdaDelimiterStatement or %NULL if @statements is %NULL
 */
GdaDelimiterStatement *
gda_delimiter_concat_list (GList *statements)
{
	GdaDelimiterStatement *retval, *stm, *copy;
	GList *list = statements;

	if (!statements)
		return NULL;

	retval = g_new0 (GdaDelimiterStatement, 1);
	while (list) {
		stm = (GdaDelimiterStatement *) list->data;
		if (list != statements) {
			GdaDelimiterExpr *expr;
			expr = gda_delimiter_expr_build (g_strdup (";"), NULL);
			retval->expr_list = g_list_append (retval->expr_list, expr);

			retval->type = GDA_DELIMITER_MULTIPLE;
		}
		else
			retval->type = stm->type;
			
		copy = gda_delimiter_parse_copy_statement (stm, NULL);
		retval->expr_list = g_list_concat (retval->expr_list, copy->expr_list);
		copy->expr_list = NULL;
		retval->params_specs = g_list_concat (retval->params_specs, copy->params_specs);
		copy->params_specs = NULL;
		gda_delimiter_destroy (copy);

		list = g_list_next (list);
	}

	return retval;
}

/*
 * To string
 */
gchar *gda_delimiter_to_string_real (GdaDelimiterStatement *statement, gboolean sep);

/**
 * gda_delimiter_to_string
 * @statement: a #GdaDelimiterStatement
 *
 * Converts a statement to a string
 */
gchar *
gda_delimiter_to_string (GdaDelimiterStatement *statement)
{
	return gda_delimiter_to_string_real (statement, FALSE);
}

static gchar * sql_to_string_expr (GdaDelimiterExpr *expr, gboolean sep);
static gchar * sql_to_string_pspec_list (GList *pspecs, gboolean sep);
gchar *
gda_delimiter_to_string_real (GdaDelimiterStatement *statement, gboolean sep)
{
	GList *list;
	GString *string;
	gchar *tmp;

	if (!statement)
		return NULL;

	string = g_string_new ("");

	if (sep) {
		switch (statement->type) {
		case GDA_DELIMITER_SQL_SELECT:
			g_string_append (string, "Select statement:\n");
			break;
		case GDA_DELIMITER_SQL_INSERT:
			g_string_append (string, "Insert statement:\n");
			break;
		case GDA_DELIMITER_SQL_UPDATE:
			g_string_append (string, "Update statement:\n");
			break;
		case GDA_DELIMITER_SQL_DELETE:
			g_string_append (string, "Delete statement:\n");
			break;
		default:
			g_string_append (string, "Unknown statement:\n");
			break;
		}
	}

	if (sep)
		g_string_append (string, "Parsed SQL:\n");
	list = statement->expr_list;
	while (list) {
		tmp = sql_to_string_expr ((GdaDelimiterExpr *)(list->data), sep);
		g_string_append (string, tmp);
		g_free (tmp);

		list = g_list_next (list);
	}

	if (sep) {
		if (!statement->params_specs)
			g_string_append (string, "No parsed parameter\n");
		else {
			g_string_append (string, "Parsed parameters:\n");
			list = statement->params_specs;
			while (list) {
				tmp = sql_to_string_pspec_list ((GList *)(list->data), sep);
				g_string_append (string, "## ");
				g_string_append (string, tmp);
				g_free (tmp);
				
				list = g_list_next (list);
			}
		}
	}

	tmp = string->str;
	g_string_free (string, FALSE);
	return tmp;
}

static gchar *
sql_to_string_expr (GdaDelimiterExpr *expr, gboolean sep)
{
	GString *string;
	gchar *tmp;

	gboolean addnl = FALSE;
	string = g_string_new ("");
	if (expr->sql_text) {
		if (sep) {
			tmp = g_strdup_printf ("\t%s", expr->sql_text);
			addnl = TRUE;
			g_string_append (string, tmp);
			g_free (tmp);
		}
		else {
			if (expr->pspec_list)
				g_string_append_c (string, ' ');
			g_string_append (string, expr->sql_text);
		}
	}
	if (expr->pspec_list) {
		addnl = FALSE;
		tmp = sql_to_string_pspec_list (expr->pspec_list, sep);
		if (! expr->sql_text)
			g_string_append (string, " ##");
		g_string_append (string, tmp);
		g_free (tmp);
	}
	if (addnl)
		g_string_append_c (string, '\n');

	tmp = string->str;
	g_string_free (string, FALSE);
	return tmp;
}

static gchar *
sql_to_string_pspec_list (GList *pspecs, gboolean sep)
{
	GList *list;
	GString *string;
	gchar *tmp;

	string = g_string_new ("");

	list = pspecs;
	if (sep)
		g_string_append_c (string, '\t');
	g_string_append (string, " /*");
	while (list) {
		GdaDelimiterParamSpec *pspec = (GdaDelimiterParamSpec *)(list->data);

		if (pspec->type != GDA_DELIMITER_PARAM_DEFAULT)
			g_string_append_c (string, ' ');

		switch (pspec->type) {
		case GDA_DELIMITER_PARAM_NAME:
			g_string_append_printf (string, "name:\"%s\"", pspec->content);
			break;
		case GDA_DELIMITER_PARAM_DESCR:
			g_string_append_printf (string, "descr:\"%s\"", pspec->content);
			break;
		case GDA_DELIMITER_PARAM_TYPE:
			g_string_append_printf (string, "type:\"%s\"", pspec->content);
			break;
		case GDA_DELIMITER_PARAM_ISPARAM:
			g_string_append_printf (string, "isparam:\"%s\"", pspec->content);
			break;
		case GDA_DELIMITER_PARAM_NULLOK:
			g_string_append_printf (string, "nullok:\"%s\"", pspec->content);
			break;
		case GDA_DELIMITER_PARAM_DEFAULT:
			/* don't print anything */
			break;
		default:
			g_string_append_printf (string, ":?? =\"%s\"", pspec->content);
			break;
		}
		list = g_list_next (list);
	}
	g_string_append (string, " */");
	if (sep)
		g_string_append_c (string, '\n');

	tmp = string->str;
	g_string_free (string, FALSE);
	return tmp;
}

/**
 * gda_delimiter_display
 * @statement:
 */
void
gda_delimiter_display (GdaDelimiterStatement *statement)
{
	gchar *str;

	str = gda_delimiter_to_string_real (statement, TRUE);
	g_print ("%s\n", str);
	g_free (str);
}

/*
 * Copy
 */

static GdaDelimiterExpr *
copy_expr (GdaDelimiterExpr *orig, GHashTable *repl)
{
	GList *list;
	GdaDelimiterExpr *expr = g_new0 (GdaDelimiterExpr, 1);

	list = orig->pspec_list;
	while (list) {
		GdaDelimiterParamSpec *pspec = g_new0 (GdaDelimiterParamSpec, 1);
		pspec->type = ((GdaDelimiterParamSpec *)(list->data))->type;
		pspec->content = g_strdup (((GdaDelimiterParamSpec *)(list->data))->content);
		expr->pspec_list = g_list_prepend (expr->pspec_list, pspec);
		list = g_list_next (list);
	}
	if (expr->pspec_list) {
		expr->pspec_list = g_list_reverse (expr->pspec_list);
		if (repl)
			g_hash_table_insert (repl, orig->pspec_list, expr->pspec_list);
	}
	if (orig->sql_text)
		expr->sql_text = g_strdup (orig->sql_text);

	return expr;
}


/**
 * gda_delimiter_parse_copy_statement
 *
 * makes a copy of @statement
 */
GdaDelimiterStatement *
gda_delimiter_parse_copy_statement (GdaDelimiterStatement *statement, GHashTable *repl)
{
	GdaDelimiterStatement *stm;
	GList *list;

	if (!statement)
		return NULL;

	stm = g_new0 (GdaDelimiterStatement, 1);
	stm->type = statement->type;
	list = statement->expr_list;
	while (list) {
		GdaDelimiterExpr *tmp = copy_expr ((GdaDelimiterExpr *)(list->data), repl);
		stm->expr_list = g_list_prepend (stm->expr_list, tmp);

		if (repl)
			g_hash_table_insert (repl, list->data, tmp);
		
		list = g_list_next (list);
	}
	stm->expr_list = g_list_reverse (stm->expr_list);

	/* make a list of the para specs */
	list = stm->expr_list;
	while (list) {
		if (((GdaDelimiterExpr *)(list->data))->pspec_list)
			stm->params_specs = g_list_append (stm->params_specs, ((GdaDelimiterExpr *)(list->data))->pspec_list);
		list = g_list_next (list);
	}

	return stm;
}
