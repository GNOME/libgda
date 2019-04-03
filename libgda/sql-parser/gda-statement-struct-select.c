/*
 * Copyright (C) 2008 - 2010 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2008 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
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

#include <libgda/gda-debug-macros.h>
#include <libgda/sql-parser/gda-statement-struct.h>
#include <libgda/sql-parser/gda-statement-struct-select.h>
#include <libgda/sql-parser/gda-statement-struct-pspec.h>
#include <string.h>
#include <glib/gi18n-lib.h>

static gpointer  gda_sql_statement_select_new (void);
static gboolean gda_sql_statement_select_check_structure (GdaSqlAnyPart *stmt, gpointer data, GError **error);
static gboolean gda_sql_statement_select_check_validity (GdaSqlAnyPart *stmt, gpointer data, GError **error);

GdaSqlStatementContentsInfo select_infos = {
	GDA_SQL_STATEMENT_SELECT,
	"SELECT",
	gda_sql_statement_select_new,
	_gda_sql_statement_select_free,
	_gda_sql_statement_select_copy,
	_gda_sql_statement_select_serialize,

	gda_sql_statement_select_check_structure,
	gda_sql_statement_select_check_validity,
	NULL,
	NULL,
	NULL,
	NULL
};

GdaSqlStatementContentsInfo *
_gda_sql_statement_select_get_infos (void)
{
	return &select_infos;
}

static gpointer
gda_sql_statement_select_new (void)
{
	GdaSqlStatementSelect *stmt;
	stmt = g_new0 (GdaSqlStatementSelect, 1);
	GDA_SQL_ANY_PART (stmt)->type = GDA_SQL_ANY_STMT_SELECT;
	return (gpointer) stmt;
}

void
_gda_sql_statement_select_free (gpointer stmt)
{
	GdaSqlStatementSelect *select = (GdaSqlStatementSelect *) stmt;

	if (select->distinct_expr)
		gda_sql_expr_free (select->distinct_expr);
	if (select->expr_list) {
		g_slist_free_full (select->expr_list, (GDestroyNotify) gda_sql_select_field_free);
	}
	gda_sql_select_from_free (select->from);
	gda_sql_expr_free (select->where_cond);
	if (select->group_by) {
		g_slist_free_full (select->group_by, (GDestroyNotify) gda_sql_expr_free);
	}
	gda_sql_expr_free (select->having_cond);
	if (select->order_by) {
		g_slist_free_full (select->order_by, (GDestroyNotify) gda_sql_select_order_free);
	}
	gda_sql_expr_free (select->limit_count);
	gda_sql_expr_free (select->limit_offset);
	g_free (select);
}

gpointer
_gda_sql_statement_select_copy (gpointer src)
{
	GdaSqlStatementSelect *dest;
	GdaSqlStatementSelect *select = (GdaSqlStatementSelect *) src;
	GSList *list;

	dest = gda_sql_statement_select_new ();
	dest->distinct = select->distinct;

	dest->distinct_expr = gda_sql_expr_copy (select->distinct_expr);
	gda_sql_any_part_set_parent (dest->distinct_expr, dest);

	for (list = select->expr_list; list; list = list->next) {
		dest->expr_list = g_slist_prepend (dest->expr_list,
						   gda_sql_select_field_copy ((GdaSqlSelectField*) list->data));
		gda_sql_any_part_set_parent (dest->expr_list->data, dest);
	}
	dest->expr_list = g_slist_reverse (dest->expr_list);

	dest->from = gda_sql_select_from_copy (select->from);
	gda_sql_any_part_set_parent (dest->from, dest);

	dest->where_cond = gda_sql_expr_copy (select->where_cond);
	gda_sql_any_part_set_parent (dest->where_cond, dest);

	for (list = select->group_by; list; list = list->next) {
		dest->group_by = g_slist_prepend (dest->group_by,
						  gda_sql_expr_copy ((GdaSqlExpr*) list->data));
		gda_sql_any_part_set_parent (dest->group_by->data, dest);
	}
	dest->group_by = g_slist_reverse (dest->group_by);

	dest->having_cond = gda_sql_expr_copy (select->having_cond);
	gda_sql_any_part_set_parent (dest->having_cond, dest);

	for (list = select->order_by; list; list = list->next) {
		dest->order_by = g_slist_prepend (dest->order_by,
						  gda_sql_select_order_copy ((GdaSqlSelectOrder*) list->data));
		gda_sql_any_part_set_parent (dest->order_by->data, dest);
	}
	dest->order_by = g_slist_reverse (dest->order_by);

	dest->limit_count = gda_sql_expr_copy (select->limit_count);
	gda_sql_any_part_set_parent (dest->limit_count, dest);

	dest->limit_offset = gda_sql_expr_copy (select->limit_offset);
	gda_sql_any_part_set_parent (dest->limit_offset, dest);

	return dest;
}

gchar *
_gda_sql_statement_select_serialize (gpointer stmt)
{
	GString *string;
	gchar *str;
	GSList *list;
	GdaSqlStatementSelect *select = (GdaSqlStatementSelect *) stmt;

	g_return_val_if_fail (stmt, NULL);

	string = g_string_new ("\"contents\":{");
	/* distinct */
	g_string_append (string, "\"distinct\":");
	g_string_append (string, select->distinct ? "\"true\"" : "\"false\"");
	if (select->distinct_expr) {
		g_string_append (string, ",\"distinct_on\":");
		str = gda_sql_expr_serialize (select->distinct_expr);
		g_string_append (string, str);
		g_free (str);
	}
	g_string_append (string, ",\"fields\":");
	if (select->expr_list) {
		g_string_append_c (string, '[');
		for (list = select->expr_list; list; list = list->next) {
			if (list != select->expr_list)
				g_string_append_c (string, ',');
			str = gda_sql_select_field_serialize ((GdaSqlSelectField*) list->data);
			g_string_append (string, str);
			g_free (str);
		}
		g_string_append_c (string, ']');
	}
	else
		g_string_append (string, "null");

	if (select->from) {
		g_string_append (string, ",\"from\":");
		str = gda_sql_select_from_serialize (select->from);
		g_string_append (string, str);
		g_free (str);
	}

	if (select->where_cond) {
		g_string_append (string, ",\"where\":");
		str = gda_sql_expr_serialize (select->where_cond);
		g_string_append (string, str);
		g_free (str);
	}

	if (select->group_by) {
		g_string_append (string, ",\"group_by\":");
		g_string_append_c (string, '[');
		for (list = select->group_by; list; list = list->next) {
			if (list != select->group_by)
				g_string_append_c (string, ',');
			str = gda_sql_expr_serialize ((GdaSqlExpr*) list->data);
			g_string_append (string, str);
			g_free (str);
		}
		g_string_append_c (string, ']');
	}

	if (select->having_cond) {
		g_string_append (string, ",\"having\":");
		str = gda_sql_expr_serialize (select->having_cond);
		g_string_append (string, str);
		g_free (str);
	}

	if (select->order_by) {
		g_string_append (string, ",\"order_by\":");
		g_string_append_c (string, '[');
		for (list = select->order_by; list; list = list->next) {
			if (list != select->order_by)
				g_string_append_c (string, ',');
			str = gda_sql_select_order_serialize ((GdaSqlSelectOrder*) list->data);
			g_string_append (string, str);
			g_free (str);
		}
		g_string_append_c (string, ']');
	}

	if (select->limit_count) {
		g_string_append (string, ",\"limit\":");
		str = gda_sql_expr_serialize (select->limit_count);
		g_string_append (string, str);
		g_free (str);

		if (select->limit_offset) {
			g_string_append (string, ",\"offset\":");
			str = gda_sql_expr_serialize (select->limit_offset);
			g_string_append (string, str);
			g_free (str);
		}
	}

	g_string_append_c (string, '}');
	str = string->str;
	g_string_free (string, FALSE);
	return str;	
}

/**
 * gda_sql_statement_select_take_distinct:
 * @stmt: a #GdaSqlStatement pointer
 * @distinct: a TRUE/FALSE value
 * @distinct_expr: (nullable): a #GdaSqlExpr pointer representing what the DISTINCT is on, or %NULL
 *
 * Sets the DISTINCT clause of @stmt. 
 *
 * @distinct_expr's ownership is transferred to
 * @stmt (which means @stmt is then responsible for freeing it when no longer needed).
 */
void
gda_sql_statement_select_take_distinct (GdaSqlStatement *stmt, gboolean distinct, GdaSqlExpr *distinct_expr)
{
	GdaSqlStatementSelect *select = (GdaSqlStatementSelect *) stmt->contents;
	select->distinct = distinct;
	select->distinct_expr = distinct_expr;
	gda_sql_any_part_set_parent (select->distinct_expr, select);
}

/**
 * gda_sql_statement_select_take_expr_list:
 * @stmt: a #GdaSqlStatement pointer
 * @expr_list: (element-type Gda.SqlSelectField): a list of #GdaSqlSelectField pointers
 *
 * Sets list of expressions selected by @stmt
 *
 * @expr_list's ownership is transferred to
 * @stmt (which means @stmt is then responsible for freeing it when no longer needed).
 */
void
gda_sql_statement_select_take_expr_list (GdaSqlStatement *stmt, GSList *expr_list)
{
	GSList *l;
	GdaSqlStatementSelect *select = (GdaSqlStatementSelect *) stmt->contents;
	select->expr_list = expr_list;
	for (l = expr_list; l; l = l->next)
		gda_sql_any_part_set_parent (l->data, select);
}

/**
 * gda_sql_statement_select_take_from:
 * @stmt: a #GdaSqlStatement pointer
 * @from: a #GdaSqlSelectFrom pointer
 *
 * Sets the FROM clause of @stmt
 *
 * @from's ownership is transferred to
 * @stmt (which means @stmt is then responsible for freeing it when no longer needed).
 */
void
gda_sql_statement_select_take_from (GdaSqlStatement *stmt, GdaSqlSelectFrom *from)
{
	GdaSqlStatementSelect *select = (GdaSqlStatementSelect *) stmt->contents;
	select->from = from;
	gda_sql_any_part_set_parent (from, select);
}

/**
 * gda_sql_statement_select_take_where_cond:
 * @stmt: a #GdaSqlStatement pointer
 * @expr: a #GdaSqlExpr pointer
 *
 * Sets the WHERE clause of @stmt
 *
 * @expr's ownership is transferred to
 * @stmt (which means @stmt is then responsible for freeing it when no longer needed).
 */
void
gda_sql_statement_select_take_where_cond (GdaSqlStatement *stmt, GdaSqlExpr *expr)
{
	GdaSqlStatementSelect *select = (GdaSqlStatementSelect *) stmt->contents;
	select->where_cond = expr;
	gda_sql_any_part_set_parent (expr, select);
}

/**
 * gda_sql_statement_select_take_group_by:
 * @stmt: a #GdaSqlStatement pointer
 * @group_by: (element-type GdaSqlExpr): a list of #GdaSqlExpr pointer
 *
 * Sets the GROUP BY clause of @stmt
 *
 * @group_by's ownership is transferred to
 * @stmt (which means @stmt is then responsible for freeing it when no longer needed).
 */
void
gda_sql_statement_select_take_group_by (GdaSqlStatement *stmt, GSList *group_by)
{
	GSList *l;
	GdaSqlStatementSelect *select = (GdaSqlStatementSelect *) stmt->contents;
	select->group_by = group_by;
	for (l = group_by; l; l = l->next)
		gda_sql_any_part_set_parent (l->data, select);
}

/**
 * gda_sql_statement_select_take_having_cond:
 * @stmt: a #GdaSqlStatement pointer
 * @expr: a #GdaSqlExpr pointer
 *
 * Sets the HAVING clause of @stmt
 *
 * @expr's ownership is transferred to
 * @stmt (which means @stmt is then responsible for freeing it when no longer needed).
 */
void
gda_sql_statement_select_take_having_cond (GdaSqlStatement *stmt, GdaSqlExpr *expr)
{
	GdaSqlStatementSelect *select = (GdaSqlStatementSelect *) stmt->contents;
	select->having_cond = expr;
	gda_sql_any_part_set_parent (expr, select);
}

/**
 * gda_sql_statement_select_take_order_by:
 * @stmt: a #GdaSqlStatement pointer
 * @order_by: (element-type GdaSqlSelectOrder): a list of #GdaSqlSelectOrder pointer
 *
 * Sets the ORDER BY clause of @stmt
 *
 * @order_by's ownership is transferred to
 * @stmt (which means @stmt is then responsible for freeing it when no longer needed).
 */
void
gda_sql_statement_select_take_order_by (GdaSqlStatement *stmt, GSList *order_by)
{
	GSList *l;
	GdaSqlStatementSelect *select = (GdaSqlStatementSelect *) stmt->contents;
	select->order_by = order_by;
	for (l = order_by; l; l = l->next)
		gda_sql_any_part_set_parent (l->data, select);
}

/**
 * gda_sql_statement_select_take_limits:
 * @stmt: a #GdaSqlStatement pointer
 * @count: a #GdaSqlExpr pointer
 * @offset: a #GdaSqlExpr pointer
 *
 * Sets the LIMIT clause of @stmt
 *
 * @count and @offset's responsibility are transferred to
 * @stmt (which means @stmt is then responsible for freeing them when no longer needed).
 */
void
gda_sql_statement_select_take_limits (GdaSqlStatement *stmt, GdaSqlExpr *count, GdaSqlExpr *offset)
{
	GdaSqlStatementSelect *select = (GdaSqlStatementSelect *) stmt->contents;
	select->limit_count = count;
	gda_sql_any_part_set_parent (count, select);
	select->limit_offset = offset;
	gda_sql_any_part_set_parent (offset, select);
}

static gboolean
gda_sql_statement_select_check_structure (GdaSqlAnyPart *stmt, G_GNUC_UNUSED gpointer data, GError **error)
{
	GdaSqlStatementSelect *select = (GdaSqlStatementSelect *) stmt;
	if (!select->expr_list) {
		g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
			      "%s", _("SELECT does not contain any expression"));
		return FALSE;
	}

	if (select->distinct_expr && !select->distinct) {
		g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
			      "%s", _("SELECT can't have a DISTINCT expression if DISTINCT is not set"));
		return FALSE;
	}

	if (select->having_cond && !select->group_by) {
		g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
			      "%s", _("SELECT can't have a HAVING without GROUP BY"));
		return FALSE;
	}

	if (select->limit_offset && !select->limit_count) {
		g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
			      "%s", _("SELECT can't have a limit offset without a limit"));
		return FALSE;
	}
	return TRUE;
}

static gboolean
gda_sql_statement_select_check_validity (GdaSqlAnyPart *stmt, G_GNUC_UNUSED gpointer data, GError **error)
{
	GdaSqlStatementSelect *select = (GdaSqlStatementSelect *) stmt;
	gboolean retval = TRUE;

	/* validate target's names and aliases:
	 * - there can't be 2 targets with the same alias
	 * - each target name or alias can only reference at most one target
	 */
	if (select->from && select->from->targets) {
		GHashTable *hash; /* key = target name or alias, value = GdaSqlSelectTarget pointer */
		GSList *list;
		hash = g_hash_table_new (g_str_hash, g_str_equal);
		for (list = select->from->targets; list; list = list->next) {
			GdaSqlSelectTarget *t = (GdaSqlSelectTarget*) list->data;
			if (t->as) {
				if (g_hash_table_lookup (hash, t->as)) {
					g_set_error (error, GDA_SQL_ERROR, GDA_SQL_VALIDATION_ERROR,
						     _("Multiple targets named or aliased '%s'"), t->as);
					retval = FALSE;
					break;
				}
				g_hash_table_insert (hash, t->as, t);
			}
			else if (t->table_name) {
				if (g_hash_table_lookup (hash, t->table_name)) {
					g_set_error (error, GDA_SQL_ERROR, GDA_SQL_VALIDATION_ERROR,
						     _("Multiple targets named or aliased '%s'"), t->table_name);
					retval = FALSE;
					break;
				}
				g_hash_table_insert (hash, t->table_name, t);
			}

			if (!retval)
				break;
		}
		g_hash_table_destroy (hash);
	}

	return retval;
}
