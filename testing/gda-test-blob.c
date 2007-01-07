#include <libgda/libgda.h>
#include <glib/gi18n-lib.h>
#include <string.h>

/* options */
gchar *pass = NULL;
gchar *user = NULL;
gchar *dsn = NULL;
gchar *direct = NULL;
gchar *prov = NULL;

static GOptionEntry entries[] = {
	{ "cnc", 'c', 0, G_OPTION_ARG_STRING, &direct, "Direct connection string", NULL},
	{ "provider", 'p', 0, G_OPTION_ARG_STRING, &prov, "Provider name", NULL},
	{ "dsn", 's', 0, G_OPTION_ARG_STRING, &dsn, "Data source", NULL},
	{ "user", 'U', 0, G_OPTION_ARG_STRING, &user, "Username", "username" },
	{ "password", 'P', 0, G_OPTION_ARG_STRING, &pass, "Password", "password" },
	{ NULL }
};

static gboolean clear_blobs (GdaDict *dict, GError **error);
static gboolean insert_blob (GdaDict *dict, gint id, const gchar *data, glong binary_length, GError **error);
static gboolean update_blob (GdaDict *dict, gint id, const gchar *data, glong binary_length, GError **error);
static gboolean update_multiple_blobs (GdaDict *dict, const gchar *data, glong binary_length, GError **error);
static gboolean display_blobs (GdaDict *dict, GError **error);
static gboolean do_query (GdaQuery *query, GdaParameterList *plist, GError **error);

int 
main (int argc, char **argv)
{
	GError *error = NULL;	
	GOptionContext *context;

	GdaClient *client;
	GdaConnection *cnc;
	GdaDict *dict;
	gchar *blob_data;

	/* command line parsing */
	context = g_option_context_new ("Tests opening a connection");
	g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
	if (!g_option_context_parse (context, &argc, &argv, &error)) {
		g_warning ("Can't parse arguments: %s", error->message);
		exit (1);
	}
	g_option_context_free (context);
	
	if (direct && dsn) {
		g_print ("DSN and connection string are exclusive\n");
		exit (1);
	}

	if (!direct && !dsn) {
		g_print ("You must specify a connection to open either as a DSN or a connection string\n");
		exit (1);
	}

	if (direct && !prov) {
		g_print ("You must specify a provider when using a connection string\n");
		exit (1);
	}

	gda_init ("Gda connection tester", PACKAGE_VERSION, argc, argv);

	/* open connection */
	client = gda_client_new ();
	if (dsn) {
		GdaDataSourceInfo *info = NULL;
		info = gda_config_find_data_source (dsn);
		if (!info)
			g_error (_("DSN '%s' is not declared"), dsn);
		else {
			cnc = gda_client_open_connection (client, info->name, 
							  user ? user : info->username, 
							  pass ? pass : ((info->password) ? info->password : ""),
							  0, &error);
			if (!cnc) {
				g_warning (_("Can't open connection to DSN %s: %s\n"), info->name,
				   error && error->message ? error->message : "???");
				exit (1);
			}
			prov = info->provider;
			gda_data_source_info_free (info);
		}
	}
	else {
		
		cnc = gda_client_open_connection_from_string (client, prov, direct, 
							      user, pass, 0, &error);
		if (!cnc) {
			g_warning (_("Can't open specified connection: %s\n"),
				   error && error->message ? error->message : "???");
			exit (1);
		}
	}

	g_print (_("Connection successfully opened!\n"));
	g_object_unref (G_OBJECT (client));

	/* dictionary */
	dict = gda_dict_new ();
	gda_dict_set_connection (dict, cnc);

	if (error) {
		g_error_free (error);
		error = NULL;
	}

	gda_connection_begin_transaction (cnc, NULL, GDA_TRANSACTION_ISOLATION_UNKNOWN, NULL);

	/* 
	 * clear all blobs 
	 */
	if (!clear_blobs (dict, &error))
		g_error ("Blobs clear error: %s", error && error->message ? error->message : "No detail");

	/* insert a blob */
	blob_data = "Blob Data 1";
	if (!insert_blob (dict, 1, blob_data, strlen (blob_data), &error)) 
		g_error ("Blob insert error: %s", error && error->message ? error->message : "No detail");
	else if (error) {
		g_print ("Msg: %s\n", error->message);
		g_error_free (error);
		error = NULL;
	}

	/* insert a blob */
	blob_data = "Blob Data 2";
	if (!insert_blob (dict, 2, blob_data, strlen (blob_data), &error)) 
		g_error ("Blob insert error: %s", error && error->message ? error->message : "No detail");
	else if (error) {
		g_print ("Msg: %s\n", error->message);
		g_error_free (error);
		error = NULL;
	}
	if (!display_blobs (dict, &error))
		g_error ("Blobs display error: %s", error && error->message ? error->message : "No detail");


	/* update blob */
	blob_data = "New blob 1 contents is now this one...";
	if (!update_blob (dict, 1, blob_data, strlen (blob_data), &error)) 
		g_error ("Blob update error: %s", error && error->message ? error->message : "No detail");
	else if (error) {
		g_print ("Msg: %s\n", error->message);
		g_error_free (error);
		error = NULL;
	}
	if (!display_blobs (dict, &error))
		g_error ("Blobs display error: %s", error && error->message ? error->message : "No detail");

	/* update blob */
	blob_data = "After several blobs updated";
	if (!update_multiple_blobs (dict, blob_data, strlen (blob_data), &error)) 
		g_error ("Multiple blob update error: %s", error && error->message ? error->message : "No detail");
	else if (error) {
		g_print ("Msg: %s\n", error->message);
		g_error_free (error);
		error = NULL;
	}
	if (!display_blobs (dict, &error))
		g_error ("Blobs display error: %s", error && error->message ? error->message : "No detail");


	/* SQL Postgres:
	   create table blobs (id serial not null primary key, name varchar (50), data oid);
	   SQL Oracle:
	   CREATE TABLE blobs (id number primary key, name varchar2 (50), data BLOB);
	*/

	gda_connection_commit_transaction (cnc, NULL, NULL);
	gda_connection_close (cnc);

	return 0;
}

static void
show_header (const gchar *str)
{
	g_print ("\n\n******** %s ********\n", str);
}

static gboolean
clear_blobs (GdaDict *dict, GError **error)
{
	GdaQuery *query;
	gboolean retval;
#define SQL_DELETE "DELETE FROM blobs"
	show_header ("Clear blobs");
	query = gda_query_new_from_sql (dict, SQL_DELETE, NULL);
	if (!query)
		return FALSE;
	
	retval = do_query (query, NULL, error);
	g_object_unref (query);

	return retval;
}

static gboolean
insert_blob (GdaDict *dict, gint id, const gchar *data, glong binary_length, GError **error)
{
	GdaQuery *query;
	GdaParameterList *plist;
	GdaParameter *param;
	GValue *value;
	gchar *str;
	gboolean retval;
	#define SQL_INSERT "INSERT INTO blobs (id, name, data) VALUES (##[:name='id' :type='gint'], ##[:name='name' :type='gchararray'], ##[:name='theblob' :type='GdaBlob'])"

	show_header ("Insert a blob");
	query = gda_query_new_from_sql (dict, SQL_INSERT, NULL);
	if (!query)
		return FALSE;
	plist = gda_query_get_parameter_list (query);

	/* blob id */
	param = gda_parameter_list_find_param (plist, "id");
	str = g_strdup_printf ("%d", id);
	gda_parameter_set_value_str (param, str);
	g_free (str);

	/* blob name */
	param = gda_parameter_list_find_param (plist, "name");
	str = g_strdup_printf ("BLOB_%d", id);
	gda_parameter_set_value_str (param, str);
	g_free (str);

	/* blob data */
	param = gda_parameter_list_find_param (plist, "theblob");
	value = gda_value_new_blob (data, binary_length);
	gda_parameter_set_value (param, value);
	gda_value_free (value);

	gda_connection_clear_events_list (gda_dict_get_connection (dict));
	retval = do_query (query, plist, error);
	g_object_unref (query);
	g_object_unref (plist);

	return retval;
}

static gboolean
update_blob (GdaDict *dict, gint id, const gchar *data, glong binary_length, GError **error)
{
	GdaQuery *query;
	GdaParameterList *plist;
	GdaParameter *param;
	GValue *value;
	gchar *str;
	gboolean retval;
	#define SQL_UPDATE "UPDATE blobs set name = ##[:name='name' :type='gchararray'], data = ##[:name='theblob' :type='GdaBlob'] WHERE id= ##[:name='id' :type='gint']"

	show_header ("Update a blob");
	query = gda_query_new_from_sql (dict, SQL_UPDATE, NULL);
	if (!query)
		return FALSE;
	plist = gda_query_get_parameter_list (query);

	/* blob id */
	param = gda_parameter_list_find_param (plist, "id");
	str = g_strdup_printf ("%d", id);
	gda_parameter_set_value_str (param, str);
	g_free (str);

	/* blob name */
	param = gda_parameter_list_find_param (plist, "name");
	str = g_strdup_printf ("BLOB_%d", id);
	gda_parameter_set_value_str (param, str);
	g_free (str);

	/* blob data */
	param = gda_parameter_list_find_param (plist, "theblob");
	value = gda_value_new_blob (data, binary_length);
	gda_parameter_set_value (param, value);
	gda_value_free (value);

	gda_connection_clear_events_list (gda_dict_get_connection (dict));
	retval = do_query (query, plist, error);
	g_object_unref (query);
	g_object_unref (plist);

	return retval;
}

static gboolean
update_multiple_blobs (GdaDict *dict, const gchar *data, glong binary_length, GError **error)
{
	GdaQuery *query;
	GdaParameterList *plist;
	GdaParameter *param;
	GValue *value;
	gchar *str;
	gboolean retval;
	#define SQL_UPDATE "UPDATE blobs set name = ##[:name='name' :type='gchararray'], data = ##[:name='theblob' :type='GdaBlob']"

	show_header ("Update several blobs at once");
	query = gda_query_new_from_sql (dict, SQL_UPDATE, NULL);
	if (!query)
		return FALSE;
	plist = gda_query_get_parameter_list (query);

	/* blob name */
	param = gda_parameter_list_find_param (plist, "name");
	gda_parameter_set_value_str (param, "---");

	/* blob data */
	param = gda_parameter_list_find_param (plist, "theblob");
	value = gda_value_new_blob (data, binary_length);
	gda_parameter_set_value (param, value);
	gda_value_free (value);

	gda_connection_clear_events_list (gda_dict_get_connection (dict));
	retval = do_query (query, plist, error);
	g_object_unref (query);
	g_object_unref (plist);

	return retval;
}

static gboolean do_query (GdaQuery *query, GdaParameterList *plist, GError **error)
{
	GdaObject *exec_res;
	exec_res = gda_query_execute (query, plist, FALSE, error);
	if (!exec_res)
		return FALSE;
	
	if (GDA_IS_DATA_MODEL (exec_res)) {
		g_print ("Query returned a GdaDataModel...\n");
		gda_data_model_dump ((GdaDataModel*) exec_res, stdout);
	}
	else {
		if (GDA_IS_PARAMETER_LIST (exec_res)) {
			GSList *list;

			g_print ("Query returned a GdaParameterList:\n");
			for (list = GDA_PARAMETER_LIST (exec_res)->parameters; list; list = list->next) {
				gchar *str;
				str = gda_parameter_get_value_str (GDA_PARAMETER (list->data));
				g_print (" %s => %s\n", gda_object_get_name (GDA_OBJECT (list->data)), str);
				g_free (str);
			}
					 
		}
		else
			g_print ("Query returned a %s object\n", G_OBJECT_TYPE_NAME (exec_res));
	}

	return TRUE;
}

static gboolean
display_blobs (GdaDict *dict, GError **error)
{
	GdaCommand *command;
	gboolean retval;
	GdaDataModel *model;

	/* no blob is actually manipulated here, so let's use a GdaCommand */
	command = gda_command_new ("SELECT * FROM blobs", GDA_COMMAND_TYPE_SQL, 0);
	model = gda_connection_execute_select_command (gda_dict_get_connection (dict), command, NULL, error);
	gda_command_free (command);

	retval = model ? TRUE : FALSE;
	if (model) {
		gda_data_model_dump (model, stdout);
		g_object_unref (model);
	}
	return retval;
}
