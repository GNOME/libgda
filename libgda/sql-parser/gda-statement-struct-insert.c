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
#include <libgda/sql-parser/gda-statement-struct-insert.h>
#include <libgda/sql-parser/gda-statement-struct-compound.h>
#include <libgda/sql-parser/gda-statement-struct-util.h>
#include <libgda/sql-parser/gda-statement-struct-pspec.h>
#include <string.h>
#include <glib/gi18n-lib.h>

static gpointer  gda_sql_statement_insert_new (void);
static void      gda_sql_statement_insert_free (gpointer stmt);
static gpointer  gda_sql_statement_insert_copy (gpointer src);
static gchar    *gda_sql_statement_insert_serialize (gpointer stmt);
static gboolean  gda_sql_statement_insert_check_structure (GdaSqlAnyPart *stmt, gpointer data, GError **error);

GdaSqlStatementContentsInfo insert_infos = {
	GDA_SQL_STATEMENT_INSERT,
	"INSERT",
	gda_sql_statement_insert_new,
	gda_sql_statement_insert_free,
	gda_sql_statement_insert_copy,
	gda_sql_statement_insert_serialize,

	gda_sql_statement_insert_check_structure,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

GdaSqlStatementContentsInfo *
_gda_sql_statement_insert_get_infos (void)
{
	return &insert_infos;
}

static gpointer
gda_sql_statement_insert_new (void)
{
	GdaSqlStatementInsert *stmt;
	stmt = g_new0 (GdaSqlStatementInsert, 1);
	GDA_SQL_ANY_PART (stmt)->type = GDA_SQL_ANY_STMT_INSERT;
	return (gpointer) stmt;
}

static void
gda_sql_statement_insert_free (gpointer stmt)
{
	GdaSqlStatementInsert *insert = (GdaSqlStatementInsert *) stmt;
	GSList *list;
	g_free (insert->on_conflict);
	gda_sql_table_free (insert->table);
	for (list = insert->values_list; list; list = list->next) {
		if (list->data) {
			g_slist_free_full ((GSList *) list->data, (GDestroyNotify) gda_sql_expr_free);
		}
	}
	g_slist_free (insert->values_list);

	g_slist_free_full (insert->fields_list, (GDestroyNotify) gda_sql_field_free);
	if (insert->select) {
		if (GDA_SQL_ANY_PART (insert->select)->type == GDA_SQL_ANY_STMT_SELECT)
			_gda_sql_statement_select_free (insert->select);
		else if (GDA_SQL_ANY_PART (insert->select)->type == GDA_SQL_ANY_STMT_COMPOUND)
			_gda_sql_statement_compound_free (insert->select);
		else
			g_assert_not_reached ();
	}

	g_free (insert);
}

static gpointer
gda_sql_statement_insert_copy (gpointer src)
{
	GdaSqlStatementInsert *dest;
	GSList *list;
	GdaSqlStatementInsert *insert = (GdaSqlStatementInsert *) src;

	dest = gda_sql_statement_insert_new ();
	if (insert->on_conflict)
		dest->on_conflict = g_strdup (insert->on_conflict);

	dest->table = gda_sql_table_copy (insert->table);
	gda_sql_any_part_set_parent (dest->table, dest);
	
	for (list = insert->fields_list; list; list = list->next) {
		dest->fields_list = g_slist_prepend (dest->fields_list, 
						     gda_sql_field_copy ((GdaSqlField*) list->data));
		gda_sql_any_part_set_parent (dest->fields_list->data, dest);
	}
	dest->fields_list = g_slist_reverse (dest->fields_list);

	for (list = insert->values_list; list; list = list->next) {
		GSList *vlist, *clist = NULL;
		for (vlist = (GSList *) list->data; vlist; vlist = vlist->next) {
			clist = g_slist_prepend (clist,
						 gda_sql_expr_copy ((GdaSqlExpr*) vlist->data));
			gda_sql_any_part_set_parent (clist->data, dest);
		}
		dest->values_list = g_slist_append (dest->values_list, g_slist_reverse (clist));
	}
	if (insert->select) {
		if (GDA_SQL_ANY_PART (insert->select)->type == GDA_SQL_ANY_STMT_SELECT)
			dest->select = _gda_sql_statement_select_copy (insert->select);
		else if (GDA_SQL_ANY_PART (insert->select)->type == GDA_SQL_ANY_STMT_COMPOUND)
			dest->select = _gda_sql_statement_compound_copy (insert->select);
		else
			g_assert_not_reached ();
		gda_sql_any_part_set_parent (dest->select, dest);
	}

	return dest;
}

static gchar *
gda_sql_statement_insert_serialize (gpointer stmt)
{
	GString *string;
	gchar *str;
	GSList *list;
	GdaSqlStatementInsert *insert = (GdaSqlStatementInsert *) stmt;

	g_return_val_if_fail (stmt, NULL);

	string = g_string_new ("\"contents\":{");

	/* table name */
	g_string_append (string, "\"table\":");
	str = gda_sql_table_serialize (insert->table);
	g_string_append (string, str);
	g_free (str);

	/* fields */
	g_string_append (string, ",\"fields\":");
	if (insert->fields_list) {
		g_string_append_c (string, '[');
		for (list = insert->fields_list; list; list = list->next) {
			if (list != insert->fields_list)
				g_string_append_c (string, ',');
			str = gda_sql_field_serialize ((GdaSqlField*) list->data);
			g_string_append (string, str);
			g_free (str);
		}
		g_string_append_c (string, ']');
	}
	else
		g_string_append (string, "null");	

	/* values */
	if (insert->values_list) {
		g_string_append (string, ",\"values\":[");
		for (list = insert->values_list; list; list = list->next) {
			if (list != insert->values_list)
				g_string_append_c (string, ',');
			if (list->data) {
				GSList *vlist;
				g_string_append_c (string, '[');
				for (vlist = (GSList *) list->data; vlist; vlist = vlist->next) {
					if (vlist != (GSList *) list->data)
						g_string_append_c (string, ',');
					str = gda_sql_expr_serialize ((GdaSqlExpr*) vlist->data);
					g_string_append (string, str);
					g_free (str);
				}
				g_string_append_c (string, ']');
			}
			else
				g_string_append (string, "null");
		}
		g_string_append_c (string, ']');
	}
	
	/* select statement */
	if (insert->select) {
		g_string_append (string, ",\"select\":{");
		if (GDA_SQL_ANY_PART (insert->select)->type == GDA_SQL_ANY_STMT_SELECT)
			str = _gda_sql_statement_select_serialize (insert->select);
		else if (GDA_SQL_ANY_PART (insert->select)->type == GDA_SQL_ANY_STMT_COMPOUND)
			str = _gda_sql_statement_compound_serialize (insert->select);
		else
			g_assert_not_reached ();
		
		g_string_append (string, str);
		g_free (str);
		g_string_append_c (string, '}');
	}

	/* conflict clause */
	if (insert->on_conflict) {
		g_string_append (string, ",\"on_conflict\":");
		str = _json_quote_string (insert->on_conflict);
		g_string_append (string, str);
		g_free (str);
	}
	g_string_append_c (string, '}');
	str = string->str;
	g_string_free (string, FALSE);
	return str;	
}

/**
 * gda_sql_statement_insert_take_table_name
 * @stmt: a #GdaSqlStatement pointer
 * @value: name of the table to insert into, as a G_TYPE_STRING #GValue
 *
 * Sets the name of the table to insert into in @stmt. @value's ownership is transferred to
 * @stmt (which means @stmt is then responsible for freeing it when no longer needed).
 */
void
gda_sql_statement_insert_take_table_name (GdaSqlStatement *stmt, GValue *value)
{
	GdaSqlStatementInsert *insert = (GdaSqlStatementInsert *) stmt->contents;
	if (value) {
		insert->table = gda_sql_table_new (GDA_SQL_ANY_PART (insert));
		gda_sql_table_take_name (insert->table, value);
	}
}

/**
 * gda_sql_statement_insert_take_on_conflict
 * @stmt: a #GdaSqlStatement pointer
 * @value: name of the resolution conflict algorithm, as a G_TYPE_STRING #GValue
 *
 * Sets the name of the resolution conflict algorithm used by @stmt. @value's ownership is transferred to
 * @stmt (which means @stmt is then responsible for freeing it when no longer needed).
 */
void
gda_sql_statement_insert_take_on_conflict (GdaSqlStatement *stmt, GValue *value)
{
	GdaSqlStatementInsert *insert = (GdaSqlStatementInsert *) stmt->contents;
	if (value) {
		insert->on_conflict = g_value_dup_string (value);
		g_value_reset (value);
		g_free (value);
	}
}

/**
 * gda_sql_statement_insert_take_fields_list
 * @stmt: a #GdaSqlStatement pointer
 * @list: (element-type Gda.SqlField): a list of #GdaSqlField pointers
 *
 * Sets the list of fields for which values will be specified in @stmt. @list's 
 * ownership is transferred to
 * @stmt (which means @stmt is then responsible for freeing it when no longer needed).
 */
void
gda_sql_statement_insert_take_fields_list (GdaSqlStatement *stmt, GSList *list)
{
	GSList *l;
	GdaSqlStatementInsert *insert = (GdaSqlStatementInsert *) stmt->contents;
	insert->fields_list = list;
	
	for (l = list; l; l = l->next)
		gda_sql_any_part_set_parent (l->data, insert);
}

/**
 * gda_sql_statement_insert_take_1_values_list:
 * @stmt: a #GdaSqlStatement pointer
 * @list: (element-type Gda.SqlExpr): a list of #GdaSqlExpr pointers
 *
 * Sets a list of values to be inserted by @stmt. @list's 
 * ownership is transferred to
 * @stmt (which means @stmt is then responsible for freeing it when no longer needed).
 */
void
gda_sql_statement_insert_take_1_values_list (GdaSqlStatement *stmt, GSList *list)
{
	GSList *l;
	GdaSqlStatementInsert *insert = (GdaSqlStatementInsert *) stmt->contents;

	for (l = list; l; l = l->next)
		gda_sql_any_part_set_parent (l->data, insert);
	insert->values_list = g_slist_prepend (insert->values_list, list);
}

/**
 * gda_sql_statement_insert_take_extra_values_list:
 * @stmt: a #GdaSqlStatement pointer
 * @list: (element-type GdaSqlExpr): a list of #GSList of #GdaSqlExpr pointers
 *
 * Sets a list of list of values to be inserted by @stmt. @list's 
 * ownership is transferred to
 * @stmt (which means @stmt is then responsible for freeing it when no longer needed).
 */
void
gda_sql_statement_insert_take_extra_values_list (GdaSqlStatement *stmt, GSList *list)
{
	GSList *l1, *l2;
	GdaSqlStatementInsert *insert = (GdaSqlStatementInsert *) stmt->contents;
	for (l1 = list; l1; l1 = l1->next) {
		for (l2 = (GSList *) l1->data; l2; l2 = l2->next)
			gda_sql_any_part_set_parent (l2->data, insert);
	}
	insert->values_list = g_slist_concat (insert->values_list, list);
}


/**
 * gda_sql_statement_insert_take_select
 * @stmt: a #GdaSqlStatement pointer
 * @select: a SELECT or COMPOUND #GdaSqlStatement pointer
 *
 * Specifies a SELECT statement, the values inserted will be the result set of @select. @select's 
 * ownership is transferred to
 * @stmt (which means @stmt is then responsible for freeing it when no longer needed).
 */
void
gda_sql_statement_insert_take_select (GdaSqlStatement *stmt, GdaSqlStatement *select)
{
	GdaSqlStatementInsert *insert = (GdaSqlStatementInsert *) stmt->contents;
	GdaSqlAnyPart *part;
	part = GDA_SQL_ANY_PART (select->contents);
	select->contents = NULL;
	gda_sql_statement_free (select);
	insert->select = _gda_sql_statement_compound_reduce (part);
	gda_sql_any_part_set_parent (insert->select, insert);	
}

static gboolean
gda_sql_statement_insert_check_structure (GdaSqlAnyPart *stmt, G_GNUC_UNUSED gpointer data, GError **error)
{
	GdaSqlStatementInsert *insert = (GdaSqlStatementInsert *) stmt;
	guint nb_values;
	GSList *list;
	if (!stmt) return TRUE;

	nb_values = g_slist_length (insert->fields_list); /* may be 0 */
	if (!insert->table) {
		g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
			      "%s", _("INSERT statement needs a table to insert into"));
		return FALSE;
	}
	if (insert->select) {
		if (insert->values_list) {
			g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
				      "%s", _("Can't specify values to insert and SELECT statement in INSERT statement"));
			return FALSE;
		}
		if (nb_values > 0) {
			guint len;
			if (GDA_SQL_ANY_PART (insert->select)->type == GDA_SQL_ANY_STMT_SELECT) {
				GdaSqlStatementSelect *select = (GdaSqlStatementSelect*) insert->select;
				len = g_slist_length (select->expr_list);
			}
			else if (GDA_SQL_ANY_PART (insert->select)->type == GDA_SQL_ANY_STMT_COMPOUND) {
				gint compound_len;
				GdaSqlStatementCompound *compound = (GdaSqlStatementCompound*) insert->select;
				compound_len = _gda_sql_statement_compound_get_n_cols (compound, error);
				len = compound_len;
				if (compound_len < 0)
					return FALSE;
			}
			else
				g_assert_not_reached ();
			if (len != nb_values) {
				g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
					      "%s", _("INSERT statement does not have the same number of target columns and expressions"));
				return FALSE;
			}
		}
	}
	else {
		/* using values list */
		if (!insert->values_list && (nb_values != 0)) {
			g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
				      "%s", _("Missing values to insert in INSERT statement"));
			return FALSE;
		}
		
		for (list = insert->values_list; list; list = list->next) {
			if (nb_values == 0) {
				nb_values = g_slist_length ((GSList *) list->data);
				if (nb_values == 0) {
					g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
						      "%s", _("Missing values to insert in INSERT statement"));
					return FALSE;
				}
			}
			else
				if (g_slist_length ((GSList *) list->data) != nb_values) {
					if (insert->fields_list) 
						g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
							      "%s", _("INSERT statement does not have the same number of target columns and expressions"));
					else
						g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
							      "%s", _("VALUES lists must all be the same length in INSERT statement"));
					return FALSE;
				}
		}
	}
        return TRUE;
}
