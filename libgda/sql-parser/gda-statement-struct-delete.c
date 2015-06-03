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
#include <libgda/sql-parser/gda-statement-struct-delete.h>
#include <libgda/sql-parser/gda-statement-struct-util.h>
#include <libgda/sql-parser/gda-statement-struct-pspec.h>
#include <string.h>
#include <glib/gi18n-lib.h>

static gpointer  gda_sql_statement_delete_new (void);
static void      gda_sql_statement_delete_free (gpointer stmt);
static gpointer  gda_sql_statement_delete_copy (gpointer src);
static gchar    *gda_sql_statement_delete_serialize (gpointer stmt);
static gboolean  gda_sql_statement_delete_check_structure (GdaSqlAnyPart *stmt, gpointer data, GError **error);

GdaSqlStatementContentsInfo delete_infos = {
	GDA_SQL_STATEMENT_DELETE,
	"DELETE",
	gda_sql_statement_delete_new,
	gda_sql_statement_delete_free,
	gda_sql_statement_delete_copy,
	gda_sql_statement_delete_serialize,

	gda_sql_statement_delete_check_structure,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

GdaSqlStatementContentsInfo *
_gda_sql_statement_delete_get_infos (void)
{
	return &delete_infos;
}

static gpointer
gda_sql_statement_delete_new (void)
{
	GdaSqlStatementDelete *stmt;
	stmt = g_new0 (GdaSqlStatementDelete, 1);
	GDA_SQL_ANY_PART (stmt)->type = GDA_SQL_ANY_STMT_DELETE;
	return (gpointer) stmt;
}

static void
gda_sql_statement_delete_free (gpointer stmt)
{
	GdaSqlStatementDelete *delete = (GdaSqlStatementDelete *) stmt;

	if (delete->table)
		gda_sql_table_free (delete->table);
	if (delete->cond)
		gda_sql_expr_free (delete->cond);
	g_free (delete);
}

static gpointer
gda_sql_statement_delete_copy (gpointer src)
{
	GdaSqlStatementDelete *dest;
	GdaSqlStatementDelete *delete = (GdaSqlStatementDelete *) src;

	dest = gda_sql_statement_delete_new ();

	dest->table = gda_sql_table_copy (delete->table);
	gda_sql_any_part_set_parent (dest->table, dest);

	dest->cond = gda_sql_expr_copy (delete->cond);
	gda_sql_any_part_set_parent (dest->cond, dest);

	return dest;
}

static gchar *
gda_sql_statement_delete_serialize (gpointer stmt)
{
	GString *string;
	gchar *str;
	GdaSqlStatementDelete *delete = (GdaSqlStatementDelete *) stmt;

	g_return_val_if_fail (stmt, NULL);

	string = g_string_new ("\"contents\":{");

	/* table name */
	g_string_append (string, "\"table\":");
	str = gda_sql_table_serialize (delete->table);
	g_string_append (string, str);
	g_free (str);

	/* condition */
	if (delete->cond) {
		g_string_append (string, ",\"condition\":");
		str = gda_sql_expr_serialize (delete->cond);
		g_string_append (string, str);
		g_free (str);
	}
	g_string_append_c (string, '}');
	str = string->str;
	g_string_free (string, FALSE);
	return str;	
}

/**
 * gda_sql_statement_delete_take_table_name
 * @stmt: a #GdaSqlStatement pointer
 * @value: a table name as a G_TYPE_STRING #GValue 
 *
 * Sets the name of the table to delete from in @stmt. @value's ownership is transferred to
 * @stmt (which means @stmt is then responsible for freeing it when no longer needed).
 */
void
gda_sql_statement_delete_take_table_name (GdaSqlStatement *stmt, GValue *value)
{
	GdaSqlStatementDelete *delete = (GdaSqlStatementDelete *) stmt->contents;
	if (value) {
		delete->table = gda_sql_table_new (GDA_SQL_ANY_PART (stmt));
		gda_sql_table_take_name (delete->table, value);
	}
}

/**
 * gda_sql_statement_delete_take_condition
 * @stmt: a #GdaSqlStatement pointer
 * @cond: the WHERE condition of the DELETE statement, as a #GdaSqlExpr 
 *
 * Sets the WHERE condition of @stmt. @cond's ownership is transferred to
 * @stmt (which means @stmt is then responsible for freeing it when no longer needed).
 */
void 
gda_sql_statement_delete_take_condition (GdaSqlStatement *stmt, GdaSqlExpr *cond)
{
	GdaSqlStatementDelete *delete = (GdaSqlStatementDelete *) stmt->contents;
	delete->cond = cond;
	gda_sql_any_part_set_parent (delete->cond, delete);
}

static gboolean
gda_sql_statement_delete_check_structure (GdaSqlAnyPart *stmt, G_GNUC_UNUSED gpointer data, GError **error)
{
	GdaSqlStatementDelete *delete = (GdaSqlStatementDelete *) stmt;
	if (!delete->table) {
		g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
			      "%s", _("DELETE statement needs a table to delete from"));
		return FALSE;
	}        

        return TRUE;
}
