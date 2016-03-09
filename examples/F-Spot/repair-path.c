/*
 * Copyright (C) 2007 - 2014 Vivien Malerba <malerba@gnome-db.org>
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
gchar *dbname = NULL;
gboolean dowrite = FALSE;

static GOptionEntry entries[] = {
        { "file", 'f', 0, G_OPTION_ARG_STRING, &dbname, "DB file", "/path/to/photos3.db"},
	{ "write", 'w', 0, G_OPTION_ARG_NONE, &dowrite, "Actually write modifications to be done", NULL},
        { NULL }
};

/* SQL queries */

/* utility functions */
static void          run_sql_non_select (GdaConnection *cnc, const gchar *sql);
static GdaDataModel *run_sql_select (GdaConnection *cnc, const gchar *sql);
static gint          run_and_show_sql_select (GdaConnection *cnc, const gchar *sql, const gchar *title);
int
main (int argc, char *argv [])
{
	GOptionContext *context;
	GdaDataModel *dir_model;
	GError *error = NULL;
	GdaConnection *hub, *cnc;
        GdaVirtualProvider *provider;
	gint nb_missing_files;

	gchar *base_dir = NULL;
	gchar *cnc_db_name, *cnc_dir_name, *cnc_string; /* used to open the SQLite connection */

	g_print ("WARNING: this is a demonstration program and should _not_ be used on production systems as\n"
		 "some yet unknown bug may corrupt your F-Spot database. You'll have been warned.\n\n");

	context = g_option_context_new ("</path/to/some/pictures>");
        g_option_context_add_main_entries (context, entries, NULL);
        if (!g_option_context_parse (context, &argc, &argv, &error)) {
                g_print ("Can't parse arguments: %s", error->message);
                exit (1);
        }
        g_option_context_free (context);

	gda_init ();

	if (argc == 2)
		base_dir = argv [1];
	if (!base_dir)
		base_dir = g_get_current_dir ();
	else {
		if (! g_path_is_absolute (base_dir)) {
			g_print ("Pictures directory must be an absolute dir\n");
			exit (1);
		}
	}

	/* compute connection string */
	if (!dbname) {
		cnc_dir_name = g_build_path (G_DIR_SEPARATOR_S, g_get_home_dir (), ".gnome2", "f-spot", NULL);
		cnc_db_name = g_strdup ("photos.db");
	}
	else {
		if (! g_path_is_absolute (dbname)) {
			cnc_dir_name = g_get_current_dir ();
			cnc_db_name = g_strdup (dbname);
		}
		else {
			cnc_dir_name = g_path_get_dirname (dbname);
			cnc_db_name = g_path_get_basename (dbname);
		}
	}
	cnc_string = g_strdup_printf ("DB_DIR=%s;DB_NAME=%s", cnc_dir_name, cnc_db_name);

	/* open connection */
	cnc = gda_connection_open_from_string ("SQLite", cnc_string, NULL, 
					       GDA_CONNECTION_OPTIONS_READ_ONLY, &error);
	if (!cnc) {
		g_print ("Could not open connection to F-Spot database file: %s\n", 
			 error && error->message ? error->message : "No detail");
		exit (1);
	}
	g_print ("Using Spot database file %s/%s\n", cnc_dir_name, cnc_db_name);
	g_free (cnc_string);
	g_free (cnc_dir_name);
	g_free (cnc_db_name);

	/* Dir data model to fetch list of pictures */
	dir_model = gda_data_model_dir_new (base_dir);
	g_print ("Using pictures in directory: %s\n", base_dir);

	/* Set up Connection hub */
        provider = gda_vprovider_hub_new ();
        hub = gda_virtual_connection_open (provider, GDA_CONNECTION_OPTIONS_NONE, NULL);
        g_assert (hub);

	if (!gda_vconnection_hub_add (GDA_VCONNECTION_HUB (hub), cnc, "spot", &error)) {
		g_print ("Could not add F-Spot connection to hub: %s\n", 
			 error && error->message ? error->message : "No detail");
		exit (1);
	}
	if (!gda_vconnection_data_model_add_model (GDA_VCONNECTION_DATA_MODEL (hub), dir_model, "files", &error)) {
                g_print ("Add GdaDataModelDir model error: %s\n", error && error->message ? error->message : "no detail");
		exit (1);
	}
        g_object_unref (dir_model);

	/* Create the missing files table */
#define SQL_SELECT_MISSING_FILES "SELECT * \
FROM photos WHERE NOT gda_file_exists (directory_path || '/' || name);"
	run_sql_non_select (hub, "CREATE TEMP TABLE missing_files AS " SQL_SELECT_MISSING_FILES); 
	run_and_show_sql_select (hub, "SELECT * FROM missing_files", "Missing files");

	/* Create the unique files table */
#define SQL_CREATE_UNIQUE_FILES_TABLE "CREATE TEMP TABLE unique_files AS \
SELECT dir_name, file_name FROM files GROUP BY file_name HAVING count (file_name) = 1"
	run_sql_non_select (hub, SQL_CREATE_UNIQUE_FILES_TABLE);
 
	/* Show missing files */	
	nb_missing_files = run_and_show_sql_select (hub, "SELECT * FROM missing_files", "Broken (missing) files");

	if (nb_missing_files != 0) {
		/* Show corrections to make */
#define SQL_SELECT_REPAIR_DIR_NAME "SELECT m.directory_path AS broken_dir, u.dir_name AS correct_dir, m.name \
FROM missing_files m INNER JOIN unique_files u ON (u.file_name = m.name)"
		run_and_show_sql_select (hub, SQL_SELECT_REPAIR_DIR_NAME, "Corrections to make");
		
		if (dowrite) {
			/* Actually write those corrections */
			run_sql_non_select (cnc, "BEGIN");
#define SQL_DELETE_REPLACEMENTS "DELETE FROM spot.photos \
WHERE id IN (SELECT m.id FROM missing_files m INNER JOIN unique_files u ON (u.file_name = m.name))"
#define SQL_SELECT_REPLACEMENTS "SELECT m.id, m.time, u.dir_name, m.name, m.description, m.default_version_id \
FROM missing_files m INNER JOIN unique_files u ON (u.file_name = m.name)"
#define SQL_INSERT_REPLACEMENTS "INSERT OR REPLACE INTO spot.photos " SQL_SELECT_REPLACEMENTS
			run_sql_non_select (hub, SQL_DELETE_REPLACEMENTS);
			run_sql_non_select (hub, SQL_INSERT_REPLACEMENTS);
			run_sql_non_select (cnc, "COMMIT");
			
			/* Re-compute the missing files */
			run_and_show_sql_select (hub, SQL_SELECT_MISSING_FILES, "Files still missing");
		}
		else {
			/* Show files which would still be missing after a write */
#define SQL_SELECT_SIMULATED_MISSING_FILES "SELECT * FROM missing_files EXCEPT \
SELECT m.* FROM missing_files m INNER JOIN unique_files u ON (u.file_name = m.name)"
			run_and_show_sql_select (hub, SQL_SELECT_SIMULATED_MISSING_FILES, 
						 "Files still missing if data was written");
		}
	}

	g_object_unref (hub);

	return 0;
}

static gint
run_and_show_sql_select (GdaConnection *cnc, const gchar *sql, const gchar *title)
{
	GdaDataModel *model;
	gint nrows;

	model = run_sql_select (cnc, sql);
	g_print ("\n====== %s ======\n", title);
	nrows = gda_data_model_get_n_rows (model);
	if (nrows != 0)
		gda_data_model_dump (model, stdout);
	else
		g_print ("None\n");
	g_object_unref (model);
	g_print ("\n");
	return nrows;
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
