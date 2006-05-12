/* gda-delimiter-tree.c
 *
 * Copyright (C) 2004 - 2005 Vivien Malerba
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
#include <stdlib.h>

#include "gda-sql-delimiter.h"
#include "gda-delimiter-tree.h"

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
		if (((GdaDelimiterExpr *)(list->data))->pspec_list)
			pspecs = g_list_append (pspecs, ((GdaDelimiterExpr *)(list->data))->pspec_list);
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

