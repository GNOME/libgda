#include <stdlib.h>
#include <string.h>
#include "prov-test-util.h"
#include <libgda/gda-server-provider-extra.h>
#include <libgda/sql-parser/gda-sql-parser.h>
#include <libgda/sql-parser/gda-sql-statement.h>
#include <unistd.h>

#define CHECK_EXTRA_INFO
/*#undef CHECK_EXTRA_INFO*/

#define DB_NAME "gda_check_db"
#define CREATE_FILES 1
GdaSqlParser *parser = NULL;

/*
 * Data model content's check
 */
static gboolean
compare_data_model_with_expected (GdaDataModel *model, const gchar *expected_file)
{
	GdaDataModel *compare_m;
	GSList *errors;
	gboolean retval = TRUE;

	/* load file into data model */
	compare_m = gda_data_model_import_new_file (expected_file, TRUE, NULL);
	if ((errors = gda_data_model_import_get_errors (GDA_DATA_MODEL_IMPORT (compare_m)))) {
#ifdef CHECK_EXTRA_INFO
		g_warning ("Could not load the expected schema file '%s': ", expected_file);
		for (; errors; errors = errors->next) 
			g_print ("\t%s\n", 
				 ((GError *)(errors->data))->message ? ((GError *)(errors->data))->message : "No detail");
#endif
		g_object_unref (compare_m);
		return FALSE;
	}
	
	/* compare number of rows and columns */
	gint ncols, nrows, row, col;
	ncols = gda_data_model_get_n_columns (model);
	nrows = gda_data_model_get_n_rows (model);

	if (ncols != gda_data_model_get_n_columns (compare_m)) {
#ifdef CHECK_EXTRA_INFO
		g_warning ("Data model has wrong number of columns: expected %d and got %d",
			   gda_data_model_get_n_columns (compare_m), ncols);
#endif
		g_object_unref (compare_m);
		return FALSE;
	}

	/* compare columns' types */
	for (col = 0; col < ncols; col++) {
		GdaColumn *m_col, *e_col;

		m_col = gda_data_model_describe_column (model, col);
		e_col = gda_data_model_describe_column (compare_m, col);
		if (gda_column_get_g_type (m_col) != gda_column_get_g_type (e_col)) {
#ifdef CHECK_EXTRA_INFO
			g_warning ("Data model has wrong column type for column %d: expected %s and got %s",
				   col, g_type_name (gda_column_get_g_type (e_col)),
				   g_type_name (gda_column_get_g_type (m_col)));
			g_print ("========= Compared Data model =========\n");
			gda_data_model_dump (model, stdout);
			
			g_print ("========= Expected Data model =========\n");
			gda_data_model_dump (compare_m, stdout);
#endif
			g_object_unref (compare_m);
			return FALSE;
		}
	}
	
	/* compare contents */
	for (row = 0; row < nrows; row++) {
		for (col = 0; col < ncols; col++) {
			if (col == 3) /* FIXME: column to ignore */
				continue;
			const GValue *m_val, *e_val;
			GdaColumn *column;

			column = gda_data_model_describe_column (model, col);
			if (!strcmp (gda_column_get_name (column), "Owner"))
				continue;
			m_val = gda_data_model_get_value_at (model, col, row);
			e_val =  gda_data_model_get_value_at (compare_m, col, row);
			if (gda_value_compare_ext (m_val, e_val)) {
#ifdef CHECK_EXTRA_INFO
				g_warning ("Reported schema error line %d, col %d: expected '%s' and got '%s'",
					   row, col, 
					   gda_value_stringify (e_val), gda_value_stringify (m_val));
#endif
				retval = FALSE;
			}
		}
	}
	
	/* FIXME: test that there are no left row in compare_m */
	g_object_unref (compare_m);

	return retval;
}

/*
 *
 * Connection SETUP
 *
 */
typedef struct {
	gchar        *db_name;
	GdaQuarkList *ql;
	GString      *string;
} Data1;

static void 
db_create_quark_foreach_func (gchar *name, gchar *value, GdaServerOperation *op)
{
	gda_server_operation_set_value_at (op, value, NULL, "/SERVER_CNX_P/%s", name);
}

static void 
cnc_quark_foreach_func (gchar *name, gchar *value, Data1 *data)
{
	if (data->db_name && !strcmp (name, "DB_NAME"))
		return;

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

GdaConnection *
prov_test_setup_connection (GdaProviderInfo *prov_info, gboolean *params_provided, gboolean *db_created)
{
	GdaConnection *cnc = NULL;
	gchar *str, *upname;
	const gchar *db_params, *cnc_params;
	GError *error = NULL;
	gchar *db_name = NULL;

	GdaQuarkList *db_quark_list = NULL, *cnc_quark_list = NULL;

	g_assert (prov_info);

	upname = prov_name_upcase (prov_info->id);
	str = g_strdup_printf ("%s_DBCREATE_PARAMS", upname);
	db_params = getenv (str);
	g_free (str);
	if (db_params) {
		GdaServerOperation *op;

		db_name = DB_NAME;
		db_quark_list = gda_quark_list_new_from_string (db_params);
		op = gda_prepare_drop_database (prov_info->id, db_name, NULL);
		gda_quark_list_foreach (db_quark_list, (GHFunc) db_create_quark_foreach_func, op);
		gda_perform_create_database (op, NULL);
		g_object_unref (op);

		op = gda_prepare_create_database (prov_info->id, db_name, NULL);
		gda_quark_list_foreach (db_quark_list, (GHFunc) db_create_quark_foreach_func, op);
		if (!gda_perform_create_database (op, &error)) {
#ifdef CHECK_EXTRA_INFO
			g_warning ("Could not create the '%s' database (provider %s): %s", db_name,
				   prov_info->id, error && error->message ? error->message : "No detail");
#endif
			g_error_free (error);
			error = NULL;
			goto out;
		}
	}

	str = g_strdup_printf ("%s_CNC_PARAMS", upname);
	cnc_params = getenv (str);
	g_free (str);
	if (cnc_params) 
		cnc_quark_list = gda_quark_list_new_from_string (cnc_params);
	
	if (db_quark_list || cnc_quark_list) {
		Data1 data;

		data.string = g_string_new ("");
		data.db_name = db_name;
		data.ql = NULL;

		if (db_quark_list) 
			gda_quark_list_foreach (db_quark_list, (GHFunc) cnc_quark_foreach_func, &data);
		data.ql = db_quark_list;
		if (cnc_quark_list)
			gda_quark_list_foreach (cnc_quark_list, (GHFunc) cnc_quark_foreach_func, &data);
		if (db_name) {
			if (*(data.string->str) != 0)
				g_string_append_c (data.string, ';');
			g_string_append_printf (data.string, "DB_NAME=%s", db_name);
		}
		g_print ("Open connection string: %s\n", data.string->str);

		/***/
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
 						       GDA_CONNECTION_OPTIONS_NONE, &error);
		g_free (auth_string);
		if (!cnc && error) {
#ifdef CHECK_EXTRA_INFO
			g_warning ("Could not open connection to %s (provider %s): %s",
				   cnc_params, prov_info->id, error->message ? error->message : "No detail");
#endif
			g_error_free (error);
			error = NULL;
		}
		g_string_free (data.string, TRUE);
	}

	if (db_quark_list)
		gda_quark_list_free (db_quark_list);
	if (cnc_quark_list)
		gda_quark_list_free (cnc_quark_list);

 out:
	*db_created = db_name ? TRUE : FALSE;

	if (db_params || cnc_params)
		*params_provided = TRUE;
	else {
		g_print ("Connection parameters not specified, test not executed (define %s_CNC_PARAMS or %s_DBCREATE_PARAMS to create a test DB)\n", upname, upname);
		*params_provided = FALSE;
	}
	g_free (upname);

	return cnc;
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

gboolean
prov_test_clean_connection (GdaConnection *cnc, gboolean destroy_db)
{
	gchar *prov_id;
	gboolean retval = TRUE;
	gchar *str, *upname;

	prov_id = g_strdup (gda_connection_get_provider_name (cnc));
	gda_connection_close (cnc);
	g_object_unref (cnc);

	upname = prov_name_upcase (prov_id);
	str = g_strdup_printf ("%s_DONT_REMOVE_DB", upname);
	if (getenv (str))
		destroy_db = FALSE;
	g_free (str);

	if (destroy_db) {
		GdaServerOperation *op;
		GError *error = NULL;
		
		const gchar *db_params;
		GdaQuarkList *db_quark_list = NULL;

#ifdef CHECK_EXTRA_INFO
		g_print ("Waiting a bit for the server to register the disconnection...\n");
#endif
		sleep (1);
		str = g_strdup_printf ("%s_DBCREATE_PARAMS", upname);
		db_params = getenv (str);
		g_free (str);
		g_assert (db_params);

		op = gda_prepare_drop_database (prov_id, DB_NAME, NULL);
		db_quark_list = gda_quark_list_new_from_string (db_params);
		gda_quark_list_foreach (db_quark_list, (GHFunc) db_drop_quark_foreach_func, op);
		gda_quark_list_free (db_quark_list);

		if (!gda_perform_drop_database (op, &error)) {
#ifdef CHECK_EXTRA_INFO
			g_warning ("Could not drop the '%s' database (provider %s): %s", DB_NAME,
				   prov_id, error && error->message ? error->message : "No detail");
#endif
			g_error_free (error);
			error = NULL;
			retval = FALSE;
		}
	}
	g_free (upname);

	g_free (prov_id);

	return retval;
}

/*
 *
 * Create tables SQL
 *
 */
gboolean
prov_test_create_tables_sql (GdaConnection *cnc)
{
	const gchar *prov_id;
	gchar *tmp, *filename;
	gboolean retval = TRUE;
	GError *error = NULL;
	const GSList *list;

	prov_id = gda_connection_get_provider_name (cnc);

	tmp = g_strdup_printf ("%s_create_tables.sql", prov_id);
	filename = g_build_filename (CHECK_SQL_FILES, "tests", "providers", tmp, NULL);
	g_free (tmp);

	if (!parser) 
		parser = gda_sql_parser_new ();

	GdaBatch *batch;
	batch = gda_sql_parser_parse_file_as_batch (parser, filename, &error);
	if (!batch) {
#ifdef CHECK_EXTRA_INFO
                g_warning ("Could not parser file '%s': %s", filename,
			   error && error->message ? error->message : "No detail");
#endif
		g_error_free (error);
		error = NULL;
		g_free (filename);
		return FALSE;
	}

	for (list = gda_batch_get_statements (batch); list; list = list->next) {
		if (gda_connection_statement_execute_non_select (cnc, GDA_STATEMENT (list->data),
								 NULL, NULL, &error) == -1) {
#ifdef CHECK_EXTRA_INFO
			g_warning ("Could execute statement: %s",
				   error && error->message ? error->message : "No detail");
			g_error_free (error);
#endif
			retval = FALSE;
			break;
		}
	}

	g_object_unref (batch);
	return retval;
}

/*
 * 
 * Check data types' schema
 *
 */
gboolean
prov_test_check_types_schema (GdaConnection *cnc)
{
if (!cnc || !gda_connection_is_opened (cnc)) {
#ifdef CHECK_EXTRA_INFO
		g_warning ("Connection is closed!");
#endif
		return FALSE;
	}

	GdaServerProvider *prov;
	GdaDataModel *schema_m;
	GError *error = NULL;
	gchar *str;
	
	prov = gda_connection_get_provider_obj (cnc);
	schema_m = gda_connection_get_meta_store_data (cnc, GDA_CONNECTION_META_TYPES, &error, 0);
	if (!schema_m) {
#ifdef CHECK_EXTRA_INFO
		g_warning ("Could not get the TYPES schema: %s",
			   error && error->message ? error->message : "No detail");
#endif
		return FALSE;
	}

	str = g_strdup_printf ("TYPES_SCHEMA_%s.xml", gda_connection_get_provider_name (cnc));
	if (CREATE_FILES) {
		/* export schema model to a file, to create first version of the files, not to be used in actual checks */
		GdaSet *plist;
		plist = gda_set_new_inline (1, "OVERWRITE", G_TYPE_BOOLEAN, TRUE);
		if (! (gda_data_model_export_to_file (schema_m, GDA_DATA_MODEL_IO_DATA_ARRAY_XML, str, 
						      NULL, 0, NULL, 0, plist, &error))) {
#ifdef CHECK_EXTRA_INFO
			g_warning ("Could not export schema to file '%s': %s", str,
				   error && error->message ? error->message : "No detail");
#endif
			return FALSE;
		}
		g_object_unref (plist);
	}
	else {
		/* compare schema with what's expected */
		gchar *file = g_build_filename (CHECK_SQL_FILES, "tests", "providers", str, NULL);
		if (!compare_data_model_with_expected (schema_m, file))
			return FALSE;
		g_free (file);
	}
	g_free (str);

	g_object_unref (schema_m);
	return TRUE;
}

/*
 * 
 * Check the datamodel accessed through cursors, if available,
 * the data model is a "SELECT * FROM <table>"
 *
 */
static gboolean iter_is_correct (GdaDataModelIter *iter, GdaDataModel *ref_model);
gboolean
prov_test_check_table_cursor_model (GdaConnection *cnc, const gchar *table)
{
	if (!cnc || !gda_connection_is_opened (cnc)) {
#ifdef CHECK_EXTRA_INFO
		g_warning ("Connection is closed!");
#endif
		return FALSE;
	}

	/* create prepared statement */
	GdaStatement *stmt = NULL;
	gchar *str;
	if (!parser) 
		parser = gda_sql_parser_new ();
	str = g_strdup_printf ("SELECT * FROM %s", table);
	stmt = gda_sql_parser_parse_string (parser, str, NULL, NULL);
	g_free (str);
	if (!stmt) {
#ifdef CHECK_EXTRA_INFO
		g_warning ("Can't create GdaStatement!");
#endif
		return FALSE;
	}
	
	/* create models */
	GdaDataModel *ref_model, *cursor_model;
	ref_model = gda_connection_statement_execute_select (cnc, stmt, NULL, NULL);
	if (!ref_model) {
#ifdef CHECK_EXTRA_INFO
		g_warning ("Can't execute GdaStatement (random access requested)!");
#endif
		return FALSE;
	}
	if (! (gda_data_model_get_access_flags (ref_model) & GDA_DATA_MODEL_ACCESS_RANDOM)) {
#ifdef CHECK_EXTRA_INFO
		g_warning ("Random access data model does not report supporting this access mode!");
#endif
		return FALSE;
	}
	cursor_model = gda_connection_statement_execute_select_fullv (cnc, stmt, NULL, GDA_STATEMENT_MODEL_CURSOR_FORWARD, NULL, -1);
	if (!cursor_model) {
#ifdef CHECK_EXTRA_INFO
		g_warning ("Can't execute GdaStatement (forward cursor access requested)!");
#endif
		return FALSE;
	}
	if (! (gda_data_model_get_access_flags (cursor_model) & GDA_DATA_MODEL_ACCESS_CURSOR_FORWARD)) {
#ifdef CHECK_EXTRA_INFO
		g_warning ("Forward cursor access data model does not report supporting this access mode!");
#endif
		return FALSE;
	}

	/* check cursor_model using ref_model as reference */
	GdaDataModelIter *iter;
	iter = gda_data_model_create_iter (cursor_model);
	while (gda_data_model_iter_move_next(iter)) {
                if (!iter_is_correct (iter, ref_model)) 
			return FALSE;
        }

	if (gda_data_model_get_n_rows (ref_model) != gda_data_model_get_n_rows (cursor_model)) {
#ifdef CHECK_EXTRA_INFO
		g_warning ("Forward cursor access data model reports %d rows where %d are expected\n",
			   gda_data_model_get_n_rows (cursor_model), gda_data_model_get_n_rows (ref_model));
#endif
		return FALSE;
	}

#ifdef CHECK_EXTRA_INFO
	g_print ("Tested %d iter positions for rows into table '%s'\n", gda_data_model_get_n_rows (cursor_model), table);
#endif

	g_object_unref (ref_model);
	g_object_unref (cursor_model);
	return TRUE;
}

static gboolean
iter_is_correct (GdaDataModelIter *iter, GdaDataModel *ref_model)
{
        gint rownum;
        gint i, cols;
        GSList *list;

        g_object_get (G_OBJECT (iter), "current-row", &rownum, NULL);

        cols = gda_data_model_get_n_columns (ref_model);
        if (cols != g_slist_length (GDA_SET (iter)->holders)) {
#ifdef CHECK_EXTRA_INFO
                g_warning ("Number of columns in iter is not the same as for the referenced data model\n");
#endif
                return FALSE;
        }
        for (i = 0, list = GDA_SET (iter)->holders; i < cols; i++, list = list->next) {
                const GValue *v1, *v2;
                v1 = gda_holder_get_value (GDA_HOLDER (list->data));
                v2 = gda_data_model_get_value_at (ref_model, i, rownum);
                if (gda_value_compare_ext (v1, v2)) {
#ifdef CHECK_EXTRA_INFO
                        g_print ("Wrong value:\n\tgot: %s\n\texp: %s\n",
                                 gda_value_stringify (v1),
                                 gda_value_stringify (v2));
#endif
                        return FALSE;
                }
        }
        return TRUE;
}

/*
 *
 * Loads data into @table
 *
 */
gboolean
prov_test_load_data (GdaConnection *cnc, const gchar *table)
{
	if (!cnc || !gda_connection_is_opened (cnc)) {
#ifdef CHECK_EXTRA_INFO
		g_warning ("Connection is closed!");
#endif
		return FALSE;
	}

	if (!parser) 
		parser = gda_sql_parser_new ();

	/* load data into @imodel */
	gchar *tmp, *filename;
	GdaDataModel *imodel;
	GSList *errors;
	tmp = g_strdup_printf ("DATA_%s.xml", table);
	filename = g_build_filename (CHECK_SQL_FILES, "tests", "providers", tmp, NULL);
	g_free (tmp);

	imodel = gda_data_model_import_new_file (filename, TRUE, NULL);
	if ((errors = gda_data_model_import_get_errors (GDA_DATA_MODEL_IMPORT (imodel)))) {
#ifdef CHECK_EXTRA_INFO
		g_warning ("Could not load the expected data file '%s': ", filename);
		for (; errors; errors = errors->next) 
			g_print ("\t%s\n", 
				 ((GError *)(errors->data))->message ? ((GError *)(errors->data))->message : "No detail");
#endif
		g_object_unref (imodel);
		return FALSE;
	}

	/* create INSERT GdaStatement */
	GdaStatement *insert;
	GdaSqlStatement *sqlst;
	GError *error = NULL;
	GValue *value;
	gint i;
	GSList *fields = NULL, *values = NULL;
	sqlst = gda_sql_statement_new (GDA_SQL_STATEMENT_INSERT);

	g_value_set_string (value = gda_value_new (G_TYPE_STRING), table);
	gda_sql_statement_insert_take_table_name (sqlst, value);
	for (i = 0; i < gda_data_model_get_n_columns (imodel); i++) {
		GdaColumn *column;
		column = gda_data_model_describe_column (imodel, i);

		GdaSqlField *field;
		field = gda_sql_field_new (GDA_SQL_ANY_PART (sqlst->contents));
		g_value_set_string (value = gda_value_new (G_TYPE_STRING), gda_column_get_name (column));
		gda_sql_field_take_name (field, value);
		fields = g_slist_append (fields, field);
		
		GdaSqlExpr *expr;
		expr = gda_sql_expr_new (GDA_SQL_ANY_PART (sqlst->contents));
		tmp = g_strdup_printf ("+%d::%s", i, g_type_name (gda_column_get_g_type (column)));
		value = gda_value_new (G_TYPE_STRING);
		g_value_take_string (value, tmp);
		expr->param_spec = gda_sql_param_spec_new (value);
		expr->param_spec->nullok = gda_column_get_allow_null (column);
		values = g_slist_append (values, expr);
	}
	gda_sql_statement_insert_take_fields_list (sqlst, fields);
	gda_sql_statement_insert_take_1_values_list (sqlst, values);
	if (! gda_sql_statement_check_structure (sqlst, &error)) {
#ifdef CHECK_EXTRA_INFO
		g_warning ("Could not build INSERT statement: %s", 
			   error && error->message ? error->message : "No detail");
#endif
		g_object_unref (imodel);
		return FALSE;
	}
	
	insert = gda_statement_new ();
	g_object_set (G_OBJECT (insert), "structure", sqlst, NULL);
	gda_sql_statement_free (sqlst);

	/* execute the INSERT statement */
	GdaSet *set;
	if (! gda_statement_get_parameters (insert, &set, &error)) {
#ifdef CHECK_EXTRA_INFO
		g_warning ("Could not get the INSERT statement's parameters: %s", 
			   error && error->message ? error->message : "No detail");
#endif
		g_object_unref (imodel);
		return FALSE;
	}
	
	/* try to start a transaction to spped things up */
	gboolean started_transaction;
	started_transaction = gda_connection_begin_transaction (cnc, NULL,
								GDA_TRANSACTION_ISOLATION_UNKNOWN,
								NULL);
	gint row, nrows;
	nrows = gda_data_model_get_n_rows (imodel);
	for (row = 0; row < nrows; row++) {
		for (i = 0; i < gda_data_model_get_n_columns (imodel); i++) {
			tmp = g_strdup_printf ("+%d", i);
			const GValue *value;
			GdaHolder *h;
			value = gda_data_model_get_value_at (imodel, i, row);
			h = gda_set_get_holder (set, tmp);
			if (! gda_holder_set_value (h, value)) {
#ifdef CHECK_EXTRA_INFO
				g_warning ("Could not set INSERT statement's parameter '%s': %s",
					   tmp,
					   error && error->message ? error->message : "No detail");
#endif
				g_object_unref (imodel);
				return FALSE;
			}
			g_free (tmp);			
		}

		if (gda_connection_statement_execute_non_select (cnc, insert, set, NULL, &error) == -1) {
#ifdef CHECK_EXTRA_INFO
			g_warning ("Could not execute the INSERT statement for row %d: %s",
				   row,
				   error && error->message ? error->message : "No detail");
#endif
			g_object_unref (imodel);
			return FALSE;
		}
	}

	if (started_transaction) 
		gda_connection_commit_transaction (cnc, NULL, NULL);

#ifdef CHECK_EXTRA_INFO
	g_print ("Loaded %d rows into table '%s'\n", nrows, table);
#endif

	return TRUE;
}
