#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <libgda/libgda.h>

#ifdef HAVE_CHECK
#include <check.h>
#else
#define fail(x) g_warning (x)
#define fail_if(x,y) if (x) g_warning (y)
#define fail_unless(x,y) if (!(x)) g_warning (y)
#endif

#define CHECK_EXTRA_INFO
static gboolean do_test_load_file (const gchar *filename);

#ifdef HAVE_CHECK

GArray *tested_files;

START_TEST (load_test)
{
	gchar *filename;

	filename = g_array_index (tested_files, gchar *, _i);
	g_print ("Tested: %s\n", filename);
	if (!do_test_load_file (filename)) {
		gchar *str = g_strdup_printf ("Could not load file '%s'", filename);
		fail (str);
	}
}
END_TEST

Suite *
test1_suite (void)
{
	Suite *s = suite_create ("Test import");
	GDir *dir;
	gchar *dirname;
	GError *err = NULL;
	const gchar *name;
	
	tested_files = g_array_new (FALSE, FALSE, sizeof (gchar *));

	/* Core test case */
	TCase *tc_core = tcase_create ("Load and compare");

	dirname = g_build_filename (CHECK_XML_FILES, "tests", "providers", NULL);
	if (!(dir = g_dir_open (dirname, 0, &err))) {
#ifdef CHECK_EXTRA_INFO
		g_warning ("Could not open directory '%s': %s", dirname,
			   err && err->message ? err->message : "No detail");
#endif
		exit (EXIT_FAILURE);
	}
	g_free (dirname);

	while ((name = g_dir_read_name (dir))) {
		if (g_str_has_suffix (name, ".xml")) {
			gchar *str = g_strdup (name);
			g_array_append_val (tested_files, str);
		}
	}
	g_dir_close (dir);

	tcase_add_loop_test (tc_core, load_test, 0, tested_files->len);
	suite_add_tcase (s, tc_core);
	
	return s;
}

int
main (int argc, char **argv)
{
	int number_failed = 0;

	gda_init ("check-model-import", PACKAGE_VERSION, argc, argv);

	Suite *s = test1_suite ();
	SRunner *sr = srunner_create (s);
		
	srunner_run_all (sr, CK_NORMAL);
	number_failed += srunner_ntests_failed (sr);
	srunner_free (sr);
	
	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
#else

/*
 * The Check library is not available
 */
int
main (int argc, char **argv)
{
	int number_failed = 0;
	GDir *dir;
	gchar *dirname;
	GError *err = NULL;
	const gchar *name;

	gda_init ("check-model-import", PACKAGE_VERSION, argc, argv);

	dirname = g_build_filename (CHECK_XML_FILES, "tests", "providers", NULL);
	if (!(dir = g_dir_open (dirname, 0, &err))) {
#ifdef CHECK_EXTRA_INFO
		g_warning ("Could not open directory '%s': %s", dirname,
			   err && err->message ? err->message : "No detail");
#endif
		return EXIT_FAILURE;
	}
	g_free (dirname);

	if (argc == 2) {
		if (g_str_has_suffix (argv[1], ".xml")) {
			g_print ("Tested: %s\n", argv[1]);
			if (!do_test_load_file (argv[1]))
				number_failed ++;
		}
	}
	else 
		while ((name = g_dir_read_name (dir))) {
			if (g_str_has_suffix (name, ".xml")) {
				g_print ("Tested: %s\n", name);
				if (!do_test_load_file (name))
					number_failed ++;
			}
		}
	g_dir_close (dir);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
#endif

static gboolean
do_test_load_file (const gchar *filename)
{
	gchar *file;
	GdaDataModel *import;
	GSList *errors;
	gboolean retval = TRUE;
	gchar *export, *contents;

	file = g_build_filename (CHECK_XML_FILES, "tests", "providers", filename, NULL);
	import = gda_data_model_import_new_file (file, TRUE, NULL);

	if ((errors = gda_data_model_import_get_errors (GDA_DATA_MODEL_IMPORT (import)))) {
#ifdef CHECK_EXTRA_INFO
		g_warning ("Could not load the expected schema file '%s': ", file);
		for (; errors; errors = errors->next) 
			g_print ("\t%s\n", 
				 ((GError *)(errors->data))->message ? ((GError *)(errors->data))->message : "No detail");
#endif
		retval = FALSE;
		goto out;
	}

	export = gda_data_model_export_to_string (import, GDA_DATA_MODEL_IO_DATA_ARRAY_XML, NULL, 0, NULL, 0, NULL);
	g_assert (g_file_get_contents (file, &contents, NULL, NULL));
	if (strcmp (export, contents)) {
#ifdef CHECK_EXTRA_INFO
		g_print ("========== Imported data model ==========\n");
		gda_data_model_dump (import, stdout);

		g_warning ("Model exported as string differs from loaded data model:");
		g_print ("========== Export as string ==========\n%s\n", export);
#endif
		retval = FALSE;
		goto out;
	}
	g_free (export);
	g_free (contents);

 out:
	g_object_unref (import);
	return retval;
}
