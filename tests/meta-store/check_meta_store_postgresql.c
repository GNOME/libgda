/*
 * Copyright (C) 2008 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2018 Daniel Espinosa <esodan@gmail.com>
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
#include <stdio.h>
#include <glib/gstdio.h>
#include "common.h"

int 
main(int argc, char ** argv)
{
	GdaConnection *cnc;
	GdaMetaStore *store;
	gchar *cnc_string;
	GError *error = NULL;

	gda_init ();

	cnc_string = "DB_NAME=test;HOST=localhost;USERNAME=test;PASSWORD=test1";
	/* connection try */
	cnc = gda_connection_open_from_string ("PostgreSQL", cnc_string, NULL, GDA_CONNECTION_OPTIONS_NONE, &error);
	if (cnc == NULL) {
		if (error) {
			g_print ("Connection no stablished. Error: %s\n", error->message);
			g_error_free (error);
		}
		g_print ("Postgres test not run, please setup a database 'test', owned by 'test' role with password 'test1' at localhost\n");
		g_print ("Test Skip.\n");
		return EXIT_SUCCESS;
	}
	g_object_unref (cnc);
	/* Clean eveything which might exist in the store */
	gchar *str;
	str = g_strdup_printf ("PostgreSQL://%s", cnc_string);
	store = gda_meta_store_new (str);
	common_drop_all_tables (store);
	g_object_unref (store);

	/* Test store setup */
	store = gda_meta_store_new (str);
	g_free (str);

	g_print ("STORE: %p, version: %d\n", store, store ? gda_meta_store_get_version (store) : 0);


	/* Tests */
	tests_group_1 (store);
	g_object_unref (store);

	g_print ("Test Ok.\n");
	
	return EXIT_SUCCESS;
}





