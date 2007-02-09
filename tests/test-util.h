#include <string.h>
#include <glib.h>
#include <libxml/tree.h>

typedef struct {
	gchar    *name;
	gchar    *type;
	gchar    *descr;
	gboolean  nullok;
	gboolean  isparam;
} SqlParam;

typedef struct {
	gchar    *test_name;
	gchar    *sql_to_test;
	gboolean  parsed;
	gchar    *sql_rendered;
	GSList   *params; /* list of @SqlParam structs */
} SqlTest;

GArray   *sql_tests_load_from_file (const gchar *xml_file);
SqlParam *sql_tests_take_param (SqlTest *test, const gchar *pname);
void      sql_param_free (SqlParam *param);
