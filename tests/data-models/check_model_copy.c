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
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <libgda/libgda.h>

#define fail(x) g_warning (x)
#define fail_if(x,y) if (x) g_warning (y)
#define fail_unless(x,y) if (!(x)) g_warning (y)

#define CHECK_EXTRA_INFO
static gboolean do_test_load_file (const gchar *filename);

int
main (int argc, char **argv)
{
	int number_failed = 0;
	GDir *dir;
	GError *err = NULL;
	const gchar *name;

	gda_init ();

	

	if (argc == 2) {
		if (g_str_has_suffix (argv[1], ".xml")) {
			g_print ("Tested: %s\n", argv[1]);
			if (!do_test_load_file (argv[1]))
				number_failed ++;
		}
	}
	else {
		/* data models in tests/providers */
		gchar *dirname;
		dirname = g_build_filename (CHECK_FILES, "tests", "providers", NULL);
		if (!(dir = g_dir_open (dirname, 0, &err))) {
#ifdef CHECK_EXTRA_INFO
			g_warning ("Could not open directory '%s': %s", dirname,
				   err && err->message ? err->message : "No detail");
#endif
			return EXIT_FAILURE;
		}

		while ((name = g_dir_read_name (dir))) {
			if (g_str_has_suffix (name, ".xml")) {
				gchar *full_path = g_build_filename (dirname, name, NULL);
				g_print ("Tested: %s\n", full_path);
				if (!do_test_load_file (full_path))
					number_failed ++;
				g_free (full_path);
			}
		}
		g_free (dirname);
		g_dir_close (dir);

		/* data models in the current dir */
		dirname = g_build_filename (CHECK_FILES, "tests", "data-models", NULL);
		if (!(dir = g_dir_open (dirname, 0, &err))) {
#ifdef CHECK_EXTRA_INFO
			g_warning ("Could not open directory '%s': %s", dirname,
				   err && err->message ? err->message : "No detail");
#endif
			return EXIT_FAILURE;
		}

		while ((name = g_dir_read_name (dir))) {
			if (g_str_has_suffix (name, ".xml")) {
				gchar *full_path = g_build_filename (dirname, name, NULL);
				g_print ("Tested: %s\n", full_path);
				if (!do_test_load_file (full_path))
					number_failed ++;
				g_free (full_path);
			}
		}
		g_free (dirname);
		g_dir_close (dir);
	}

	if (number_failed == 0)
		g_print ("Ok.\n");
	else
		g_print ("%d failed\n", number_failed);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

static gboolean
do_test_load_file (const gchar *filename)
{
	GdaDataModel *import, *copy = NULL;
	GdaDataComparator *cmp = NULL;
	GSList *errors;
	gboolean retval = TRUE;
	GError *error = NULL;

	/* make sure we only test data model dumps */
	xmlDocPtr doc;
	xmlNodePtr root;
	doc = xmlParseFile (filename);
	if (!doc) 
		/* abort the test */
		return TRUE;
	root = xmlDocGetRootElement (doc);
	if (g_strcmp0 ((const gchar*) root->name, "gda_array")) {
		/* abort the test */
		xmlFreeDoc (doc);
		return TRUE;
	}
	xmlFreeDoc (doc);

	import = gda_data_model_import_new_file (filename, TRUE, NULL);

	if ((errors = gda_data_model_import_get_errors (GDA_DATA_MODEL_IMPORT (import)))) {
#ifdef CHECK_EXTRA_INFO
		g_warning ("Could not load file '%s': ", filename);
		for (; errors; errors = errors->next) 
			g_print ("\t%s\n", 
				 ((GError *)(errors->data))->message ? ((GError *)(errors->data))->message : "No detail");
#endif
		retval = FALSE;
		goto out;
	}

	copy = (GdaDataModel*) gda_data_model_array_copy_model (import, &error);
	if (!copy) {
#ifdef CHECK_EXTRA_INFO
		g_warning ("Could not copy data model: %s", error && error->message ? error->message : "No detail");
#endif
		g_error_free (error);
		retval = FALSE;
		goto out;
	}

	cmp = (GdaDataComparator*) gda_data_comparator_new (import, copy);
	if (! gda_data_comparator_compute_diff (cmp, &error)) {
#ifdef CHECK_EXTRA_INFO
		g_warning ("Could not compute differences: %s", error && error->message ? error->message : "No detail");
#endif
		g_error_free (error);
		retval = FALSE;
		goto out;
	}

	if (gda_data_comparator_get_n_diffs (cmp) > 0) {
#ifdef CHECK_EXTRA_INFO
		g_print ("There are %d difference(s)\n", gda_data_comparator_get_n_diffs (cmp));
		gda_data_model_dump (import, stdout);
		gda_data_model_dump (copy, stdout);
#endif
		retval = FALSE;
	}

 out:
	g_object_unref (import);
	if (copy)
		g_object_unref (copy);
	if (cmp)
		g_object_unref (cmp);
	return retval;
}
