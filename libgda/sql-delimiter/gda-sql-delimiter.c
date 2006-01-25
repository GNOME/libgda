/* gda-sql-delimiter.c
 *
 * Copyright (C) 2004 - 2006 Vivien Malerba
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <stdio.h>
#include <glib.h>
#include <strings.h>
#include <string.h>

#include "gda-sql-delimiter.h"
#include "gda-delimiter-tree.h"

#ifndef _
#define _(x) (x)
#endif

extern char *gda_delimitertext;
extern void gda_delimiter_switch_to_buffer (void *buffer);
extern void *gda_delimiter_scan_string (const char *string);

void gda_delimitererror (char *error);

GdaDelimiterStatement *sql_result;
GError **gda_sql_error;

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
		if (!strcmp (string, "parse error"))
			g_set_error (gda_sql_error, 0, 0, _("Parse error near `%s'"), gda_delimitertext);
		if (!strcmp (string, "syntax error"))
			g_set_error (gda_sql_error, 0, 0, _("Syntax error near `%s'"), gda_delimitertext);
	}
	else
		fprintf (stderr, "SQL Parser error: %s near `%s'\n", string, gda_delimitertext);
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
	g_free (statement->full_query);
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
 * Generate #GdaDelimiterStatement which is an in memory a structure of the 
 * @sqlquery in an easy to use way. It basically makes chuncks of string and
 * identifies required parameters.
 * 
 * Returns: A generated GdaDelimiterStatement or %NULL on error.
 */
GdaDelimiterStatement *
gda_delimiter_parse_with_error (const char *sqlquery, GError ** error)
{
	if (!sqlquery) {
		if (error)
			g_set_error (error, 0, 0, _("Empty query to parse"));
		return NULL;
	}
	
	gda_sql_error = error;
	gda_delimiter_switch_to_buffer (gda_delimiter_scan_string (g_strdup (sqlquery)));
	if (! gda_delimiterparse ()) {
		sql_result->full_query = g_strdup (sqlquery);
		return sql_result;
	}

	return NULL;
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
GdaDelimiterStatement *
gda_delimiter_parse (const char *sqlquery)
{
	return gda_delimiter_parse_with_error (sqlquery, NULL);
}

/**
 * gda_delimiter_no_parse
 * @sqlquery: A SQL query string. ie SELECT * FROM FOO
 * 
 * Generates a GdaDelimiterStatement structure without trying to actually
 * analyse @sqlquery. This is usefull as a fallback where any other
 * analysis methods have failed.
 *
 * It is not much usefull except that it creates a valid #GdaDelimiterStatement
 * structure.
 */
GdaDelimiterStatement *
gda_delimiter_no_parse (const char *sql_text)
{
	GdaDelimiterStatement *retval;
	GdaDelimiterExpr *expr;

	expr = g_new0 (GdaDelimiterExpr, 1);
	expr->sql_text = g_strdup (sql_text);
	expr->pspec_list = NULL;

	retval = g_new0 (GdaDelimiterStatement, 1);
	retval->type = GDA_DELIMITER_UNKNOWN;
	retval->full_query = g_strdup (sql_text);
	retval->expr_list = g_list_append (NULL, expr);

	return retval;
}

static void sql_display_expr (GdaDelimiterExpr *expr);
static void sql_display_pspec_list (GList *pspecs);
void
gda_delimiter_display (GdaDelimiterStatement *statement)
{
	GList *list;

	if (!statement)
		return;

	switch (statement->type) {
	case GDA_DELIMITER_SQL_SELECT:
		g_print ("Select statement:\n");
		break;
	case GDA_DELIMITER_SQL_INSERT:
		g_print ("Insert statement:\n");
		break;
	case GDA_DELIMITER_SQL_UPDATE:
		g_print ("Update statement:\n");
		break;
	case GDA_DELIMITER_SQL_DELETE:
		g_print ("Delete statement:\n");
		break;
	default:
		g_print ("Unknown statement:\n");
		break;
	}

	g_print ("Original SQL: %s\n", statement->full_query);
	g_print ("Parsed SQL:\n");
	list = statement->expr_list;
	while (list) {
		sql_display_expr ((GdaDelimiterExpr *)(list->data));
		list = g_list_next (list);
	}
	g_print ("Parsed parameters:\n");
	list = statement->params_specs;
	while (list) {
		sql_display_pspec_list ((GList *)(list->data));
		list = g_list_next (list);
	}
}

static void
sql_display_expr (GdaDelimiterExpr *expr)
{
	if (expr->sql_text)
		g_print ("\t%s\n", expr->sql_text);
	if (expr->pspec_list)
		sql_display_pspec_list (expr->pspec_list);
}

static void
sql_display_pspec_list (GList *pspecs)
{
	GList *list;

	list = pspecs;
	g_print ("\t## [");
	while (list) {
		GdaDelimiterParamSpec *pspec = (GdaDelimiterParamSpec *)(list->data);

		if (list != pspecs)
			g_print (" ");

		switch (pspec->type) {
		case GDA_DELIMITER_PARAM_NAME:
			g_print (":name=\"%s\"", pspec->content);
			break;
		case GDA_DELIMITER_PARAM_DESCR:
			g_print (":descr=\"%s\"", pspec->content);
			break;
		case GDA_DELIMITER_PARAM_TYPE:
			g_print (":type=\"%s\"", pspec->content);
			break;
		case GDA_DELIMITER_PARAM_ISPARAM:
			g_print (":isparam=\"%s\"", pspec->content);
			break;
		case GDA_DELIMITER_PARAM_NULLOK:
			g_print (":nullok=\"%s\"", pspec->content);
			break;
		default:
			g_print (":?? =\"%s\"", pspec->content);
			break;
		}
		list = g_list_next (list);
	}
	g_print ("]\n");
}


static GdaDelimiterExpr *
copy_expr (GdaDelimiterExpr *orig)
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
	if (expr->pspec_list)
		expr->pspec_list = g_list_reverse (expr->pspec_list);
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
gda_delimiter_parse_copy_statement (GdaDelimiterStatement *statement)
{
	GdaDelimiterStatement *stm;
	GList *list;

	if (!statement)
		return NULL;

	stm = g_new0 (GdaDelimiterStatement, 1);
	stm->type = statement->type;
	stm->full_query = g_strdup (statement->full_query);
	list = statement->expr_list;
	while (list) {
		GdaDelimiterExpr *tmp = copy_expr ((GdaDelimiterExpr *)(list->data));
		stm->expr_list = g_list_prepend (stm->expr_list, tmp);
		
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
