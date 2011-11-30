/*
 * Copyright (C) 2008 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2008 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <stdlib.h>
#include <string.h>
#include "test-cnc-utils.h"
#include "gda-ddl-creator.h"
#include <libgda/gda-server-provider-extra.h>
#include <libgda/sql-parser/gda-sql-parser.h>
#include <libgda/sql-parser/gda-sql-statement.h>
#include <unistd.h>

#define CHECK_EXTRA_INFO
/*#undef CHECK_EXTRA_INFO*/

#define CREATE_FILES 1

/*
 *
 * Connection SETUP
 *
 */
typedef struct {
	GdaQuarkList *ql;
	GString      *string;
	gchar        *requested_db_name;
} Data1;

static void 
db_create_quark_foreach_func (gchar *name, gchar *value, GdaServerOperation *op)
{
	gda_server_operation_set_value_at (op, value, NULL, "/SERVER_CNX_P/%s", name);
}

static void 
cnc_quark_foreach_func (gchar *name, gchar *value, Data1 *data)
{
	if (!strcmp (name, "DB_NAME")) {
		data->requested_db_name = g_strdup (value);
		return;
	}

	if (data->ql) {
		if (!gda_quark_list_find (data->ql, name)) {
			if (*(data->string->str) != 0)
				g_string_append_c (data->string, ';');
			g_string_append_printf (data->string, "%s=%s", name, value);
		}
	}
	else {
		if (*(data->string->str) != 0)
			g_string_append_c (data->string, ';');
		g_string_append_printf (data->string, "%s=%s", name, value);
	}
}

static gchar *
prov_name_upcase (const gchar *prov_name)
{
	gchar *str, *ptr;

	str = g_ascii_strup (prov_name, -1);
	for (ptr = str; *ptr; ptr++) {
		if (! g_ascii_isalnum (*ptr))
			*ptr = '_';
	}

	return str;
}

/*
 * Set up a connection.
 *
 * Optionnally the database can be created if the <upper_case_provider_name>_DBCREATE_PARAMS 
 * environment variable exists. Examples are:
 *     MYSQL_DBCREATE_PARAMS "HOST=localhost"
 *     POSTGRESQL_DBCREATE_PARAMS "HOST=localhost;PORT=5432"
 *     SQLITE_DBCREATE_PARAMS "DB_DIR=."
 *     BERKELEY_DB_CNC_PARAMS "DB_NAME=gda_check_bdb.db"
 *
 * The connection is opened if the <upper_case_provider_name>_CNC_PARAMS environment variable exists.
 * For example:
 *     MSACCESS_CNC_PARAMS "DB_DIR=/home/me/libgda/tests/providers;DB_NAME=gda_check_db"
 *     ORACLE_CNC_PARAMS TNSNAME=//127.0.0.1
 *
 *
 * If the <upper_case_provider_name>_DBCREATE_PARAMS is supplied, then its contents can be used
 * to complement the <upper_case_provider_name>_CNC_PARAMS.
 *
 * Returns: a GdaConnection if no error occurred
 */
GdaConnection *
test_cnc_setup_connection (const gchar *provider, const gchar *dbname, GError **error)
{
	GdaConnection *cnc = NULL;
	gchar *str, *upname;
	const gchar *db_params, *cnc_params;
	GdaProviderInfo *prov_info;
	GdaQuarkList *db_quark_list = NULL, *cnc_quark_list = NULL;
	gboolean db_created = FALSE;

	g_return_val_if_fail (dbname && *dbname, NULL);

	prov_info = gda_config_get_provider_info (provider);
	if (!prov_info) {
		g_set_error (error, 0, 0,
			     "Provider '%s' not found", provider);
		return NULL;
	}

	/* create database if requested */
	upname = prov_name_upcase (prov_info->id);
	str = g_strdup_printf ("%s_DBCREATE_PARAMS", upname);
	db_params = getenv (str);
	g_free (str);
	if (db_params) {
		GdaServerOperation *op;

		db_quark_list = gda_quark_list_new_from_string (db_params);
		op = gda_server_operation_prepare_drop_database (prov_info->id, dbname, NULL);
		gda_quark_list_foreach (db_quark_list, (GHFunc) db_create_quark_foreach_func, op);
		gda_server_operation_perform_drop_database (op, NULL, NULL);
		g_object_unref (op);

		op = gda_server_operation_prepare_create_database (prov_info->id, dbname, NULL);
		gda_quark_list_foreach (db_quark_list, (GHFunc) db_create_quark_foreach_func, op);
		if (!gda_server_operation_perform_create_database (op, NULL, error)) 
			goto out;
		db_created = TRUE;
	}

	/* open connection to database */
	str = g_strdup_printf ("%s_CNC_PARAMS", upname);
	cnc_params = getenv (str);
	g_free (str);
	if (cnc_params) 
		cnc_quark_list = gda_quark_list_new_from_string (cnc_params);
	
	if (db_quark_list || cnc_quark_list) {
		Data1 data;

		data.string = g_string_new ("");
		data.ql = NULL;
		data.requested_db_name = NULL;

		if (db_quark_list) 
			gda_quark_list_foreach (db_quark_list, (GHFunc) cnc_quark_foreach_func, &data);
		data.ql = db_quark_list;
		if (cnc_quark_list)
			gda_quark_list_foreach (cnc_quark_list, (GHFunc) cnc_quark_foreach_func, &data);

		if (*(data.string->str) != 0)
			g_string_append_c (data.string, ';');
		g_string_append_printf (data.string, "DB_NAME=%s", 
					data.requested_db_name ? data.requested_db_name : dbname);
		g_print ("Open connection string: %s\n", data.string->str);

		gchar *auth_string = NULL;
		GSList *current = prov_info->auth_params->holders;
		while (current) {
			GdaHolder *holder = (GdaHolder *) current->data;

			const gchar *id = gda_holder_get_id (holder);
			const gchar *env = NULL;
			if (g_strrstr (id, "USER") != NULL) {
				str = g_strdup_printf ("%s_USER", upname);
				env = getenv (str);
				g_free (str);
			} else if (g_strrstr (id, "PASS") != NULL) {
				str = g_strdup_printf ("%s_PASS", upname);
				env = getenv (str);
				g_free (str);
			}

			if (env) {
				str = g_strdup_printf ("%s=%s;", id, env);

				gchar *tmp = auth_string;
				auth_string = g_strconcat (auth_string, str, NULL);
				g_free (str);
				g_free (tmp);
			}

			current = g_slist_next (current);
		}

		cnc = gda_connection_open_from_string (prov_info->id, data.string->str, auth_string,
 						       GDA_CONNECTION_OPTIONS_NONE, error);
		g_free (auth_string);
		g_string_free (data.string, TRUE);
	}

	if (db_quark_list)
		gda_quark_list_free (db_quark_list);
	if (cnc_quark_list)
		gda_quark_list_free (cnc_quark_list);

 out:
	if (!db_params && !cnc_params) 
		g_set_error (error, 0, 0,
			     "Connection parameters not specified, test not executed (define %s_CNC_PARAMS or %s_DBCREATE_PARAMS to create a test DB)\n", upname, upname);
	g_free (upname);

	if (cnc) {
		g_object_set_data_full (G_OBJECT (cnc), "dbname", g_strdup (dbname), g_free);
		g_object_set_data (G_OBJECT (cnc), "db_created", GINT_TO_POINTER (db_created));
		g_print ("Connection now set up (%s)\n", db_created ? "database created" : "reusing database");
	}

	return cnc;
}

/*
 * Creates the structure of the database pointed by @cnc, as specified in @schema_file
 *
 */
gboolean
test_cnc_setup_db_structure (GdaConnection *cnc, const gchar *schema_file, GError **error)
{
	GdaDDLCreator *ddl;
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	
	ddl = gda_ddl_creator_new ();
        if (!gda_ddl_creator_set_dest_from_file (ddl, schema_file, error)) {
		g_object_unref (ddl);
		return FALSE;
        }

	gda_ddl_creator_set_connection (ddl, cnc);
	if (!gda_ddl_creator_execute (ddl, error)) {
		g_object_unref (ddl);
		return FALSE;
        }

        g_object_unref (ddl);
	return TRUE;
}

/*
 *
 * Connection CLEAN
 *
 */
static void 
db_drop_quark_foreach_func (gchar *name, gchar *value, GdaServerOperation *op)
{
	gda_server_operation_set_value_at (op, value, NULL, "/SERVER_CNX_P/%s", name);
	gda_server_operation_set_value_at (op, value, NULL, "/DB_DESC_P/%s", name);
}

/*
 * Cleans up a connection.
 *
 * If @destroy_db is TRUE, then the database is destroyed, except if <upper_case_provider_name>_DONT_REMOVE_DB
 * is set.
 *
 * WARNING: the @cnc connection destroyed closed by this function
 */
gboolean
test_cnc_clean_connection (GdaConnection *cnc, GError **error)
{
	gchar *prov_id;
	gboolean retval = TRUE;
	gchar *str, *upname;
	gboolean destroy_db;

	prov_id = g_strdup (gda_connection_get_provider_name (cnc));

	upname = prov_name_upcase (prov_id);
	str = g_strdup_printf ("%s_DONT_REMOVE_DB", upname);
	if (getenv (str))
		destroy_db = FALSE;
	else
		destroy_db = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (cnc), "db_created"));
	g_free (str);

	if (destroy_db) {
		GdaServerOperation *op;
		gchar *dbname;
		
		const gchar *db_params;
		GdaQuarkList *db_quark_list = NULL;

		dbname = (gchar *) g_object_get_data (G_OBJECT (cnc), "dbname");
		g_assert (dbname);
		dbname = g_strdup (dbname);
		
		gda_connection_close (cnc);
		g_object_unref (cnc);

#ifdef CHECK_EXTRA_INFO
		g_print ("Waiting a bit for the server to register the disconnection...\n");
#endif
		sleep (1);
		str = g_strdup_printf ("%s_DBCREATE_PARAMS", upname);
		db_params = getenv (str);
		g_free (str);
		g_assert (db_params);

		op = gda_server_operation_prepare_drop_database (prov_id, dbname, NULL);
		g_free (dbname);
		db_quark_list = gda_quark_list_new_from_string (db_params);
		gda_quark_list_foreach (db_quark_list, (GHFunc) db_drop_quark_foreach_func, op);
		gda_quark_list_free (db_quark_list);

		if (!gda_server_operation_perform_drop_database (op, NULL, error))
			retval = FALSE;
		g_object_unref (op);
	}
	else {
		TO_IMPLEMENT;
		gda_connection_close (cnc);
		g_object_unref (cnc);
	}
	g_free (upname);
	g_free (prov_id);

	return retval;
}

gboolean
test_cnc_setup_db_contents (G_GNUC_UNUSED GdaConnection *cnc, G_GNUC_UNUSED const gchar *data_file,
			    G_GNUC_UNUSED GError **error)
{
	TO_IMPLEMENT;
	return FALSE;
}

/*
 * Load data from file @file into table @table
 */
gboolean
test_cnc_load_data_from_file (GdaConnection *cnc, const gchar *table, const gchar *full_file, GError **error)
{
	GdaStatement *stmt = NULL;
	GdaSet *params = NULL;
	GdaDataModel *import;
	gint nrows, ncols, i;
	GdaMetaStruct *mstruct = NULL;
	GSList *list;
	gboolean retval = TRUE;

	/* loading XML file */
	import = gda_data_model_import_new_file (full_file, TRUE, NULL);
	if (gda_data_model_import_get_errors (GDA_DATA_MODEL_IMPORT (import))) {
		g_set_error (error, 0, 0, "Error loading '%s' file", full_file);
		return FALSE;
	}

	/* retrieving meta data info */
	GdaMetaDbObject *table_dbo;
	GValue *name_value;
	g_value_set_string ((name_value = gda_value_new (G_TYPE_STRING)), table);
	mstruct = gda_meta_struct_new (gda_connection_get_meta_store (cnc), GDA_META_STRUCT_FEATURE_NONE);
	table_dbo = gda_meta_struct_complement (mstruct, GDA_META_DB_TABLE,
						NULL, NULL, name_value, error);
	gda_value_free (name_value);
	if (! table_dbo) {
		retval = FALSE;
		goto out;
	}

	/* creating INSERT statement */
	GdaSqlStatement *st;
	GdaSqlStatementInsert *ist;
	GSList *insert_values_list = NULL;
	
	ist = g_new0 (GdaSqlStatementInsert, 1);
        GDA_SQL_ANY_PART (ist)->type = GDA_SQL_ANY_STMT_INSERT;
	ist->table = gda_sql_table_new (GDA_SQL_ANY_PART (ist));
        ist->table->table_name = g_strdup (table);

	GdaMetaTable *mtable = GDA_META_TABLE (table_dbo);
	for (list = mtable->columns; list; list = list->next) {
		GdaMetaTableColumn *tcol = GDA_META_TABLE_COLUMN (list->data);
		GdaSqlField *field;

		/* field */
		field = gda_sql_field_new (GDA_SQL_ANY_PART (ist));
		field->field_name = g_strdup (tcol->column_name);
		ist->fields_list = g_slist_append (ist->fields_list, field);

		/* value */
		GdaSqlParamSpec *pspec = g_new0 (GdaSqlParamSpec, 1);
		GdaSqlExpr *expr;
		pspec->name = g_strdup (tcol->column_name);
		pspec->g_type = tcol->gtype;
		pspec->nullok = tcol->nullok;
		expr = gda_sql_expr_new (GDA_SQL_ANY_PART (ist));
		expr->param_spec = pspec;
		insert_values_list = g_slist_append (insert_values_list, expr);
	}

        ist->values_list = g_slist_append (NULL, insert_values_list);
        st = gda_sql_statement_new (GDA_SQL_STATEMENT_INSERT);
        st->contents = ist;
	stmt = g_object_new (GDA_TYPE_STATEMENT, "structure", st, NULL);
        gda_sql_statement_free (st);
	g_object_unref (mstruct);

	if (! gda_statement_get_parameters (stmt, &params, error)) {
		retval = FALSE;
		goto out;
	}

	/* executing inserts */
	nrows = gda_data_model_get_n_rows (import);
	ncols = gda_data_model_get_n_columns (import);
	if (!gda_connection_begin_transaction (cnc, NULL, GDA_TRANSACTION_ISOLATION_UNKNOWN, error)) {
		retval = FALSE;
		goto out;
	}
	for (i = 0; i < nrows; i++) {
		gint j;
		GSList *list;
		for (list = params->holders, j = 0; list && (j < ncols); list = list->next, j++) {
			const GValue *cvalue = gda_data_model_get_value_at (import, j, i, error);
			if (!cvalue) {
				gda_connection_rollback_transaction (cnc, NULL, NULL);
				retval = FALSE;
				goto out;
			}
			if (! gda_holder_set_value (GDA_HOLDER (list->data), cvalue, error)) {
				gda_connection_rollback_transaction (cnc, NULL, NULL);
				retval = FALSE;
				goto out;
			}
		}

		if (list || (j < ncols)) {
			g_set_error (error, 0, 0, "%s", 
				     "Incoherent number of columns in table and imported data");
			gda_connection_rollback_transaction (cnc, NULL, NULL);
			retval = FALSE;
			goto out;
		}

		if (gda_connection_statement_execute_non_select (cnc, stmt, params, NULL, error) == -1) {
			gda_connection_rollback_transaction (cnc, NULL, NULL);
			retval = FALSE;
			goto out;
		}
	}

	if (! gda_connection_commit_transaction (cnc, NULL, error))
		retval = FALSE;

 out:
	if (import)
		g_object_unref (import);
	if (stmt)
		g_object_unref (stmt);
	if (params)
		g_object_unref (params);

	return retval;
}
