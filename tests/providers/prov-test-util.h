#ifndef __PROV_TEST_UTIL_H__
#define __PROV_TEST_UTIL_H__

#include <string.h>
#include <glib.h>
#include <libgda/libgda.h>

gboolean       prov_test_check_types_schema (GdaConnection *cnc);
gboolean       prov_test_load_data (GdaConnection *cnc, const gchar *table);
gboolean       prov_test_check_table_cursor_model (GdaConnection *cnc, const gchar *table);

#endif
