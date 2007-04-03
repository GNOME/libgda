#include <stdlib.h>
#include <string.h>
#include "prov-test-util.h"
#include <libgda/gda-server-provider-extra.h>

#define CHECK_EXTRA_INFO
/*#undef CHECK_EXTRA_INFO*/

#define DB_NAME "gda_check_db"
#define CREATE_FILES 0
GdaDict *dict = NULL;

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
 * Close and re-open @cnc
 */
static gboolean
prov_test_reopen_connection (GdaConnection *cnc, GError **error)
{
	gda_connection_close (cnc);
	if (!gda_connection_open (cnc, error)) {
#ifdef CHECK_EXTRA_INFO
		g_warning ("Could not re-open connection: %s",
			   error && *error && (*error)->message ? (*error)->message : "No detail");
#endif
		return FALSE;
	}
	return TRUE;
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
	GdaClient *client;
	gchar *str, *upname;
	const gchar *db_params, *cnc_params;
	GError *error = NULL;
	gchar *db_name = NULL;

	GdaQuarkList *db_quark_list = NULL, *cnc_quark_list = NULL;

	g_assert (prov_info);

	client = gda_client_new ();

	upname = prov_name_upcase (prov_info->id);
	str = g_strdup_printf ("%s_DBCREATE_PARAMS", upname);
	db_params = getenv (str);
	g_free (str);
	if (db_params) {
		GdaServerOperation *op;

		db_name = DB_NAME;
		db_quark_list = gda_quark_list_new_from_string (db_params);
		op = gda_client_prepare_drop_database (client, db_name, prov_info->id);
		gda_quark_list_foreach (db_quark_list, (GHFunc) db_create_quark_foreach_func, op);
		gda_client_perform_create_database (client, op, NULL);
		g_object_unref (op);

		op = gda_client_prepare_create_database (client, db_name, prov_info->id);
		gda_quark_list_foreach (db_quark_list, (GHFunc) db_create_quark_foreach_func, op);
		if (!gda_client_perform_create_database (client, op, &error)) {
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
		cnc_quark_list = gda_quark_list_new_from_string (db_params);
	
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

		const gchar *username, *password;
		str = g_strdup_printf ("%s_USER", upname);
		username = getenv (str);
		g_free (str);
		str = g_strdup_printf ("%s_PASS", upname);
		password = getenv (str);
		g_free (str);

		cnc = gda_client_open_connection_from_string (client, prov_info->id, data.string->str, username, password, 
							      GDA_CONNECTION_OPTIONS_NONE, &error);
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
	GdaClient *client;
	gchar *prov_id;
	gboolean retval = TRUE;
	gchar *str, *upname;

	client = gda_connection_get_client (cnc);
	prov_id = g_strdup (gda_connection_get_provider (cnc));
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

		
		str = g_strdup_printf ("%s_DBCREATE_PARAMS", upname);
		db_params = getenv (str);
		g_free (str);
		g_assert (db_params);

		op = gda_client_prepare_drop_database (client, DB_NAME, prov_id);
		db_quark_list = gda_quark_list_new_from_string (db_params);
		gda_quark_list_foreach (db_quark_list, (GHFunc) db_drop_quark_foreach_func, op);
		gda_quark_list_free (db_quark_list);

		if (!gda_client_perform_drop_database (client, op, &error)) {
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
	gchar *contents;
        gsize size;
	GError *error = NULL;

	prov_id = gda_connection_get_provider (cnc);

	tmp = g_strdup_printf ("%s_create_tables.sql", prov_id);
	filename = g_build_filename (CHECK_SQL_FILES, "tests", "providers", tmp, NULL);
	g_free (tmp);

	/* read the contents of the file */
        if (! g_file_get_contents (filename, &contents, &size, &error)) {
#ifdef CHECK_EXTRA_INFO
                g_warning ("Could not read file '%s': %s", filename,
			   error && error->message ? error->message : "No detail");
#endif
		g_error_free (error);
		error = NULL;
		retval = FALSE;
        }
	else {
		GdaCommand *command;
		command = gda_command_new (contents, GDA_COMMAND_TYPE_SQL,
					   GDA_COMMAND_OPTION_STOP_ON_ERRORS);
		g_free (contents);
		if (gda_connection_execute_non_select_command (cnc, command, NULL, &error) < 0) {
#ifdef CHECK_EXTRA_INFO
			g_warning ("Could execute SQL in file '%s': %s", filename,
				   error && error->message ? error->message : "No detail");
#endif
			g_error_free (error);
			error = NULL;
			retval = FALSE;
		}
		gda_command_free (command);
	}
	g_free (filename);

	return retval;
}

/*
 * 
 * Check a table's schema
 *
 */
gboolean
prov_test_check_table_schema (GdaConnection *cnc, const gchar *table)
{
	if (!cnc || !gda_connection_is_opened (cnc)) {
#ifdef CHECK_EXTRA_INFO
		g_warning ("Connection is closed!");
#endif
		return FALSE;
	}

	if (!dict)
		dict = gda_dict_new ();

	GdaServerProvider *prov;
	GdaDataModel *schema_m;
	GError *error = NULL;
	GdaParameterList *plist;
	gchar *str;
	
	if (!prov_test_reopen_connection (cnc, &error))
		return FALSE;

	prov = gda_connection_get_provider_obj (cnc);
	plist = gda_parameter_list_new_inline (dict, "name", G_TYPE_STRING, table, NULL);
	schema_m = gda_server_provider_get_schema (prov, cnc, GDA_CONNECTION_SCHEMA_FIELDS, plist, &error);
	g_object_unref (plist);
	if (!schema_m) {
#ifdef CHECK_EXTRA_INFO
		g_warning ("Could not get FIELDS schema for table '%s': %s", table,
			   error && error->message ? error->message : "No detail");
#endif
		return FALSE;
	}

	str = g_strdup_printf ("FIELDS_SCHEMA_%s_%s.xml", gda_connection_get_provider (cnc), table);
	if (CREATE_FILES) {
		/* export schema model to a file, to create first version of the files, not to be used in actual checks */
		plist = gda_parameter_list_new_inline (dict, "OVERWRITE", G_TYPE_BOOLEAN, TRUE, NULL);
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
		/* test schema validity */
		if (!gda_server_provider_test_schema_model (schema_m, GDA_CONNECTION_SCHEMA_FIELDS, &error)) {
#ifdef CHECK_EXTRA_INFO
			g_warning ("Reported schema does not conform to GDA's requirements: %s", 
				   error && error->message ? error->message : "No detail");
#endif
			return FALSE;
		}

		/* compare schema with what's expected */
		gchar *file = g_build_filename (CHECK_SQL_FILES, "tests", "providers", str, NULL);
		if (!compare_data_model_with_expected (schema_m, file))
			return FALSE;
		g_free (file);
	}
	g_free (str);

	/*gda_data_model_dump (schema_m, stdout);*/
	g_object_unref (schema_m);
	return TRUE;
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
	
	if (!prov_test_reopen_connection (cnc, &error))
		return FALSE;

	prov = gda_connection_get_provider_obj (cnc);
	schema_m = gda_server_provider_get_schema (prov, cnc, GDA_CONNECTION_SCHEMA_TYPES, NULL, &error);
	if (!schema_m) {
#ifdef CHECK_EXTRA_INFO
		g_warning ("Could not get the TYPES schema: %s",
			   error && error->message ? error->message : "No detail");
#endif
		return FALSE;
	}

	str = g_strdup_printf ("TYPES_SCHEMA_%s.xml", gda_connection_get_provider (cnc));
	if (CREATE_FILES) {
		/* export schema model to a file, to create first version of the files, not to be used in actual checks */
		GdaParameterList *plist;
		plist = gda_parameter_list_new_inline (dict, "OVERWRITE", G_TYPE_BOOLEAN, TRUE, NULL);
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
		/* test schema validity */
		if (!gda_server_provider_test_schema_model (schema_m, GDA_CONNECTION_SCHEMA_TYPES, &error)) {
#ifdef CHECK_EXTRA_INFO
			g_warning ("Reported schema does not conform to GDA's requirements: %s", 
				   error && error->message ? error->message : "No detail");
#endif
			return FALSE;
		}

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
