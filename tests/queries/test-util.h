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

typedef enum {
	/* dictionary presence */
	TEST_RENDERING_WHEN_DICT = 1 << 0,
	TEST_RENDERING_WHEN_NO_DICT = 1 << 1,
	TEST_RENDERING_WHEN_ANY_DICT = TEST_RENDERING_WHEN_DICT | TEST_RENDERING_WHEN_NO_DICT,

	/* parsing status */
	TEST_RENDERING_SQL_DELIMITER = 1 << 2,
	TEST_RENDERING_SQL_PARSER = 1 << 3,
	TEST_RENDERING_SQL_ACTIVE = 1 << 4,
	TEST_RENDERING_SQL_ANY = TEST_RENDERING_SQL_DELIMITER | TEST_RENDERING_SQL_PARSER | TEST_RENDERING_SQL_ACTIVE,

	/* GdaQuery or gda_delimiter context */
	TEST_RENDERING_GDA_QUERY = 1 << 5,
	TEST_RENDERING_GDA_DELIMITER = 1 << 6,
	TEST_RENDERING_GDA_ANY = TEST_RENDERING_GDA_DELIMITER | TEST_RENDERING_GDA_QUERY
} SqlRenderingAttr;

typedef struct {
	SqlRenderingAttr attrs;
	gchar           *sql;
} SqlRendering;

typedef struct {
	TestSuite *suite;
	gchar     *test_name;
	gchar     *sql_to_test;

	gboolean   delim_parsed; /* TRUE if the delimiter can work on the statement */
	gboolean   sql_parsed;   /* TRUE if the SQL parser (libsql) can work on the statement */
	gboolean   sql_active;   /* TRUE if the parsed query must be active (if there is a dictionary) */

	GArray    *renderings; /* array of SqlRendering* */
	GSList    *params; /* list of @SqlParam structs */
	gint       n_statements;
} SqlTest;


SqlRendering *sql_test_rendering_new (SqlRenderingAttr attrs, const gchar *sql);
void          sql_test_rendering_free (SqlRendering *ren);

GArray   *sql_tests_load_from_file (const gchar *xml_file);
SqlParam *sql_tests_take_param (SqlTest *test, const gchar *pname);
void      sql_param_free (SqlParam *param);
gboolean  sql_test_rendering_is_correct (SqlTest *test, const gchar *sql);
