/*
 * Copyright (C) 2008 - 2012 Vivien Malerba <malerba@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <stdio.h>
#include <glib.h>
#include <glib-object.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <libgda/gda-debug-macros.h>
#include <sql-parser/gda-sql-parser.h>
#include <sql-parser/gda-sql-statement.h>
#include <stdarg.h>
#include "graph.h"

static gchar *
quote_string_for_html (const gchar *str)
{
	gchar *r, *wptr;
	const gchar *rptr;

	if (!str)
		return NULL;
	r = g_new0 (gchar, strlen (str) * 4 + 1);
	for (rptr = str, wptr = r; *rptr; rptr++) {
		if (*rptr == '<') {
			*wptr = '&'; wptr++;
			*wptr = 'l'; wptr++;
			*wptr = 't'; wptr++;
			*wptr = ';'; wptr++;
		}
		else if (*rptr == '>') {
			*wptr = '&'; wptr++;
			*wptr = 'g'; wptr++;
			*wptr = 't'; wptr++;
			*wptr = ';'; wptr++;
		}
		else 
			*wptr = *rptr; wptr++;
	}
	return r;
}

static gchar *
quote_string_for_string (const gchar *str)
{
	gchar *r, *wptr;
	const gchar *rptr;

	if (!str)
		return NULL;
	r = g_new0 (gchar, strlen (str) * 2 + 1);
	for (rptr = str, wptr = r; *rptr; rptr++) {
		if (*rptr == '"') {
			*wptr = '\\'; wptr++;
			*wptr = '"'; wptr++;
		}
		else if (*rptr == '\n') {
			*wptr = '\\'; wptr++;
			*wptr = 'n'; wptr++;
		}
		else 
			*wptr = *rptr; wptr++;
	}
	return r;
}

/*
 * ... is a NULL terminated list of (att_name as string, value type as GType, and value)
 */
static void
add_node (GString *string, gpointer part, const gchar *name, ...)
{
	va_list ap;
	gchar *att_name;
	GType type;
	gboolean firstatt = TRUE;
	g_string_append_printf (string, "\n\"node%p\" [shape=\"plaintext\" label=<"
				"<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">"
				"<TR><TD>%s</TD></TR>",
				part, name);
	
	va_start (ap, name);
	for (att_name = va_arg (ap, char *); att_name; att_name = va_arg (ap, char *)) {
		type = va_arg (ap, GType);
		if (firstatt) {
			g_string_append (string, "<TR><TD BALIGN=\"LEFT\">");
			firstatt = FALSE;
		}
		else
			g_string_append (string, "<BR/>");
		if (type == G_TYPE_STRING) {
			gchar *val, *tmp;
			val = va_arg (ap, char *);
			tmp = quote_string_for_html (val);
			g_string_append_printf (string, "%s=%s", att_name, tmp);
			g_free (tmp);
		}
		else if (type == G_TYPE_BOOLEAN) {
			gboolean val;
			val = va_arg (ap, gboolean);
			g_string_append_printf (string, "%s=%s", att_name, val ? "TRUE" : "FALSE");
		}
		else if (type == G_TYPE_INT) {
			gint val;
			val = va_arg (ap, gint);
			g_string_append_printf (string, "%s=%d", att_name, val);
		}
		else if (type == G_TYPE_VALUE) {
			GValue *val;
			val = va_arg (ap, GValue *);
			if (!val)
				g_string_append_printf (string, "%s=(null)", att_name);
			else {
				gchar *str, *tmp;
				str = gda_value_stringify (val);
				tmp = quote_string_for_html (str);
				g_string_append_printf (string, "%s=%s", att_name, tmp);
				g_free (tmp);
				g_free (str);
			}
		}
		else
			g_assert_not_reached ();
	}
	if (!firstatt) 
		g_string_append (string, "</TD></TR>");
	g_string_append (string, "</TABLE>");
	va_end (ap);
	g_string_append_c (string, '>');
	if (!strcmp (name, "GdaSqlParamSpec"))
		g_string_append (string, ",color=\"deepskyblue\"");
	g_string_append (string, "];");
}

static void
link_a_node (GString *string, gpointer from, gpointer to, const gchar *label)
{
	if (!to)
		return;
	if (label)
		g_string_append_printf (string, "\n\"node%p\" -> \"node%p\" [label = \"%s\" ];", from, to, label);
	else
		g_string_append_printf (string, "\n\"node%p\" -> \"node%p\"", from, to);
}

/*
 * if @label is %NULL, then it means that @from is itself a list node and not a part
 */
static void
link_a_list (GString *string, gpointer from, GSList *to_list, const gchar *label)
{
	GSList *list, *prev;
	gint pos;
	if (!to_list)
		return;
	for (list = to_list, prev = NULL, pos = 0; list; prev = list, list = list->next, pos++) {
		/* list container */
		g_string_append_printf (string, "\n\"list%p\" [shape=invhouse,label=\"N%d\",color=gray,fontcolor=gray];",
					list, pos);

		/* item */
		g_string_append_printf (string, "\n\"list%p\" -> \"node%p\" [label=\"data\",color=gray,fontcolor=gray];",
					list, list->data);
		
		/* list container links */
		if (list == to_list) {
			if (label)
				g_string_append_printf (string, "\n\"node%p\" -> \"list%p\" [label=\"%s\"];", 
							from, list, label);
			else
				g_string_append_printf (string, "\n\"list%p\" -> \"list%p\" [label=\"data\",color=gray,fontcolor=gray];", 
							from, list);
		}
		else
			g_string_append_printf (string, 
						"\n\"list%p\" -> \"list%p\" [label=\"next\",color=gray,fontcolor=gray];",
						prev, list);
	}
}

static gchar *
trans_isol_to_string (GdaTransactionIsolation level)
{
	switch (level) {
	default:
	case GDA_TRANSACTION_ISOLATION_UNKNOWN:
		return "UNKNOWN";
	case GDA_TRANSACTION_ISOLATION_READ_COMMITTED:
		return "READ COMMITTED";
	case GDA_TRANSACTION_ISOLATION_READ_UNCOMMITTED:
		return "READ UNCOMMITTED";
	case GDA_TRANSACTION_ISOLATION_REPEATABLE_READ:
		return "REPEATABLE READ";
	case GDA_TRANSACTION_ISOLATION_SERIALIZABLE:
		return "SERIALIZABLE";
	}
}

static gchar *
trans_type_to_string (GdaSqlAnyPartType type)
{
	switch (type) {
	case GDA_SQL_ANY_STMT_BEGIN:
		return "BEGIN";
        case GDA_SQL_ANY_STMT_ROLLBACK:
		return "ROLLBACK";
        case GDA_SQL_ANY_STMT_COMMIT:
		return "COMMIT";
        case GDA_SQL_ANY_STMT_SAVEPOINT:
		return "SAVEPOINT";
        case GDA_SQL_ANY_STMT_ROLLBACK_SAVEPOINT:
		return "ROLLBACK SAVEPOINT";
        case GDA_SQL_ANY_STMT_DELETE_SAVEPOINT:
		return "DELETE SAVEPOINT";
	default:
		return NULL;
	}
}

static gboolean
parts_foreach_func (GdaSqlAnyPart *part, GString *string, GError **error)
{
	switch (part->type) {
	case GDA_SQL_ANY_STMT_SELECT: {
		GdaSqlStatementSelect *st = (GdaSqlStatementSelect*) part;
		add_node (string, part , "GdaSqlStatementSelect", 
			  "distinct", G_TYPE_BOOLEAN, st->distinct, 
			  NULL);

		link_a_node (string, part, st->distinct_expr, "distinct_expr");
		link_a_list (string, part, st->expr_list, "expr_list");
		link_a_node (string, part, st->from, "from");
		link_a_node (string, part, st->where_cond, "where_cond");
		link_a_list (string, part, st->group_by, "group_by");
		link_a_node (string, part, st->having_cond, "having_cond");
		link_a_list (string, part, st->order_by, "order_by");
		link_a_node (string, part, st->limit_count, "limit_count");
		link_a_node (string, part, st->limit_offset, "limit_offset");
		break;
	}
        case GDA_SQL_ANY_STMT_INSERT: {
		GdaSqlStatementInsert *st = (GdaSqlStatementInsert*) part;
		GSList *vlist, *prev;
		gint pos;
		add_node (string, part , "GdaSqlStatementInsert", 
			  "on_conflict", G_TYPE_STRING, st->on_conflict, 
			  NULL);

		link_a_node (string, part, st->table, "table");
		link_a_list (string, part, st->fields_list, "fields_list");
		for (vlist = st->values_list, prev = NULL, pos = 0; vlist; prev = vlist, vlist = vlist->next, pos++) {
			/* list container */
			g_string_append_printf (string, "\n\"list%p\" [shape=invhouse,label=\"N%d\",color=gray,fontcolor=gray];",
						vlist, pos);
			/* list container links */
			if (vlist == st->values_list)
				g_string_append_printf (string, "\n\"node%p\" -> \"list%p\" [label=\"values_list\"];", 
							part, vlist);
			else
				g_string_append_printf (string, 
							"\n\"list%p\" -> \"list%p\" [label=\"next\",color=gray,fontcolor=gray];",
							prev, vlist);
			link_a_list (string, vlist, (GSList *) vlist->data, NULL);
		}
		link_a_node (string, part, st->select, "select");
		break;
	}
        case GDA_SQL_ANY_STMT_UPDATE: {
		GdaSqlStatementUpdate *st = (GdaSqlStatementUpdate*) part;
		add_node (string, part , "GdaSqlStatementUpdate", 
			  "on_conflict", G_TYPE_STRING, st->on_conflict, 
			  NULL);

		link_a_node (string, part, st->table, "table");
		link_a_list (string, part, st->fields_list, "fields_list");
		link_a_list (string, part, st->expr_list, "expr_list");
		link_a_node (string, part, st->cond, "cond");
		break;
	}
        case GDA_SQL_ANY_STMT_DELETE: {
		GdaSqlStatementDelete *st = (GdaSqlStatementDelete*) part;
		add_node (string, part , "GdaSqlStatementDelete", NULL);

		link_a_node (string, part, st->table, "table");
		link_a_node (string, part, st->cond, "cond");
		break;
	}
        case GDA_SQL_ANY_STMT_COMPOUND: {
		GdaSqlStatementCompound *st = (GdaSqlStatementCompound*) part;
		GSList *list;
		add_node (string, part , "GdaSqlStatementCompound", 
			  "compound_type", G_TYPE_INT, st->compound_type, NULL);

		for (list = st->stmt_list; list; list = list->next) {
			GdaSqlStatement *sqlst = (GdaSqlStatement*) list->data;
			g_string_append_printf (string, "\n\"node%p\" [label=\"GdaSqlStatement\",shape=octagon];", sqlst);
			g_string_append_printf (string, "\n\"node%p\" -> \"node%p\" [label = \"contents\" ];", sqlst, sqlst->contents);
		}
		link_a_list (string, part, st->stmt_list, "stmt_list");
		break;
	}
        case GDA_SQL_ANY_STMT_BEGIN:
        case GDA_SQL_ANY_STMT_ROLLBACK:
        case GDA_SQL_ANY_STMT_COMMIT:
        case GDA_SQL_ANY_STMT_SAVEPOINT:
        case GDA_SQL_ANY_STMT_ROLLBACK_SAVEPOINT:
        case GDA_SQL_ANY_STMT_DELETE_SAVEPOINT: {
		GdaSqlStatementTransaction *st = (GdaSqlStatementTransaction*) part;
		add_node (string, part , "GdaSqlStatementTransaction",
			  "__type__", G_TYPE_STRING, trans_type_to_string (part->type),
			  "isolation_level", G_TYPE_STRING, trans_isol_to_string (st->isolation_level),
			  "trans_mode", G_TYPE_STRING, st->trans_mode,
			  "trans_name", G_TYPE_STRING, st->trans_name,
			  NULL);
		break;
	}
        case GDA_SQL_ANY_STMT_UNKNOWN: {
		GdaSqlStatementUnknown *st = (GdaSqlStatementUnknown*) part;
		add_node (string, part , "GdaSqlStatementUnknown", NULL);

		link_a_list (string, part, st->expressions, "expressions");
		break;
	}
        case GDA_SQL_ANY_EXPR: {
		GdaSqlExpr *expr = (GdaSqlExpr*) part;
		add_node (string, part , "GdaSqlExpr", 
			  "value", G_TYPE_VALUE, expr->value, 
			  "cast_as", G_TYPE_STRING, expr->cast_as,
			  NULL);
		link_a_node (string, part, expr->func, "func");
		link_a_node (string, part, expr->cond, "cond");
		link_a_node (string, part, expr->select, "select");
		link_a_node (string, part, expr->case_s, "case_s");
		if (expr->param_spec) {
			add_node (string, expr->param_spec , "GdaSqlParamSpec", 
				  "name", G_TYPE_STRING, expr->param_spec->name, 
				  "descr", G_TYPE_STRING, expr->param_spec->descr,
				  "is_param", G_TYPE_BOOLEAN, expr->param_spec->is_param,
				  "nullok", G_TYPE_BOOLEAN, expr->param_spec->nullok,
				  "g-type", G_TYPE_STRING, g_type_name (expr->param_spec->g_type),
				  NULL);
			link_a_node (string, part, expr->param_spec, "param_spec");
		}
		break;
	}
        case GDA_SQL_ANY_SQL_FIELD: {
		GdaSqlField *field = (GdaSqlField*) part;
		add_node (string, part , "GdaSqlField", 
			  "field_name", G_TYPE_STRING, field->field_name,
			  NULL);
		break;
	}
        case GDA_SQL_ANY_SQL_TABLE: {
		GdaSqlTable *table = (GdaSqlTable*) part;
		add_node (string, part , "GdaSqlTable", 
			  "table_name", G_TYPE_STRING, table->table_name,
			  NULL);
		break;
	}
        case GDA_SQL_ANY_SQL_FUNCTION: {
		GdaSqlFunction *function = (GdaSqlFunction*) part;
		add_node (string, part , "GdaSqlFunction", 
			  "function_name", G_TYPE_STRING, function->function_name,
			  NULL);
		link_a_list (string, part, function->args_list, "args_list");
		break;
	}
        case GDA_SQL_ANY_SQL_OPERATION: {
		GdaSqlOperation *op = (GdaSqlOperation*) part;
		add_node (string, part , "GdaSqlOperation", 
			  "operator", G_TYPE_STRING, gda_sql_operation_operator_to_string (op->operator_type),
			  NULL);
		link_a_list (string, part, op->operands, "operands");
		break;
	}
        case GDA_SQL_ANY_SQL_CASE: {
		GdaSqlCase *c = (GdaSqlCase*) part;
		add_node (string, part , "GdaSqlCase", NULL);
		link_a_node (string, part, c->base_expr, "base_expr");
		link_a_list (string, part, c->when_expr_list, "when_expr_list");
		link_a_list (string, part, c->then_expr_list, "then_expr_list");
		link_a_node (string, part, c->else_expr, "else_expr");
		break;
	}
        case GDA_SQL_ANY_SQL_SELECT_FIELD: {
		GdaSqlSelectField *field = (GdaSqlSelectField*) part;
		add_node (string, part , "GdaSqlSelectField", 
			  "field_name", G_TYPE_STRING, field->field_name, 
			  "table_name", G_TYPE_STRING, field->table_name, 
			  "as", G_TYPE_STRING, field->as,
			  NULL);
		link_a_node (string, part, field->expr, "expr");
		break;
	}
        case GDA_SQL_ANY_SQL_SELECT_TARGET: {
		GdaSqlSelectTarget *target = (GdaSqlSelectTarget*) part;
		add_node (string, part , "GdaSqlSelectTarget", 
			  "table_name", G_TYPE_STRING, target->table_name, 
			  "as", G_TYPE_STRING, target->as,
			  NULL);
		link_a_node (string, part, target->expr, "expr");
		break;
	}
        case GDA_SQL_ANY_SQL_SELECT_JOIN: {
		GdaSqlSelectJoin *join = (GdaSqlSelectJoin*) part;
		add_node (string, part, "GdaSqlSelectJoin", 
			  "join_type", G_TYPE_STRING, gda_sql_select_join_type_to_string (join->type), 
			  "position", G_TYPE_INT, join->position,
			  NULL);
		link_a_node (string, part, join->expr, "expr");
		link_a_list (string, part, join->use, "using");
		break;
	}
        case GDA_SQL_ANY_SQL_SELECT_FROM: {
		GdaSqlSelectFrom *from = (GdaSqlSelectFrom*) part;
		add_node (string, part , "GdaSqlSelectFrom", NULL); 
		link_a_list (string, part, from->targets, "targets");
		link_a_list (string, part, from->joins, "joins");
		break;
	}
        case GDA_SQL_ANY_SQL_SELECT_ORDER: {
		GdaSqlSelectOrder *order = (GdaSqlSelectOrder*) part;
		add_node (string, part , "GdaSqlSelectOrder", 
			  "asc", G_TYPE_BOOLEAN, order->asc,
			  "collation_name", G_TYPE_STRING, order->collation_name,
			  NULL); 
		link_a_node (string, part, order->expr, "expr");
		break;
	}
	default:
		g_assert_not_reached ();
	}
	
	return TRUE;
}

gchar *
sql_statement_to_graph (GdaSqlStatement *sqlst)
{
	GString *string;
	gchar *str;
	string = g_string_new ("digraph G {graph [rankdir = \"TB\"");
	if (sqlst->sql) {
		gchar *tmp;
		tmp = quote_string_for_string (sqlst->sql);
		g_string_append_printf (string, ",label=\"%s\"];", tmp);
		g_free (tmp);
	}
	else
		g_string_append (string, "];");
	g_string_append (string, "\n\"GdaSqlStatement\" [shape=octagon];");
	g_string_append_printf (string, "\n\"GdaSqlStatement\" -> \"node%p\" [label = \"contents\" ];", sqlst->contents);	
	gda_sql_any_part_foreach (GDA_SQL_ANY_PART (sqlst->contents), (GdaSqlForeachFunc) parts_foreach_func,
				  string, NULL);
	g_string_append (string, "}");

	str = string->str;
	g_string_free (string, FALSE);
	return str;
}
