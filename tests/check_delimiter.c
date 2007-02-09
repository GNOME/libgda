#include <stdlib.h>
#include <string.h>

#ifdef HAVE_CHECK
#include <check.h>
#else
#define fail(x) g_warning (x)
#define fail_if(x,y) if (x) g_warning (y)
#define fail_unless(x,y) if (!(x)) g_warning (y)
#endif

#include "test-util.h"
#include "../libgda/sql-delimiter/gda-sql-delimiter.h"

GArray *basic_tests;

static void test_sql_statement (SqlTest *test, gint test_index);

#ifdef HAVE_CHECK
/*
 * The Check library is available
 */
START_TEST (basic_test_delimiter)
{
	test_sql_statement (g_array_index (basic_tests, SqlTest*, _i), _i);
}
END_TEST

Suite *
delimiter_suite (void)
{
	basic_tests = sql_tests_load_from_file ("basic_sql.xml");

	Suite *s = suite_create ("Gda Delimiter");

	/* Core test case */
	TCase *tc_core = tcase_create ("Basic");
	tcase_add_loop_test (tc_core, basic_test_delimiter, 0, basic_tests->len);
	suite_add_tcase (s, tc_core);
	
	return s;
}

int
main (void)
{
	int number_failed;
	Suite *s = delimiter_suite ();
	SRunner *sr = srunner_create (s);
	srunner_run_all (sr, CK_NORMAL);
	number_failed = srunner_ntests_failed (sr);
	srunner_free (sr);
	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
#else
/*
 * The Check library is not available
 */
int
main (void)
{
	gint i;
	basic_tests = sql_tests_load_from_file ("basic_sql.xml");
	for (i = 0; i < basic_tests->len; i++)
		test_sql_statement (g_array_index (basic_tests, SqlTest*, i), i);
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
	if (test->parsed && !statements) {
		gchar *str;

		str = g_strdup_printf ("Cannot parse SQL %s", tname);
		fail (str);
	}
	else if (!test->parsed && statements) {
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
				abool = attr && ((*attr == 't') || (*attr == 'T')) ? TRUE : FALSE;
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
			if (strcmp (test->sql_rendered ? test->sql_rendered : test->sql_to_test, str)) {
				gchar *str2;
#ifndef HAVE_CHECK
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
