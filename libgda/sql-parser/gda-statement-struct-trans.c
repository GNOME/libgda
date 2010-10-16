/* 
 * Copyright (C) 2007 - 2009 Vivien Malerba
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

#include <libgda/sql-parser/gda-statement-struct.h>
#include <libgda/gda-debug-macros.h>
#include <libgda/sql-parser/gda-statement-struct-trans.h>
#include <libgda/sql-parser/gda-statement-struct-util.h>
#include <string.h>

static gpointer  gda_sql_statement_trans_new (void);
static void      gda_sql_statement_trans_free (gpointer stmt);
static gpointer  gda_sql_statement_trans_copy (gpointer src);
static gchar    *gda_sql_statement_trans_serialize (gpointer stmt);

/* BEGIN */
GdaSqlStatementContentsInfo begin_infos = {
	GDA_SQL_STATEMENT_BEGIN,
	"BEGIN",
	gda_sql_statement_trans_new,
	gda_sql_statement_trans_free,
	gda_sql_statement_trans_copy,
	gda_sql_statement_trans_serialize,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

GdaSqlStatementContentsInfo *
_gda_sql_statement_begin_get_infos (void)
{
	return &begin_infos;
}

/* COMMIT */
GdaSqlStatementContentsInfo commit_infos = {
	GDA_SQL_STATEMENT_COMMIT,
	"COMMIT",
	gda_sql_statement_trans_new,
	gda_sql_statement_trans_free,
	gda_sql_statement_trans_copy,
	gda_sql_statement_trans_serialize,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

GdaSqlStatementContentsInfo *
_gda_sql_statement_commit_get_infos (void)
{
	return &commit_infos;
}

/* ROLLBACK */
GdaSqlStatementContentsInfo rollback_infos = {
	GDA_SQL_STATEMENT_ROLLBACK,
	"ROLLBACK",
	gda_sql_statement_trans_new,
	gda_sql_statement_trans_free,
	gda_sql_statement_trans_copy,
	gda_sql_statement_trans_serialize,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

GdaSqlStatementContentsInfo *
_gda_sql_statement_rollback_get_infos (void)
{
	return &rollback_infos;
}

/* SAVEPOINT */
GdaSqlStatementContentsInfo svp_infos = {
	GDA_SQL_STATEMENT_SAVEPOINT,
	"SAVEPOINT",
	gda_sql_statement_trans_new,
	gda_sql_statement_trans_free,
	gda_sql_statement_trans_copy,
	gda_sql_statement_trans_serialize,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

GdaSqlStatementContentsInfo *
_gda_sql_statement_savepoint_get_infos (void)
{
	return &svp_infos;
}

/* ROLLBACK SAVEPOINT */
GdaSqlStatementContentsInfo rollback_svp_infos = {
	GDA_SQL_STATEMENT_ROLLBACK_SAVEPOINT,
	"ROLLBACK_SAVEPOINT",
	gda_sql_statement_trans_new,
	gda_sql_statement_trans_free,
	gda_sql_statement_trans_copy,
	gda_sql_statement_trans_serialize,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

GdaSqlStatementContentsInfo *
_gda_sql_statement_rollback_savepoint_get_infos (void)
{
	return &rollback_svp_infos;
}

/* DELETE SAVEPOINT */
GdaSqlStatementContentsInfo delete_svp_infos = {
	GDA_SQL_STATEMENT_DELETE_SAVEPOINT,
	"DELETE_SAVEPOINT",
	gda_sql_statement_trans_new,
	gda_sql_statement_trans_free,
	gda_sql_statement_trans_copy,
	gda_sql_statement_trans_serialize,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

GdaSqlStatementContentsInfo *
_gda_sql_statement_delete_savepoint_get_infos (void)
{
	return &delete_svp_infos;
}


static gpointer 
gda_sql_statement_trans_new (void)
{
	GdaSqlStatementTransaction *trans;
	trans = g_new0 (GdaSqlStatementTransaction, 1);
	trans->isolation_level = GDA_TRANSACTION_ISOLATION_UNKNOWN;
	return trans;
}

static void
gda_sql_statement_trans_free (gpointer stmt)
{
	GdaSqlStatementTransaction *trans = (GdaSqlStatementTransaction *) stmt;
	g_free (trans->trans_mode);
	g_free (trans->trans_name);
	g_free (trans);
}

static gpointer 
gda_sql_statement_trans_copy (gpointer src)
{
	GdaSqlStatementTransaction *dest;
	GdaSqlStatementTransaction *trans = (GdaSqlStatementTransaction *) src;

	dest = g_new0 (GdaSqlStatementTransaction, 1);
	if (trans->trans_mode)
		dest->trans_mode = g_strdup (trans->trans_mode);
	if (trans->trans_name)
		dest->trans_name = g_strdup (trans->trans_name);
	dest->isolation_level = trans->isolation_level;

	return dest;
}

static gchar *
gda_sql_statement_trans_serialize (gpointer stmt)
{
        GString *string;
        gchar *str;
	GdaSqlStatementTransaction *trans = (GdaSqlStatementTransaction *) stmt;

        g_return_val_if_fail (stmt, NULL);

        string = g_string_new ("\"contents\":{");
	g_string_append (string, "\"trans_mode\":"); 

	str = _json_quote_string (trans->trans_mode);
	g_string_append (string, str);
	g_free (str);

	g_string_append (string, ",\"trans_name\":"); 
	str = _json_quote_string (trans->trans_name);
	g_string_append (string, str);
	g_free (str);

	g_string_append (string, ",\"isol_level\":"); 
	switch (trans->isolation_level) {
	default:
	case GDA_TRANSACTION_ISOLATION_UNKNOWN:
		str = NULL;
		break;
	case GDA_TRANSACTION_ISOLATION_READ_COMMITTED:
		str = "COMMITTED_READ";
		break;
	case GDA_TRANSACTION_ISOLATION_READ_UNCOMMITTED:
		str = "UNCOMMITTED_READ";
		break;
	case GDA_TRANSACTION_ISOLATION_REPEATABLE_READ:
		str = "REPEATABLE_READ";
		break;
	case GDA_TRANSACTION_ISOLATION_SERIALIZABLE:
		str = "SERIALIZABLE";
		break;
	}
	if (str)
		g_string_append_printf (string, "\"%s\"", str);
	else
		g_string_append (string, "null");

        g_string_append_c (string, '}');
        str = string->str;
        g_string_free (string, FALSE);
        return str;
}

/**
 * gda_sql_statement_trans_take_name
 * @stmt: a #GdaSqlStatement pointer
 * @value: a G_TYPE_STRING value
 *
 * Sets the name of the transaction
 *
 * @value's responsibility is transferred to
 * @stmt (which means @stmt is then responsible for freeing it when no longer needed).
 */
void
gda_sql_statement_trans_take_name (GdaSqlStatement *stmt, GValue *value)
{
	GdaSqlStatementTransaction *trans = (GdaSqlStatementTransaction *) stmt->contents;
	if (trans->trans_name) {
		g_free (trans->trans_name);
		trans->trans_name = NULL;
	}
	if (value) {
		trans->trans_name = g_value_dup_string (value);
		g_value_unset (value);
		g_free (value);
	}
}

/**
 * gda_sql_statement_trans_take_mode
 * @stmt: a #GdaSqlStatement pointer
 * @value: a G_TYPE_STRING value
 *
 * Sets the model of the transaction
 *
 * @value's responsibility is transferred to
 * @stmt (which means @stmt is then responsible for freeing it when no longer needed).
 */
void
gda_sql_statement_trans_take_mode (GdaSqlStatement *stmt, GValue *value)
{
	GdaSqlStatementTransaction *trans = (GdaSqlStatementTransaction *) stmt->contents;
	if (trans->trans_mode) {
		g_free (trans->trans_mode);
		trans->trans_mode = NULL;
	}
	if (value) {
		trans->trans_mode = g_value_dup_string (value);
		g_value_unset (value);
		g_free (value);
	}
}

/**
 * gda_sql_statement_set_isol_level
 * @stmt: a #GdaSqlStatement pointer
 * @level: the transacion level
 *
 * Sets the transaction level of the transaction
 */
void
gda_sql_statement_trans_set_isol_level (GdaSqlStatement *stmt, GdaTransactionIsolation level)
{
	GdaSqlStatementTransaction *trans = (GdaSqlStatementTransaction *) stmt->contents;
	trans->isolation_level = level;
}
