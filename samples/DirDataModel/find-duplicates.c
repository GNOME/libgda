/*
 * Copyright (C) 2007 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
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
#include <libgda/libgda.h>
#include <virtual/libgda-virtual.h>
#include <sql-parser/gda-sql-parser.h>

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
SELECT min (dir_name || '/' || file_name) AS filename, count (data) AS occurrences, data \
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

	gda_init ();

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
        cnc = gda_virtual_connection_open (provider, GDA_CONNECTION_OPTIONS_NONE, NULL);

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

		GdaSqlParser *parser;
		GdaStatement *stmt;
		GdaSet *plist;
		GdaHolder *param;

		parser = gda_connection_create_parser (cnc);
		if (!delete) 
			stmt = gda_sql_parser_parse_string (parser, SQL_SELECT_FILE_DUPLICATES, NULL, NULL);
		else 
			stmt = gda_sql_parser_parse_string (parser, "CREATE TEMP TABLE to_del_files AS " SQL_SELECT_FILE_DUPLICATES, 
							    NULL, NULL);
		
		gda_statement_get_parameters (stmt, &plist, NULL);
		param = gda_set_get_holder (plist, "dir_name");
		g_assert (gda_holder_set_value_str (param, NULL, dirname, NULL));
		param = gda_set_get_holder (plist, "dir_name2");
		g_assert (gda_holder_set_value_str (param, NULL, dirname, NULL));
		param = gda_set_get_holder (plist, "filename");
		g_assert (gda_holder_set_value_str (param, NULL, filename, NULL));
		param = gda_set_get_holder (plist, "filename2");
		g_assert (gda_holder_set_value_str (param, NULL, filename, NULL));
		g_object_unref (parser);

		/* list files duplicates */
		if (!delete) {
			duplicates = gda_connection_statement_execute_select (cnc, stmt, plist, NULL);
			if (gda_data_model_get_n_rows (duplicates) > 0)
				gda_data_model_dump (duplicates, stdout);
			else
				g_print ("No duplicate file found\n");
			g_object_unref (duplicates);
		}
		else {
			gda_connection_statement_execute_non_select (cnc, stmt, plist, NULL, NULL);

			duplicates = run_sql_select (cnc, SQL_SELECT_FILES_TO_DEL);
			if (gda_data_model_get_n_rows (duplicates) > 0) {
				gda_data_model_dump (duplicates, stdout);
				run_sql_non_select (cnc, SQL_DELETE_FILE_DUPLICATES);
			}
			else
				g_print ("No duplicate file found\n");
			g_object_unref (duplicates);
		}

		g_object_unref (stmt);
		g_object_unref (plist);
	}

	return 0;
}

static GdaDataModel *
run_sql_select (GdaConnection *cnc, const gchar *sql)
{
	GdaStatement *stmt;
	GError *error = NULL;
	GdaDataModel *res;
	GdaSqlParser *parser;

	parser = gda_connection_create_parser (cnc);
	stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
	g_object_unref (parser);

        res = gda_connection_statement_execute_select (cnc, stmt, NULL, &error);
        g_object_unref (stmt);
	if (!res) 
		g_error ("Could not execute query: %s\n", 
			 error && error->message ? error->message : "no detail");
	return res;
}

static void
run_sql_non_select (GdaConnection *cnc, const gchar *sql)
{
	GdaStatement *stmt;
        GError *error = NULL;
        gint nrows;
	GdaSqlParser *parser;

	parser = gda_connection_create_parser (cnc);
	stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
	g_object_unref (parser);

	nrows = gda_connection_statement_execute_non_select (cnc, stmt, NULL, NULL, &error);
	g_object_unref (stmt);
        if (nrows == -1) 
                g_error ("NON SELECT error: %s\n", error && error->message ? error->message : "no detail");
}
