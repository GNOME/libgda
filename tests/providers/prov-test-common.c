#include <stdlib.h>
#include <string.h>
#include "prov-test-common.h"
#include "prov-test-util.h"

GdaProviderInfo *pinfo;
GdaConnection   *cnc;
gboolean         params_provided;
gboolean         db_created;
gboolean         fork_tests = TRUE;

/*
 *
 * SETUP
 *
 */

int
prov_test_common_setup ()
{
	int number_failed = 0;
	cnc = prov_test_setup_connection (pinfo, &params_provided, &db_created);
	if (params_provided)
		fail_if (!cnc, "Could not setup connection");
	return number_failed;
}


/*
 *
 * CLEAN
 *
 */
int
prov_test_common_clean ()
{
	int number_failed = 0;

	if (!prov_test_clean_connection (cnc, db_created))
		number_failed++;

	return number_failed;	
}

/*
 *
 * CREATE_TABLES_SQL
 *
 */
int
prov_test_common_create_tables_sql ()
{
	int number_failed = 0;
	if (!prov_test_create_tables_sql (cnc))
		number_failed++;

	return number_failed;	
}

/*
 *
 * CHECK_SCHEMAS
 *
 */

int
prov_test_common_check_schemas ()
{
	int number_failed = 0;
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

	return number_failed;	
}

/*
 *
 * LOAD_DATA
 *
 */
int
prov_test_common_load_data ()
{
	int number_failed = 0;
	if (!prov_test_load_data (cnc, "actor"))
		number_failed++;
	if (!prov_test_load_data (cnc, "language"))
		number_failed++;
	if (!prov_test_load_data (cnc, "film"))
		number_failed++;
	if (!prov_test_load_data (cnc, "film_actor"))
		number_failed++;

	return number_failed;	
}

/*
 *
 * CHECK_CURSOR_MODELS
 *
 */
int
prov_test_common_check_cursor_models ()
{
	int number_failed = 0;
	if (!prov_test_check_table_cursor_model (cnc, "actor"))
		number_failed++;
	if (!prov_test_check_table_cursor_model (cnc, "language"))
		number_failed++;
	if (!prov_test_check_table_cursor_model (cnc, "film"))
		number_failed++;
	if (!prov_test_check_table_cursor_model (cnc, "film_actor"))
		number_failed++;
	if (!prov_test_check_types_schema (cnc))
		number_failed++;

	return number_failed;	
}
