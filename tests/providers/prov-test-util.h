#ifndef __PROV_TEST_UTIL_H__
#define __PROV_TEST_UTIL_H__

#include <string.h>
#include <glib.h>
#include <libgda/libgda.h>

/*
 * Uses env variables to
 *    - create a DB if env. variable <upper_case_provider_name>_DBCREATE_PARAMS exists
 *    - open a connection if env. variable <upper_case_provider_name>_CNC_PARAMS exists
 *
 * If for the @prov_info, those env. variables don't exist, then @params_provided is set to FALSE
 */
GdaConnection *prov_test_setup_connection (GdaProviderInfo *prov_info, gboolean *params_provided, gboolean *db_created);
gboolean       prov_test_clean_connection (GdaConnection *cnc, gboolean destroy_db);
gboolean       prov_test_create_tables_sql (GdaConnection *cnc);

#endif
