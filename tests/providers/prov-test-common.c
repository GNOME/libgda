#include <stdlib.h>
#include <string.h>
#include "prov-test-common.h"
#include "prov-test-util.h"

GdaProviderInfo *pinfo;
GdaConnection   *cnc;
gboolean         params_provided;
gboolean         db_created;


/*
 *
 * SETUP
 *
 */
#ifdef HAVE_CHECK
START_TEST (setup_cnc_test)
{
	cnc = prov_test_setup_connection (pinfo, &params_provided, &db_created);
	if (params_provided)
		fail_if (!cnc);
}
END_TEST

Suite *
setup_suite (void)
{
	Suite *s = suite_create ("Setup");
	
	/* Core test case */
	TCase *tc_core = tcase_create ("Open connection");
	tcase_set_timeout (tc_core, 10);
	tcase_add_test (tc_core, setup_cnc_test);
	suite_add_tcase (s, tc_core);
	
	return s;
}
#endif

int
prov_test_common_setup ()
{
	int number_failed = 0;
#ifdef HAVE_CHECK
	Suite *s = setup_suite ();
	SRunner *sr = srunner_create (s);
	srunner_set_fork_status (sr, CK_NOFORK); /* run in NO_FORK because we want to use @cnc and @params_provided later */
	srunner_run_all (sr, CK_NORMAL);
	number_failed = srunner_ntests_failed (sr);
	srunner_free (sr);
#else
	cnc = prov_test_setup_connection (pinfo, &params_provided, &db_created);
	if (params_provided)
		fail_if (!cnc, "Could not setup connection");
#endif
	return number_failed;
}


/*
 *
 * CLEAN
 *
 */
#ifdef HAVE_CHECK
START_TEST (clean_cnc_test)
{
	if (!prov_test_clean_connection (cnc, db_created))
		fail ("Could not clean test database");

}
END_TEST

Suite *
clean_suite (void)
{
	Suite *s = suite_create ("Clean");
	
	/* Core test case */
	TCase *tc_core = tcase_create ("Clean connection");
	tcase_set_timeout (tc_core, 10);
	tcase_add_test (tc_core, clean_cnc_test);
	suite_add_tcase (s, tc_core);
	
	return s;
}
#endif

int
prov_test_common_clean ()
{
	int number_failed = 0;

#ifdef HAVE_CHECK
	Suite *s = clean_suite ();
	SRunner *sr = srunner_create (s);
	
	srunner_run_all (sr, CK_NORMAL);
	number_failed = srunner_ntests_failed (sr);
	srunner_free (sr);
#else
	if (!prov_test_clean_connection (cnc, db_created))
		number_failed++;
#endif

	return number_failed;	
}

/*
 *
 * CREATE_TABLES_SQL
 *
 */
#ifdef HAVE_CHECK
START_TEST (create_tables_sql)
{
	if (!prov_test_create_tables_sql (cnc))
		fail ("Could not create tables from SQL");

}
END_TEST

Suite *
create_tables_sql_suite (void)
{
	Suite *s = suite_create ("Create tables");
	
	/* Core test case */
	TCase *tc_core = tcase_create ("Create tables from SQL");
	tcase_set_timeout (tc_core, 10);
	tcase_add_test (tc_core, create_tables_sql);
	suite_add_tcase (s, tc_core);
	
	return s;
}
#endif

int
prov_test_common_create_tables_sql ()
{
	int number_failed = 0;
#ifdef HAVE_CHECK
	Suite *s = create_tables_sql_suite ();
	SRunner *sr = srunner_create (s);
	
	srunner_run_all (sr, CK_NORMAL);
	number_failed = srunner_ntests_failed (sr);
	srunner_free (sr);
#else
	if (!prov_test_create_tables_sql (cnc))
		number_failed++;
#endif

	return number_failed;	
}

/*
 *
 * SHECK_SCHEMAS
 *
 */
#ifdef HAVE_CHECK
START_TEST (check_table_schemas)
{
	if (!prov_test_check_table_schema (cnc, "actor"))
		fail ("Reported schemas failed for table 'actor'");
	if (!prov_test_check_table_schema (cnc, "language"))
		fail ("Reported schemas failed for table 'language'");
	if (!prov_test_check_table_schema (cnc, "film"))
		fail ("Reported schemas failed for table 'film'");
	if (!prov_test_check_table_schema (cnc, "film_actor"))
		fail ("Reported schemas failed for table 'film_actor'");
}
END_TEST

START_TEST (check_types_schemas)
{
	if (!prov_test_check_types_schema (cnc))
		fail ("Reported schemas failed for types");
}
END_TEST

Suite *
check_schemas_suite (void)
{
	Suite *s = suite_create ("Check reported schemas");
	
	/* Core test case */
	TCase *tc_core = tcase_create ("Check tables schemas");
	tcase_set_timeout (tc_core, 10);
	tcase_add_test (tc_core, check_table_schemas);
	tcase_add_test (tc_core, check_types_schemas);
	suite_add_tcase (s, tc_core);
	
	return s;
}
#endif

int
prov_test_common_check_schemas ()
{
	int number_failed = 0;
#ifdef HAVE_CHECK
	Suite *s = check_schemas_suite ();
	SRunner *sr = srunner_create (s);
	
	srunner_run_all (sr, CK_NORMAL);
	number_failed = srunner_ntests_failed (sr);
	srunner_free (sr);
#else
	if (!prov_test_check_table_schema (cnc, "actor"))
		number_failed++;
	if (!prov_test_check_table_schema (cnc, "language"))
		number_failed++;
	if (!prov_test_check_table_schema (cnc, "film"))
		number_failed++;
	if (!prov_test_check_table_schema (cnc, "film_actor"))
		number_failed++;
	if (!prov_test_check_types_schema (cnc))
		number_failed++;
#endif

	return number_failed;	
}
