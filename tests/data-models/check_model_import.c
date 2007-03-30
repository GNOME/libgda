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
START_TEST (load_test_1)
{
	if (!do_test_load_file ("FIELDS_SCHEMA_MySQL_actor.xml"))
		fail ("Could not load file");
}
END_TEST

START_TEST (load_test_2)
{
	if (!do_test_load_file ("TYPES_SCHEMA_MySQL.xml"))
		fail ("Could not load file");
}
END_TEST

Suite *
test1_suite (void)
{
	Suite *s = suite_create ("Test import");
	
	/* Core test case */
	TCase *tc_core = tcase_create ("Load and compare");
	tcase_set_timeout (tc_core, 10);
	tcase_add_test (tc_core, load_test_1);
	tcase_add_test (tc_core, load_test_2);
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

	gda_init ("check-model-import", PACKAGE_VERSION, argc, argv);

	if (!do_test_load_file ("FIELDS_SCHEMA_MySQL_actor.xml"))
		number_failed ++;
	if (!do_test_load_file ("TYPES_SCHEMA_MySQL.xml"))
		number_failed ++;

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
		g_warning ("Model exported as string differs from loaded data model:");
		g_print ("========== From File ==========\n%s\n", contents);
		g_print ("========== Export as string ==========\n%s\n", export);
#endif
		retval = FALSE;
		goto out;
	}
	g_free (export);
	g_free (contents);

	gda_data_model_dump (import, stdout);

 out:
	g_object_unref (import);
	return retval;
}
