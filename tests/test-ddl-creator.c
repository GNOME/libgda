/*
 * Copyright (C) 2008 - 2011 Vivien Malerba <malerba@gnome-db.org>
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
#include <glib/gstdio.h>
#include "raw-ddl-creator.h"

int
main (G_GNUC_UNUSED int argc, G_GNUC_UNUSED char** argv)
{
	RawDDLCreator *ddl;
	GError *error = NULL;
	GdaConnection *cnc;
	gchar *str;
	gchar *file;

	gda_init ();
	
	ddl = raw_ddl_creator_new ();
	file = g_build_filename (CHECK_FILES, "tests", "dbstruct.xml", NULL);
	if (!raw_ddl_creator_set_dest_from_file (ddl, file, &error)) {
		g_print ("Error creating RawDDLCreator: %s\n", error && error->message ? error->message : "No detail");
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
	raw_ddl_creator_set_connection (ddl, cnc);
	g_object_unref (cnc);

	/* get SQL */
	str = raw_ddl_creator_get_sql (ddl, &error);
	if (error != NULL) {
		g_print ("Error getting SQL: %s\n", error && error->message ? error->message : "No detail");
		g_error_free (error);
	} else {
		g_print ("Error getting SQL without error set\n");
	}
	g_assert (str != NULL);
	g_print ("%s\n", str);
	g_free (str);

	/* execute */
	if (!raw_ddl_creator_execute (ddl, &error)) {
		g_print ("Error creating database objects: %s\n", error && error->message ? error->message : "No detail");
		g_error_free (error);
		return EXIT_FAILURE;
	}

	g_object_unref (ddl);
	g_unlink ("creator.db");

	return EXIT_SUCCESS;
}
