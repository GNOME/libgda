#include <string.h>
#include <glib.h>
#include <libxml/tree.h>
#include <libgda/libgda.h>

typedef struct {
	gchar   *name;
	GdaDict *dict;
	GArray  *unit_files;
	GArray  *unit_tests; /* an array of GArray of SqlTest */
} TestSuite;

GSList *test_suite_load_from_file (const gchar *xml_file); /* returns a list of TestSuite structs. */

typedef struct {
	gchar    *name;
	gchar    *type;
	gchar    *descr;
	gboolean  nullok;
	gboolean  isparam;
	gchar    *default_string;
	GValue   *default_gvalue;
} SqlParam;

typedef struct {
	gchar    *test_name;
	gchar    *sql_to_test;

	gboolean  delim_parsed; /* TRUE if the delimiter can work on the statement */
	gboolean  sql_parsed;   /* TRUE if the SQL parser (libsql) can work on the statement */
	gboolean  sql_active;   /* TRUE if the parsed query must be active (if there is a dictionary) */

	GArray   *renderings; /* array of gchar* */
	GSList   *params; /* list of @SqlParam structs */
	gint      n_statements;
} SqlTest;

GArray   *sql_tests_load_from_file (const gchar *xml_file);
SqlParam *sql_tests_take_param (SqlTest *test, const gchar *pname);
void      sql_param_free (SqlParam *param);
gboolean  sql_test_rendering_is_correct (SqlTest *test, const gchar *sql);
