/* gda-delimiter-tree.c
 *
 * Copyright (C) 2004 - 2005 Vivien Malerba
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
#include <string.h>
#include <stdlib.h>

#include "gda-sql-delimiter.h"
#include "gda-delimiter-tree.h"

GList *
gda_delimiter_param_spec_build_simple (gchar *content)
{
	gchar *ptr;
	gchar *pname = NULL, *ptype = NULL, *pnnul = NULL;
	GList *retval;
	
	/* search the "##" start string */
	for (ptr = content; *ptr; ptr++) {
		if ((*ptr == '#') && (*(ptr+1) == '#'))
			pname = ptr+2;
	}

	g_assert (pname);
	/* delimit param name and param type */
	for (ptr = pname; *ptr; ptr++) {
		if ((*ptr == ':') && (*(ptr+1) == ':')) {
			*ptr = 0;
			if (!ptype) 
				ptype = ptr+2;
			else {
				pnnul = ptr+2;
				break;
			}
			ptr ++;
		}
	}
	
	retval = g_list_append (NULL, gda_delimiter_param_spec_build (GDA_DELIMITER_PARAM_NAME, g_strdup (pname)));
	if (ptype)
		retval = g_list_append (retval, gda_delimiter_param_spec_build (GDA_DELIMITER_PARAM_TYPE, g_strdup (ptype)));
	else
		retval = g_list_append (retval, gda_delimiter_param_spec_build (GDA_DELIMITER_PARAM_TYPE, g_strdup ("gchararray")));
	if (pnnul)
		retval = g_list_append (retval, gda_delimiter_param_spec_build (GDA_DELIMITER_PARAM_NULLOK, g_strdup ("TRUE")));

	g_free (content);
	return retval;
}

GdaDelimiterStatement *
gda_delimiter_statement_build (GdaDelimiterStatementType type, GList *expr_list)
{
	GdaDelimiterStatement *retval;
	GList *list, *pspecs = NULL;
	GdaDelimiterExpr *first = NULL;

	switch (type) {
	case GDA_DELIMITER_SQL_SELECT:
		first = gda_delimiter_expr_build (g_strdup ("SELECT"), NULL);
		break;
	case GDA_DELIMITER_SQL_INSERT:
		first = gda_delimiter_expr_build (g_strdup ("INSERT"), NULL);
		break;
	case GDA_DELIMITER_SQL_DELETE:
		first = gda_delimiter_expr_build (g_strdup ("DELETE"), NULL);
		break;
	case GDA_DELIMITER_SQL_UPDATE:
		first = gda_delimiter_expr_build (g_strdup ("UPDATE"), NULL);
		break;
	case GDA_DELIMITER_UNKNOWN:
		first = NULL;
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	/* build returned structure */
	retval = g_new0 (GdaDelimiterStatement, 1);
	retval->type = type;
	if (first)
		retval->expr_list = g_list_prepend (expr_list, first);
	else
		retval->expr_list = expr_list;

	/* make a list of the para specs */
	list = expr_list;
	while (list) {
		GdaDelimiterExpr *expr = (GdaDelimiterExpr *)(list->data);
		if (expr->pspec_list) {
			if (expr->sql_text) {
				GdaDelimiterParamSpec *default_spec;
				gchar *ptr = NULL;
				gint len;

				/* add a new GDA_DELIMITER_PARAM_DEFAULT GdaDelimiterParamSpec 
				 * to the list of spec items */
				default_spec = g_new0 (GdaDelimiterParamSpec, 1);
				default_spec->type = GDA_DELIMITER_PARAM_DEFAULT;
				default_spec->content = g_strdup (expr->sql_text);
				if ((*default_spec->content != '\'') && (*default_spec->content != '"')) {
					len = strlen (default_spec->content);
					for (ptr = default_spec->content + (len - 1); ptr > default_spec->content; ptr--) {
						if ((*ptr == ' ') || (*ptr == '\t') || (*ptr == '\n') || (*ptr == '\r'))
							*ptr = 0;
						else
							break;
					}
					for (; ptr > default_spec->content; ptr--) {
						if ((*ptr == ' ') || (*ptr == '\t') || (*ptr == '\n') || (*ptr == '\r')) {
							memmove (default_spec->content, ptr+1, 
								 len - (ptr - default_spec->content));
							break;
						}
					}
				}
				expr->pspec_list = g_list_prepend (expr->pspec_list, default_spec);

				if (ptr) {
					/* remove the default value from the expr->sql_text, and create a new
					 * GdaDelimiterExpr structure to hold that part only */
					GdaDelimiterExpr *nexpr;
					nexpr = gda_delimiter_expr_build (expr->sql_text, NULL);
					retval->expr_list = g_list_insert_before (retval->expr_list, 
										   list, nexpr);
					expr->sql_text = g_strdup (default_spec->content);
					len = ptr - default_spec->content;
					for (ptr = nexpr->sql_text + len ; ptr > nexpr->sql_text; ptr--) {
						if ((*ptr == ' ') || (*ptr == '\t') || (*ptr == '\n') || 
						    (*ptr == '\r'))
							*ptr = 0;
						else
							break;
					}
					/*g_print ("TRUNCATED expr to #%s#\n", nexpr->sql_text);*/
				}
				/*g_print ("Added DEFAULT spec with: #%s#\n", default_spec->content);*/

			}
			pspecs = g_list_append (pspecs, expr->pspec_list);
		}
		list = g_list_next (list);
	}
	retval->params_specs = pspecs;

	return retval;
}

GdaDelimiterExpr *
gda_delimiter_expr_build (gchar *str, GList *pspec_list)
{
	GdaDelimiterExpr *retval;

	retval = g_new0 (GdaDelimiterExpr, 1);
	retval->pspec_list = pspec_list;
	retval->sql_text = str;

	return retval;
}

GdaDelimiterParamSpec *
gda_delimiter_param_spec_build (GdaDelimiterParamSpecType type, char *content)
{
	GdaDelimiterParamSpec *retval;

	retval = g_new0 (GdaDelimiterParamSpec, 1);
	retval->type = type;
	retval->content = content;

	return retval;
}

