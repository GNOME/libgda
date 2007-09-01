#include <stdlib.h>
#include <string.h>

#ifdef HAVE_CHECK
#include <check.h>
#else
#define fail(x) g_warning (x)
#define fail_if(x,y) if (x) g_warning (y)
#define fail_unless(x,y) if (!(x)) g_warning (y)
#endif

#define CHECK_EXTRA_INFO

#include "test-util.h"
#include <libgda/libgda.h>

TestSuite *current_ts;

#define TEST_FILENAME "/all_sql_tests.xml"
static void test_sql_statement (SqlTest *test, gint test_index);

#ifdef HAVE_CHECK
/*
 * The Check library is available
 */
START_TEST (do_test)
{
	gint index = _i - ((_i >> 16) << 16);
	GArray *unit_tests = g_array_index (current_ts->unit_tests, GArray *, (_i >> 16));
	
	test_sql_statement (g_array_index (unit_tests, SqlTest*, index), index);
}
END_TEST

Suite *
make_suite (TestSuite *ts)
{
	TCase *tc_core;
	Suite *s;
	gint i;
	
	current_ts = ts;
	s = suite_create (ts->name);
	for (i = 0; i < ts->unit_files->len; i++) {
		GArray *unit_tests = g_array_index (ts->unit_tests, GArray *, i);
		gint min, max;

		min = i << 16;
		max = min + unit_tests->len;
		tc_core = tcase_create (g_array_index (ts->unit_files, gchar *, i));
		tcase_add_loop_test (tc_core, do_test, min, max);
		suite_add_tcase (s, tc_core);
	}
	
	return s;
}

int
main (int argc, char **argv)
{
	GSList *suites, *list;
	int number_failed = 0;

	gda_init ("check-gdaquery", PACKAGE_VERSION, argc, argv);
	suites = test_suite_load_from_file (TEST_FILENAME);
	for (list = suites; list; list = list->next) {
		TestSuite *ts = (TestSuite *)(list->data);
		Suite *s = make_suite (ts);
		SRunner *sr = srunner_create (s);
		
		srunner_run_all (sr, CK_NORMAL);
		number_failed += srunner_ntests_failed (sr);
		srunner_free (sr);
	}
	
	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
#else
/*
 * The Check library is not available
 */
int
main (int argc, char **argv)
{
	GSList *suites, *list;
	gint i;

	gda_init ("check-gdaquery", PACKAGE_VERSION, argc, argv);
	suites = test_suite_load_from_file (TEST_FILENAME);
	for (list = suites; list; list = list->next) {
		current_ts = (TestSuite *)(list->data);
		for (i = 0; i < current_ts->unit_files->len; i++) {
			gint j;
			GArray *unit_tests = g_array_index (current_ts->unit_tests, GArray *, i);
			for (j = 0; j < unit_tests->len; j++)
				test_sql_statement (g_array_index (unit_tests, SqlTest*, j), j);
		}
	}
	return EXIT_SUCCESS;
}
#endif

static void
check_rendering (SqlTest *test, GdaQuery *query, const gchar *tname)
{
	gchar *str;
	GError *error = NULL;

	str = gda_renderer_render_as_sql (GDA_RENDERER (query), NULL, NULL,
					  GDA_RENDERER_PARAMS_AS_DETAILED, &error);
	if (!str) {
		str = g_strdup_printf ("Cannot render query as SQL: %s %s", 
				       error && error->message ? error->message : "No detail",
				       tname);
		fail (str);
	}
	if (!sql_test_rendering_is_correct (test, str)) {
		gchar *str2;
#ifdef CHECK_EXTRA_INFO
		g_print ("Rendered %s as: %s\n", tname, str);
#endif
		str2 = g_strdup_printf ("Wrong rendering %s", tname);
		fail (str2);
	}
	g_free (str);
}

static void
test_sql_statement (SqlTest *test, gint test_index)
{
	GdaQuery *query;
	GError *error = NULL;
	gchar *tname;
	gboolean is_non_parsed;
	gchar *str;

	if (test->test_name)
		tname = g_strdup_printf ("(Sql '%s')", test->test_name);
	else
		tname = g_strdup_printf ("(Sql n.%d)", test_index);

	query = gda_query_new_from_sql (current_ts->dict, test->sql_to_test, &error);
	is_non_parsed = (gda_query_get_query_type (query) == GDA_QUERY_TYPE_NON_PARSED_SQL);

	if (!test->sql_parsed) {
		if (!is_non_parsed) {
			str = g_strdup_printf ("Query should not be parsed %s", tname);
			fail (str);
		}
		/* test rendering */
		check_rendering (test, query, tname);
		return;
	}

#ifdef CHECK_SHOW_ALL_ERRORS
	if (is_non_parsed && error)
		g_print ("PARSE ERROR %s=>%s\n", tname, error->message ? error->message: "No detail");
#endif

	if (is_non_parsed) {
		if (test->sql_parsed) {
			str = g_strdup_printf ("Cannot parse SQL %s", tname);
#ifdef CHECK_EXTRA_INFO
			g_print ("PARSE ERROR %s=>%s\n", tname, error->message ? error->message: "No detail");
#endif
			fail (str);
		}
	}
	else {
		if (current_ts->dict) {
			gboolean active = gda_referer_is_active (GDA_REFERER (query));
			if (!active && test->sql_active) {
				str = g_strdup_printf ("Query should be active %s", tname);
				fail (str);
			}
			else if (active && !test->sql_active) {
				str = g_strdup_printf ("Query should not be active %s", tname);
				fail (str);
			}
		}
	}

	GdaParameterList *plist;
	GSList *params;
	plist = gda_query_get_parameter_list (query);

	/* test parsed parameters */
	for (params = plist ? plist->parameters : NULL; params; params = params->next) {
		const gchar *attr, *pname;
		SqlParam *param;
		gboolean abool;
		GdaParameter *query_param;

		query_param = GDA_PARAMETER (params->data);
		pname = gda_object_get_name (GDA_OBJECT (query_param));
		param = sql_tests_take_param (test, pname);
		if (!param) {
			str = g_strdup_printf ("Extra parameter '%s' %s", pname, tname);
			fail (str);
		}
		else {
			const GValue *value;
			/* don't yet compare param's type */

			/* compare param's description */
			attr = gda_object_get_description (GDA_OBJECT (query_param));
			if (param->descr && !attr) {
				str = g_strdup_printf ("Parameter '%s' should have a description %s", pname, tname);
				fail (str);
			}
			else if (!param->descr && attr) {
				str = g_strdup_printf ("Parameter '%s' should not have a description %s", pname, tname);
				fail (str);
			}
			else if (param->descr && strcmp (param->descr, attr)) {
				str = g_strdup_printf ("Parameter '%s' has a wrong description %s", pname, tname);
				fail (str);
			}
			/* compare param's NULLOK */
			abool = gda_parameter_get_not_null (query_param);
			if (param->nullok && abool) {
				str = g_strdup_printf ("Parameter '%s' expected to have NULLOK %s", pname, tname);
				fail (str);
			}
			else if (!param->nullok && !abool) {
				str = g_strdup_printf ("Parameter '%s' not expected to have NULLOK %s", pname, tname);
				fail (str);
			}

			/* default value */
			value = gda_parameter_get_default_value (query_param);
			if (value && !param->default_string) {
				str = g_strdup_printf ("Parameter '%s' should not have a default value %s", pname, tname);
				fail (str);
			}
			else if (param->default_string && !value) {
				str = g_strdup_printf ("Parameter '%s' should have a default value of '%s' %s", 
						       pname, param->default_string, tname);
				fail (str);
			}
			else if (value) {
				if (param->default_gvalue) {
					if (G_VALUE_TYPE (value) != G_VALUE_TYPE (param->default_gvalue)) {
						str = g_strdup_printf ("Expecting default value of type '%s' for parameter '%s', "
								       "got it of type '%s' %s", 
								       g_type_name (G_VALUE_TYPE (param->default_gvalue)),
								       pname,
								       g_type_name (G_VALUE_TYPE (value)), tname);
						fail (str);
					}
					if (gda_value_compare (value, param->default_gvalue)) {
						str = g_strdup_printf ("Expecting default value '%s' for parameter '%s', "
								       "got '%s' %s",
								       gda_value_stringify (param->default_gvalue), pname, 
								       gda_value_stringify (value), tname);
						fail (str);
					}
				}
				else {
					gchar *dstr = gda_value_stringify (value);
					if (strcmp (dstr, param->default_string)) {
						str = g_strdup_printf ("Expecting default value '%s' for parameter '%s', "
								       "got '%s' %s",
								       param->default_string, pname, dstr, tname);
						fail (str);
					}
					g_free (dstr);
				}
			}
		}
	}

	/* test for missing parameters */
	if (test->params) {
		gchar *str;	
		SqlParam *param = (SqlParam *) (test->params->data);
		if (!test->params->next)
			str = g_strdup_printf ("Expected parameter '%s' %s", param->name, tname);
		else
			str = g_strdup_printf ("Expected parameter '%s' (and %d others) %s", param->name, 
					       g_slist_length (test->params) -1, tname);
		fail (str);
	}

	/* test rendering */
	check_rendering (test, query, tname);

	g_object_unref (query);
}
