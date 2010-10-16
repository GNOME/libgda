/* 
 * Copyright (C) 2007 - 2008 Vivien Malerba
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

#include <string.h>
#include <libgda/sql-parser/gda-statement-struct.h>
#include <libgda/sql-parser/gda-statement-struct-unknown.h>
#include <libgda/sql-parser/gda-statement-struct-parts.h>

static gpointer  gda_sql_statement_unknown_new (void);
static void      gda_sql_statement_unknown_free (gpointer stmt);
static gpointer  gda_sql_statement_unknown_copy (gpointer src);
static gchar    *gda_sql_statement_unknown_serialize (gpointer stmt);

GdaSqlStatementContentsInfo unknown_infos = {
	GDA_SQL_STATEMENT_UNKNOWN,
	"UNKNOWN",
	gda_sql_statement_unknown_new,
	gda_sql_statement_unknown_free,
	gda_sql_statement_unknown_copy,
	gda_sql_statement_unknown_serialize,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

GdaSqlStatementContentsInfo *
_gda_sql_statement_unknown_get_infos (void)
{
	return &unknown_infos;
}

static gpointer
gda_sql_statement_unknown_new (void)
{
	GdaSqlStatementUnknown *stmt;
	stmt = g_new0 (GdaSqlStatementUnknown, 1);
	GDA_SQL_ANY_PART (stmt)->type = GDA_SQL_ANY_STMT_UNKNOWN;
	return (gpointer) stmt;
}

static void
gda_sql_statement_unknown_free (gpointer stmt)
{
	GdaSqlStatementUnknown *unknown = (GdaSqlStatementUnknown *) stmt;
	g_slist_foreach (unknown->expressions, (GFunc) gda_sql_expr_free, NULL);
	g_slist_free (unknown->expressions);
	g_free (unknown);
}

static gpointer
gda_sql_statement_unknown_copy (gpointer src)
{
	GdaSqlStatementUnknown *dest;
	GSList *list;
	GdaSqlStatementUnknown *unknown = (GdaSqlStatementUnknown *) src;

	dest = gda_sql_statement_unknown_new ();
	for (list = unknown->expressions; list; list = list->next) {
		dest->expressions = g_slist_prepend (dest->expressions, 
						     gda_sql_expr_copy ((GdaSqlExpr*) list->data));
		gda_sql_any_part_set_parent (dest->expressions->data, dest);
	}
	dest->expressions = g_slist_reverse (dest->expressions);
	return dest;
}

static gchar *
gda_sql_statement_unknown_serialize (gpointer stmt)
{
	GString *string;
	gchar *str;
	GSList *list;
	GdaSqlStatementUnknown *unknown = (GdaSqlStatementUnknown *) stmt;

	g_return_val_if_fail (stmt, NULL);

	string = g_string_new ("\"contents\":[");
	for (list = unknown->expressions; list; list = list->next) {
		gchar *str;
		str = gda_sql_expr_serialize ((GdaSqlExpr*) list->data);
		if (list != unknown->expressions)
			g_string_append_c (string, ',');
		g_string_append (string, str);
		g_free (str);
	}
	g_string_append_c (string, ']');
	str = string->str;
	g_string_free (string, FALSE);
	return str;	
}

/**
 * gda_sql_statement_unknown_take_expressions
 * @stmt: a #GdaSqlStatement pointer
 * @expressions: a list of #GdaSqlExpr pointers
 *
 * Sets @stmt's list of expressions
 *
 * @expressions's 
 * responsibility is transferred to
 * @stmt (which means @stmt is then responsible for freeing it when no longer needed).
 */
void
gda_sql_statement_unknown_take_expressions (GdaSqlStatement *stmt, GSList *expressions)
{
	GSList *l;
	GdaSqlStatementUnknown *unknown = (GdaSqlStatementUnknown *) stmt->contents;
	unknown->expressions = expressions;

	for (l = expressions; l; l = l->next)
		gda_sql_any_part_set_parent (l->data, unknown);
}
