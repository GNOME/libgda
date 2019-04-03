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
#include <libgda/sql-parser/gda-statement-struct-compound.h>
#include <libgda/sql-parser/gda-statement-struct-pspec.h>
#include <string.h>
#include <glib/gi18n-lib.h>

static gpointer gda_sql_statement_compound_new (void);
static gboolean gda_sql_statement_compound_check_structure (GdaSqlAnyPart *stmt, gpointer data, GError **error);

GdaSqlStatementContentsInfo compound_infos = {
	GDA_SQL_STATEMENT_COMPOUND,
	"COMPOUND",
	gda_sql_statement_compound_new,
	_gda_sql_statement_compound_free,
	_gda_sql_statement_compound_copy,
	_gda_sql_statement_compound_serialize,

	gda_sql_statement_compound_check_structure,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

GdaSqlStatementContentsInfo *
_gda_sql_statement_compound_get_infos (void)
{
	return &compound_infos;
}

static gpointer
gda_sql_statement_compound_new (void)
{
	GdaSqlStatementCompound *stmt;
	stmt = g_new0 (GdaSqlStatementCompound, 1);
	stmt->compound_type = -1;
	GDA_SQL_ANY_PART (stmt)->type = GDA_SQL_ANY_STMT_COMPOUND;
	return stmt;
}

void
_gda_sql_statement_compound_free (gpointer stmt)
{
	GdaSqlStatementCompound *compound = (GdaSqlStatementCompound *) stmt;

	if (compound->stmt_list) {
		g_slist_free_full (compound->stmt_list, (GDestroyNotify) gda_sql_statement_free);
	}
	g_free (compound);
}

gpointer
_gda_sql_statement_compound_copy (gpointer src)
{
	GdaSqlStatementCompound *dest;
	GdaSqlStatementCompound *compound = (GdaSqlStatementCompound *) src;
	GSList *list;

	dest = gda_sql_statement_compound_new ();
	dest->compound_type = compound->compound_type;
	for (list = compound->stmt_list; list; list = list->next) {
		GdaSqlStatement *sqlst;
		sqlst = gda_sql_statement_copy ((GdaSqlStatement*) list->data);
		gda_sql_any_part_set_parent (sqlst->contents, dest);
		dest->stmt_list = g_slist_prepend (dest->stmt_list, sqlst);
	}
	dest->stmt_list = g_slist_reverse (dest->stmt_list);

	return dest;
}

gchar *
_gda_sql_statement_compound_serialize (gpointer stmt)
{
	GString *string;
	gchar *str;
	GSList *list;
	GdaSqlStatementCompound *compound = (GdaSqlStatementCompound *) stmt;

	g_return_val_if_fail (stmt, NULL);

	string = g_string_new ("\"contents\":{");
	g_string_append (string, "\"compount_type\":");
	switch (compound->compound_type) {
	case GDA_SQL_STATEMENT_COMPOUND_UNION:
		str = "UNION"; break;
	case GDA_SQL_STATEMENT_COMPOUND_UNION_ALL:
		str = "AUNION"; break;
	case GDA_SQL_STATEMENT_COMPOUND_INTERSECT:
		str = "INTERSECT"; break;
	case GDA_SQL_STATEMENT_COMPOUND_INTERSECT_ALL:
		str = "AINTERSECT"; break;
	case GDA_SQL_STATEMENT_COMPOUND_EXCEPT:
		str = "EXCEPT"; break;
	case GDA_SQL_STATEMENT_COMPOUND_EXCEPT_ALL:
		str = "AEXCEPT"; break;
	default:
		str = NULL;
		g_assert_not_reached ();
	}
	g_string_append_printf (string, "\"%s\"", str);

	if (compound->stmt_list) {
		g_string_append (string, ",\"select_stmts\":[");
		for (list = compound->stmt_list; list; list = list->next) {
			if (list != compound->stmt_list)
				g_string_append_c (string, ',');
			str = gda_sql_statement_serialize ((GdaSqlStatement*) list->data);
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

/**
 * gda_sql_statement_compound_take_stmt
 * @stmt: a #GdaSqlStatement pointer
 * @s: a #GdaSqlStatement pointer
 *
 * Adds the @s sub-statement to the @stmt compound statement. @s's reference is transferred to
 * @stmt (which means @stmt is then responsible for freeing it when no longer needed).
 */
void
gda_sql_statement_compound_take_stmt (GdaSqlStatement *stmt, GdaSqlStatement *s)
{
	GdaSqlStatementCompound *compound = (GdaSqlStatementCompound *) stmt->contents;

	if (s->stmt_type == GDA_SQL_STATEMENT_COMPOUND) {
		GdaSqlStatementCompound *scompound = (GdaSqlStatementCompound *) s->contents;
		if (scompound->stmt_list) {
			if (scompound->stmt_list->next) {
				compound->stmt_list = g_slist_append (compound->stmt_list, s);
				gda_sql_any_part_set_parent (s->contents, stmt);
			}
			else {
				compound->stmt_list = g_slist_append (compound->stmt_list, scompound->stmt_list->data);
				gda_sql_any_part_set_parent (((GdaSqlStatement*) scompound->stmt_list->data)->contents, stmt);
				g_slist_free (scompound->stmt_list);
				scompound->stmt_list = NULL;
				gda_sql_statement_free (s);
			}
		}
		else {
			/* ignore @s */
			gda_sql_statement_free (s);
			return;
		}
	}
	else {
		compound->stmt_list = g_slist_append (compound->stmt_list, s);
		gda_sql_any_part_set_parent (s->contents, stmt);
	}
}

GdaSqlAnyPart *
_gda_sql_statement_compound_reduce (GdaSqlAnyPart *compound_or_select)
{
	GdaSqlAnyPart *part;

	part = compound_or_select;
	if (part->type == GDA_SQL_ANY_STMT_COMPOUND) {
		/* if only one child, then use that child instead */
		GdaSqlStatementCompound *compound = (GdaSqlStatementCompound*) part;
		if (compound->stmt_list && !compound->stmt_list->next) {
			GdaSqlAnyPart *rpart;
			GdaSqlStatement *substmt;
			substmt = (GdaSqlStatement *) compound->stmt_list->data;

			rpart = GDA_SQL_ANY_PART (substmt->contents);
			substmt->contents = NULL;
			gda_sql_statement_free (substmt);

			g_slist_free (compound->stmt_list);
			compound->stmt_list = NULL;
			_gda_sql_statement_compound_free (compound);
			part = _gda_sql_statement_compound_reduce (rpart);
		}
	}

	return part;
}


/**
 * gda_sql_statement_compound_set_type
 * @stmt: a #GdaSqlStatement pointer
 * @type: a #GdaSqlStatementCompoundType value
 *
 * Specifies @stmt's type of compound
 */
void
gda_sql_statement_compound_set_type (GdaSqlStatement *stmt, GdaSqlStatementCompoundType type)
{
	GdaSqlStatementCompound *compound = (GdaSqlStatementCompound *) stmt->contents;
	compound->compound_type = type;
}

gint
_gda_sql_statement_compound_get_n_cols (GdaSqlStatementCompound *compound, GError **error)
{
	if (!compound || !compound->stmt_list) {
		g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
			      "%s", _("COMPOUND statement contains an undefined COMPOUND statement"));
		return -1;
	}
	
	/* @compound's children */
	GdaSqlStatement *sqlstmt = (GdaSqlStatement*) compound->stmt_list->data;
	if (sqlstmt->stmt_type == GDA_SQL_STATEMENT_SELECT) {
		if (!sqlstmt->contents) {
			g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
				      "%s", _("COMPOUND statement contains an undefined SELECT statement"));
			return -1;
		}
		return g_slist_length (((GdaSqlStatementSelect*) sqlstmt->contents)->expr_list);
	}
	else if (sqlstmt->stmt_type == GDA_SQL_STATEMENT_COMPOUND) 
		return _gda_sql_statement_compound_get_n_cols ((GdaSqlStatementCompound*) sqlstmt->contents, error);
	else {
		g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
			      "%s", _("COMPOUND statement contains a non SELECT statement"));
		return -1;
	}
}

static gboolean
gda_sql_statement_compound_check_structure (GdaSqlAnyPart *stmt, G_GNUC_UNUSED gpointer data, GError **error)
{
	GdaSqlStatementCompound *compound = (GdaSqlStatementCompound *) stmt;
	gint nb_cols = -1;
	GSList *list;

	if (!compound->stmt_list) {
		g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
			      "%s", _("COMPOUND statement does not contain any SELECT statement"));
		return FALSE;
	}

	if (!compound->stmt_list->next) {
		g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
			      "%s", _("COMPOUND statement only contains one SELECT statement"));
		return FALSE;
	}
	
	for (list = compound->stmt_list; list; list = list->next) {
		GdaSqlStatement *sqlstmt = (GdaSqlStatement*) list->data;
		gint nb;
		if (sqlstmt->stmt_type == GDA_SQL_STATEMENT_SELECT) {
			if (!sqlstmt->contents) {
				g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
					      "%s", _("COMPOUND statement contains an undefined SELECT statement"));
				return FALSE;
			}
			nb = g_slist_length (((GdaSqlStatementSelect*) sqlstmt->contents)->expr_list);
		}
		else if (sqlstmt->stmt_type == GDA_SQL_STATEMENT_COMPOUND) {
			nb = _gda_sql_statement_compound_get_n_cols ((GdaSqlStatementCompound*) sqlstmt->contents, error);
			if (nb < 0)
				return FALSE;
		}
		else {
			g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
			      "%s", _("COMPOUND statement contains a non SELECT statement"));
			return FALSE;
		}

		if (nb_cols == -1) {
			nb_cols = nb;
			if (nb == 0) {
				g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
					      "%s", _("COMPOUND statement contains an empty SELECT statement"));
				return FALSE;
			}
		}
		else if (nb != nb_cols) {
			g_set_error (error, GDA_SQL_ERROR, GDA_SQL_STRUCTURE_CONTENTS_ERROR,
				      "%s", _("All statements in a COMPOUND must have the same number of columns"));
			return FALSE;
		}
	}

	return TRUE;
}
