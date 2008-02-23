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

#include <libgda/gda-debug-macros.h>
#include <libgda/sql-parser/gda-statement-struct-parts.h>
#include <libgda/sql-parser/gda-statement-struct-util.h>
#include <libgda/sql-parser/gda-statement-struct-pspec.h>
#include <libgda/sql-parser/gda-statement-struct-select.h>
#include <libgda/sql-parser/gda-statement-struct-insert.h>
#include <libgda/sql-parser/gda-statement-struct-update.h>
#include <libgda/sql-parser/gda-statement-struct-compound.h>
#include <glib/gi18n-lib.h>

/*
 *
 * Sql expression
 *
 */
GdaSqlExpr *
gda_sql_expr_new (GdaSqlAnyPart *parent)
{
	GdaSqlExpr *expr;
	expr = g_new0 (GdaSqlExpr, 1);
	GDA_SQL_ANY_PART(expr)->type = GDA_SQL_ANY_EXPR;
	GDA_SQL_ANY_PART(expr)->parent = parent;
	return expr;
}

void
gda_sql_expr_free (GdaSqlExpr *expr)
{
	if (!expr) return;

	gda_sql_expr_check_clean (expr);
	if (expr->value) {
		g_value_unset (expr->value);
		g_free (expr->value);
	}
	gda_sql_param_spec_free (expr->param_spec);
	gda_sql_function_free (expr->func);
	gda_sql_operation_free (expr->cond);
	if (expr->select) {
		if (GDA_SQL_ANY_PART (expr->select)->type == GDA_SQL_ANY_STMT_SELECT)
			gda_sql_statement_select_free (expr->select);
		else if (GDA_SQL_ANY_PART (expr->select)->type == GDA_SQL_ANY_STMT_COMPOUND)
			gda_sql_statement_compound_free (expr->select);
		else
			g_assert_not_reached ();
	}
	gda_sql_case_free (expr->case_s);
	g_free (expr->cast_as);

	g_free (expr);
}

GdaSqlExpr *
gda_sql_expr_copy (GdaSqlExpr *expr)
{
	GdaSqlExpr *copy;
	if (!expr) return NULL;

	copy = gda_sql_expr_new (NULL);
	if (expr->value) {
		GValue *value;

		value = g_new0 (GValue, 1);
		g_value_init (value, G_VALUE_TYPE (expr->value));
		g_value_copy (expr->value, value);
		copy->value = value;
	}
	copy->param_spec = gda_sql_param_spec_copy (expr->param_spec);

	copy->func = gda_sql_function_copy (expr->func);
	gda_sql_any_part_set_parent (copy->func, copy);

	copy->cond = gda_sql_operation_copy (expr->cond);
	gda_sql_any_part_set_parent (copy->cond, copy);

	if (expr->select) {
		if (GDA_SQL_ANY_PART (expr->select)->type == GDA_SQL_ANY_STMT_SELECT)
			copy->select = gda_sql_statement_select_copy (expr->select);
		else if (GDA_SQL_ANY_PART (expr->select)->type == GDA_SQL_ANY_STMT_COMPOUND)
			copy->select = gda_sql_statement_compound_copy (expr->select);
		else
			g_assert_not_reached ();
		gda_sql_any_part_set_parent (copy->select, copy);
	}

	copy->case_s = gda_sql_case_copy (expr->case_s);
	gda_sql_any_part_set_parent (copy->case_s, copy);

	if (expr->cast_as)
		copy->cast_as = g_strdup (expr->cast_as);
	return copy;
}

gchar *
gda_sql_expr_serialize (GdaSqlExpr *expr)
{
	GString *string;
	gchar *str, *tmp;

	if (!expr)
		return g_strdup ("null");

	string = g_string_new ("{");
	if (expr->cond) {
		str = gda_sql_operation_serialize (expr->cond);
		g_string_append_printf (string, "\"operation\":%s", str);
		g_free (str);
	}
	else if (expr->func) {
		str = gda_sql_function_serialize (expr->func);
		g_string_append_printf (string, "\"func\":%s", str);
		g_free (str);
	}
	else if (expr->select) {
		if (GDA_SQL_ANY_PART (expr->select)->type == GDA_SQL_ANY_STMT_SELECT)
			str = gda_sql_statement_select_serialize (expr->select);
		else if (GDA_SQL_ANY_PART (expr->select)->type == GDA_SQL_ANY_STMT_COMPOUND)
			str = gda_sql_statement_compound_serialize (expr->select);
		else
			g_assert_not_reached ();
		g_string_append_printf (string, "\"select\":{%s}", str);
		g_free (str);
	}
	else if (expr->case_s) {
		str = gda_sql_case_serialize (expr->case_s);
		g_string_append_printf (string, "\"case\":%s", str);
		g_free (str);
	}
	else {
		if (expr->value) {
			tmp = gda_sql_value_stringify (expr->value);
			str = _json_quote_string (tmp);
			g_free (tmp);
			g_string_append_printf (string, "\"value\":%s", str);
			g_free (str);
		}
		else 
			g_string_append_printf (string, "\"value\":null");
		if (expr->param_spec) {
			str = gda_sql_param_spec_serialize (expr->param_spec);
			g_string_append_printf (string, ",\"param_spec\":%s", str);
			g_free (str);
		}
	}

	if (expr->cast_as) {
		str = _json_quote_string (expr->cast_as);
		g_string_append_printf (string, ",\"cast\":%s", str);
		g_free (str);
	}

	g_string_append_c (string, '}');
	str = string->str;
	g_string_free (string, FALSE);
	return str;
}

void
gda_sql_expr_take_select (GdaSqlExpr *expr, GdaSqlStatement *stmt)
{
	if (stmt) {
		GdaSqlAnyPart *part;
		part = GDA_SQL_ANY_PART (stmt->contents);
		stmt->contents = NULL;
		gda_sql_statement_free (stmt);
		expr->select = gda_sql_statement_compound_reduce (part);
		gda_sql_any_part_set_parent (expr->select, expr);
	}
}

/*
 * Any Table's field
 */
GdaSqlField *
gda_sql_field_new (GdaSqlAnyPart *parent)
{
	GdaSqlField *field;
	field = g_new0 (GdaSqlField, 1);
	GDA_SQL_ANY_PART(field)->type = GDA_SQL_ANY_SQL_FIELD;
	GDA_SQL_ANY_PART(field)->parent = parent;
	return field;
}

void
gda_sql_field_free (GdaSqlField *field)
{
	if (!field) return;

	gda_sql_field_check_clean (field);
	g_free (field->field_name);
	g_free (field);
}

GdaSqlField *
gda_sql_field_copy (GdaSqlField *field)
{
	GdaSqlField *copy;
	if (!field) return NULL;

	copy = gda_sql_field_new (NULL);
	if (field->field_name)
		copy->field_name = g_strdup (field->field_name);

	return copy;
}

gchar *
gda_sql_field_serialize (GdaSqlField *field)
{
	if (!field) 
		return g_strdup ("null");
	else
		return _json_quote_string (field->field_name);
}

void
gda_sql_field_take_name (GdaSqlField *field, GValue *value)
{
	if (value) {
		field->field_name = g_value_dup_string (value);
		g_value_reset (value);
		g_free (value);
	}
}

/*
 * Structure to hold one table
 */
GdaSqlTable *
gda_sql_table_new (GdaSqlAnyPart *parent)
{
	GdaSqlTable *table;
	table = g_new0 (GdaSqlTable, 1);
	GDA_SQL_ANY_PART(table)->type = GDA_SQL_ANY_SQL_TABLE;
	GDA_SQL_ANY_PART(table)->parent = parent;
	return table;
}

void
gda_sql_table_free (GdaSqlTable *table)
{
	if (!table) return;

	gda_sql_table_check_clean (table);
	g_free (table->table_name);
	g_free (table);
}

GdaSqlTable *
gda_sql_table_copy (GdaSqlTable *table)
{
	GdaSqlTable *copy;
	if (!table) return NULL;

	copy = gda_sql_table_new (NULL);
	if (table->table_name)
		copy->table_name = g_strdup (table->table_name);

	return copy;
}

gchar *
gda_sql_table_serialize (GdaSqlTable *table)
{
	if (!table) 
		return g_strdup ("null");
	else
		return _json_quote_string (table->table_name);
}

void
gda_sql_table_take_name (GdaSqlTable *table, GValue *value)
{
	if (value) {
		table->table_name = g_value_dup_string (value);
		g_value_reset (value);
		g_free (value);
	}
}


/*
 * A function with any number of arguments
 */

GdaSqlFunction *
gda_sql_function_new (GdaSqlAnyPart *parent)
{
	GdaSqlFunction *function;
	function = g_new0 (GdaSqlFunction, 1);
	GDA_SQL_ANY_PART(function)->type = GDA_SQL_ANY_SQL_FUNCTION;
	GDA_SQL_ANY_PART(function)->parent = parent;
	return function;
}

void
gda_sql_function_free (GdaSqlFunction *function)
{
	if (!function) return;

	gda_sql_function_check_clean (function);
	g_free (function->function_name);
	if (function->args_list) {
		g_slist_foreach (function->args_list, (GFunc) gda_sql_expr_free, NULL);
		g_slist_free (function->args_list);
	}
	g_free (function);
}

GdaSqlFunction *
gda_sql_function_copy (GdaSqlFunction *function)
{
	GdaSqlFunction *copy;

	if (!function) return NULL;

	copy = gda_sql_function_new (NULL);
	if (function->function_name)
		copy->function_name = g_strdup (function->function_name);

	if (function->args_list) {
		GSList *list;
		for (list = function->args_list; list; list = list->next) {
			copy->args_list = g_slist_prepend (copy->args_list, 
							   gda_sql_expr_copy ((GdaSqlExpr *) list->data));
			gda_sql_any_part_set_parent (copy->args_list->data, copy);
		}
		copy->args_list = g_slist_reverse (copy->args_list);
	}	

	return copy;
}

gchar *
gda_sql_function_serialize (GdaSqlFunction *function)
{	
	if (!function) 
		return g_strdup ("null");
	else {
		GString *string;
		gchar *str;

		string = g_string_new ("{");

		g_string_append (string, "\"function_name\":");
		str = _json_quote_string (function->function_name);
		g_string_append (string, str);
		g_free (str);

		g_string_append (string, ",\"function_args\":");
		if (function->args_list) {
			GSList *list;
			g_string_append_c (string, '[');
			for (list = function->args_list; list; list = list->next) {
				if (list != function->args_list)
					g_string_append_c (string, ',');
				str = gda_sql_expr_serialize ((GdaSqlExpr*) list->data);
				g_string_append (string, str);
				g_free (str);
			}
			g_string_append_c (string, ']');
		}
		else
			g_string_append (string, "null");
		g_string_append_c (string, '}');
		str = string->str;
		g_string_free (string, FALSE);
		return str;
	}
}

void
gda_sql_function_take_name (GdaSqlFunction *function, GValue *value)
{
	if (value) {
		function->function_name = g_value_dup_string (value);
		g_value_reset (value);
		g_free (value);
	}
}

void gda_sql_function_take_args_list (GdaSqlFunction *function, GSList *args)
{
	GSList *list;
	function->args_list = args;

	for (list = args; list; list = list->next)
		gda_sql_any_part_set_parent (list->data, function);
}

/*
 * An operation
 */
GdaSqlOperation *
gda_sql_operation_new (GdaSqlAnyPart *parent)
{
	GdaSqlOperation *operation;
	operation = g_new0 (GdaSqlOperation, 1);
	GDA_SQL_ANY_PART(operation)->type = GDA_SQL_ANY_SQL_OPERATION;
	GDA_SQL_ANY_PART(operation)->parent = parent;
	return operation;
}

void
gda_sql_operation_free (GdaSqlOperation *operation)
{
	if (!operation) return;

	if (operation->operands) {
		g_slist_foreach (operation->operands, (GFunc) gda_sql_expr_free, NULL);
		g_slist_free (operation->operands);
	}
	g_free (operation);
}

GdaSqlOperation *
gda_sql_operation_copy (GdaSqlOperation *operation)
{
	GdaSqlOperation *copy;
	GSList *list;

	if (!operation) return NULL;

	copy = gda_sql_operation_new (NULL);
	copy->operator = operation->operator;
	
	for (list = operation->operands; list; list = list->next) {
		copy->operands = g_slist_prepend (copy->operands, 
						  gda_sql_expr_copy ((GdaSqlExpr*) list->data));
		gda_sql_any_part_set_parent (copy->operands->data, copy);
	}
	copy->operands = g_slist_reverse (copy->operands);

	return copy;
}

gchar *
gda_sql_operation_serialize (GdaSqlOperation *operation)
{	
	if (!operation) 
		return g_strdup ("null");
	else {
		GString *string;
		gchar *str;
		GSList *list;
		gint i = 0;

		string = g_string_new ("{");

		g_string_append (string, "\"operator\":");
		str = _json_quote_string (gda_sql_operation_operator_to_string (operation->operator));
		g_string_append (string, str);
		g_free (str);

		for (list = operation->operands; list; list = list->next) {
			g_string_append_printf (string, ",\"operand%d\":", i++);
			if (list->data) {
				str = gda_sql_expr_serialize ((GdaSqlExpr*) list->data);
				g_string_append (string, str);
				g_free (str);
			}
			else
				g_string_append (string, "null");
		}

		g_string_append_c (string, '}');
		str = string->str;
		g_string_free (string, FALSE);
		return str;
	}
}

const gchar *
gda_sql_operation_operator_to_string (GdaSqlOperator op)
{
	switch (op) {
	case GDA_SQL_OPERATOR_AND:
		return "AND";
	case GDA_SQL_OPERATOR_OR:
		return "OR";
	case GDA_SQL_OPERATOR_NOT:
		return "NOT";
	case GDA_SQL_OPERATOR_EQ:
		return "=";
	case GDA_SQL_OPERATOR_IS:
		return "IS";
	case GDA_SQL_OPERATOR_ISNULL:
		return "IS NULL";
	case GDA_SQL_OPERATOR_ISNOTNULL:
		return "IS NOT NULL";
	case GDA_SQL_OPERATOR_IN:
		return "IN";
	case GDA_SQL_OPERATOR_NOTIN:
		return "NOT IN";
	case GDA_SQL_OPERATOR_LIKE:
		return "LIKE";
	case GDA_SQL_OPERATOR_BETWEEN:
		return "BETWEEN";
	case GDA_SQL_OPERATOR_GT:
		return ">";
	case GDA_SQL_OPERATOR_LT:
		return "<";
	case GDA_SQL_OPERATOR_GEQ:
		return ">=";
	case GDA_SQL_OPERATOR_LEQ:
		return "<=";
	case GDA_SQL_OPERATOR_DIFF:
		return "!=";
	case GDA_SQL_OPERATOR_REGEXP:
		return "RE";
	case GDA_SQL_OPERATOR_REGEXP_CI:
		return "CI_RE";
	case GDA_SQL_OPERATOR_NOT_REGEXP:
		return "!RE";
	case GDA_SQL_OPERATOR_NOT_REGEXP_CI:
		return "!CI_RE";
	case GDA_SQL_OPERATOR_SIMILAR:
		return "SIMILAR TO";
	case GDA_SQL_OPERATOR_CONCAT:
		return "||";
	case GDA_SQL_OPERATOR_PLUS:
		return "+";
	case GDA_SQL_OPERATOR_MINUS:
		return "-";
	case GDA_SQL_OPERATOR_STAR:
		return "*";
	case GDA_SQL_OPERATOR_DIV:
		return "/";
	case GDA_SQL_OPERATOR_REM:
		return "%";
	case GDA_SQL_OPERATOR_BITAND:
		return "&";
	case GDA_SQL_OPERATOR_BITOR:
		return "|";
	case GDA_SQL_OPERATOR_BITNOT:
		return "~";
	default:
		g_error ("Unhandled operator constant %d\n", op);
		return NULL;
	}
}

GdaSqlOperator 
gda_sql_operation_operator_from_string (const gchar *op)
{
	switch (g_ascii_toupper (*op)) {
	case 'A':
		return GDA_SQL_OPERATOR_AND;
	case 'O':
		return GDA_SQL_OPERATOR_OR;
	case 'N':
		return GDA_SQL_OPERATOR_NOT;
	case '=':
		return GDA_SQL_OPERATOR_EQ;
	case 'I':
		if (op[1] == 'S')
			return GDA_SQL_OPERATOR_IS;
		else if (op[1] == 'N')
			return GDA_SQL_OPERATOR_IN;
		break;
	case 'L':
		return GDA_SQL_OPERATOR_LIKE;
	case 'B':
		return GDA_SQL_OPERATOR_BETWEEN;
	case '>':
		if (op[1] == '=')
			return GDA_SQL_OPERATOR_GEQ;
		else if (op[1] == 0)
			return GDA_SQL_OPERATOR_GT;
		break;
	case '<':
		if (op[1] == '=')
			return GDA_SQL_OPERATOR_LEQ;
		else if (op[1] == 0)
			return GDA_SQL_OPERATOR_LT;
		break;
	case '!':
		if (op[1] == '=')
			return GDA_SQL_OPERATOR_DIFF;
		else if (op[1] == 'R') 
			return GDA_SQL_OPERATOR_NOT_REGEXP;
		else
			return GDA_SQL_OPERATOR_NOT_REGEXP_CI;
	case 'R':
		return GDA_SQL_OPERATOR_REGEXP;
	case 'C':
		return GDA_SQL_OPERATOR_REGEXP_CI;
	case 'S':
		return GDA_SQL_OPERATOR_SIMILAR;
	case '|':
		if (op[1] == '|')
			return GDA_SQL_OPERATOR_CONCAT;
		else
			return GDA_SQL_OPERATOR_BITOR;
	case '+':
		return GDA_SQL_OPERATOR_PLUS;
	case '-':
		return GDA_SQL_OPERATOR_MINUS;
	case '*':
		return GDA_SQL_OPERATOR_STAR;
	case '/':
		return GDA_SQL_OPERATOR_DIV;
	case '%':
		return GDA_SQL_OPERATOR_REM;
	case '&':
		return GDA_SQL_OPERATOR_BITAND;
	}
	g_error ("Unhandled operator named '%s'\n", op);
	return 0;
}

/*
 * A CASE expression
 */
GdaSqlCase *
gda_sql_case_new (GdaSqlAnyPart *parent)
{
	GdaSqlCase *sc;
	sc = g_new0 (GdaSqlCase, 1);
	GDA_SQL_ANY_PART(sc)->type = GDA_SQL_ANY_SQL_CASE;
	GDA_SQL_ANY_PART(sc)->parent = parent;
	return sc;
}

void
gda_sql_case_free (GdaSqlCase *sc)
{
	if (!sc) return;

	gda_sql_expr_free (sc->base_expr);
	gda_sql_expr_free (sc->else_expr);
	if (sc->when_expr_list) {
		g_slist_foreach (sc->when_expr_list, (GFunc) gda_sql_expr_free, NULL);
		g_slist_free (sc->when_expr_list);
	}
	if (sc->then_expr_list) {
		g_slist_foreach (sc->then_expr_list, (GFunc) gda_sql_expr_free, NULL);
		g_slist_free (sc->then_expr_list);
	}
	g_free (sc);
}

GdaSqlCase *
gda_sql_case_copy (GdaSqlCase *sc)
{
	GdaSqlCase *copy;
	GSList *list;

	if (!sc) return NULL;

	copy = gda_sql_case_new (NULL);
	copy->base_expr = gda_sql_expr_copy (sc->base_expr);
	gda_sql_any_part_set_parent (copy->base_expr, copy);
	copy->else_expr = gda_sql_expr_copy (sc->else_expr);
	gda_sql_any_part_set_parent (copy->else_expr, copy);
	
	for (list = sc->when_expr_list; list; list = list->next) {
		copy->when_expr_list = g_slist_prepend (copy->when_expr_list, 
							gda_sql_expr_copy ((GdaSqlExpr*) list->data));
		gda_sql_any_part_set_parent (copy->when_expr_list->data, copy);
	}
	
	copy->when_expr_list = g_slist_reverse (copy->when_expr_list);
	for (list = sc->then_expr_list; list; list = list->next) {
		copy->then_expr_list = g_slist_prepend (copy->then_expr_list, 
						       gda_sql_expr_copy ((GdaSqlExpr*) list->data));
		gda_sql_any_part_set_parent (copy->then_expr_list->data, copy);
	}
	copy->then_expr_list = g_slist_reverse (copy->then_expr_list);

	return copy;
}

gchar *
gda_sql_case_serialize (GdaSqlCase *sc)
{	
	if (!sc) 
		return g_strdup ("null");
	else {
		GString *string;
		gchar *str;
		GSList *wlist, *tlist;

		string = g_string_new ("{");

		g_string_append (string, "\"base_expr\":");
		str = gda_sql_expr_serialize (sc->base_expr);
		g_string_append (string, str);
		g_free (str);

		g_string_append (string, ",\"body\":[");
		for (wlist = sc->when_expr_list, tlist = sc->then_expr_list; 
		     wlist && tlist; wlist = wlist->next, tlist = tlist->next) {
			if (wlist != sc->when_expr_list)
				g_string_append_c (string, ',');
			g_string_append_c (string, '{');
			g_string_append (string, "\"when\":");
			str = gda_sql_expr_serialize ((GdaSqlExpr*) wlist->data);
			g_string_append (string, str);
			g_free (str);
			g_string_append (string, ",\"then\":");
			str = gda_sql_expr_serialize ((GdaSqlExpr*) tlist->data);
			g_string_append (string, str);
			g_free (str);
			g_string_append_c (string, '}');
		}
		g_string_append_c (string, ']');
		g_assert (!wlist && !tlist);

		g_string_append (string, ",\"else_expr\":");
		str = gda_sql_expr_serialize (sc->else_expr);
		g_string_append (string, str);
		g_free (str);

		g_string_append_c (string, '}');
		str = string->str;
		g_string_free (string, FALSE);
		return str;
	}
}

/*
 * Any expression in a SELECT ... before the FROM
 */
GdaSqlSelectField *
gda_sql_select_field_new (GdaSqlAnyPart *parent)
{
	GdaSqlSelectField *field;
	field = g_new0 (GdaSqlSelectField, 1);
	GDA_SQL_ANY_PART(field)->type = GDA_SQL_ANY_SQL_SELECT_FIELD;
	GDA_SQL_ANY_PART(field)->parent = parent;
	return field;
}

void
gda_sql_select_field_free (GdaSqlSelectField *field)
{
	if (!field) return;

	gda_sql_select_field_check_clean (field);
	gda_sql_expr_free (field->expr);
	g_free (field->field_name);
	g_free (field->table_name);
	g_free (field->as);

	g_free (field);
}

GdaSqlSelectField *
gda_sql_select_field_copy (GdaSqlSelectField *field)
{
	GdaSqlSelectField *copy;

	if (!field) return NULL;

	copy = gda_sql_select_field_new (NULL);
	copy->expr = gda_sql_expr_copy (field->expr);
	gda_sql_any_part_set_parent (copy->expr, copy);

	if (field->field_name)
		copy->field_name = g_strdup (field->field_name);
	if (field->table_name)
		copy->table_name = g_strdup (field->table_name);
	if (field->as)
		copy->as = g_strdup (field->as);
	return copy;
}

gchar *
gda_sql_select_field_serialize (GdaSqlSelectField *field)
{	
	if (!field) 
		return g_strdup ("null");
	else {
		GString *string;
		gchar *str;

		string = g_string_new ("{");

		g_string_append (string, "\"expr\":");
		str = gda_sql_expr_serialize (field->expr);
		g_string_append (string, str);
		g_free (str);

		if (field->field_name) {
			g_string_append (string, ",\"field_name\":");
			str = _json_quote_string (field->field_name);
			g_string_append (string, str);
			g_free (str);
		}
		if (field->table_name) {
			g_string_append (string, ",\"table_name\":");
			str = _json_quote_string (field->table_name);
			g_string_append (string, str);
			g_free (str);
		}
		if (field->as) {
			g_string_append (string, ",\"as\":");
			str = _json_quote_string (field->as);
			g_string_append (string, str);
			g_free (str);
		}

		g_string_append_c (string, '}');
		str = string->str;
		g_string_free (string, FALSE);
		return str;
	}
}

void
gda_sql_select_field_take_expr (GdaSqlSelectField *field, GdaSqlExpr *expr)
{
	field->expr = expr;
	gda_sql_any_part_set_parent (field->expr, field);

	if (expr && expr->value) {
		const gchar *str;
		gchar *ptr;

		str = g_value_get_string (expr->value);
		if (_string_is_identifier (str)) {
			for (ptr = (gchar *) str; *ptr; ptr++);
			for (; (ptr > str) && (*ptr != '.'); ptr --);
			if (ptr != str) {
				field->field_name = g_strdup (ptr + 1);
				*ptr = 0;
				field->table_name = g_strdup (str);
				*ptr = '.';
			}
			else 
				field->field_name = g_strdup (ptr);
		}
	}
}

void
gda_sql_select_field_take_star_value (GdaSqlSelectField *field, GValue *value)
{
	if (value) {
		field->expr = gda_sql_expr_new (GDA_SQL_ANY_PART (field));
		field->expr->value = value;
	}
}

void
gda_sql_select_field_take_alias (GdaSqlSelectField *field, GValue *alias)
{
	if (alias) {
		field->as = g_value_dup_string (alias);
		g_value_reset (alias);
		g_free (alias);
	}
}

/*
 * Any TARGET ... in a SELECT statement
 */
GdaSqlSelectTarget *
gda_sql_select_target_new (GdaSqlAnyPart *parent)
{
	GdaSqlSelectTarget *target;
	target = g_new0 (GdaSqlSelectTarget, 1);
	GDA_SQL_ANY_PART(target)->type = GDA_SQL_ANY_SQL_SELECT_TARGET;
	GDA_SQL_ANY_PART(target)->parent = parent;
	return target;
}

void
gda_sql_select_target_free (GdaSqlSelectTarget *target)
{
	if (!target) return;

	gda_sql_select_target_check_clean (target);
	gda_sql_expr_free (target->expr);
	g_free (target->table_name);
	g_free (target->as);

	g_free (target);
}

GdaSqlSelectTarget *
gda_sql_select_target_copy (GdaSqlSelectTarget *target)
{
	GdaSqlSelectTarget *copy;

	if (!target) return NULL;

	copy = gda_sql_select_target_new (NULL);
	copy->expr = gda_sql_expr_copy (target->expr);
	gda_sql_any_part_set_parent (copy->expr, copy);

	if (target->table_name)
		copy->table_name = g_strdup (target->table_name);
	if (target->as)
		copy->as = g_strdup (target->as);
	return copy;
}

gchar *
gda_sql_select_target_serialize (GdaSqlSelectTarget *target)
{	
	if (!target) 
		return g_strdup ("null");
	else {
		GString *string;
		gchar *str;

		string = g_string_new ("{");

		g_string_append (string, "\"expr\":");
		str = gda_sql_expr_serialize (target->expr);
		g_string_append (string, str);
		g_free (str);

		if (target->table_name) {
			g_string_append (string, ",\"table_name\":");
			str = _json_quote_string (target->table_name);
			g_string_append (string, str);
			g_free (str);
		}
		if (target->as) {
			g_string_append (string, ",\"as\":");
			str = _json_quote_string (target->as);
			g_string_append (string, str);
			g_free (str);
		}

		g_string_append_c (string, '}');
		str = string->str;
		g_string_free (string, FALSE);
		return str;
	}
}

void
gda_sql_select_target_take_table_name (GdaSqlSelectTarget *target, GValue *value)
{
	if (value) {
		target->table_name = g_value_dup_string (value);
		target->expr = gda_sql_expr_new (GDA_SQL_ANY_PART (target));
		target->expr->value = value;
	}
}

void
gda_sql_select_target_take_select (GdaSqlSelectTarget *target, GdaSqlStatement *stmt)
{
	if (stmt) {
		target->expr = gda_sql_expr_new (GDA_SQL_ANY_PART (target));
		gda_sql_expr_take_select (target->expr, stmt);
	}
}

void
gda_sql_select_target_take_alias (GdaSqlSelectTarget *target, GValue *alias)
{
	if (alias) {
		target->as = g_value_dup_string (alias);
		g_value_reset (alias);
		g_free (alias);
	}
}

/*
 * Any JOIN ... in a SELECT statement
 */
GdaSqlSelectJoin *
gda_sql_select_join_new (GdaSqlAnyPart *parent)
{
	GdaSqlSelectJoin *join;
	join = g_new0 (GdaSqlSelectJoin, 1);
	GDA_SQL_ANY_PART(join)->type = GDA_SQL_ANY_SQL_SELECT_JOIN;
	GDA_SQL_ANY_PART(join)->parent = parent;
	return join;
}

void
gda_sql_select_join_free (GdaSqlSelectJoin *join)
{
	if (!join) return;

	gda_sql_expr_free (join->expr);
	g_slist_foreach (join->using, (GFunc) gda_sql_field_free, NULL);
	g_slist_free (join->using);

	g_free (join);
}

GdaSqlSelectJoin *
gda_sql_select_join_copy (GdaSqlSelectJoin *join)
{
	GdaSqlSelectJoin *copy;
	GSList *list;

	if (!join) return NULL;

	copy = gda_sql_select_join_new (NULL);
	copy->type = join->type;
	copy->position = join->position;

	copy->expr = gda_sql_expr_copy (join->expr);
	gda_sql_any_part_set_parent (copy->expr, copy);

	for (list = join->using; list; list = list->next) {
		copy->using = g_slist_prepend (copy->using,
					       gda_sql_field_copy ((GdaSqlField*) list->data));
		gda_sql_any_part_set_parent (copy->using->data, copy);
	}
	copy->using = g_slist_reverse (copy->using);

	return copy;
}

const gchar *
gda_sql_select_join_type_to_string (GdaSqlSelectJoinType type)
{
	switch (type) {
	case GDA_SQL_SELECT_JOIN_CROSS:
		return "CROSS";
	case GDA_SQL_SELECT_JOIN_NATURAL:
		return "NATURAL";
	case GDA_SQL_SELECT_JOIN_INNER:
		return "INNER";
	case GDA_SQL_SELECT_JOIN_LEFT:
		return "LEFT";
	case GDA_SQL_SELECT_JOIN_RIGHT:
		return "RIGHT";
	case GDA_SQL_SELECT_JOIN_FULL:
		return "FULL";
	default:
		g_error ("Unhandled join type constant %d\n", type);
		return NULL;
	}
}

gchar *
gda_sql_select_join_serialize (GdaSqlSelectJoin *join)
{	
	if (!join) 
		return g_strdup ("null");
	else {
		GString *string;
		gchar *str;

		string = g_string_new ("{");

		g_string_append (string, "\"join_type\":");
		g_string_append_c (string, '"');
		g_string_append (string, gda_sql_select_join_type_to_string (join->type));
		g_string_append_c (string, '"');

		g_string_append (string, ",\"join_pos\":");
		str = g_strdup_printf ("\"%d\"", join->position);
		g_string_append (string, str);
		g_free (str);

		if (join->expr) {
			g_string_append (string, ",\"on_cond\":");
			str = gda_sql_expr_serialize (join->expr);
			g_string_append (string, str);
			g_free (str);
		}

		if (join->using) {
			GSList *list;
			g_string_append (string, ",\"using\":");
			g_string_append_c (string, '[');
			for (list = join->using; list; list = list->next) {
				if (list != join->using)
					g_string_append_c (string, ',');
				str = gda_sql_field_serialize ((GdaSqlField*) list->data);
				g_string_append (string, str);
				g_free (str);
			}
			g_string_append_c (string, ']');
		}

		g_string_append_c (string, '}');
		str = string->str;
		g_string_free (string, FALSE);
		return str;
	}
}

/*
 * Any FROM ... in a SELECT statement
 */
GdaSqlSelectFrom *
gda_sql_select_from_new (GdaSqlAnyPart *parent)
{
	GdaSqlSelectFrom *from;
	from = g_new0 (GdaSqlSelectFrom, 1);
	GDA_SQL_ANY_PART(from)->type = GDA_SQL_ANY_SQL_SELECT_FROM;
	GDA_SQL_ANY_PART(from)->parent = parent;
	return from;
}

void
gda_sql_select_from_free (GdaSqlSelectFrom *from)
{
	if (!from) return;

	if (from->targets) {
		g_slist_foreach (from->targets, (GFunc) gda_sql_select_target_free, NULL);
		g_slist_free (from->targets);
	}
	if (from->joins) {
		g_slist_foreach (from->joins, (GFunc) gda_sql_select_join_free, NULL);
		g_slist_free (from->joins);
	}

	g_free (from);
}

GdaSqlSelectFrom *
gda_sql_select_from_copy (GdaSqlSelectFrom *from)
{
	GdaSqlSelectFrom *copy;
	GSList *list;

	if (!from) return NULL;

	copy = gda_sql_select_from_new (NULL);
	for (list = from->targets; list; list = list->next) {
		copy->targets = g_slist_prepend (copy->targets,
						 gda_sql_select_target_copy ((GdaSqlSelectTarget *) list->data));
		gda_sql_any_part_set_parent (copy->targets->data, copy);
	}
	copy->targets = g_slist_reverse (copy->targets);

	for (list = from->joins; list; list = list->next) {
		copy->joins = g_slist_prepend (copy->joins,
					       gda_sql_select_join_copy ((GdaSqlSelectJoin *) list->data));
		gda_sql_any_part_set_parent (copy->joins->data, copy);
	}
	copy->joins = g_slist_reverse (copy->joins);

	return copy;
}

gchar *
gda_sql_select_from_serialize (GdaSqlSelectFrom *from)
{	
	if (!from) 
		return g_strdup ("null");
	else {
		GString *string;
		gchar *str;
		GSList *list;

		string = g_string_new ("{");

		g_string_append (string, "\"targets\":");
		if (from->targets) {
			g_string_append_c (string, '[');
			for (list = from->targets; list; list = list->next) {
				if (list != from->targets)
					g_string_append_c (string, ',');
				str = gda_sql_select_target_serialize ((GdaSqlSelectTarget *) list->data);
				g_string_append (string, str);
				g_free (str);
			}
			g_string_append_c (string, ']');
		}
		else
			g_string_append (string, "null");

		if (from->joins) {
			g_string_append (string, ",\"joins\":");
			g_string_append_c (string, '[');
			for (list = from->joins; list; list = list->next) {
				if (list != from->joins)
					g_string_append_c (string, ',');
				str = gda_sql_select_join_serialize ((GdaSqlSelectJoin *) list->data);
				g_string_append (string, str);
				g_free (str);
			}
			g_string_append_c (string, ']');
		}

		g_string_append_c (string, '}');
		str = string->str;
		g_string_free (string, FALSE);
		return str;
	}
}

void
gda_sql_select_from_take_new_target (GdaSqlSelectFrom *from, GdaSqlSelectTarget *target)
{
	from->targets = g_slist_append (from->targets, target);
	gda_sql_any_part_set_parent (target, from);
}

void
gda_sql_select_from_take_new_join  (GdaSqlSelectFrom *from, GdaSqlSelectJoin *join)
{
	from->joins = g_slist_append (from->joins, join);
	gda_sql_any_part_set_parent (join, from);
}

/*
 * Any expression in a SELECT ... after the ORDER BY
 */
GdaSqlSelectOrder *
gda_sql_select_order_new (GdaSqlAnyPart *parent)
{
	GdaSqlSelectOrder *order;
	order = g_new0 (GdaSqlSelectOrder, 1);
	GDA_SQL_ANY_PART(order)->type = GDA_SQL_ANY_SQL_SELECT_ORDER;
	GDA_SQL_ANY_PART(order)->parent = parent;
	return order;
}

void
gda_sql_select_order_free (GdaSqlSelectOrder *order)
{
	if (!order) return;

	gda_sql_expr_free (order->expr);
	g_free (order->collation_name);

	g_free (order);
}

GdaSqlSelectOrder *
gda_sql_select_order_copy (GdaSqlSelectOrder *order)
{
	GdaSqlSelectOrder *copy;

	if (!order) return NULL;

	copy = gda_sql_select_order_new (NULL);

	copy->expr = gda_sql_expr_copy (order->expr);
	gda_sql_any_part_set_parent (copy->expr, copy);

	if (order->collation_name)
		copy->collation_name = g_strdup (order->collation_name);
	copy->asc = order->asc;
	return copy;
}

gchar *
gda_sql_select_order_serialize (GdaSqlSelectOrder *order)
{	
	if (!order) 
		return g_strdup ("null");
	else {
		GString *string;
		gchar *str;

		string = g_string_new ("{");

		g_string_append (string, "\"expr\":");
		str = gda_sql_expr_serialize (order->expr);
		g_string_append (string, str);
		g_free (str);

		g_string_append (string, ",\"sort\":");
		g_string_append (string, order->asc ? "\"ASC\"" : "\"DESC\"");

		if (order->collation_name) {
			g_string_append (string, ",\"collation\":");
			str = _json_quote_string (order->collation_name);
			g_string_append (string, str);
			g_free (str);
		}

		g_string_append_c (string, '}');
		str = string->str;
		g_string_free (string, FALSE);
		return str;
	}
}

