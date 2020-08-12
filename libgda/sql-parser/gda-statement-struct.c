/*
 * Copyright (C) 2008 - 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2008 - 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2008 - 2013 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2010 Jonh Wendell <jwendell@gnome.org>
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

#include <string.h>
#include <glib/gi18n-lib.h>
#include <libgda/sql-parser/gda-statement-struct.h>
#include <libgda/gda-debug-macros.h>
#include <libgda/gda-connection.h>
#include <libgda/gda-meta-struct.h>
#include <libgda/gda-util.h>
#include <libgda/sql-parser/gda-statement-struct-util.h>
#include <libgda/sql-parser/gda-statement-struct-unknown.h>
#include <libgda/sql-parser/gda-statement-struct-trans.h>
#include <libgda/sql-parser/gda-statement-struct-insert.h>
#include <libgda/sql-parser/gda-statement-struct-select.h>
#include <libgda/sql-parser/gda-statement-struct-delete.h>
#include <libgda/sql-parser/gda-statement-struct-update.h>
#include <libgda/sql-parser/gda-statement-struct-compound.h>

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

GdaSqlStatementContentsInfo *
gda_sql_statement_contents_info_copy (GdaSqlStatementContentsInfo *src) {
	GdaSqlStatementContentsInfo *cp = g_new0(GdaSqlStatementContentsInfo, 1);
	cp->name = src->name;
	cp->construct = src->construct;
	cp->free = src->free;
	cp->copy = src->copy;
	cp->serialize = src->serialize;

	/* augmenting information precision using a dictionary */
	cp->check_structure_func = src->check_structure_func;
	cp->check_validity_func = src->check_validity_func;
	return cp;
}
void
gda_sql_statement_contents_info_free (GdaSqlStatementContentsInfo *cinfo) {
	g_free (cinfo);
}

G_DEFINE_BOXED_TYPE(GdaSqlStatementContentsInfo, gda_sql_statement_contents_info, gda_sql_statement_contents_info_copy, gda_sql_statement_contents_info_free)
/*
 * statement's infos retrieval
 */
GdaSqlStatementContentsInfo *
gda_sql_statement_get_contents_infos  (GdaSqlStatementType type)
{
	static GMutex mutex;
	static GdaSqlStatementContentsInfo **contents = NULL;

	g_mutex_lock (&mutex);
	if (!contents) {
		contents = g_new0 (GdaSqlStatementContentsInfo *, GDA_SQL_STATEMENT_NONE);

		contents [GDA_SQL_STATEMENT_SELECT] = _gda_sql_statement_select_get_infos ();
		contents [GDA_SQL_STATEMENT_INSERT] = _gda_sql_statement_insert_get_infos ();
		contents [GDA_SQL_STATEMENT_DELETE] = _gda_sql_statement_delete_get_infos ();
		contents [GDA_SQL_STATEMENT_UPDATE] = _gda_sql_statement_update_get_infos ();
		contents [GDA_SQL_STATEMENT_BEGIN] = _gda_sql_statement_begin_get_infos ();
		contents [GDA_SQL_STATEMENT_COMPOUND] = _gda_sql_statement_compound_get_infos ();
		contents [GDA_SQL_STATEMENT_COMMIT] = _gda_sql_statement_commit_get_infos ();
		contents [GDA_SQL_STATEMENT_ROLLBACK] = _gda_sql_statement_rollback_get_infos ();
		contents [GDA_SQL_STATEMENT_UNKNOWN] = _gda_sql_statement_unknown_get_infos ();
		contents [GDA_SQL_STATEMENT_SAVEPOINT] = _gda_sql_statement_savepoint_get_infos ();
		contents [GDA_SQL_STATEMENT_ROLLBACK_SAVEPOINT] = _gda_sql_statement_rollback_savepoint_get_infos ();
		contents [GDA_SQL_STATEMENT_DELETE_SAVEPOINT] = _gda_sql_statement_delete_savepoint_get_infos ();
	}
	g_mutex_unlock (&mutex);

	return contents[type];
}

G_DEFINE_BOXED_TYPE(GdaSqlStatement, gda_sql_statement, gda_sql_statement_copy, gda_sql_statement_free)

/**
 * gda_sql_statement_new
 * @type: type of statement to create
 *
 * Use this function to create a #GdaSqlStatement of the specified @type type.
 *
 * Returns: a new #GdaSqlStatement
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
		GDA_SQL_ANY_PART (stmt->contents)->type = (GdaSqlAnyPartType) type;
	}
	else
		TO_IMPLEMENT;

	return stmt;
}

/**
 * gda_sql_statement_copy
 * @stmt: a #GdaSqlStatement pointer
 *
 * Creates a copy of @stmt.
 *
 * Returns: a new #GdaSqlStatement
 */
GdaSqlStatement *
gda_sql_statement_copy (GdaSqlStatement *stmt)
{
	GdaSqlStatement *copy;
	GdaSqlStatementContentsInfo *infos;

	if (!stmt)
		return NULL;

	infos = gda_sql_statement_get_contents_infos (stmt->stmt_type);
	copy = g_new0 (GdaSqlStatement, 1);
	copy->stmt_type = stmt->stmt_type;

	if (stmt->sql)
		copy->sql = g_strdup (stmt->sql);
	if (infos && infos->copy) {
		copy->contents = infos->copy (stmt->contents);
		GDA_SQL_ANY_PART (copy->contents)->type = GDA_SQL_ANY_PART (stmt->contents)->type;
	}
	else if (infos && infos->construct) {
		copy->contents = infos->construct ();
		GDA_SQL_ANY_PART (copy->contents)->type = (GdaSqlAnyPartType) stmt->stmt_type;
	}
	else
		TO_IMPLEMENT;
	if (stmt->validity_meta_struct) {
		copy->validity_meta_struct = stmt->validity_meta_struct;
		g_object_ref (copy->validity_meta_struct);
	}

	return copy;
}

/**
 * gda_sql_statement_free
 * @stmt: a #GdaSqlStatement pointer
 *
 * Releases any memory associated to @stmt.
 */
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
	if (stmt->validity_meta_struct)
		g_object_unref (stmt->validity_meta_struct);

	g_free (stmt);
}

/**
 * gda_sql_statement_type_to_string
 * @type: a #GdaSqlStatementType value
 *
 * Converts a #GdaSqlStatementType to a string, see also gda_sql_statement_string_to_type()
 *
 * Returns: a constant string
 */
const gchar *
gda_sql_statement_type_to_string (GdaSqlStatementType type)
{
	GdaSqlStatementContentsInfo *infos;
	infos = gda_sql_statement_get_contents_infos (type);
	if (!infos)
		return "NONE";
	else
		return infos->name;
}

/**
 * gda_sql_statement_string_to_type
 * @type: a string representing a #GdaSqlStatementType type
 *
 * Converts a string to a #GdaSqlStatementType value, see also gda_sql_statement_type_to_string()
 *
 * Returns: a #GdaSqlStatementType value
 */
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


/**
 * gda_sql_statement_serialize
 * @stmt: a #GdaSqlStatement pointer
 *
 * Creates a string representation of @stmt.
 *
 * Returns: a new string
 */
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
static gboolean foreach_check_clean (GdaSqlAnyPart *node, gpointer data, GError **error);

static gboolean foreach_check_validity (GdaSqlAnyPart *node, GdaSqlStatementCheckValidityData *data, GError **error);
static gboolean gda_sql_expr_check_validity (GdaSqlExpr *expr, GdaSqlStatementCheckValidityData *data, GError **error);
static gboolean gda_sql_field_check_validity (GdaSqlField *field, GdaSqlStatementCheckValidityData *data, GError **error);
static gboolean gda_sql_table_check_validity (GdaSqlTable *table, GdaSqlStatementCheckValidityData *data, GError **error);
static gboolean gda_sql_select_field_check_validity (GdaSqlSelectField *field, GdaSqlStatementCheckValidityData *data, GError **error);
static gboolean gda_sql_select_target_check_validity (GdaSqlSelectTarget *target, GdaSqlStatementCheckValidityData *data, GError **error);

/**
 * gda_sql_statement_check_validity:
 * @stmt: a #GdaSqlStatement pointer
 * @cnc: (nullable): a #GdaConnection object, or %NULL
 * @error: a place to store errors, or %NULL
 *
 * If @cnc is not %NULL, then checks that all the database objects referenced in the statement actually
 * exist in the connection's database (for example the table being updated in a UPDATE statement must exist in the
 * connection's database for the check to succeed). This method fills the @stmt-&gt;validity_meta_struct attribute.
 *
 * If @cnc is %NULL, then remove any information from a previous call to this method stored in @stmt. In this case,
 * the @stmt-&gt;validity_meta_struct attribute is cleared.
 *
 * Also note that some parts of @stmt may be modified: for example leading and trailing spaces in aliases or
 * objects names will be removed.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_sql_statement_check_validity (GdaSqlStatement *stmt, GdaConnection *cnc, GError **error)
{
	g_return_val_if_fail (stmt, FALSE);
	g_return_val_if_fail (!cnc || GDA_IS_CONNECTION (cnc), FALSE);

	/* check the structure first */
	if (!gda_sql_statement_check_structure (stmt, error))
		return FALSE;

	/* clear any previous setting */
	gda_sql_statement_check_clean (stmt);

	if (cnc) {
		GdaSqlStatementCheckValidityData data;
		gboolean retval;

		/* prepare data */
		data.cnc = cnc;
		data.store = gda_connection_get_meta_store (cnc);
		data.mstruct = (GdaMetaStruct*) g_object_new (GDA_TYPE_META_STRUCT, "meta-store", data.store, "features", GDA_META_STRUCT_FEATURE_NONE, NULL);

		/* attach the GdaMetaStruct to @stmt */
		stmt->validity_meta_struct = data.mstruct;
		retval = gda_sql_any_part_foreach (GDA_SQL_ANY_PART (stmt->contents),
						   (GdaSqlForeachFunc) foreach_check_validity, &data, error);
		return retval;
	}
	else
		return TRUE;
}

/**
 * gda_sql_statement_check_validity_m
 * @stmt: a #GdaSqlStatement pointer
 * @mstruct: (nullable): a #GdaMetaStruct object, or %NULL
 * @error: a place to store errors, or %NULL
 *
 * If @mstruct is not %NULL, then checks that all the database objects referenced in the statement i
 * actually referenced in @mstruct
 *  (for example the table being updated in a UPDATE statement must exist in the
 * connection's database for the check to succeed).
 * This method sets the @stmt-&gt;validity_meta_struct attribute to @mstruct.
 *
 * If @mstruct is %NULL, then remove any information from a previous call to this method stored in @stmt. In this case,
 * the @stmt-&gt;validity_meta_struct attribute is cleared.
 *
 * Also note that some parts of @stmt may be modified: for example leading and trailing spaces in aliases or
 * objects names will be removed.
 *
 * Returns: TRUE if no error occurred
 *
 * Since: 4.2
 */
gboolean
gda_sql_statement_check_validity_m (GdaSqlStatement *stmt, GdaMetaStruct *mstruct, GError **error)
{
	g_return_val_if_fail (stmt, FALSE);
	g_return_val_if_fail (!mstruct || GDA_IS_META_STRUCT (mstruct), FALSE);

	/* check the structure first */
	if (!gda_sql_statement_check_structure (stmt, error))
		return FALSE;

	/* clear any previous setting */
	gda_sql_statement_check_clean (stmt);

	if (mstruct) {
		GdaSqlStatementCheckValidityData data;
		gboolean retval;

		/* prepare data */
		data.cnc = NULL;
		data.store = NULL;
		data.mstruct = g_object_ref (mstruct);

		/* attach the GdaMetaStruct to @stmt */
		stmt->validity_meta_struct = data.mstruct;
		retval = gda_sql_any_part_foreach (GDA_SQL_ANY_PART (stmt->contents),
						   (GdaSqlForeachFunc) foreach_check_validity, &data, error);
		return retval;
	}
	else
		return TRUE;
}

static gboolean
foreach_check_validity (GdaSqlAnyPart *node, GdaSqlStatementCheckValidityData *data, GError **error)
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
		cinfo = gda_sql_statement_get_contents_infos ((GdaSqlStatementType) node->type);
		if (cinfo->check_validity_func)
			return cinfo->check_validity_func (node, data, error);
		break;
	}
	case GDA_SQL_ANY_EXPR: {
		GdaSqlExpr *expr = (GdaSqlExpr *) node;
		if (expr->cast_as) {
			g_strchomp (expr->cast_as);
			if (! *(expr->cast_as)) {
				g_free (expr->cast_as);
				expr->cast_as = NULL;
			}
		}
		return gda_sql_expr_check_validity (expr, data, error);
	}
	case GDA_SQL_ANY_SQL_FIELD: {
		GdaSqlField *field = (GdaSqlField*) node;
		if (field->field_name) {
			g_strchomp (field->field_name);
			if (! *(field->field_name)) {
				g_free (field->field_name);
				field->field_name = NULL;
			}
		}
		return gda_sql_field_check_validity (field, data, error);
	}
	case GDA_SQL_ANY_SQL_TABLE: {
		GdaSqlTable *table = (GdaSqlTable*) node;
		if (table->table_name) {
			g_strchomp (table->table_name);
			if (! *(table->table_name)) {
				g_free (table->table_name);
				table->table_name = NULL;
			}
		}
		return gda_sql_table_check_validity (table, data, error);
	}
	case GDA_SQL_ANY_SQL_FUNCTION: {
		GdaSqlFunction *function = (GdaSqlFunction*) node;
		if (function->function_name) {
			g_strchomp (function->function_name);
			if (! *(function->function_name)) {
				g_free (function->function_name);
				function->function_name = NULL;
			}
		}
		return TRUE;
	}
	case GDA_SQL_ANY_SQL_SELECT_FIELD: {
		GdaSqlSelectField *field = (GdaSqlSelectField*) node;
		if (field->as) {
			g_strchomp (field->as);
			if (! *(field->as)) {
				g_free (field->as);
				field->as = NULL;
			}
		}

		if (field->expr && field->expr->value && (G_VALUE_TYPE (field->expr->value) == G_TYPE_STRING)) {
			g_free (field->field_name);
			g_free (field->table_name);
			_split_identifier_string (g_value_dup_string (field->expr->value), &(field->table_name),
						  &(field->field_name));
		}
		if (field->table_name) {
			g_strchomp (field->table_name);
			if (! *(field->table_name)) {
				g_free (field->table_name);
				field->table_name = NULL;
			}
		}
		if (field->field_name) {
			g_strchomp (field->field_name);
			if (! *(field->field_name)) {
				g_free (field->field_name);
				field->field_name = NULL;
			}
		}
		return gda_sql_select_field_check_validity (field, data, error);
	}
	case GDA_SQL_ANY_SQL_SELECT_TARGET: {
		GdaSqlSelectTarget *target = (GdaSqlSelectTarget*) node;
		if (target->as) {
			g_strchomp (target->as);
			if (! *(target->as)) {
				g_free (target->as);
				target->as = NULL;
			}
		}
		if (target->expr && target->expr->value && (G_VALUE_TYPE (target->expr->value) == G_TYPE_STRING)) {
			g_free (target->table_name);
			target->table_name = g_value_dup_string (target->expr->value);
		}
		if (target->table_name) {
			g_strchomp (target->table_name);
			if (! *(target->table_name)) {
				g_free (target->table_name);
				target->table_name = NULL;
			}
		}
		return gda_sql_select_target_check_validity (target, data, error);
	}
	default:
		break;
	}
	return TRUE;
}

static gboolean
gda_sql_expr_check_validity (GdaSqlExpr *expr, G_GNUC_UNUSED GdaSqlStatementCheckValidityData *data, G_GNUC_UNUSED GError **error)
{
	if (!expr) return TRUE;
	if (!expr->param_spec) return TRUE;
	_gda_sql_expr_check_clean (expr);

	/* 2 checks to do here:
	 *  - try to find the data type from expr->param_spec->type using data->mstruct (not yet possible)
	 *  - if expr->param_spec->type is NULL, then try to identify it (and expr->param_spec->g_type)
	 *    using @expr context, set expr->param_spec->validity_meta_dict.
	 */
        /*TO_IMPLEMENT;*/
        return TRUE;
}

void
_gda_sql_expr_check_clean (GdaSqlExpr *expr)
{
	if (!expr) return;
	if (expr->param_spec && expr->param_spec->validity_meta_dict)
		TO_IMPLEMENT;
}

static gboolean
gda_sql_field_check_validity (GdaSqlField *field, GdaSqlStatementCheckValidityData *data, GError **error)
{
	GdaSqlAnyPart *any;
	GdaSqlTable *stable;

	if (!field) return TRUE;
	_gda_sql_field_check_clean (field);

	for (any = GDA_SQL_ANY_PART(field)->parent;
	     any && (any->type != GDA_SQL_ANY_STMT_INSERT) && (any->type != GDA_SQL_ANY_STMT_UPDATE);
	     any = any->parent);
	if (!any) {
		g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
			      "%s", _("GdaSqlField is not part of an INSERT or UPDATE statement"));
		return FALSE;
	}

	if (any->type == GDA_SQL_ANY_STMT_INSERT)
		stable = ((GdaSqlStatementInsert*) any)->table;
	else if (any->type == GDA_SQL_ANY_STMT_UPDATE)
		stable = ((GdaSqlStatementUpdate*) any)->table;
	else
		g_assert_not_reached ();

	if (!stable) {
		g_set_error (error, GDA_SQL_ERROR, GDA_SQL_VALIDATION_ERROR,
			      "%s", _("Missing table in statement"));
		return FALSE;
	}
	if (!stable->validity_meta_object) {
		if (! gda_sql_table_check_validity (stable, data, error))
			return FALSE;
	}

	g_assert (stable->validity_meta_object);
	GdaMetaTableColumn *tcol;
	GValue value;
	memset (&value, 0, sizeof (GValue));
	g_value_set_string (g_value_init (&value, G_TYPE_STRING), field->field_name);
	tcol = gda_meta_struct_get_table_column (data->mstruct,
						 GDA_META_TABLE (stable->validity_meta_object),
						 &value);
	g_value_unset (&value);
	field->validity_meta_table_column = tcol;
	if (!tcol) {
		g_set_error (error, GDA_SQL_ERROR, GDA_SQL_VALIDATION_ERROR,
			     _("Column '%s' not found"), field->field_name);
		return FALSE;
	}

        return TRUE;
}

void
_gda_sql_field_check_clean (GdaSqlField *field)
{
	if (!field) return;
	field->validity_meta_table_column = NULL;
}

/* For GdaSqlSelectTarget, GdaSqlSelectField and GdaSqlTable */
static GdaMetaDbObject *
find_table_or_view (GdaSqlAnyPart *part, GdaSqlStatementCheckValidityData *data, const gchar *name, GError **error)
{
	GdaMetaDbObject *dbo;
	GValue value;
	GError *lerror = NULL;

	memset (&value, 0, sizeof (GValue));

	/* use @name as the table or view's real name */
	g_value_set_string (g_value_init (&value, G_TYPE_STRING), name);
	dbo = gda_meta_struct_complement (data->mstruct, GDA_META_DB_UNKNOWN,
					  NULL, NULL, &value, &lerror);
	g_value_unset (&value);
	if (!dbo && (*name == '"')) {
		gchar *tmp;
		g_clear_error (&lerror);
		tmp = gda_sql_identifier_quote (name, data->cnc, NULL, TRUE, FALSE);
		dbo = find_table_or_view (part, data, tmp, error);
		g_free (tmp);
		return dbo;
	}
	if (!dbo) {
		/* use @name as a table alias in the statement */
		GdaSqlAnyPart *any;

		for (any = part->parent;
		     any && any->parent && (any->type != GDA_SQL_ANY_STMT_SELECT);
		     any = any->parent);
		if (!any)
			g_set_error (&lerror, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
				      "%s", _("GdaSqlSelectField is not part of a SELECT statement"));
		else {
			switch (any->type) {
			case GDA_SQL_ANY_STMT_SELECT: {
				GdaSqlStatementSelect *select = (GdaSqlStatementSelect*) any;
				if (select->from) {
					GSList *targets;
					for (targets = select->from->targets; targets; targets = targets->next) {
						GdaSqlSelectTarget *target = (GdaSqlSelectTarget*) targets->data;
						if (!target->as || strcmp (target->as, name))
							continue;
						g_value_set_string (g_value_init (&value, G_TYPE_STRING),
								    target->table_name);
						dbo = gda_meta_struct_complement (data->mstruct,
										  GDA_META_DB_UNKNOWN,
										  NULL, NULL, &value, NULL);
						g_value_unset (&value);
						if (dbo)
							break;
					}
				}
				break;
			}
			case GDA_SQL_ANY_STMT_INSERT:
				TO_IMPLEMENT;
				break;
			case GDA_SQL_ANY_STMT_UPDATE:
				TO_IMPLEMENT;
				break;
			case GDA_SQL_ANY_STMT_DELETE:
				TO_IMPLEMENT;
				break;
			case GDA_SQL_ANY_STMT_COMPOUND:
				TO_IMPLEMENT;
				break;
			default:
				g_assert_not_reached ();
				break;
			}
		}
	}
	if (dbo) {
		if (lerror)
			g_error_free (lerror);
	}
	else {
		if (lerror)
			g_propagate_error (error, lerror);
	}

	return dbo;
}

static gboolean
gda_sql_table_check_validity (GdaSqlTable *table, GdaSqlStatementCheckValidityData *data, GError **error)
{
	GdaMetaDbObject *dbo;

	if (!table) return TRUE;
	_gda_sql_table_check_clean (table);

	if (!table->table_name) {
		g_set_error (error, GDA_SQL_ERROR, GDA_SQL_VALIDATION_ERROR,
			      "%s", _("Missing table name in statement"));
		return FALSE;
	}

	dbo = find_table_or_view ((GdaSqlAnyPart*) table, data, table->table_name, error);
	if (dbo && ((dbo->obj_type != GDA_META_DB_TABLE) &&
		    (dbo->obj_type != GDA_META_DB_VIEW))) {
		g_set_error (error, GDA_SQL_ERROR, GDA_SQL_VALIDATION_ERROR,
			     _("Table '%s' not found"), table->table_name);
		return FALSE;
	}
	table->validity_meta_object = dbo;

	return table->validity_meta_object ? TRUE : FALSE;
}

void
_gda_sql_table_check_clean (GdaSqlTable *table)
{
	if (!table) return;
	table->validity_meta_object = NULL;
}

static gboolean
gda_sql_select_field_check_validity (GdaSqlSelectField *field, GdaSqlStatementCheckValidityData *data, GError **error)
{
	GValue value;
	GdaMetaDbObject *dbo = NULL;
	gboolean starred_field = FALSE;

	if (!field) return TRUE;
	_gda_sql_select_field_check_clean (field);

	if (!field->field_name)
		/* field is not a table.field */
		return TRUE;

	memset (&value, 0, sizeof (GValue));
	if (gda_identifier_equal (field->field_name, "*"))
		starred_field = TRUE;

	if (!field->table_name) {
		/* go through all the SELECT's targets to see if there is a table with the corresponding field */
		GdaSqlAnyPart *any;
		GSList *targets;
		GdaMetaTableColumn *tcol = NULL;

		for (any = GDA_SQL_ANY_PART(field)->parent;
		     any && (any->type != GDA_SQL_ANY_STMT_SELECT);
		     any = any->parent);
		if (!any) {
			g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
				      "%s", _("GdaSqlSelectField is not part of a SELECT statement"));
			return FALSE;
		}

		if (((GdaSqlStatementSelect *)any)->from) {
			for (targets = ((GdaSqlStatementSelect *)any)->from->targets;
			     targets;
			     targets = targets->next) {
				GdaSqlSelectTarget *target = (GdaSqlSelectTarget *) targets->data;
				if (!target->validity_meta_object &&
				    !gda_sql_select_target_check_validity (target, data, error))
					return FALSE;
				
				g_value_set_string (g_value_init (&value, G_TYPE_STRING), field->field_name);
				tcol = gda_meta_struct_get_table_column (data->mstruct,
									 GDA_META_TABLE (target->validity_meta_object),
									 &value);
				g_value_unset (&value);
				if (tcol) {
					/* found a candidate */
					if (dbo) {
						g_set_error (error, GDA_SQL_ERROR, GDA_SQL_VALIDATION_ERROR,
							     _("Could not identify table for field '%s'"), field->field_name);
						return FALSE;
					}
					dbo = target->validity_meta_object;
				}
			}
			if (!dbo) {
				targets = ((GdaSqlStatementSelect *)any)->from->targets;
				if (starred_field && targets && !targets->next)
					/* only one target => it's the one */
					dbo = ((GdaSqlSelectTarget*) targets->data)->validity_meta_object;
			}
		}
		if (!dbo) {
			g_set_error (error, GDA_SQL_ERROR, GDA_SQL_VALIDATION_ERROR,
				     _("Could not identify table for field '%s'"), field->field_name);
			return FALSE;
		}
		field->validity_meta_object = dbo;
		field->validity_meta_table_column = tcol;
	}
	else {
		/* table part */
		dbo = find_table_or_view ((GdaSqlAnyPart*) field, data, field->table_name, error);
		if (dbo && (dbo->obj_type != GDA_META_DB_TABLE) &&
		    (dbo->obj_type != GDA_META_DB_VIEW)) {
			g_set_error (error, GDA_SQL_ERROR, GDA_SQL_VALIDATION_ERROR,
				     _("Table '%s' not found"), field->table_name);
			return FALSE;
		}
		field->validity_meta_object = dbo;
		if (!field->validity_meta_object)
			return FALSE;

		/* field part */
		if (!gda_identifier_equal (field->field_name, "*")) {
			GdaMetaTableColumn *tcol;
			g_value_set_string (g_value_init (&value, G_TYPE_STRING), field->field_name);
			tcol = gda_meta_struct_get_table_column (data->mstruct,
								 GDA_META_TABLE (field->validity_meta_object),
								 &value);
			g_value_unset (&value);
			field->validity_meta_table_column = tcol;
			if (!tcol) {
				g_set_error (error, GDA_SQL_ERROR, GDA_SQL_VALIDATION_ERROR,
					     _("Column '%s' not found"), field->field_name);
				return FALSE;
			}
		}
	}
	return TRUE;
}

void
_gda_sql_select_field_check_clean (GdaSqlSelectField *field)
{
	if (!field) return;
	field->validity_meta_object = NULL;
	field->validity_meta_table_column = NULL;
}

static gboolean
gda_sql_select_target_check_validity (GdaSqlSelectTarget *target, GdaSqlStatementCheckValidityData *data, GError **error)
{
	GdaMetaDbObject *dbo;
	if (!target || !target->table_name)
		return TRUE;

	dbo = find_table_or_view ((GdaSqlAnyPart*) target, data, target->table_name, error);
	if (dbo && (dbo->obj_type != GDA_META_DB_TABLE) &&
	    (dbo->obj_type != GDA_META_DB_VIEW)) {
		g_set_error (error, GDA_SQL_ERROR, GDA_SQL_VALIDATION_ERROR,
			     _("Table '%s' not found"), target->table_name);
		return FALSE;
	}
	target->validity_meta_object = dbo;
	return target->validity_meta_object ? TRUE : FALSE;
}

void
_gda_sql_select_target_check_clean (GdaSqlSelectTarget *target)
{
	if (!target) return;
	target->validity_meta_object = NULL;
}



/**
 * gda_sql_statement_check_clean
 * @stmt: a pinter to a #GdaSqlStatement structure
 *
 * Cleans any data set by a previous call to gda_sql_statement_check_validity().
 */
void
gda_sql_statement_check_clean (GdaSqlStatement *stmt)
{
	g_return_if_fail (stmt);

	if (stmt->validity_meta_struct) {
		gda_sql_any_part_foreach (GDA_SQL_ANY_PART (stmt->contents),
					  (GdaSqlForeachFunc) foreach_check_clean, NULL, NULL);
		g_object_unref (stmt->validity_meta_struct);
		stmt->validity_meta_struct = NULL;
	}
}

static gboolean
foreach_check_clean (GdaSqlAnyPart *node, G_GNUC_UNUSED gpointer data, G_GNUC_UNUSED GError **error)
{
	if (!node) return TRUE;

	switch (node->type) {
	case GDA_SQL_ANY_EXPR:
		_gda_sql_expr_check_clean ((GdaSqlExpr*) node);
		break;
	case GDA_SQL_ANY_SQL_FIELD:
		_gda_sql_field_check_clean ((GdaSqlField*) node);
		break;
	case GDA_SQL_ANY_SQL_TABLE:
		_gda_sql_table_check_clean ((GdaSqlTable*) node);
		break;
	case GDA_SQL_ANY_SQL_SELECT_FIELD:
		_gda_sql_select_field_check_clean ((GdaSqlSelectField*) node);
		break;
	case GDA_SQL_ANY_SQL_SELECT_TARGET:
		_gda_sql_select_target_check_clean ((GdaSqlSelectTarget*) node);
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
 * is targeted).
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
foreach_check_struct (GdaSqlAnyPart *node, G_GNUC_UNUSED gpointer data, GError **error)
{
	return gda_sql_any_part_check_structure (node, error);
}

/**
 * gda_sql_any_part_check_structure
 * @node: a #GdaSqlAnyPart pointer
 * @error: a place to store errors, or %NULL
 *
 * Checks for any error in @node's structure to make sure it is valid. This
 * is the same as gda_sql_statement_check_structure() but for individual #GdaSqlAnyPart
 * parts. This function is mainly for database provider's implementations
 *
 * Returns: TRUE if no error occurred
 */
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
		cinfo = gda_sql_statement_get_contents_infos ((GdaSqlStatementType) node->type);
		if (cinfo->check_structure_func)
			return cinfo->check_structure_func (node, NULL, error);
		break;
	}
	case GDA_SQL_ANY_EXPR: {
		GdaSqlExpr *expr = (GdaSqlExpr*) node;
		if (expr->cast_as && expr->param_spec) {
			g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
				      "%s", _("Expression can't have both a type cast and a parameter specification"));
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
					      "%s", _("Empty identifier"));
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
					      "%s", _("Empty identifier"));
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
					      "%s", _("Empty identifier"));
			return FALSE;
		}
		break;
	}
        case GDA_SQL_ANY_SQL_OPERATION: {
		GdaSqlOperation *operation = (GdaSqlOperation*) node;
		if (!operation->operands) {
			g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
				      "%s", _("Operation has no operand"));
			return FALSE;
		}
		switch (operation->operator_type) {
		case GDA_SQL_OPERATOR_TYPE_EQ:
		case GDA_SQL_OPERATOR_TYPE_IS:
		case GDA_SQL_OPERATOR_TYPE_LIKE:
		case GDA_SQL_OPERATOR_TYPE_NOTLIKE:
		case GDA_SQL_OPERATOR_TYPE_ILIKE:
		case GDA_SQL_OPERATOR_TYPE_NOTILIKE:
		case GDA_SQL_OPERATOR_TYPE_GT:
		case GDA_SQL_OPERATOR_TYPE_LT:
		case GDA_SQL_OPERATOR_TYPE_GEQ:
		case GDA_SQL_OPERATOR_TYPE_LEQ:
		case GDA_SQL_OPERATOR_TYPE_DIFF:
		case GDA_SQL_OPERATOR_TYPE_REGEXP:
		case GDA_SQL_OPERATOR_TYPE_REGEXP_CI:
		case GDA_SQL_OPERATOR_TYPE_NOT_REGEXP:
		case GDA_SQL_OPERATOR_TYPE_NOT_REGEXP_CI:
		case GDA_SQL_OPERATOR_TYPE_SIMILAR:
		case GDA_SQL_OPERATOR_TYPE_REM:
		case GDA_SQL_OPERATOR_TYPE_DIV:
		case GDA_SQL_OPERATOR_TYPE_BITAND:
		case GDA_SQL_OPERATOR_TYPE_BITOR:
			if (g_slist_length (operation->operands) != 2) {
				g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
					      "%s", _("Wrong number of operands"));
				return FALSE;
			}
			break;
		case GDA_SQL_OPERATOR_TYPE_BETWEEN:
			if (g_slist_length (operation->operands) != 3) {
				g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
					      "%s", _("Wrong number of operands"));
				return FALSE;
			}
			break;
		case GDA_SQL_OPERATOR_TYPE_BITNOT:
		case GDA_SQL_OPERATOR_TYPE_ISNULL:
		case GDA_SQL_OPERATOR_TYPE_ISNOTNULL:
		case GDA_SQL_OPERATOR_TYPE_NOT:
			if (g_slist_length (operation->operands) != 1) {
				g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
					      "%s", _("Wrong number of operands"));
				return FALSE;
			}
			break;
		case GDA_SQL_OPERATOR_TYPE_AND:
		case GDA_SQL_OPERATOR_TYPE_OR:
		case GDA_SQL_OPERATOR_TYPE_IN:
		case GDA_SQL_OPERATOR_TYPE_NOTIN:
		case GDA_SQL_OPERATOR_TYPE_CONCAT:
		case GDA_SQL_OPERATOR_TYPE_STAR:
			if (g_slist_length (operation->operands) < 2) {
				g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
					      "%s", _("Wrong number of operands"));
				return FALSE;
			}
			break;
		case GDA_SQL_OPERATOR_TYPE_MINUS:
		case GDA_SQL_OPERATOR_TYPE_PLUS:
			if (g_slist_length (operation->operands) == 0) {
				g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
					      "%s", _("Wrong number of operands"));
				return FALSE;
			}
			break;
		default:
			g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
				     _("Unknown operator %d"), operation->operator_type);
			return FALSE;
		}
		break;
	}
        case GDA_SQL_ANY_SQL_CASE: {
		GdaSqlCase *sc = (GdaSqlCase*) node;
		if (g_slist_length (sc->when_expr_list) != g_slist_length (sc->then_expr_list)) {
			g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
				      "%s", "Number of WHEN is not the same as number of THEN in CASE expression");
			return FALSE;
		}
		if (!sc->when_expr_list) {
			g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
					      "%s", "CASE expression must have at least one WHEN ... THEN element");
			return FALSE;
		}
		break;
	}
	case GDA_SQL_ANY_SQL_SELECT_FIELD: {
		GdaSqlSelectField *field = (GdaSqlSelectField*) node;
		if (!field->expr) {
			g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
				      "%s", _("Missing expression in select field"));
			return FALSE;
		}
		break;
	}
	case GDA_SQL_ANY_SQL_SELECT_TARGET: {
		GdaSqlSelectTarget *target = (GdaSqlSelectTarget*) node;
		if (!target->expr) {
			g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
				      "%s", _("Missing expression in select target"));
			return FALSE;
		}
		break;
	}
	case GDA_SQL_ANY_SQL_SELECT_JOIN: {
		GdaSqlSelectJoin *join = (GdaSqlSelectJoin*) node;
		if (join->expr && join->use) {
			g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
				      "%s", _("Join can't at the same time specify a join condition and a list of fields to join on"));
			return FALSE;
		}
		if ((join->type == GDA_SQL_SELECT_JOIN_CROSS) && (join->expr || join->use)) {
			g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
				      "%s", _("Cross join can't have a join condition or a list of fields to join on"));
			return FALSE;
		}
		break;
	}
	case GDA_SQL_ANY_SQL_SELECT_FROM: {
		GdaSqlSelectFrom *from = (GdaSqlSelectFrom*) node;
		if (!from->targets) {
			g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
				      "%s", _("Empty FROM clause"));
			return FALSE;
		}
		break;
	}
        case GDA_SQL_ANY_SQL_SELECT_ORDER: {
		GdaSqlSelectOrder *order = (GdaSqlSelectOrder*) node;
		if (!order->expr) {
			g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
				      "%s", "ORDER BY expression must have an expression");
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
 * gda_sql_any_part_foreach:
 * @node: the stat node
 * @func: (scope call): function to call for each sub node
 * @data: (closure): data to pass to @func each time it is called
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
		for (l = join->use; l; l = l->next)
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


static gboolean foreach_normalize (GdaSqlAnyPart *node, GdaConnection *cnc, GError **error);

/**
 * gda_sql_statement_normalize:
 * @stmt: a pointer to a #GdaSqlStatement structure
 * @cnc: (nullable): a #GdaConnection object, or %NULL
 * @error: a place to store errors, or %NULL
 *
 * "Normalizes" (in place) some parts of @stmt, which means @stmt may be modified.
 * At the moment any "*" field in a SELECT statement will be replaced by one
 * #GdaSqlSelectField structure for each field in the referenced table.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_sql_statement_normalize (GdaSqlStatement *stmt, GdaConnection *cnc, GError **error)
{
	gboolean retval;
	g_return_val_if_fail (stmt, FALSE);

	if (!stmt->validity_meta_struct && !gda_sql_statement_check_validity (stmt, cnc, error))
		return FALSE;

	retval = gda_sql_any_part_foreach (GDA_SQL_ANY_PART (stmt->contents),
					   (GdaSqlForeachFunc) foreach_normalize, cnc, error);
#ifdef GDA_DEBUG
	GError *lerror = NULL;
	if (retval && !gda_sql_statement_check_validity (stmt, cnc, &lerror)) {
		g_warning ("Internal error in %s(): statement is not valid anymore after: %s", __FUNCTION__,
			   lerror && lerror->message ? lerror->message :  "No detail");
		if (lerror)
			g_error_free (lerror);
	}
#endif
	return retval;
}

static gboolean
foreach_normalize (GdaSqlAnyPart *node, G_GNUC_UNUSED GdaConnection *cnc, GError **error)
{
	if (!node) return TRUE;

	if (node->type == GDA_SQL_ANY_SQL_SELECT_FIELD) {
		GdaSqlSelectField *field = (GdaSqlSelectField*) node;
		if (((field->field_name && gda_identifier_equal (field->field_name, "*")) ||
		    (field->expr && field->expr->value && (G_VALUE_TYPE (field->expr->value) == G_TYPE_STRING) &&
		     gda_identifier_equal (g_value_get_string (field->expr->value), "*"))) &&
		    field->validity_meta_object) {
			/* expand * to all the fields */
			GdaMetaTable *mtable = GDA_META_TABLE (field->validity_meta_object);
			GSList *list;
			GdaSqlAnyPart *parent_node = ((GdaSqlAnyPart*) field)->parent;
			gint nodepos = g_slist_index (((GdaSqlStatementSelect*) parent_node)->expr_list, node);
			if (parent_node->type != GDA_SQL_ANY_STMT_SELECT) {
				g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
					      "%s", _("Select field is not in a SELECT statement"));
				return FALSE;
			}
			for (list = mtable->columns; list; list = list->next) {
				GdaSqlSelectField *nfield;
				GdaMetaTableColumn *tcol = (GdaMetaTableColumn *) list->data;

				nfield = gda_sql_select_field_new (parent_node);
				nfield->field_name = g_strdup (tcol->column_name);
				if (field->table_name)
					nfield->table_name = g_strdup (field->table_name);
				nfield->validity_meta_object = field->validity_meta_object;
				nfield->validity_meta_table_column = tcol;
				nfield->expr = gda_sql_expr_new ((GdaSqlAnyPart*) nfield);
				nfield->expr->value = gda_value_new (G_TYPE_STRING);
				if (field->table_name)
					g_value_take_string (nfield->expr->value, g_strdup_printf ("%s.%s",
												   nfield->table_name,
												   nfield->field_name));
				else
					g_value_set_string (nfield->expr->value, nfield->field_name);

				/* insert nfield into expr_list */
				GSList *expr_list = ((GdaSqlStatementSelect*) parent_node)->expr_list;
				if (list == mtable->columns) {
					GSList *lnode = g_slist_nth (expr_list, nodepos);
					lnode->data = nfield;
				}
				else
					((GdaSqlStatementSelect*) parent_node)->expr_list =
						g_slist_insert (expr_list, nfield, ++nodepos);
			}
			/* get rid of @field */
			gda_sql_select_field_free (field);
		}
	}

	return TRUE;
}

/* GdaSql* definitions */


GdaSqlAnyPart*
gda_sql_any_part_copy (GdaSqlAnyPart *src) {
	GdaSqlAnyPart *cp = g_new0(GdaSqlAnyPart, 1);
	cp->type = src->type;
	cp->parent = src->parent;
	return cp;
}
void
gda_sql_any_part_free (GdaSqlAnyPart *anyp) {
	g_free (anyp);
}

G_DEFINE_BOXED_TYPE(GdaSqlAnyPart, gda_sql_any_part, gda_sql_any_part_copy, gda_sql_any_part_free)

