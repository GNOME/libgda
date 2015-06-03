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
#include <libgda/sql-parser/gda-statement-struct-update.h>
#include <libgda/sql-parser/gda-statement-struct-util.h>
#include <libgda/sql-parser/gda-statement-struct-pspec.h>
#include <string.h>
#include <glib/gi18n-lib.h>

static gpointer  gda_sql_statement_update_new (void);
static void      gda_sql_statement_update_free (gpointer stmt);
static gpointer  gda_sql_statement_update_copy (gpointer src);
static gchar    *gda_sql_statement_update_serialize (gpointer stmt);
static gboolean  gda_sql_statement_update_check_structure (GdaSqlAnyPart *stmt, gpointer data, GError **error);

GdaSqlStatementContentsInfo update_infos = {
	GDA_SQL_STATEMENT_UPDATE,
	"UPDATE",
	gda_sql_statement_update_new,
	gda_sql_statement_update_free,
	gda_sql_statement_update_copy,
	gda_sql_statement_update_serialize,

	gda_sql_statement_update_check_structure,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

GdaSqlStatementContentsInfo *
_gda_sql_statement_update_get_infos (void)
{
	return &update_infos;
}

static gpointer
gda_sql_statement_update_new (void)
{
	GdaSqlStatementUpdate *stmt;
	stmt = g_new0 (GdaSqlStatementUpdate, 1);
	GDA_SQL_ANY_PART (stmt)->type = GDA_SQL_ANY_STMT_UPDATE;
	return (gpointer) stmt;
}

static void
gda_sql_statement_update_free (gpointer stmt)
{
	GdaSqlStatementUpdate *update = (GdaSqlStatementUpdate *) stmt;
	GSList *list;

	if (update->table)
		gda_sql_table_free (update->table);
	for (list = update->fields_list; list; list = list->next) {
		if (list->data)
			gda_sql_field_free ((GdaSqlField *) list->data);
	}
	if (update->fields_list)
		g_slist_free (update->fields_list);

	for (list = update->expr_list; list; list = list->next) {
		if (list->data)
			gda_sql_expr_free ((GdaSqlExpr *) list->data);
	}
	if (update->expr_list)
		g_slist_free (update->expr_list);

	if (update->cond)
		gda_sql_expr_free (update->cond);
	g_free (update);
}

static gpointer
gda_sql_statement_update_copy (gpointer src)
{
	GdaSqlStatementUpdate *dest;
	GdaSqlStatementUpdate *update = (GdaSqlStatementUpdate *) src;
	GSList *list;

	dest = gda_sql_statement_update_new ();
	if (update->on_conflict)
                dest->on_conflict = g_strdup (update->on_conflict);

	dest->table = gda_sql_table_copy (update->table);
	gda_sql_any_part_set_parent (dest->table, dest);

	for (list = update->fields_list; list; list = list->next) {
		dest->fields_list = g_slist_prepend (dest->fields_list,
						     gda_sql_field_copy ((GdaSqlField *) list->data));
		gda_sql_any_part_set_parent (dest->fields_list->data, dest);
	}
	dest->fields_list = g_slist_reverse (dest->fields_list);

	for (list = update->expr_list; list; list = list->next) {
		dest->expr_list = g_slist_prepend (dest->expr_list,
						   gda_sql_expr_copy ((GdaSqlExpr *) list->data));
		gda_sql_any_part_set_parent (dest->expr_list->data, dest);
	}
	dest->expr_list = g_slist_reverse (dest->expr_list);

	dest->cond = gda_sql_expr_copy (update->cond);
	gda_sql_any_part_set_parent (dest->cond, dest);

	return dest;
}

static gchar *
gda_sql_statement_update_serialize (gpointer stmt)
{
	GString *string;
	gchar *str;
	GSList *list;
	GdaSqlStatementUpdate *update = (GdaSqlStatementUpdate *) stmt;

	g_return_val_if_fail (stmt, NULL);

	string = g_string_new ("\"contents\":{");

	/* table name */
	g_string_append (string, "\"table\":");
	str = gda_sql_table_serialize (update->table);
	g_string_append (string, str);
	g_free (str);

	/* fields */
	g_string_append (string, ",\"fields\":");
	if (update->fields_list) {
		g_string_append_c (string, '[');
		for (list = update->fields_list; list; list = list->next) {
			if (list != update->fields_list)
				g_string_append_c (string, ',');
			str = gda_sql_field_serialize ((GdaSqlField *) list->data);
			g_string_append (string, str);
			g_free (str);
		}
		g_string_append_c (string, ']');
	}
	else
		g_string_append (string, "null");

	/* expressions */
	g_string_append (string, ",\"expressions\":");
	if (update->expr_list) {
		g_string_append_c (string, '[');
		for (list = update->expr_list; list; list = list->next) {
			if (list != update->expr_list)
				g_string_append_c (string, ',');
			str = gda_sql_expr_serialize ((GdaSqlExpr *) list->data);
			g_string_append (string, str);
			g_free (str);
		}
		g_string_append_c (string, ']');
	}
	else
		g_string_append (string, "null");	

	/* condition */
	if (update->cond) {
		g_string_append (string, ",\"condition\":");
		str = gda_sql_expr_serialize (update->cond);
		g_string_append (string, str);
		g_free (str);
	}

	/* conflict clause */
        if (update->on_conflict) {
                g_string_append (string, ",\"on_conflict\":");
                str = _json_quote_string (update->on_conflict);
                g_string_append (string, str);
                g_free (str);
        }
	g_string_append_c (string, '}');
	str = string->str;
	g_string_free (string, FALSE);
	return str;	
}

/**
 * gda_sql_statement_update_take_table_name
 * @stmt: a #GdaSqlStatement pointer
 * @value: a table name, as a G_TYPE_STRING #GValue
 *
 * Sets the name of the table to delete from in @stmt.
 *
 * @value's ownership is transferred to
 * @stmt (which means @stmt is then responsible for freeing it when no longer needed).
 */
void
gda_sql_statement_update_take_table_name (GdaSqlStatement *stmt, GValue *value)
{
	GdaSqlStatementUpdate *update = (GdaSqlStatementUpdate *) stmt->contents;
	if (value) {
		update->table = gda_sql_table_new (GDA_SQL_ANY_PART (update));
		gda_sql_table_take_name (update->table, value);
	}
}

/**
 * gda_sql_statement_update_take_on_conflict
 * @stmt: a #GdaSqlStatement pointer
 * @value: name of the resolution conflict algorithm, as a G_TYPE_STRING #GValue
 *
 * Sets the name of the resolution conflict algorithm used by @stmt. @value's ownership is transferred to
 * @stmt (which means @stmt is then responsible for freeing it when no longer needed).
 */
void
gda_sql_statement_update_take_on_conflict (GdaSqlStatement *stmt, GValue *value)
{
	GdaSqlStatementUpdate *update = (GdaSqlStatementUpdate *) stmt->contents;
        if (value) {
                update->on_conflict = g_value_dup_string (value);
                g_value_reset (value);
                g_free (value);
        }
}

/**
 * gda_sql_statement_update_take_condition
 * @stmt: a #GdaSqlStatement pointer
 * @cond: a #GdaSqlExpr pointer
 *
 * Sets the WHERE clause of @stmt
 *
 * @expr's ownership is transferred to
 * @stmt (which means @stmt is then responsible for freeing it when no longer needed).
 */
void 
gda_sql_statement_update_take_condition (GdaSqlStatement *stmt, GdaSqlExpr *cond)
{
	GdaSqlStatementUpdate *update = (GdaSqlStatementUpdate *) stmt->contents;
	update->cond = cond;
	gda_sql_any_part_set_parent (cond, update);
}


/**
 * gda_sql_statement_update_take_set_value
 * @stmt: a #GdaSqlStatement pointer
 * @fname: a field name, as a G_TYPE_STRING #GValue
 * @expr: a #GdaSqlExpr pointer
 *
 * Specifies that the field named @fname will be updated with the expression @expr.
 *
 * @fname and @expr's responsibility are transferred to
 * @stmt (which means @stmt is then responsible for freeing them when no longer needed).
 */
void
gda_sql_statement_update_take_set_value (GdaSqlStatement *stmt, GValue *fname, GdaSqlExpr *expr)
{
	GdaSqlStatementUpdate *update = (GdaSqlStatementUpdate *) stmt->contents;
	GdaSqlField *sf;

	sf = gda_sql_field_new (GDA_SQL_ANY_PART (update));
	gda_sql_field_take_name (sf, fname);
	update->fields_list = g_slist_append (update->fields_list, sf);

	update->expr_list = g_slist_append (update->expr_list, expr);
	gda_sql_any_part_set_parent (expr, update);
}

static gboolean
gda_sql_statement_update_check_structure (GdaSqlAnyPart *stmt, G_GNUC_UNUSED gpointer data, GError **error)
{
	GdaSqlStatementUpdate *update = (GdaSqlStatementUpdate *) stmt;

	if (!update->table) {
		g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
			      "%s", _("UPDATE statement needs a table to update data"));
		return FALSE;
	}

	if (g_slist_length (update->fields_list) != g_slist_length (update->expr_list)) {
		g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
			      "%s", _("UPDATE statement does not have the same number of target columns and expressions"));
		return FALSE;
	}

	if (!update->fields_list) {
		g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
			      "%s", _("UPDATE statement does not have any target columns to update"));
		return FALSE;
	}

        return TRUE;
}
