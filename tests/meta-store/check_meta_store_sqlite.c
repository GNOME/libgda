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
#include <gio/gio.h>
#include "common.h"

int 
main(G_GNUC_UNUSED int argc, G_GNUC_UNUSED char ** argv)
{
	GdaMetaStore *store;
	GFile *pdir;
	GFile *fstore;

	gda_init ();

	gchar **penv = g_get_environ ();
	const gchar *dir = g_environ_getenv (penv, "GDA_TOP_BUILD_DIR");
	GRand *rand = g_rand_new ();
	pdir = g_file_new_for_path (dir);
	if (!g_file_query_exists (pdir, NULL)) {
		return 1;
	}

	gchar *duri = g_file_get_uri (pdir);
	gchar *db = g_strdup_printf ("%s/tests/meta-store/test%d.db", duri, g_rand_int (rand));
	g_free (duri);
	fstore = g_file_new_for_uri (db);
	g_free (db);
	gchar *furi = g_file_get_uri (fstore);
	g_print ("Try to use file for metastore: %s\n", furi);
	g_free (furi);
	while (g_file_query_exists (fstore, NULL)) {
		g_print ("File exists for metastore\n");
		g_free (db);
		duri = g_file_get_uri (pdir);
		db = g_strdup_printf ("%s/tests/meta-store/test%d.db", duri, g_rand_int (rand));
		g_free (duri);
		g_object_unref (fstore);
		fstore = g_file_new_for_uri (db);
		g_free (db);
		furi = g_file_get_uri (fstore);
		g_print ("Tes for new file: %s\n", furi);
		g_free (furi);
	}

	/* Clean eveything which might exist in the store */
	gchar *pfstore = g_file_get_path (fstore);
	store = gda_meta_store_new_with_file (pfstore);
	common_drop_all_tables (store);
	g_object_unref (store);

	/* Test store setup*/
	store = gda_meta_store_new_with_file (pfstore);
	g_print ("STORE: %p, version: %d\n", store, store ? gda_meta_store_get_version (store) : 0);

	/* Tests */
	tests_group_1 (store);
	g_object_unref (store);
	g_free (pfstore);
	if (g_file_query_exists (fstore, NULL)) {
		g_file_delete (fstore, NULL, NULL);
	}
	g_object_unref (fstore);
	g_strfreev (penv);

	g_print ("Test Ok.\n");

	return EXIT_SUCCESS;
}





