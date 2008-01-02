/* 
 * Copyright (C) 2007 Vivien Malerba
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
#include <glib/gi18n-lib.h>
#include <libgda/sql-parser/gda-statement-struct.h>
#include <libgda/gda-debug-macros.h>
#include <libgda/sql-parser/gda-statement-struct-util.h>
#include <libgda/sql-parser/gda-statement-struct-unknown.h>
#include <libgda/sql-parser/gda-statement-struct-trans.h>
#include <libgda/sql-parser/gda-statement-struct-insert.h>
#include <libgda/sql-parser/gda-statement-struct-select.h>
#include <libgda/sql-parser/gda-statement-struct-delete.h>
#include <libgda/sql-parser/gda-statement-struct-update.h>
#include <libgda/sql-parser/gda-statement-struct-compound.h>
#include <libgda/gda-entity.h>

/*
 * Error reporting
 */
GQuark 
gda_sql_error_quark (void)
{
        static GQuark quark;
        if (!quark)
                quark = g_quark_from_static_string ("gda_sql_error");
        return quark;
}

/* 
 * statement's infos retreival
 */
GdaSqlStatementContentsInfo *
gda_sql_statement_get_contents_infos  (GdaSqlStatementType type) 
{
	static GdaSqlStatementContentsInfo **contents = NULL;
	if (!contents) {
		contents = g_new0 (GdaSqlStatementContentsInfo *, GDA_SQL_STATEMENT_NONE);

		contents [GDA_SQL_STATEMENT_SELECT] = gda_sql_statement_select_get_infos ();
		contents [GDA_SQL_STATEMENT_INSERT] = gda_sql_statement_insert_get_infos ();
		contents [GDA_SQL_STATEMENT_DELETE] = gda_sql_statement_delete_get_infos ();
		contents [GDA_SQL_STATEMENT_UPDATE] = gda_sql_statement_update_get_infos ();
		contents [GDA_SQL_STATEMENT_BEGIN] = gda_sql_statement_begin_get_infos ();
		contents [GDA_SQL_STATEMENT_COMPOUND] = gda_sql_statement_compound_get_infos ();
		contents [GDA_SQL_STATEMENT_COMMIT] = gda_sql_statement_commit_get_infos ();
		contents [GDA_SQL_STATEMENT_ROLLBACK] = gda_sql_statement_rollback_get_infos ();
		contents [GDA_SQL_STATEMENT_UNKNOWN] = gda_sql_statement_unknown_get_infos ();
		contents [GDA_SQL_STATEMENT_SAVEPOINT] = gda_sql_statement_savepoint_get_infos ();
		contents [GDA_SQL_STATEMENT_ROLLBACK_SAVEPOINT] = gda_sql_statement_rollback_savepoint_get_infos ();
		contents [GDA_SQL_STATEMENT_DELETE_SAVEPOINT] = gda_sql_statement_delete_savepoint_get_infos ();
	}
	return contents[type];
}

/*
 *
 * Statement
 *
 */
GdaSqlStatement *
gda_sql_statement_new (GdaSqlStatementType type)
{
	GdaSqlStatement *stmt;
	GdaSqlStatementContentsInfo *infos = gda_sql_statement_get_contents_infos (type);
	
	stmt = g_new0 (GdaSqlStatement, 1);
	stmt->stmt_type = type;
	if (infos && infos->construct) {
		stmt->contents = infos->construct ();
		GDA_SQL_ANY_PART (stmt->contents)->type = type;
	}
	else 
		TO_IMPLEMENT;

	return stmt;
}

GdaSqlStatement *
gda_sql_statement_copy (GdaSqlStatement *stmt)
{
	GdaSqlStatement *copy;
	GdaSqlStatementContentsInfo *infos;

	if (!stmt)
		return NULL;

	infos = gda_sql_statement_get_contents_infos (stmt->stmt_type);
	copy = gda_sql_statement_new (stmt->stmt_type);
	if (stmt->sql)
		copy->sql = g_strdup (stmt->sql);
	if (infos && infos->copy) {
		copy->contents = infos->copy (stmt->contents);
		GDA_SQL_ANY_PART (copy->contents)->type = GDA_SQL_ANY_PART (stmt->contents)->type;
	}
	else 
		TO_IMPLEMENT;

	return copy;
}

void 
gda_sql_statement_free (GdaSqlStatement *stmt)
{
	GdaSqlStatementContentsInfo *infos;

	infos = gda_sql_statement_get_contents_infos (stmt->stmt_type);
	g_free (stmt->sql);
	
	if (stmt->contents) {
		if (infos && infos->free)
			infos->free (stmt->contents);
		else 
			TO_IMPLEMENT;
	}
	g_free (stmt);
}

const gchar *
gda_sql_statement_type_to_string (GdaSqlStatementType type)
{
	GdaSqlStatementContentsInfo *infos;
	infos = gda_sql_statement_get_contents_infos (type);
	if (!infos) {
		return "NONE";
		TO_IMPLEMENT;
	}
	else
		return infos->name;
}

GdaSqlStatementType
gda_sql_statement_string_to_type (const gchar *type)
{
	g_return_val_if_fail (type, GDA_SQL_STATEMENT_NONE);
	
	switch (*type) {
	case 'B':
		return GDA_SQL_STATEMENT_BEGIN;
	case 'C':
		if ((type[1] == 'O') && (type[2] == 'M') && (type[2] == 'P'))
			return GDA_SQL_STATEMENT_COMPOUND;
		else
			return GDA_SQL_STATEMENT_COMMIT;	
	case 'D':
		if (!strcmp (type, "DELETE"))
			return GDA_SQL_STATEMENT_DELETE;
		else
			return GDA_SQL_STATEMENT_DELETE_SAVEPOINT;
	case 'I':
		return GDA_SQL_STATEMENT_INSERT;
	case 'R':
		if (!strcmp (type, "ROLLBACK"))
			return GDA_SQL_STATEMENT_ROLLBACK;
		else
			return GDA_SQL_STATEMENT_ROLLBACK_SAVEPOINT;
	case 'S':
		if (type[1] == 'E')
			return GDA_SQL_STATEMENT_SELECT;
		else
			return GDA_SQL_STATEMENT_SAVEPOINT;
	case 'U':
		if (type[1] == 'N')
			return GDA_SQL_STATEMENT_UNKNOWN;
		else
			return GDA_SQL_STATEMENT_UPDATE;
	default:
		TO_IMPLEMENT;
		return GDA_SQL_STATEMENT_NONE;
	}
}


gchar *
gda_sql_statement_serialize (GdaSqlStatement *stmt)
{
	GString *string;
	gchar *str;
	GdaSqlStatementContentsInfo *infos;

	if (!stmt)
		return NULL;

	infos = gda_sql_statement_get_contents_infos (stmt->stmt_type);
	string = g_string_new ("{");
	str = _json_quote_string (stmt->sql);
	g_string_append_printf (string, "\"sql\":%s", str);
	g_free (str);
	g_string_append_printf (string, ",\"stmt_type\":\"%s\"", infos->name);
	if (infos && infos->serialize) {
		str = infos->serialize (stmt->contents);
		g_string_append_c (string, ',');
		g_string_append (string, str);
		g_free (str);
	}
	else 
		TO_IMPLEMENT;

	g_string_append_c (string, '}');
	str = string->str;
	g_string_free (string, FALSE);
	return str;
	
}


/*
 * Check with dict data structure
 */
typedef struct {
	GdaDict             *dict;
	GdaSqlStatementFunc  func;
	gpointer             func_data;
} DictCheckData;

static gboolean foreach_check_with_dict (GdaSqlAnyPart *node, DictCheckData *data, GError **error);
static gboolean gda_sql_expr_check_with_dict (GdaSqlExpr *expr, DictCheckData *data, GError **error);
static gboolean gda_sql_field_check_with_dict (GdaSqlField *field, DictCheckData *data, GError **error);
static gboolean gda_sql_table_check_with_dict (GdaSqlTable *table, DictCheckData *data, GError **error);
static gboolean gda_sql_function_check_with_dict (GdaSqlFunction *function, DictCheckData *data, GError **error);
static gboolean gda_sql_select_field_check_with_dict (GdaSqlSelectField *field, DictCheckData *data, GError **error);
static gboolean gda_sql_select_target_check_with_dict (GdaSqlSelectTarget *target, DictCheckData *data, GError **error);

/**
 * gda_sql_statement_check_with_dict
 * @stmt: a #GdaSqlStatement pointer
 * @dict: a #GdaDict object, or %NULL
 * @func: a #GdaSqlStatementFunc function to be called when the representation of a database object in @dict is removed
 * @func_data: data to pass to @func when it's called
 * @error: a place to store errors, or %NULL
 *
 * If @dict is not %NULL, then checks that all the database objects referenced in the statement actually
 * exist in the dictionary (for example the table being updated in a UPDATE statement must exist in the
 * dictionary for the check to succeed).
 *
 * If @dict is %NULL, then remove any reference to any @dict's object used in @stmt.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_sql_statement_check_with_dict (GdaSqlStatement *stmt, GdaDict *dict, GdaSqlStatementFunc func, gpointer func_data, 
				   GError **error)
{
	DictCheckData data;

	g_return_val_if_fail (stmt, FALSE);
	g_return_val_if_fail (!dict || GDA_IS_DICT (dict), FALSE);

	data.dict = dict;
	data.func = func;
	data.func_data = func_data;

	return gda_sql_any_part_foreach (GDA_SQL_ANY_PART (stmt->contents), 
					 (GdaSqlForeachFunc) foreach_check_with_dict, &data, error);
}

static gboolean
foreach_check_with_dict (GdaSqlAnyPart *node, DictCheckData *data, GError **error)
{
	if (!node) return TRUE;

	switch (node->type) {
	case GDA_SQL_ANY_EXPR:
		return gda_sql_expr_check_with_dict ((GdaSqlExpr*) node, data, error);
	case GDA_SQL_ANY_SQL_FIELD:
		return gda_sql_field_check_with_dict ((GdaSqlField*) node, data, error);
	case GDA_SQL_ANY_SQL_TABLE:
		return gda_sql_table_check_with_dict ((GdaSqlTable*) node, data, error);
	case GDA_SQL_ANY_SQL_FUNCTION:
		return gda_sql_function_check_with_dict ((GdaSqlFunction*) node, data, error);
	case GDA_SQL_ANY_SQL_SELECT_FIELD:
		return gda_sql_select_field_check_with_dict ((GdaSqlSelectField*) node, data, error);
	case GDA_SQL_ANY_SQL_SELECT_TARGET:
		return gda_sql_select_target_check_with_dict ((GdaSqlSelectTarget*) node, data, error);
	default:
		break;
	}
	return TRUE;
}

static gboolean
gda_sql_expr_check_with_dict (GdaSqlExpr *expr, DictCheckData *data, GError **error)
{
	if (!expr) return TRUE;
	if (!expr->param_spec) return TRUE;
	gda_sql_expr_check_clean (expr);

	/* 2 checks to do here:
	 *  - try to find the GdaDictType from expr->param_spec->type
	 *  - if expr->param_spec->type is NULL, then try to identify it (and expr->param_spec->g_type)
	 *    using @expr context
	 */
        TO_IMPLEMENT;
        return FALSE;
}


static void
gda_sql_field_weak_notify (GdaSqlField *field, GObject *obj)
{
	field->dict_field = NULL;
	if (field->dict_func)
		field->dict_func (GDA_SQL_ANY_PART (field), field->dict_data);
	field->dict_func = NULL;
	field->dict_data = NULL;
}

static gboolean
gda_sql_field_check_with_dict (GdaSqlField *field, DictCheckData *data, GError **error)
{
	GdaSqlAnyPart *any;
	GdaSqlTable *stable;

	if (!field) return TRUE;
	gda_sql_field_check_clean (field);

	if (!data->dict) return TRUE;

	for (any = GDA_SQL_ANY_PART(field)->parent; 
	     any && (any->type != GDA_SQL_ANY_STMT_INSERT) && (any->type != GDA_SQL_ANY_STMT_UPDATE); 
	     any = any->parent);
	if (!any) {
		g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
			     _("GdaSqlField is not part of an INSERT or UPDATE statement"));
		return FALSE;
	}
	
	if (any->type == GDA_SQL_ANY_STMT_INSERT) 
		stable = ((GdaSqlStatementInsert*) any)->table;
	else if (any->type == GDA_SQL_ANY_STMT_UPDATE) 
		stable = ((GdaSqlStatementUpdate*) any)->table;
	else 
		g_assert_not_reached ();

	if (!stable) {
		if (any->type == GDA_SQL_ANY_STMT_INSERT)
			g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
				     _("Missing table in INSERT statement"));
		else
			g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
				     _("Missing table in UPDATE statement"));
		return FALSE;
	}
	if (!stable->dict_table) {
		if (! gda_sql_table_check_with_dict (stable, data, error))
			return FALSE;
		g_assert (stable->dict_table);
		GdaDictDatabase *db;

		db = gda_dict_get_database (data->dict);
		field->dict_field = gda_entity_get_field_by_name (GDA_ENTITY (stable->dict_table), 
								  field->field_name);
		if (!field->dict_field) {
			g_set_error (error, GDA_SQL_ERROR, GDA_SQL_DICT_ELEMENT_MISSING_ERROR,
				     _("Field %s.%s not found in dictionary"), stable->table_name, field->field_name);
			return FALSE;
		}
		g_object_weak_ref (G_OBJECT (field->dict_field), (GWeakNotify) gda_sql_field_weak_notify, field);
	}

        return TRUE;
}

void
gda_sql_expr_check_clean (GdaSqlExpr *expr)
{
	if (!expr) return;
	if (expr->param_spec && expr->param_spec->dict_type)
		TO_IMPLEMENT;
}

void 
gda_sql_field_check_clean (GdaSqlField *field)
{
	if (!field) return;
	if (field->dict_field)
		g_object_weak_unref (G_OBJECT (field->dict_field), (GWeakNotify) gda_sql_field_weak_notify, field);
}

static gboolean
gda_sql_table_check_with_dict (GdaSqlTable *table, DictCheckData *data, GError **error)
{
	if (!table) return TRUE;

        TO_IMPLEMENT;
        return FALSE;
}

void 
gda_sql_table_check_clean (GdaSqlTable *table)
{
	if (!table) return;
	if (table->dict_table)
		TO_IMPLEMENT;
}

static gboolean
gda_sql_function_check_with_dict (GdaSqlFunction *function, DictCheckData *data, GError **error)
{
	if (!function) return TRUE;

        TO_IMPLEMENT;
        return FALSE;
}

void 
gda_sql_function_check_clean (GdaSqlFunction *function)
{
	if (!function) return;
	if (function->dict_function)
		TO_IMPLEMENT;
}

static gboolean
gda_sql_select_field_check_with_dict (GdaSqlSelectField *field, DictCheckData *data, GError **error)
{
	if (!field) return TRUE;

	TO_IMPLEMENT;
	return FALSE;
}

void 
gda_sql_select_field_check_clean (GdaSqlSelectField *field)
{
	if (!field) return;
	if (field->dict_field || field->dict_table)
		TO_IMPLEMENT;
}

static gboolean
gda_sql_select_target_check_with_dict (GdaSqlSelectTarget *target, DictCheckData *data, GError **error)
{
	if (!target) return TRUE;

	TO_IMPLEMENT;
	return FALSE;
}

void 
gda_sql_select_target_check_clean (GdaSqlSelectTarget *target)
{
	if (!target) return;
	if (target->dict_table)
		TO_IMPLEMENT;
}



static gboolean foreach_check_clean (GdaSqlAnyPart *node, gpointer data, GError **error);
/**
 * gda_sql_statement_check_clean
 */
void
gda_sql_statement_check_clean (GdaSqlStatement *stmt)
{
	g_return_if_fail (stmt);

	gda_sql_any_part_foreach (GDA_SQL_ANY_PART (stmt->contents), 
				  (GdaSqlForeachFunc) foreach_check_clean, NULL, NULL);
}

static gboolean
foreach_check_clean (GdaSqlAnyPart *node, gpointer data, GError **error)
{
	if (!node) return TRUE;

	switch (node->type) {
	case GDA_SQL_ANY_EXPR:
		gda_sql_expr_check_clean ((GdaSqlExpr*) node);
		break;
	case GDA_SQL_ANY_SQL_FIELD:
		gda_sql_field_check_clean ((GdaSqlField*) node);
		break;
	case GDA_SQL_ANY_SQL_TABLE:
		gda_sql_table_check_clean ((GdaSqlTable*) node);
		break;
	case GDA_SQL_ANY_SQL_FUNCTION:
		gda_sql_function_check_clean ((GdaSqlFunction*) node);
		break;
	case GDA_SQL_ANY_SQL_SELECT_FIELD:
		gda_sql_select_field_check_clean ((GdaSqlSelectField*) node);
		break;
	case GDA_SQL_ANY_SQL_SELECT_TARGET:
		gda_sql_select_target_check_clean ((GdaSqlSelectTarget*) node);
		break;
	default:
		break;
	}

	return TRUE;
}

static gboolean foreach_check_struct (GdaSqlAnyPart *node, gpointer data, GError **error);

/**
 * gda_sql_statement_check_structure
 * @stmt: a #GdaSqlStatement pointer
 * @error: a place to store errors, or %NULL
 *
 * Checks for any error in @stmt's structure to make sure the statement is valid
 * (for example a SELECT statement must at least return a column, a DELETE statement must specify which table
 * is targetted).
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_sql_statement_check_structure (GdaSqlStatement *stmt, GError **error)
{
	g_return_val_if_fail (stmt, FALSE);

	return gda_sql_any_part_foreach (GDA_SQL_ANY_PART (stmt->contents), 
					 (GdaSqlForeachFunc) foreach_check_struct, NULL, error);
}

static gboolean
foreach_check_struct (GdaSqlAnyPart *node, gpointer data, GError **error)
{
	return gda_sql_any_part_check_structure (node, error);
}

gboolean
gda_sql_any_part_check_structure (GdaSqlAnyPart *node, GError **error)
{
	if (!node) return TRUE;

	switch (node->type) {
	case GDA_SQL_ANY_STMT_SELECT:
        case GDA_SQL_ANY_STMT_INSERT:
        case GDA_SQL_ANY_STMT_UPDATE:
        case GDA_SQL_ANY_STMT_DELETE:
        case GDA_SQL_ANY_STMT_COMPOUND:
        case GDA_SQL_ANY_STMT_BEGIN:
        case GDA_SQL_ANY_STMT_ROLLBACK:
        case GDA_SQL_ANY_STMT_COMMIT:
        case GDA_SQL_ANY_STMT_SAVEPOINT:
        case GDA_SQL_ANY_STMT_ROLLBACK_SAVEPOINT:
        case GDA_SQL_ANY_STMT_DELETE_SAVEPOINT:
        case GDA_SQL_ANY_STMT_UNKNOWN: {
		GdaSqlStatementContentsInfo *cinfo;
		cinfo = gda_sql_statement_get_contents_infos (node->type);
		if (cinfo->check_structure_func)
			return cinfo->check_structure_func (node, NULL, error);
		break;
	}
	case GDA_SQL_ANY_EXPR: {
		GdaSqlExpr *expr = (GdaSqlExpr*) node;
		if (expr->cast_as && expr->param_spec) {
			g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
				     _("Expression can't have both a type cast and a parameter specification"));
			return FALSE;
		}
		break;
	}
        case GDA_SQL_ANY_SQL_FIELD: {
		GdaSqlField *field = (GdaSqlField*) node;
		if (!_string_is_identifier (field->field_name)) {
			if (field->field_name)
				g_set_error (error, GDA_SQL_ERROR, GDA_SQL_MALFORMED_IDENTIFIER_ERROR,
					     _("'%s' is not a valid identifier"), field->field_name);
			else
				g_set_error (error, GDA_SQL_ERROR, GDA_SQL_MALFORMED_IDENTIFIER_ERROR,
					     _("Empty identifier"));
			return FALSE;
		}
		break;
	}
        case GDA_SQL_ANY_SQL_TABLE: {
		GdaSqlTable *table = (GdaSqlTable*) node;
		if (!_string_is_identifier (table->table_name)) {
			if (table->table_name)
				g_set_error (error, GDA_SQL_ERROR, GDA_SQL_MALFORMED_IDENTIFIER_ERROR,
					     _("'%s' is not a valid identifier"), table->table_name);
			else
				g_set_error (error, GDA_SQL_ERROR, GDA_SQL_MALFORMED_IDENTIFIER_ERROR,
					     _("Empty identifier"));
			return FALSE;
		}
		break;
	}
        case GDA_SQL_ANY_SQL_FUNCTION:  {
		GdaSqlFunction *function = (GdaSqlFunction*) node;
		if (!_string_is_identifier (function->function_name)) {
			if (function->function_name)
				g_set_error (error, GDA_SQL_ERROR, GDA_SQL_MALFORMED_IDENTIFIER_ERROR,
					     _("'%s' is not a valid identifier"), function->function_name);
			else
				g_set_error (error, GDA_SQL_ERROR, GDA_SQL_MALFORMED_IDENTIFIER_ERROR,
					     _("Empty identifier"));
			return FALSE;
		}
		break;
	}
        case GDA_SQL_ANY_SQL_OPERATION: {
		GdaSqlOperation *operation = (GdaSqlOperation*) node;
		if (!operation->operands) {
			g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
				     _("Operation has no operand"));
			return FALSE;
		}
		switch (operation->operator) {
		case GDA_SQL_OPERATOR_EQ:
		case GDA_SQL_OPERATOR_IS:
		case GDA_SQL_OPERATOR_LIKE:
		case GDA_SQL_OPERATOR_GT:
		case GDA_SQL_OPERATOR_LT:
		case GDA_SQL_OPERATOR_GEQ:
		case GDA_SQL_OPERATOR_LEQ:
		case GDA_SQL_OPERATOR_DIFF:
		case GDA_SQL_OPERATOR_REGEXP:
		case GDA_SQL_OPERATOR_REGEXP_CI:
		case GDA_SQL_OPERATOR_NOT_REGEXP:
		case GDA_SQL_OPERATOR_NOT_REGEXP_CI:
		case GDA_SQL_OPERATOR_SIMILAR:
		case GDA_SQL_OPERATOR_REM:
		case GDA_SQL_OPERATOR_DIV:
		case GDA_SQL_OPERATOR_BITAND:
		case GDA_SQL_OPERATOR_BITOR:
			if (g_slist_length (operation->operands) != 2) {
				g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
					     _("Wrong number of operands"));
				return FALSE;
			}
			break;
		case GDA_SQL_OPERATOR_BETWEEN:
			if (g_slist_length (operation->operands) != 3) {
				g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
					     _("Wrong number of operands"));
				return FALSE;
			}
			break;
		case GDA_SQL_OPERATOR_BITNOT:
		case GDA_SQL_OPERATOR_ISNULL:
		case GDA_SQL_OPERATOR_ISNOTNULL:
		case GDA_SQL_OPERATOR_NOT:
			if (g_slist_length (operation->operands) != 1) {
				g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
					     _("Wrong number of operands"));
				return FALSE;
			}
			break;
		case GDA_SQL_OPERATOR_AND:
		case GDA_SQL_OPERATOR_OR:
		case GDA_SQL_OPERATOR_IN:
		case GDA_SQL_OPERATOR_CONCAT:
		case GDA_SQL_OPERATOR_STAR:
			if (g_slist_length (operation->operands) < 2) {
				g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
					     _("Wrong number of operands"));
				return FALSE;
			}
			break;
		case GDA_SQL_OPERATOR_MINUS:
		case GDA_SQL_OPERATOR_PLUS:
			if (g_slist_length (operation->operands) == 0) {
				g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
					     _("Wrong number of operands"));
				return FALSE;
			}
			break;
		default:
			g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
				     _("Unknown operator %d"), operation->operator);
			return FALSE;
		}
		break;
	}
        case GDA_SQL_ANY_SQL_CASE: {
		GdaSqlCase *sc = (GdaSqlCase*) node;
		if (g_slist_length (sc->when_expr_list) != g_slist_length (sc->then_expr_list)) {
			g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
				     "Number of WHEN is not the same as number of THEN in CASE expression");
			return FALSE;
		}
		if (!sc->when_expr_list) {
			g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
					     "CASE expression must have at least one WHEN ... THEN element");
			return FALSE;
		}
		break;
	}
	case GDA_SQL_ANY_SQL_SELECT_FIELD: {
		GdaSqlSelectField *field = (GdaSqlSelectField*) node;
		if (!field->expr) {
			g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
				     _("Missing expression in select field"));
			return FALSE;
		}
		break;
	}
	case GDA_SQL_ANY_SQL_SELECT_TARGET: {
		GdaSqlSelectTarget *target = (GdaSqlSelectTarget*) node;
		if (!target->expr) {
			g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
				     _("Missing expression in select target"));
			return FALSE;
		}
		break;
	}
	case GDA_SQL_ANY_SQL_SELECT_JOIN: {
		GdaSqlSelectJoin *join = (GdaSqlSelectJoin*) node;
		if (join->expr && join->using) {
			g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
				     _("Join can't at the same time specify a join condition and a list of fields to join on"));
			return FALSE;
		}
		if ((join->type == GDA_SQL_SELECT_JOIN_CROSS) && (join->expr || join->using)) {
			g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
				     _("Cross join can't have a join condition or a list of fields to join on"));
			return FALSE;
		}
		break;
	}
	case GDA_SQL_ANY_SQL_SELECT_FROM: {
		GdaSqlSelectFrom *from = (GdaSqlSelectFrom*) node;
		if (!from->targets) {
			g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
				     _("Empty FROM clause"));
			return FALSE;
		}
		break;
	}
        case GDA_SQL_ANY_SQL_SELECT_ORDER: {
		GdaSqlSelectOrder *order = (GdaSqlSelectOrder*) node;
		if (!order->expr) {
			g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
				     "ORDER BY expression must have an expression");
			return FALSE;
		}
		break;
	}

	default:
		break;
	}

	return TRUE;
}

/**
 * gda_sql_any_part_foreach
 * @node: the stat node
 * @func: function to call for each sub node
 * @data: data to pass to @func each time it is called
 * @error: a place to store errors, or %NULL (is also passed to @func)
 *
 * Calls a function for each element of a #GdaSqlAnyPart node
 *
 * Returns: TRUE if @func has been called for any sub node of @node and always returned TRUE, or FALSE
 * otherwise.
 */
gboolean
gda_sql_any_part_foreach (GdaSqlAnyPart *node, GdaSqlForeachFunc func, gpointer data, GError **error)
{
	GSList *l;
	if (!node) return TRUE;
	
	/* call @func for the node's contents */
	switch (node->type) {
	case GDA_SQL_ANY_STMT_BEGIN:
	case GDA_SQL_ANY_STMT_ROLLBACK:
	case GDA_SQL_ANY_STMT_COMMIT:
	case GDA_SQL_ANY_STMT_SAVEPOINT:
	case GDA_SQL_ANY_STMT_ROLLBACK_SAVEPOINT:
	case GDA_SQL_ANY_STMT_DELETE_SAVEPOINT:
	case GDA_SQL_ANY_SQL_FIELD:
	case GDA_SQL_ANY_SQL_TABLE:
		/* nothing to do */
		break;
	case GDA_SQL_ANY_STMT_SELECT: {
		GdaSqlStatementSelect *stmt = (GdaSqlStatementSelect*) node;
		if (!gda_sql_any_part_foreach (GDA_SQL_ANY_PART (stmt->distinct_expr), func, data, error))
				return FALSE;
		for (l = stmt->expr_list; l; l = l->next)
			if (!gda_sql_any_part_foreach (GDA_SQL_ANY_PART (l->data), func, data, error))
				return FALSE;
		if (!gda_sql_any_part_foreach (GDA_SQL_ANY_PART (stmt->from), func, data, error))
				return FALSE;
		if (!gda_sql_any_part_foreach (GDA_SQL_ANY_PART (stmt->where_cond), func, data, error))
				return FALSE;
		for (l = stmt->group_by; l; l = l->next)
			if (!gda_sql_any_part_foreach (GDA_SQL_ANY_PART (l->data), func, data, error))
				return FALSE;
		if (!gda_sql_any_part_foreach (GDA_SQL_ANY_PART (stmt->having_cond), func, data, error))
				return FALSE;
		for (l = stmt->order_by; l; l = l->next)
			if (!gda_sql_any_part_foreach (GDA_SQL_ANY_PART (l->data), func, data, error))
				return FALSE;
		if (!gda_sql_any_part_foreach (GDA_SQL_ANY_PART (stmt->limit_count), func, data, error))
				return FALSE;
		if (!gda_sql_any_part_foreach (GDA_SQL_ANY_PART (stmt->limit_offset), func, data, error))
				return FALSE;
		break;
	}
	case GDA_SQL_ANY_STMT_INSERT: {
		GdaSqlStatementInsert *stmt = (GdaSqlStatementInsert*) node;
		if (!gda_sql_any_part_foreach (GDA_SQL_ANY_PART (stmt->table), func, data, error))
			return FALSE;
		for (l = stmt->fields_list; l; l = l->next)
			if (!gda_sql_any_part_foreach (GDA_SQL_ANY_PART (l->data), func, data, error))
				return FALSE;
		for (l = stmt->values_list; l; l = l->next) {
			GSList *sl;
			for (sl = (GSList *) l->data; sl; sl = sl->next) {
				if (!gda_sql_any_part_foreach (GDA_SQL_ANY_PART (sl->data), func, data, error))
					return FALSE;
			}
		}
		if (!gda_sql_any_part_foreach (GDA_SQL_ANY_PART (stmt->select), func, data, error))
			return FALSE;
		break;
	}
	case GDA_SQL_ANY_STMT_UPDATE: {
		GdaSqlStatementUpdate *stmt = (GdaSqlStatementUpdate*) node;
		if (!gda_sql_any_part_foreach (GDA_SQL_ANY_PART (stmt->table), func, data, error))
			return FALSE;
		for (l = stmt->fields_list; l; l = l->next)
			if (!gda_sql_any_part_foreach (GDA_SQL_ANY_PART (l->data), func, data, error))
				return FALSE;
		for (l = stmt->expr_list; l; l = l->next)
			if (!gda_sql_any_part_foreach (GDA_SQL_ANY_PART (l->data), func, data, error))
				return FALSE;
		if (!gda_sql_any_part_foreach (GDA_SQL_ANY_PART (stmt->cond), func, data, error))
			return FALSE;
		break;
	}
	case GDA_SQL_ANY_STMT_DELETE: {
		GdaSqlStatementDelete *stmt = (GdaSqlStatementDelete*) node;
		if (!gda_sql_any_part_foreach (GDA_SQL_ANY_PART (stmt->table), func, data, error))
			return FALSE;
		if (!gda_sql_any_part_foreach (GDA_SQL_ANY_PART (stmt->cond), func, data, error))
			return FALSE;
		break;
	}
	case GDA_SQL_ANY_STMT_COMPOUND: {
		GdaSqlStatementCompound *stmt = (GdaSqlStatementCompound*) node;
		for (l = stmt->stmt_list; l; l = l->next)
			if (!gda_sql_any_part_foreach (GDA_SQL_ANY_PART (((GdaSqlStatement*) l->data)->contents), 
						       func, data, error))
				return FALSE;
		break;
	}
	case GDA_SQL_ANY_STMT_UNKNOWN: {
		GdaSqlStatementUnknown *stmt = (GdaSqlStatementUnknown*) node;
		for (l = stmt->expressions; l; l = l->next)
			if (!gda_sql_any_part_foreach (GDA_SQL_ANY_PART (l->data), func, data, error))
				return FALSE;
		break;
	}
	
	/* individual parts */
	case GDA_SQL_ANY_EXPR: {
		GdaSqlExpr *expr = (GdaSqlExpr*) node;
		if (!gda_sql_any_part_foreach (GDA_SQL_ANY_PART (expr->func), func, data, error))
			return FALSE;
		if (!gda_sql_any_part_foreach (GDA_SQL_ANY_PART (expr->cond), func, data, error))
			return FALSE;
		if (!gda_sql_any_part_foreach (GDA_SQL_ANY_PART (expr->select), func, data, error))
			return FALSE;
		if (!gda_sql_any_part_foreach (GDA_SQL_ANY_PART (expr->case_s), func, data, error))
			return FALSE;
		break;
	}
	case GDA_SQL_ANY_SQL_FUNCTION: {
		GdaSqlFunction *function = (GdaSqlFunction *) node;
		for (l = function->args_list; l; l = l->next)
			if (!gda_sql_any_part_foreach (GDA_SQL_ANY_PART (l->data), func, data, error))
				return FALSE;
		break;
	}
	case GDA_SQL_ANY_SQL_OPERATION: {
		GdaSqlOperation *operation = (GdaSqlOperation *) node;
		for (l = operation->operands; l; l = l->next)
			if (!gda_sql_any_part_foreach (GDA_SQL_ANY_PART (l->data), func, data, error))
				return FALSE;
		break;
	}
	case GDA_SQL_ANY_SQL_CASE: {
		GdaSqlCase *sc = (GdaSqlCase*) node;
		if (!gda_sql_any_part_foreach (GDA_SQL_ANY_PART (sc->base_expr), func, data, error))
			return FALSE;
		for (l = sc->when_expr_list; l; l = l->next)
			if (!gda_sql_any_part_foreach (GDA_SQL_ANY_PART (l->data), func, data, error))
				return FALSE;
		for (l = sc->then_expr_list; l; l = l->next)
			if (!gda_sql_any_part_foreach (GDA_SQL_ANY_PART (l->data), func, data, error))
				return FALSE;
		if (!gda_sql_any_part_foreach (GDA_SQL_ANY_PART (sc->else_expr), func, data, error))
			return FALSE;
		break;
	}
	case GDA_SQL_ANY_SQL_SELECT_FIELD: {
		GdaSqlSelectField *field = (GdaSqlSelectField *) node;
		if (!gda_sql_any_part_foreach (GDA_SQL_ANY_PART (field->expr), func, data, error))
			return FALSE;
		break;
	}
	case GDA_SQL_ANY_SQL_SELECT_TARGET: {
		GdaSqlSelectTarget *target = (GdaSqlSelectTarget *) node;
		if (!gda_sql_any_part_foreach (GDA_SQL_ANY_PART (target->expr), func, data, error))
			return FALSE;
		break;
	}
	case GDA_SQL_ANY_SQL_SELECT_JOIN: {
		GdaSqlSelectJoin *join = (GdaSqlSelectJoin *) node;
		if (!gda_sql_any_part_foreach (GDA_SQL_ANY_PART (join->expr), func, data, error))
			return FALSE;
		for (l = join->using; l; l = l->next)
			if (!gda_sql_any_part_foreach (GDA_SQL_ANY_PART (l->data), func, data, error))
				return FALSE;
		break;
	}
	case GDA_SQL_ANY_SQL_SELECT_FROM: {
		GdaSqlSelectFrom *from = (GdaSqlSelectFrom *) node;
		for (l = from->targets; l; l = l->next)
			if (!gda_sql_any_part_foreach (GDA_SQL_ANY_PART (l->data), func, data, error))
				return FALSE;
		for (l = from->joins; l; l = l->next)
			if (!gda_sql_any_part_foreach (GDA_SQL_ANY_PART (l->data), func, data, error))
				return FALSE;
		break;
	}
	case GDA_SQL_ANY_SQL_SELECT_ORDER: {
		GdaSqlSelectOrder *order = (GdaSqlSelectOrder *) node;
		if (!gda_sql_any_part_foreach (GDA_SQL_ANY_PART (order->expr), func, data, error))
			return FALSE;
		break;
	}
	default:
		g_assert_not_reached ();
	}

	/* finally call @func for this node */
	return func (node, data, error);
}
