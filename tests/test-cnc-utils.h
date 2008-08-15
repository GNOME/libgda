#ifndef __TEST_CNC_UTIL_H__
#define __TEST_CNC_UTIL_H__

#include <string.h>
#include <glib.h>
#include <libgda/libgda.h>

GdaConnection *test_cnc_setup_connection (const gchar *provider, const gchar *dbname, GError **error);
gboolean       test_cnc_setup_db_structure (GdaConnection *cnc, const gchar *schema_file, GError **error);
gboolean       test_cnc_setup_db_contents (GdaConnection *cnc, const gchar *data_file, GError **error);
gboolean       test_cnc_clean_connection (GdaConnection *cnc, GError **error);

gboolean       test_cnc_load_data_from_file (GdaConnection *cnc, const gchar *table, const gchar *full_file, GError **error);

#endif
