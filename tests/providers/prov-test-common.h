#ifndef __PROV_TEST_COMMON_H__
#define __PROV_TEST_COMMON_H__

#include <string.h>
#include <glib.h>
#include <libgda/libgda.h>
#include "prov-test-util.h"

#define fail(x) g_warning (x)
#define fail_if(x,y) if (x) g_warning (y)
#define fail_unless(x,y) if (!(x)) g_warning (y)

int prov_test_common_setup ();
int prov_test_common_create_tables_sql ();
int prov_test_common_load_data ();
int prov_test_common_check_meta ();
int prov_test_common_check_cursor_models ();
int prov_test_common_clean ();

#endif
