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
#include "../libgda/sql-delimiter/gda-sql-delimiter.h"

TestSuite *current_ts;

static void test_sql_statement (SqlTest *test, gint test_index);

#define TEST_FILENAME "/all_sql_tests.xml"

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

	gda_init ("check-delimiter", PACKAGE_VERSION, argc, argv);
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

	gda_init ("check-delimiter", PACKAGE_VERSION, argc, argv);
	suites = test_suite_load_from_file (TEST_FILENAME);
	for (list = suites; list; list = list->next) {
		TestSuite *ts = (TestSuite *)(list->data);
		for (i = 0; i < ts->unit_files->len; i++) {
			gint j;
			GArray *unit_tests = g_array_index (ts->unit_tests, GArray *, i);
			for (j = 0; j < unit_tests->len; j++)
				test_sql_statement (g_array_index (unit_tests, SqlTest*, j), j);
		}
	}
	return EXIT_SUCCESS;
}
#endif

static gchar *
get_param_attr_from_pspec_list (GList *pspec_list, GdaDelimiterStatementType type)
{
	GList *list;
	for (list = pspec_list; list; list = list->next) {
		GdaDelimiterParamSpec *pspec = (GdaDelimiterParamSpec*) (list->data);
		if (pspec->type == type)
			return pspec->content;
	}
	return NULL;
}

static void
test_sql_statement (SqlTest *test, gint test_index)
{
	GList *statements;
	GError *error = NULL;
	gchar *tname;

	if (test->test_name)
		tname = g_strdup_printf ("(Sql '%s')", test->test_name);
	else
		tname = g_strdup_printf ("(Sql n.%d)", test_index);

	statements = gda_delimiter_parse_with_error (test->sql_to_test, &error);
#ifdef CHECK_SHOW_ALL_ERRORS
	if (!statements && error)
		g_print ("PARSE ERROR %s=>%s\n", tname, error->message ? error->message: "No detail");
#endif
	if (g_list_length (statements) != (test->delim_parsed ? test->n_statements : 0)) {
		gchar *str;

		str = g_strdup_printf ("Recognized %d statements when %d are expected %s", 
				       g_list_length (statements), test->delim_parsed ? test->n_statements : 0, tname);
		fail (str);
	}

	if (test->delim_parsed && !statements && error) {
		gchar *str;

		str = g_strdup_printf ("Cannot parse SQL %s", tname);
#ifdef CHECK_EXTRA_INFO
		if (!statements && error)
			g_print ("PARSE ERROR %s=>%s\n", tname, error->message ? error->message: "No detail");
#endif
		fail (str);
	}
	else if (!test->delim_parsed && statements) {
		gchar *str;

		str = g_strdup_printf ("Should not parse SQL %s", tname);
		fail (str);
	}
	else if (statements) {
		GdaDelimiterStatement *concat;
		gchar *str;
		GList *params;

		concat = gda_delimiter_concat_list (statements);

		/* test parsed parameters */
		for (params = concat->params_specs; params; params = params->next) {
			gchar *attr, *pname;
			SqlParam *param;
			gboolean abool;

			/* fetch declared param */
			pname = get_param_attr_from_pspec_list ((GList *)(params->data), GDA_DELIMITER_PARAM_NAME);
			if (!pname) {
				/* unnamed parameter is allowed as long as there is only one expected unnamed parameter */
				str = g_strdup_printf ("unexpected unnamed parameter %s", tname);
				fail_unless (test->params && !test->params->next && !((SqlParam *)(test->params->data))->name,
					     str);
			}
			param = sql_tests_take_param (test, pname);
			if (!param) {
				str = g_strdup_printf ("Extra parameter '%s' %s", pname, tname);
				fail (str);
			}
			else {
				/* compare param's type */
				attr = get_param_attr_from_pspec_list ((GList *)(params->data), GDA_DELIMITER_PARAM_TYPE);
				if (param->type && !attr) {
					str = g_strdup_printf ("Parameter '%s' should have a type %s", pname, tname);
					fail (str);
				}
				else if (!param->type && attr) {
					str = g_strdup_printf ("Parameter '%s' should not have a type %s", pname, tname);
					fail (str);
				}
				else if (param->type && strcmp (param->type, attr)) {
					str = g_strdup_printf ("Parameter '%s' has a wrong type %s", pname, tname);
					fail (str);
				}
				/* compare param's description */
				attr = get_param_attr_from_pspec_list ((GList *)(params->data), GDA_DELIMITER_PARAM_DESCR);
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
				attr = get_param_attr_from_pspec_list ((GList *)(params->data), GDA_DELIMITER_PARAM_NULLOK);
				abool = !attr || (attr && ((*attr == 't') || (*attr == 'T'))) ? TRUE : FALSE;
				if (param->nullok && !abool) {
					str = g_strdup_printf ("Parameter '%s' should have NULLOK %s", pname, tname);
					fail (str);
				}
				else if (!param->nullok && abool) {
					str = g_strdup_printf ("Parameter '%s' should not have NULLOK %s", pname, tname);
					fail (str);
				}
				/* compare param's ISPARAM */
				attr = get_param_attr_from_pspec_list ((GList *)(params->data), GDA_DELIMITER_PARAM_ISPARAM);
				abool = attr && ((*attr == 't') || (*attr == 'T')) ? TRUE : FALSE;
				if (param->isparam && !abool) {
					str = g_strdup_printf ("Parameter '%s' should have ISPARAM %s", pname, tname);
					fail (str);
				}
				else if (!param->isparam && abool) {
					str = g_strdup_printf ("Parameter '%s' should not have ISPARAM %s", pname, tname);
					fail (str);
				}
				/* compare default values */
				attr = get_param_attr_from_pspec_list ((GList *)(params->data), GDA_DELIMITER_PARAM_DEFAULT);
				if (attr && !param->default_string) {
					str = g_strdup_printf ("Parameter '%s' should not have a default value %s", 
							       pname, tname);
					fail (str);
				}
				else if (param->default_string && !attr) {
					str = g_strdup_printf ("Parameter '%s' should have a default value of '%s' %s", 
							       pname, param->default_string, tname);
					fail (str);
				}
				else if (attr) {
					if (strcmp (attr, param->default_string)) {
						str = g_strdup_printf ("Expecting default value '%s' for parameter '%s', "
								       "got '%s' %s",
								       param->default_string, pname, attr, tname);
						fail (str);
					}
				}
				sql_param_free (param);
			}
		}

		/* test for missing parameters */
		if (test->params) {
			SqlParam *param = (SqlParam *) (test->params->data);
			if (!test->params->next)
				str = g_strdup_printf ("Expected parameter '%s' %s", param->name, tname);
			else
				str = g_strdup_printf ("Expected parameter '%s' (and %d others) %s", param->name, 
						       g_slist_length (test->params) -1, tname);
			fail (str);
		}
		else {
			/* test rendering */
			str = gda_delimiter_to_string (concat);
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

		gda_delimiter_destroy (concat);
		gda_delimiter_free_list (statements);
	}
}
