/* 
 * Copyright (C) 2008 The GNOME Foundation.
 *
 * AUTHORS:
 *      Vivien Malerba <malerba@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <libgda/libgda.h>
#include <gda-ddl-creator.h>

int
main (int argc, char** argv)
{
	GdaDDLCreator *ddl;
	GError *error = NULL;
	GdaConnection *cnc;
	gchar *str;
	gchar *file;

	/* set up test environment */
        g_setenv ("GDA_TOP_BUILD_DIR", TOP_BUILD_DIR, 0);
	gda_init ();
	
	ddl = gda_ddl_creator_new ();
	file = g_build_filename (CHECK_FILES, "tests", "dbstruct.xml", NULL);
	if (!gda_ddl_creator_set_dest_from_file (ddl, file, &error)) {
		g_print ("Error creating GdaDDLCreator: %s\n", error && error->message ? error->message : "No detail");
		g_error_free (error);
		return EXIT_FAILURE;
	}
	g_free (file);
	
	/* open a connection */
	g_unlink ("creator.db");
	cnc = gda_connection_open_from_string ("SQLite", "DB_DIR=.;DB_NAME=creator", NULL, GDA_CONNECTION_OPTIONS_NONE, &error);
	if (!cnc) {
		g_print ("Error opening connection: %s\n", error && error->message ? error->message : "No detail");
		g_error_free (error);
		return EXIT_FAILURE;
	}
	gda_ddl_creator_set_connection (ddl, cnc);
	g_object_unref (cnc);

	/* get SQL */
	str = gda_ddl_creator_get_sql (ddl, &error);
	if (!str) {
		g_print ("Error getting SQL: %s\n", error && error->message ? error->message : "No detail");
		g_error_free (error);
		return EXIT_FAILURE;
	}
	g_print ("%s\n", str);
	g_free (str);

	/* execute */
	if (!gda_ddl_creator_execute (ddl, &error)) {
		g_print ("Error creating database objects: %s\n", error && error->message ? error->message : "No detail");
		g_error_free (error);
		return EXIT_FAILURE;
	}

	g_object_unref (ddl);
	g_unlink ("creator.db");

	return EXIT_SUCCESS;
}
