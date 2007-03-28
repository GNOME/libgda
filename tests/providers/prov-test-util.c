#include <stdlib.h>
#include <string.h>
#include "prov-test-util.h"

#define CHECK_EXTRA_INFO
/*#undef CHECK_EXTRA_INFO*/

#define DB_NAME "gda_check_db"

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

	upname = g_ascii_strup (prov_info->id, -1);
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

	client = gda_connection_get_client (cnc);
	prov_id = g_strdup (gda_connection_get_provider (cnc));
	gda_connection_close (cnc);
	g_object_unref (cnc);

	if (destroy_db) {
		GdaServerOperation *op;
		GError *error = NULL;

		gchar *str, *upname;
		const gchar *db_params;
		GdaQuarkList *db_quark_list = NULL;

		upname = g_ascii_strup (prov_id, -1);
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
