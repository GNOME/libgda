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
	NULL
};

GdaSqlStatementContentsInfo *
gda_sql_statement_insert_get_infos (void)
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
			g_slist_foreach ((GSList *) list->data, (GFunc) gda_sql_expr_free, NULL);
			g_slist_free ((GSList *) list->data);
		}
	}
	g_slist_free (insert->values_list);

	g_slist_foreach (insert->fields_list, (GFunc) gda_sql_field_free, NULL);
	g_slist_free (insert->fields_list);
	if (insert->select) {
		if (GDA_SQL_ANY_PART (insert->select)->type == GDA_SQL_ANY_STMT_SELECT)
			gda_sql_statement_select_free (insert->select);
		else if (GDA_SQL_ANY_PART (insert->select)->type == GDA_SQL_ANY_STMT_COMPOUND)
			gda_sql_statement_compound_free (insert->select);
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
			dest->select = gda_sql_statement_select_copy (insert->select);
		else if (GDA_SQL_ANY_PART (insert->select)->type == GDA_SQL_ANY_STMT_COMPOUND)
			dest->select = gda_sql_statement_compound_copy (insert->select);
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
			str = gda_sql_statement_select_serialize (insert->select);
		else if (GDA_SQL_ANY_PART (insert->select)->type == GDA_SQL_ANY_STMT_COMPOUND)
			str = gda_sql_statement_compound_serialize (insert->select);
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

void
gda_sql_statement_insert_take_table_name (GdaSqlStatement *stmt, GValue *value)
{
	GdaSqlStatementInsert *insert = (GdaSqlStatementInsert *) stmt->contents;
	if (value) {
		insert->table = gda_sql_table_new (GDA_SQL_ANY_PART (insert));
		gda_sql_table_take_name (insert->table, value);
	}
}

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

void
gda_sql_statement_insert_take_fields_list (GdaSqlStatement *stmt, GSList *list)
{
	GSList *l;
	GdaSqlStatementInsert *insert = (GdaSqlStatementInsert *) stmt->contents;
	insert->fields_list = list;
	
	for (l = list; l; l = l->next)
		gda_sql_any_part_set_parent (l->data, insert);
}

/*
 * @list is a list of GdaSqlExpr
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

/*
 * @list is a list of list of GdaSqlExpr
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

void
gda_sql_statement_insert_take_select (GdaSqlStatement *stmt, GdaSqlStatement *select)
{
	GdaSqlStatementInsert *insert = (GdaSqlStatementInsert *) stmt->contents;
	GdaSqlAnyPart *part;
	part = GDA_SQL_ANY_PART (select->contents);
	select->contents = NULL;
	gda_sql_statement_free (select);
	insert->select = gda_sql_statement_compound_reduce (part);
	gda_sql_any_part_set_parent (insert->select, insert);	
}

static gboolean
gda_sql_statement_insert_check_structure (GdaSqlAnyPart *stmt, gpointer data, GError **error)
{
	GdaSqlStatementInsert *insert = (GdaSqlStatementInsert *) stmt;
	gint nb_values;
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
			gint len;
			if (GDA_SQL_ANY_PART (insert->select)->type == GDA_SQL_ANY_STMT_SELECT) {
				GdaSqlStatementSelect *select = (GdaSqlStatementSelect*) insert->select;
				len = g_slist_length (select->expr_list);
			}
			else if (GDA_SQL_ANY_PART (insert->select)->type == GDA_SQL_ANY_STMT_COMPOUND) {
				GdaSqlStatementCompound *compound = (GdaSqlStatementCompound*) insert->select;
				len = gda_sql_statement_compound_get_n_cols (compound, error);
				if (len < 0)
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
		if (!insert->values_list) {
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
						g_set_error (error, GDA_SQL_ERROR, 0,
							      "%s", _("VALUES lists must all be the same length in INSERT statement"));
					return FALSE;
				}
		}
	}
        return TRUE;
}
