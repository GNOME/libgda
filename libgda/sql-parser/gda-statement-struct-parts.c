/*
 * Copyright (C) 2008 - 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2008 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 - 2011 Murray Cumming <murrayc@murrayc.com>
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

GType
gda_sql_expr_get_type (void)
{
	static GType our_type = 0;

	if (our_type == 0)
		our_type = g_boxed_type_register_static ("GdaSqlExpr",
			(GBoxedCopyFunc) gda_sql_expr_copy,
			(GBoxedFreeFunc) gda_sql_expr_free);
	return our_type;
}


/**
 * gda_sql_expr_new
 * @parent: a #GdaSqlStatementInsert, #GdaSqlStatementUpdate, #GdaSqlSelectField, #GdaSqlSelectTarget, #GdaSqlOperation
 *
 * Creates a new #GdaSqlField structure, using @parent as its parent part.
 *
 * Returns: a new #GdaSqlField structure.
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

/**
 * gda_sql_expr_free
 * @expr: a #GdaSqlExpr to be freed.
 *
 * Frees a #GdaSqlExpr structure and its members.
 *
 */
void
gda_sql_expr_free (GdaSqlExpr *expr)
{
	if (!expr) return;

	_gda_sql_expr_check_clean (expr);
	gda_value_free (expr->value);
	gda_sql_param_spec_free (expr->param_spec);
	gda_sql_function_free (expr->func);
	gda_sql_operation_free (expr->cond);
	if (expr->select) {
		if (GDA_SQL_ANY_PART (expr->select)->type == GDA_SQL_ANY_STMT_SELECT)
			_gda_sql_statement_select_free (expr->select);
		else if (GDA_SQL_ANY_PART (expr->select)->type == GDA_SQL_ANY_STMT_COMPOUND)
			_gda_sql_statement_compound_free (expr->select);
		else
			g_assert_not_reached ();
	}
	gda_sql_case_free (expr->case_s);
	g_free (expr->cast_as);
	g_free (expr);
}

/**
 * gda_sql_expr_copy
 * @expr: a #GdaSqlExpr
 *
 * Creates a new #GdaSqlExpr structure initiated with the values stored in @expr.
 *
 * Returns: a new #GdaSqlExpr structure.
 */
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
			copy->select = _gda_sql_statement_select_copy (expr->select);
		else if (GDA_SQL_ANY_PART (expr->select)->type == GDA_SQL_ANY_STMT_COMPOUND)
			copy->select = _gda_sql_statement_compound_copy (expr->select);
		else
			g_assert_not_reached ();
		gda_sql_any_part_set_parent (copy->select, copy);
	}

	copy->case_s = gda_sql_case_copy (expr->case_s);
	gda_sql_any_part_set_parent (copy->case_s, copy);

	if (expr->cast_as)
		copy->cast_as = g_strdup (expr->cast_as);

	copy->value_is_ident = expr->value_is_ident;
	return copy;
}

/**
 * gda_sql_expr_serialize
 * @expr: a #GdaSqlExpr structure
 *
 * Creates a new string representation of the SQL expression. You need to free the returned string
 * using g_free();
 *
 * Returns: a new string with the SQL expression or "null" in case @expr is invalid.
 */
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
			str = _gda_sql_statement_select_serialize (expr->select);
		else if (GDA_SQL_ANY_PART (expr->select)->type == GDA_SQL_ANY_STMT_COMPOUND)
			str = _gda_sql_statement_compound_serialize (expr->select);
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

	if (expr->value_is_ident) {
		str = _json_quote_string (expr->cast_as);
		g_string_append (string, ",\"sqlident\":\"TRUE\"");
		g_free (str);
	}

	g_string_append_c (string, '}');
	str = string->str;
	g_string_free (string, FALSE);
	return str;
}

/**
 * gda_sql_expr_take_select
 * @expr: a #GdaSqlExpr structure
 * @stmt: a #GdaSqlStatement holding the #GdaSqlStatementSelect to take from
 *
 * Sets the expression's parent to the #GdaSqlStatementSelect held by @stmt. After
 * calling this function @stmt is freed.
 *
 */
void
gda_sql_expr_take_select (GdaSqlExpr *expr, GdaSqlStatement *stmt)
{
	if (stmt) {
		GdaSqlAnyPart *part;
		part = GDA_SQL_ANY_PART (stmt->contents);
		stmt->contents = NULL;
		gda_sql_statement_free (stmt);
		expr->select = _gda_sql_statement_compound_reduce (part);
		gda_sql_any_part_set_parent (expr->select, expr);
	}
}

/**
 * gda_sql_field_new
 * @parent: a #GdaSqlStatementSelect, #GdaSqlStatementInsert, #GdaSqlStatementDelete, #GdaSqlStatementUpdate
 *
 * Creates a new #GdaSqlField structure, using @parent as its parent part.
 *
 * Returns: a new #GdaSqlField structure.
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

/**
 * gda_sql_field_free
 * @field: a #GdaSqlField to be freed.
 *
 * Frees a #GdaSqlField structure and its members.
 *
 */
void
gda_sql_field_free (GdaSqlField *field)
{
	if (!field) return;

	_gda_sql_field_check_clean (field);
	g_free (field->field_name);
	g_free (field);
}

/**
 * gda_sql_field_copy
 * @field: a #GdaSqlAnyPart
 *
 * Creates a new GdaSqlField structure initiated with the values stored in @field.
 *
 * Returns: a new #GdaSqlField structure.
 */
GdaSqlField *
gda_sql_field_copy (GdaSqlField *field)
{
	GdaSqlField *copy;
	if (!field) return NULL;

	copy = gda_sql_field_new (NULL);
	if (field->field_name)
		copy->field_name = g_strdup (field->field_name);
	copy->validity_meta_table_column = field->validity_meta_table_column;

	return copy;
}


G_DEFINE_BOXED_TYPE(GdaSqlField, gda_sql_field, gda_sql_field_copy, gda_sql_field_free)

/**
 * gda_sql_field_serialize
 * @field: a #GdaSqlField structure
 *
 * Creates a new string representing a field. You need to free the returned string
 * using g_free();
 *
 * Returns: a new string with the name of the field or "null" in case @field is invalid.
 */
gchar *
gda_sql_field_serialize (GdaSqlField *field)
{
	if (!field)
		return g_strdup ("null");
	else
		return _json_quote_string (field->field_name);
}

/**
 * gda_sql_field_take_name
 * @field: a #GdaSqlField structure
 * @value: a #GValue holding a string to take from
 *
 * Sets the field's name using the string held by @value. When call, @value is freed using
 * #gda_value_free().
 *
 */
void
gda_sql_field_take_name (GdaSqlField *field, GValue *value)
{
	if (value) {
		field->field_name = g_value_dup_string (value);
		gda_value_free (value);
	}
}

/**
 * gda_sql_table_new
 * @parent: a #GdaSqlStatementSelect, #GdaSqlStatementInsert, #GdaSqlStatementDelete, #GdaSqlStatementUpdate
 *
 * Creates a new #GdaSqlTable structure, using @parent as its parent part.
 *
 * Returns: a new #GdaSqlTable structure.
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

/**
 * gda_sql_table_free
 * @table: a #GdaSqlTable structure to be freed
 *
 * Frees a #GdaSqlTable structure and its members.
 */
void
gda_sql_table_free (GdaSqlTable *table)
{
	if (!table) return;

	_gda_sql_table_check_clean (table);
	g_free (table->table_name);
	g_free (table);
}

/**
 * gda_sql_table_copy
 * @table: a #GdaSqlTable structure to be copied
 *
 * Creates a new #GdaSqlTable structure initiated with the values stored in @table.
 *
 * Returns: (transfer full): a new #GdaSqlTable structure.
 */
GdaSqlTable *
gda_sql_table_copy (GdaSqlTable *table)
{
	GdaSqlTable *copy;
	if (!table) return NULL;

	copy = gda_sql_table_new (NULL);
	if (table->table_name)
		copy->table_name = g_strdup (table->table_name);
	copy->validity_meta_object = table->validity_meta_object;

	return copy;
}

G_DEFINE_BOXED_TYPE(GdaSqlTable, gda_sql_table, gda_sql_table_copy, gda_sql_table_free)

/**
 * gda_sql_table_serialize:
 * @table: a #GdaSqlTable structure
 *
 * Creates a new string representing a table. You need to free the returned string
 * using g_free();
 *
 * Returns: (transfer full): a new string with the name of the field or "null" in case @table is invalid.
 */
gchar *
gda_sql_table_serialize (GdaSqlTable *table)
{
	if (!table)
		return g_strdup ("null");
	else
		return _json_quote_string (table->table_name);
}

/**
 * gda_sql_table_take_name:
 * @table: a #GdaSqlTable structure
 * @value: a #GValue holding a string to take from
 *
 * Sets the table's name using the string held by @value. When call, @value is freed using
 * gda_value_free().
 *
 */
void
gda_sql_table_take_name (GdaSqlTable *table, GValue *value)
{
	if (value) {
		table->table_name = g_value_dup_string (value);
		gda_value_free (value);
	}
}

/**
 * gda_sql_function_new
 * @parent: a #GdaSqlAnyPart structure
 *
 * Creates a new #GdaSqlFunction structure initiated.
 *
 * Returns: a new #GdaSqlFunction structure.
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

/**
 * gda_sql_function_free
 * @function: a #GdaSqlFunction structure to be freed
 *
 * Frees a #GdaSqlFunction structure and its members.
 */
void
gda_sql_function_free (GdaSqlFunction *function)
{
	if (!function) return;

	g_free (function->function_name);
	if (function->args_list) {
		g_slist_free_full (function->args_list, (GDestroyNotify) gda_sql_expr_free);
	}
	g_free (function);
}

/**
 * gda_sql_function_copy
 * @function: a #GdaSqlFunction structure to be copied
 *
 * Creates a new #GdaSqlFunction structure initiated with the values stored in @function.
 *
 * Returns: (transfer full): a new #GdaSqlFunction structure.
 */
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


G_DEFINE_BOXED_TYPE(GdaSqlFunction, gda_sql_function, gda_sql_function_copy, gda_sql_function_free)

/**
 * gda_sql_function_serialize
 * @function: a #GdaSqlFunction structure
 *
 * Creates a new string representing a function. You need to free the returned string
 * using g_free();
 *
 * Returns: a new string with the description of the function or "null" in case @function is invalid.
 */
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

/**
 * gda_sql_function_take_name
 * @function: a #GdaSqlFunction structure
 * @value: a #GValue holding a string to take from
 *
 * Sets the function's name using the string held by @value. When call, @value is freed using
 * #gda_value_free().
 */
void
gda_sql_function_take_name (GdaSqlFunction *function, GValue *value)
{
	if (value) {
		function->function_name = g_value_dup_string (value);
		gda_value_free (value);
	}
}

/**
 * gda_sql_function_take_args_list
 * @function: a #GdaSqlFunction structure
 * @args: (element-type Gda.SqlExpr): a #GSList to take from
 *
 * Sets the function's arguments to point to @args, then sets the
 * list's data elements' parent to @function.
 *
 */
void
gda_sql_function_take_args_list (GdaSqlFunction *function, GSList *args)
{
	GSList *list;
	function->args_list = args;

	for (list = args; list; list = list->next)
		gda_sql_any_part_set_parent (list->data, function);
}

/**
 * gda_sql_operation_new
 * @parent: a #GdaSqlAnyPart structure
 *
 * Creates a new #GdaSqlOperation structure and sets its parent to @parent.
 *
 * Returns: a new #GdaSqlOperation structure.
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

/**
 * gda_sql_operation_free
 * @operation: a #GdaSqlOperation structure to be freed
 *
 * Frees a #GdaSqlOperation structure and its members.
 */
void
gda_sql_operation_free (GdaSqlOperation *operation)
{
	if (!operation) return;

	if (operation->operands) {
		g_slist_free_full (operation->operands, (GDestroyNotify) gda_sql_expr_free);
	}
	g_free (operation);
}

/**
 * gda_sql_operation_copy
 * @operation: a #GdaSqlOperation structure to be copied
 *
 * Creates a new #GdaSqlOperation structure initiated with the values stored in @operation.
 *
 * Returns: a new #GdaSqlOperation structure.
 */
GdaSqlOperation *
gda_sql_operation_copy (GdaSqlOperation *operation)
{
	GdaSqlOperation *copy;
	GSList *list;

	if (!operation) return NULL;

	copy = gda_sql_operation_new (NULL);
	copy->operator_type = operation->operator_type;

	for (list = operation->operands; list; list = list->next) {
		copy->operands = g_slist_prepend (copy->operands,
						  gda_sql_expr_copy ((GdaSqlExpr*) list->data));
		gda_sql_any_part_set_parent (copy->operands->data, copy);
	}
	copy->operands = g_slist_reverse (copy->operands);

	return copy;
}


G_DEFINE_BOXED_TYPE(GdaSqlOperation, gda_sql_operation, gda_sql_operation_copy, gda_sql_operation_free)

/**
 * gda_sql_operation_serialize
 * @operation: a #GdaSqlOperation structure
 *
 * Creates a new string representing an operator. You need to free the returned string
 * using g_free();
 *
 * Returns: a new string with the description of the operator or "null" in case @operation is invalid.
 */
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
		str = _json_quote_string (gda_sql_operation_operator_to_string (operation->operator_type));
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

/**
 * gda_sql_operation_operator_to_string
 * @op: a #GdaSqlOperation structure
 *
 * Returns a constant string representing a operator name. You don't need to free
 * the returned string.
 *
 * Returns: a string with the operator's name or NULL in case @op is invalid.
 */
const gchar *
gda_sql_operation_operator_to_string (GdaSqlOperatorType op)
{
	switch (op) {
	case GDA_SQL_OPERATOR_TYPE_AND:
		return "AND";
	case GDA_SQL_OPERATOR_TYPE_OR:
		return "OR";
	case GDA_SQL_OPERATOR_TYPE_NOT:
		return "NOT";
	case GDA_SQL_OPERATOR_TYPE_EQ:
		return "=";
	case GDA_SQL_OPERATOR_TYPE_IS:
		return "IS";
	case GDA_SQL_OPERATOR_TYPE_ISNULL:
		return "IS NULL";
	case GDA_SQL_OPERATOR_TYPE_ISNOTNULL:
		return "IS NOT NULL";
	case GDA_SQL_OPERATOR_TYPE_IN:
		return "IN";
	case GDA_SQL_OPERATOR_TYPE_NOTIN:
		return "NOT IN";
	case GDA_SQL_OPERATOR_TYPE_LIKE:
		return "LIKE";
        /* ILIKE is a PostgreSQL extension for case-insensitive comparison.
         * See http://developer.postgresql.org/pgdocs/postgres/functions-matching.html
         */ 
	case GDA_SQL_OPERATOR_TYPE_ILIKE:
		return "ILIKE";
	case GDA_SQL_OPERATOR_TYPE_BETWEEN:
		return "BETWEEN";
	case GDA_SQL_OPERATOR_TYPE_GT:
		return ">";
	case GDA_SQL_OPERATOR_TYPE_LT:
		return "<";
	case GDA_SQL_OPERATOR_TYPE_GEQ:
		return ">=";
	case GDA_SQL_OPERATOR_TYPE_LEQ:
		return "<=";
	case GDA_SQL_OPERATOR_TYPE_DIFF:
		return "!=";
	case GDA_SQL_OPERATOR_TYPE_REGEXP:
		return "RE";
	case GDA_SQL_OPERATOR_TYPE_REGEXP_CI:
		return "CI_RE";
	case GDA_SQL_OPERATOR_TYPE_NOT_REGEXP:
		return "!RE";
	case GDA_SQL_OPERATOR_TYPE_NOT_REGEXP_CI:
		return "!CI_RE";
	case GDA_SQL_OPERATOR_TYPE_SIMILAR:
		return "SIMILAR TO";
	case GDA_SQL_OPERATOR_TYPE_CONCAT:
		return "||";
	case GDA_SQL_OPERATOR_TYPE_PLUS:
		return "+";
	case GDA_SQL_OPERATOR_TYPE_MINUS:
		return "-";
	case GDA_SQL_OPERATOR_TYPE_STAR:
		return "*";
	case GDA_SQL_OPERATOR_TYPE_DIV:
		return "/";
	case GDA_SQL_OPERATOR_TYPE_REM:
		return "%";
	case GDA_SQL_OPERATOR_TYPE_BITAND:
		return "&";
	case GDA_SQL_OPERATOR_TYPE_BITOR:
		return "|";
	case GDA_SQL_OPERATOR_TYPE_BITNOT:
		return "~";
	case GDA_SQL_OPERATOR_TYPE_NOTLIKE:
		return "NOT LIKE";
	case GDA_SQL_OPERATOR_TYPE_NOTILIKE:
		return "NOT ILIKE";
	default:
		g_error ("Unhandled operator constant %d\n", op);
		return NULL;
	}
}

/**
 * gda_sql_operation_operator_from_string
 * @op: a #GdaSqlOperation structure
 *
 * Returns #GdaSqlOperatorType that correspond with the string @op.
 *
 * Returns: #GdaSqlOperatorType
 */
GdaSqlOperatorType
gda_sql_operation_operator_from_string (const gchar *op)
{
	switch (g_ascii_toupper (*op)) {
	case 'A':
		return GDA_SQL_OPERATOR_TYPE_AND;
	case 'O':
		return GDA_SQL_OPERATOR_TYPE_OR;
	case 'N':
		return GDA_SQL_OPERATOR_TYPE_NOT;
	case '=':
		return GDA_SQL_OPERATOR_TYPE_EQ;
	case 'I':
		if (op[1] == 'S')
			return GDA_SQL_OPERATOR_TYPE_IS;
		else if (op[1] == 'N')
			return GDA_SQL_OPERATOR_TYPE_IN;
		else if (op[1] == 'L')
			return GDA_SQL_OPERATOR_TYPE_ILIKE;
		break;
	case 'L':
		return GDA_SQL_OPERATOR_TYPE_LIKE;
	case 'B':
		return GDA_SQL_OPERATOR_TYPE_BETWEEN;
	case '>':
		if (op[1] == '=')
			return GDA_SQL_OPERATOR_TYPE_GEQ;
		else if (op[1] == 0)
			return GDA_SQL_OPERATOR_TYPE_GT;
		break;
	case '<':
		if (op[1] == '=')
			return GDA_SQL_OPERATOR_TYPE_LEQ;
		else if (op[1] == 0)
			return GDA_SQL_OPERATOR_TYPE_LT;
		break;
	case '!':
		if (op[1] == '=')
			return GDA_SQL_OPERATOR_TYPE_DIFF;
		else if (op[1] == 'R')
			return GDA_SQL_OPERATOR_TYPE_NOT_REGEXP;
		else
			return GDA_SQL_OPERATOR_TYPE_NOT_REGEXP_CI;
	case 'R':
		return GDA_SQL_OPERATOR_TYPE_REGEXP;
	case 'C':
		return GDA_SQL_OPERATOR_TYPE_REGEXP_CI;
	case 'S':
		return GDA_SQL_OPERATOR_TYPE_SIMILAR;
	case '|':
		if (op[1] == '|')
			return GDA_SQL_OPERATOR_TYPE_CONCAT;
		else
			return GDA_SQL_OPERATOR_TYPE_BITOR;
	case '+':
		return GDA_SQL_OPERATOR_TYPE_PLUS;
	case '-':
		return GDA_SQL_OPERATOR_TYPE_MINUS;
	case '*':
		return GDA_SQL_OPERATOR_TYPE_STAR;
	case '/':
		return GDA_SQL_OPERATOR_TYPE_DIV;
	case '%':
		return GDA_SQL_OPERATOR_TYPE_REM;
	case '&':
		return GDA_SQL_OPERATOR_TYPE_BITAND;
	}
	g_error ("Unhandled operator named '%s'\n", op);
	return 0;
}

/**
 * gda_sql_case_new
 * @parent: a #GdaSqlAnyPart structure
 *
 * Creates a new #GdaSqlCase structure and sets its parent to @parent.
 *
 * Returns: a new #GdaSqlCase structure.
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

/**
 * gda_sql_case_free
 * @sc: a #GdaSqlCase structure to be freed
 *
 * Frees a #GdaSqlCase structure and its members.
 */
void
gda_sql_case_free (GdaSqlCase *sc)
{
	if (!sc) return;

	gda_sql_expr_free (sc->base_expr);
	gda_sql_expr_free (sc->else_expr);
	if (sc->when_expr_list) {
		g_slist_free_full (sc->when_expr_list, (GDestroyNotify) gda_sql_expr_free);
	}
	if (sc->then_expr_list) {
		g_slist_free_full (sc->then_expr_list, (GDestroyNotify) gda_sql_expr_free);
	}
	g_free (sc);
}

/**
 * gda_sql_case_copy
 * @sc: a #GdaSqlCase structure to be copied
 *
 * Creates a new #GdaSqlCase structure initiated with the values stored in @sc.
 *
 * Returns: a new #GdaSqlCase structure.
 */
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

G_DEFINE_BOXED_TYPE(GdaSqlCase, gda_sql_case, gda_sql_case_copy, gda_sql_case_free)

/**
 * gda_sql_case_serialize
 * @sc: a #GdaSqlCase structure
 *
 * Creates a new string representing a CASE clause. You need to free the returned string
 * using g_free();
 *
 * Returns: a new string with the description of the CASE clause or "null" in case @sc is invalid.
 */
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

/**
 * gda_sql_select_field_new
 * @parent: a #GdaSqlStatementSelect structure
 *
 * Creates a new #GdaSqlSelectField structure and sets its parent to @parent. A
 * #GdaSqlSelectField is any expression in SELECT statements before the FROM clause.
 *
 * Returns: a new #GdaSqlSelectField structure.
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

/**
 * gda_sql_select_field_free
 * @field: a #GdaSqlSelectField structure to be freed
 *
 * Frees a #GdaSqlSelectField structure and its members.
 */
void
gda_sql_select_field_free (GdaSqlSelectField *field)
{
	if (!field) return;

	_gda_sql_select_field_check_clean (field);
	gda_sql_expr_free (field->expr);
	g_free (field->field_name);
	g_free (field->table_name);
	g_free (field->as);

	g_free (field);
}

/**
 * gda_sql_select_field_copy
 * @field: a #GdaSqlSelectField structure to be copied
 *
 * Creates a new #GdaSqlSelectField structure initiated with the values stored in @field.
 *
 * Returns: (transfer full): a new #GdaSqlSelectField structure.
 */
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

	copy->validity_meta_object = field->validity_meta_object;
	copy->validity_meta_table_column = field->validity_meta_table_column;

	return copy;
}

G_DEFINE_BOXED_TYPE(GdaSqlSelectField, gda_sql_select_field, gda_sql_select_field_copy, gda_sql_select_field_free)

/**
 * gda_sql_select_field_serialize
 * @field: a #GdaSqlSelectField structure
 *
 * Creates a new string representing an expression used as field in a SELECT statement
 * before the FROM clause.
 *
 * Returns: a new string with the description of the expression or "null" in case @field is invalid.
 */
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

/**
 * gda_sql_select_field_take_expr
 * @field: a #GdaSqlSelectField structure
 * @expr: a #GdaSqlExpr to take from
 *
 * Sets the expression field in the #GdaSqlSelectField structure to point to @expr
 * and modify it to sets its parent to @field.
 */
void
gda_sql_select_field_take_expr (GdaSqlSelectField *field, GdaSqlExpr *expr)
{
	field->expr = expr;
	g_assert (GDA_SQL_ANY_PART (expr)->type == GDA_SQL_ANY_EXPR);
	gda_sql_any_part_set_parent (field->expr, field);

	if (expr && expr->value) {
		const gchar *dup;
		dup = g_value_get_string (expr->value);
		if (dup && *dup)
			_split_identifier_string (g_strdup (dup), &(field->table_name), &(field->field_name));
	}
}

/**
 * gda_sql_select_field_take_star_value
 * @field: a #GdaSqlSelectField structure
 * @value: a #GValue to take from
 *
 * Sets the expression field's value in the #GdaSqlSelectField structure to point to @value;
 * after this @field is the owner of @value.
 *
 */
void
gda_sql_select_field_take_star_value (GdaSqlSelectField *field, GValue *value)
{
	if (value) {
		field->expr = gda_sql_expr_new (GDA_SQL_ANY_PART (field));
		field->expr->value = value;
	}
}

/**
 * gda_sql_select_field_take_alias
 * @field: a #GdaSqlSelectField structure
 * @alias: a #GValue to take from
 *
 * Sets the 'as' field's string in the #GdaSqlSelectField structure. @alias is freed
 * after call this function.
 *
 */
void
gda_sql_select_field_take_alias (GdaSqlSelectField *field, GValue *alias)
{
	if (alias) {
		field->as = g_value_dup_string (alias);
		gda_value_free (alias);
	}
}

/**
 * gda_sql_select_target_new
 * @parent: a #GdaSqlSelectFrom
 *
 * Creates a new #GdaSqlSelectTarget structure and sets its parent to @parent. A
 * #GdaSqlSelectTarget is the table in a SELECT statement.
 *
 * Returns: a new #GdaSqlSelectTarget structure.
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

/**
 * gda_sql_select_target_free
 * @target: a #GdaSqlSelectTarget structure to be freed
 *
 * Frees a #GdaSqlSelectTarget structure and its members.
 */
void
gda_sql_select_target_free (GdaSqlSelectTarget *target)
{
	if (!target) return;

	_gda_sql_select_target_check_clean (target);
	gda_sql_expr_free (target->expr);
	g_free (target->table_name);
	g_free (target->as);

	g_free (target);
}

/**
 * gda_sql_select_target_copy
 * @target: a #GdaSqlSelectTarget structure to be copied
 *
 * Creates a new #GdaSqlSelectTarget structure initiated with the values stored in @target.
 *
 * Returns: a new #GdaSqlSelectTarget structure.
 */
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

	copy->validity_meta_object = target->validity_meta_object;

	return copy;
}

G_DEFINE_BOXED_TYPE(GdaSqlSelectTarget, gda_sql_select_target, gda_sql_select_target_copy, gda_sql_select_target_free)

/**
 * gda_sql_select_target_serialize
 * @target: a #GdaSqlSelectTarget structure
 *
 * Creates a new string representing a target used in a SELECT statement
 * after the FROM clause.
 *
 * Returns: a new string with the description of the expression or "null" in case @field is invalid.
 */
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

/**
 * gda_sql_select_target_take_table_name:
 * @target: a #GdaSqlSelectTarget structure
 * @value: a #GValue to take from
 *
 * Sets the target's name using the string stored in @value and the expression
 * to set its value to point to value; after call this function the target owns
 * @value, then you must not free it.
 */
void
gda_sql_select_target_take_table_name (GdaSqlSelectTarget *target, GValue *value)
{
	if (value) {
		target->table_name = g_value_dup_string (value);
		target->expr = gda_sql_expr_new (GDA_SQL_ANY_PART (target));
		target->expr->value = value;
	}
}

/**
 * gda_sql_select_target_take_select:
 * @target: a #GdaSqlSelectTarget structure
 * @stmt: a #GValue to take from
 *
 * Sets the target to be a SELECT subquery setting target's expression to use
 * @stmt; after call this function the target owns @stmt, then you must not free it.
 */
void
gda_sql_select_target_take_select (GdaSqlSelectTarget *target, GdaSqlStatement *stmt)
{
	if (stmt) {
		target->expr = gda_sql_expr_new (GDA_SQL_ANY_PART (target));
		gda_sql_expr_take_select (target->expr, stmt);
	}
}

/**
 * gda_sql_select_target_take_table_alias
 * @target: a #GdaSqlSelectTarget structure
 * @alias: a #GValue holding the alias string to take from
 *
 * Sets the target alias (AS) to the string held by @alias; after call
 * this function @alias is freed.
 */
void
gda_sql_select_target_take_alias (GdaSqlSelectTarget *target, GValue *alias)
{
	if (alias) {
		target->as = g_value_dup_string (alias);
		gda_value_free (alias);
	}
}

/*
 * Any JOIN ... in a SELECT statement
 */

/**
 * gda_sql_select_join_new
 * @parent: a #GdaSqlSelectFrom
 *
 * Creates a new #GdaSqlSelectJoin structure and sets its parent to @parent.
 *
 * Returns: a new #GdaSqlSelectJoin structure
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

/**
 * gda_sql_select_join_free
 * @join: a #GdaSqlSelectJoin structure to be freed
 *
 * Frees a #GdaSqlSelectJoin structure and its members.
 */
void
gda_sql_select_join_free (GdaSqlSelectJoin *join)
{
	if (!join) return;

	gda_sql_expr_free (join->expr);
	g_slist_free_full (join->use, (GDestroyNotify) gda_sql_field_free);

	g_free (join);
}

/**
 * gda_sql_select_join_copy
 * @join: a #GdaSqlSelectJoin structure to be copied
 *
 * Creates a new #GdaSqlSelectJoin structure initiated with the values stored in @join.
 *
 * Returns: a new #GdaSqlSelectJoin structure.
 */
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

	for (list = join->use; list; list = list->next) {
		copy->use = g_slist_prepend (copy->use,
					       gda_sql_field_copy ((GdaSqlField*) list->data));
		gda_sql_any_part_set_parent (copy->use->data, copy);
	}
	copy->use = g_slist_reverse (copy->use);

	return copy;
}

G_DEFINE_BOXED_TYPE(GdaSqlSelectJoin, gda_sql_select_join, gda_sql_select_join_copy, gda_sql_select_join_free)

/**
 * gda_sql_select_join_type_to_string
 * @type: a #GdaSqlSelectJoinType structure to be copied
 *
 * Creates a new string representing the join type.
 *
 * Returns: a string representing the Join type.
 */
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

/**
 * gda_sql_select_join_serialize
 * @join: a #GdaSqlSelectJoin structure
 *
 * Creates a new string description of the join used in a SELECT statement.
 *
 * Returns: a new string with the description of the join or "null" in case @join is invalid.
 */
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

		if (join->use) {
			GSList *list;
			g_string_append (string, ",\"using\":");
			g_string_append_c (string, '[');
			for (list = join->use; list; list = list->next) {
				if (list != join->use)
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

/**
 * gda_sql_select_from_new
 * @parent: a #GdaSqlStatementSelect
 *
 * Creates a new #GdaSqlSelectFrom structure and sets its parent to @parent.
 *
 * Returns: a new #GdaSqlSelectFrom structure
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

/**
 * gda_sql_select_from_free
 * @from: a #GdaSqlSelectFrom structure to be freed
 *
 * Frees a #GdaSqlSelectFrom structure and its members.
 */
void
gda_sql_select_from_free (GdaSqlSelectFrom *from)
{
	if (!from) return;

	if (from->targets) {
		g_slist_free_full (from->targets, (GDestroyNotify) gda_sql_select_target_free);
	}
	if (from->joins) {
		g_slist_free_full (from->joins, (GDestroyNotify) gda_sql_select_join_free);
	}

	g_free (from);
}

/**
 * gda_sql_select_from_copy
 * @from: a #GdaSqlSelectFrom structure to be copied
 *
 * Creates a new #GdaSqlSelectFrom structure initiated with the values stored in @from.
 *
 * Returns: a new #GdaSqlSelectFrom structure.
 */
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

G_DEFINE_BOXED_TYPE(GdaSqlSelectFrom, gda_sql_select_from, gda_sql_select_from_copy, gda_sql_select_from_free)


/**
 * gda_sql_select_from_serialize
 * @from: a #GdaSqlSelectFrom structure
 *
 * Creates a new string description of the FROM clause used in a SELECT statement.
 *
 * Returns: a new string with the description of the FROM or "null" in case @from is invalid.
 */
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

/**
 * gda_sql_select_from_take_new_target
 * @from: a #GdaSqlSelectFrom structure
 * @target: a #GdaSqlSelectTarget to take from
 *
 * Append @target to the targets in the FROM clause and set @target's parent to
 * @from; after call this function @from owns @target then you must not free it.
 */
void
gda_sql_select_from_take_new_target (GdaSqlSelectFrom *from, GdaSqlSelectTarget *target)
{
	from->targets = g_slist_append (from->targets, target);
	gda_sql_any_part_set_parent (target, from);
}

/**
 * gda_sql_select_from_take_new_join
 * @from: a #GdaSqlSelectFrom structure
 * @join: a #GdaSqlSelectJoin to take from
 *
 * Append @join to the joins in the FROM clause and set @join's parent to
 * @from; after call this function @from owns @join then you must not free it.
 */
void
gda_sql_select_from_take_new_join  (GdaSqlSelectFrom *from, GdaSqlSelectJoin *join)
{
	from->joins = g_slist_append (from->joins, join);
	gda_sql_any_part_set_parent (join, from);
}

/*
 * Any expression in a SELECT ... after the ORDER BY
 */

/**
 * gda_sql_select_order_new
 * @parent: a #GdaSqlStatementSelect
 *
 * Creates a new #GdaSqlSelectOrder structure and sets its parent to @parent.
 *
 * Returns: a new #GdaSqlSelectOrder structure
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

/**
 * gda_sql_select_order_free
 * @order: a #GdaSqlSelectOrder structure to be freed
 *
 * Frees a #GdaSqlSelectOrder structure and its members.
 */
void
gda_sql_select_order_free (GdaSqlSelectOrder *order)
{
	if (!order) return;

	gda_sql_expr_free (order->expr);
	g_free (order->collation_name);

	g_free (order);
}

/**
 * gda_sql_select_order_copy
 * @order: a #GdaSqlSelectOrder structure to be copied
 *
 * Creates a new #GdaSqlSelectOrder structure initiated with the values stored in @order.
 *
 * Returns: a new #GdaSqlSelectOrder structure.
 */
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

G_DEFINE_BOXED_TYPE(GdaSqlSelectOrder, gda_sql_select_order, gda_sql_select_order_copy, gda_sql_select_order_free)

/**
 * gda_sql_select_order_serialize
 * @order: a #GdaSqlSelectOrder structure
 *
 * Creates a new string description of the ORDER BY clause used in a SELECT statement.
 *
 * Returns: a new string with the description of the ORDER BY or "null" in case @order is invalid.
 */
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
