/*AA*/
#include <libgda/libgda.h>
#include <virtual/libgda-virtual.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* command line options */
gchar *file = NULL;
gboolean delete = FALSE;

static GOptionEntry entries[] = {
        { "file", 'f', 0, G_OPTION_ARG_STRING, &file, "Find all files which are duplicates of this file", "filename"},
	{ "delete", 'd', 0, G_OPTION_ARG_NONE, &delete, "Delete the duplicates of file specified by the -f option", NULL },
        { NULL }
};

/* SQL queries */
#define SQL_CREATE_FILTERED_TABLE "CREATE TEMP TABLE filtered_files AS \
SELECT * FROM files WHERE md5sum IN (SELECT sf.md5sum FROM files sf GROUP BY sf.md5sum HAVING count (sf.md5sum) > 1)"

#define SQL_CREATE_ALL_DUPLICATES_TABLE "CREATE TEMP TABLE duplicates AS \
SELECT min (dir_name || '/' || file_name) AS filename, count (data) AS occurences, data \
FROM filtered_files \
GROUP BY data HAVING count (data) > 1"

#define SQL_SELECT_DUPLICATES "SELECT f.dir_name || '/' || f.file_name AS file , d.filename AS copy_of FROM filtered_files f \
INNER JOIN duplicates d on (f.data = d.data) \
WHERE f.dir_name || '/' || f.file_name != d.filename"

#define SQL_SELECT_FILE_DUPLICATES "SELECT f.dir_name || '/' || f.file_name AS file FROM filtered_files f \
WHERE f.dir_name || f.file_name != ## /* name:dir_name2 type:gchararray */ || ## /* name:filename2 type:gchararray */ AND \
f.data = (SELECT data FROM filtered_files sf WHERE sf.dir_name= ## /* name:dir_name type:gchararray */ AND sf.file_name = ## /* name:filename type:gchararray */) "

#define SQL_SELECT_FILES_TO_DEL "SELECT * FROM to_del_files"
#define SQL_DELETE_FILE_DUPLICATES "DELETE FROM files WHERE dir_name || '/' || file_name IN (SELECT file FROM to_del_files)"

/* utility functions */
static void          run_sql_non_select (GdaConnection *cnc, const gchar *sql);
static GdaDataModel *run_sql_select (GdaConnection *cnc, const gchar *sql);

int
main (int argc, char *argv [])
{
	GOptionContext *context;
	GdaDataModel *model, *duplicates;
	GError *error = NULL;
	GdaConnection *cnc;
        GdaVirtualProvider *provider;
	gchar *dirname = NULL;
	gchar *filename = NULL;

	g_print ("WARNING: this is a demonstration program and should _not_ be used on production systems as\n"
		 "some yet unknown bug may remove all your files. You'll have been warned.\n\n");

	context = g_option_context_new ("[start directory]");
        g_option_context_add_main_entries (context, entries, NULL);
        if (!g_option_context_parse (context, &argc, &argv, &error)) {
                g_print ("Can't parse arguments: %s", error->message);
                exit (1);
        }
        g_option_context_free (context);

	gda_init ("find_duplicates", "3.1.1", argc, argv);

	if (argc == 2)
		dirname = argv [1];

	if (file) {
		/* if a file to find duplicates for is provided, then make it fit with the dirname */
		if (!dirname) {
			dirname = g_path_get_dirname (file);
			filename = g_path_get_basename (file);
		}
		else {
			gchar *path = g_path_get_dirname (file);
			if ((strlen (path) < strlen (dirname)) || 
			    strncmp (path, dirname, strlen (dirname))) {
				g_print ("Cannot find a file duplicate if not in start directory\n");
				exit (1);
			}
			filename = g_path_get_basename (file);
			g_free (path);
		}
	}

	if (!dirname)
		dirname = ".";
	
	g_print ("Start directory: %s\n", dirname);
	if (filename) {
		gchar *tmp = g_build_filename (dirname, filename, NULL);
		g_print ("Find duplicates of: %s\n", tmp);
		g_free (tmp);
	}

	/* set up virtual environment */
	provider = gda_vprovider_data_model_new ();
        cnc = gda_server_provider_create_connection (NULL, GDA_SERVER_PROVIDER (provider), NULL, NULL, NULL, 0);
        g_assert (gda_connection_open (cnc, NULL));

	model = gda_data_model_dir_new (dirname);
	g_print ("Finding duplicates among %d files\n", gda_data_model_get_n_rows (model));
	if (!gda_vconnection_data_model_add_model (GDA_VCONNECTION_DATA_MODEL (cnc), model, "files", &error))
                g_error ("Add GdaDataModelDir model error: %s\n", error && error->message ? error->message : "no detail");
	g_object_unref (model);

	if (!filename) {
		/* create a temproary table which contains only the potential duplicated files (from the MD5SUM) */
		run_sql_non_select (cnc, SQL_CREATE_FILTERED_TABLE);
		
		/* create a "duplicates" temporary table */
		run_sql_non_select (cnc, SQL_CREATE_ALL_DUPLICATES_TABLE);
		
		/* actually list the duplicated files */
		duplicates = run_sql_select (cnc, SQL_SELECT_DUPLICATES);
		if (gda_data_model_get_n_rows (duplicates) > 0)
			gda_data_model_dump (duplicates, stdout);
		else
			g_print ("No duplicate file found\n");
		g_object_unref (duplicates);
	}
	else {
		/* create a temproary table which contains only the potential duplicated files (from the MD5SUM) */
		run_sql_non_select (cnc, SQL_CREATE_FILTERED_TABLE);

		GdaQuery *query;
		GdaParameterList *plist;
		GdaParameter *param;
		gchar *sql;
		if (!delete) 
			query = gda_query_new_from_sql (NULL, SQL_SELECT_FILE_DUPLICATES, NULL);
		else 
			query = gda_query_new_from_sql (NULL, "CREATE TEMP TABLE to_del_files AS " SQL_SELECT_FILE_DUPLICATES, NULL);
		
		plist = gda_query_get_parameter_list (query);
		param = gda_parameter_list_find_param (plist, "dir_name");
		gda_parameter_set_value_str (param, dirname);
		param = gda_parameter_list_find_param (plist, "dir_name2");
		gda_parameter_set_value_str (param, dirname);
		param = gda_parameter_list_find_param (plist, "filename");
		gda_parameter_set_value_str (param, filename);
		param = gda_parameter_list_find_param (plist, "filename2");
		gda_parameter_set_value_str (param, filename);
		sql = gda_renderer_render_as_sql (GDA_RENDERER (query), plist, NULL, 0, NULL);
		g_object_unref (plist);

		/* list files duplicates */
		if (!delete) {
			duplicates = run_sql_select (cnc, sql);
			if (gda_data_model_get_n_rows (duplicates) > 0)
				gda_data_model_dump (duplicates, stdout);
			else
				g_print ("No duplicate file found\n");
			g_object_unref (duplicates);
		}
		else {
			run_sql_non_select (cnc, sql);

			duplicates = run_sql_select (cnc, SQL_SELECT_FILES_TO_DEL);
			if (gda_data_model_get_n_rows (duplicates) > 0) {
				gda_data_model_dump (duplicates, stdout);
				run_sql_non_select (cnc, SQL_DELETE_FILE_DUPLICATES);
			}
			else
				g_print ("No duplicate file found\n");
			g_object_unref (duplicates);
		}
		g_free (sql);
	}

	return 0;
}

static GdaDataModel *
run_sql_select (GdaConnection *cnc, const gchar *sql)
{
	GdaCommand *command;
	GError *error = NULL;
	GdaDataModel *res;

	command = gda_command_new (sql, GDA_COMMAND_TYPE_SQL, 0);
        res = gda_connection_execute_select_command (cnc, command, NULL, &error);
        gda_command_free (command);
	if (!res) 
		g_error ("Could not execute query: %s\n", 
			 error && error->message ? error->message : "no detail");
	return res;
}

static void
run_sql_non_select (GdaConnection *cnc, const gchar *sql)
{
        GdaCommand *command;
        GError *error = NULL;
        gint nrows;

        command = gda_command_new (sql, GDA_COMMAND_TYPE_SQL, 0);
        nrows = gda_connection_execute_non_select_command (cnc, command, NULL, &error);
        gda_command_free (command);
        if (nrows == -1) 
                g_error ("NON SELECT error: %s\n", error && error->message ? error->message : "no detail");
}
