/*
 * Copyright (C) 2008 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include "common.h"
#include <sql-parser/gda-sql-parser.h>
#include "../test-cnc-utils.h"

/* list of changes expected during a modify operation */
GSList *expected_changes;

static void meta_changed_cb (GdaMetaStore *store, GSList *changes, gpointer data);
static GError *suggest_update_cb (GdaMetaStore *store, GdaMetaContext *context, gpointer data);
gchar *dbname = NULL;
static GdaQuarkList *quark_list = NULL;
/*
 * Declare a GdaMetaStore to test
 */
void
common_declare_meta_store (GdaMetaStore *store)
{
	g_signal_connect (store, "meta-changed", G_CALLBACK (meta_changed_cb), NULL);
	g_signal_connect (store, "suggest-update", G_CALLBACK (suggest_update_cb), NULL);
}

typedef struct {
	const gchar  *key;
	const GValue *value;
} HData;

static gint
hd_compare_func (const HData *v1, const HData *v2)
{
	return strcmp (v1->key, v2->key);
}

static void
change_key_func (const gchar *key, const GValue *value, GSList **list)
{
	HData *hd;
	hd = g_new (HData, 1);
	hd->key = key;
	hd->value = value;
	*list = g_slist_insert_sorted (*list, hd, (GCompareFunc) hd_compare_func);
}

static gchar *
stringify_a_change (GdaMetaStoreChange *ac)
{
	GSList *hlist = NULL; /* will be a list of HData pointers */
	gchar *str;
	GString *string;

	string = g_string_new (gda_meta_store_change_get_table_name (ac));
	if (gda_meta_store_change_get_change_type (ac) == GDA_META_STORE_ADD)
		g_string_append (string, " (ADD)");
	else if (gda_meta_store_change_get_change_type (ac) == GDA_META_STORE_MODIFY)
		g_string_append (string, " (MOD)");
	else
		g_string_append (string, " (DEL)");
	g_string_append_c (string, '\n');
	g_hash_table_foreach (gda_meta_store_change_get_keys (ac), (GHFunc) change_key_func, &hlist);
	
	GSList *list;
	for (list = hlist; list; list = list->next) {
		HData *hd = (HData*) list->data;
		gchar *str = gda_value_stringify (hd->value);
		g_string_append_printf (string, "\t%s => %s\n", hd->key, str);
		g_free (str);
		g_free (hd);
	}

	str = g_string_free (string, FALSE);
	return str;
}

static gboolean
find_expected_change (const gchar *change_as_str)
{
	GSList *el;
	for (el = expected_changes; el; el = el->next) {
		gchar *estr = (gchar *) el->data;
		if (!g_strcmp0 (estr, change_as_str)) {
			g_free (estr);
			expected_changes = g_slist_delete_link (expected_changes, el);
			return TRUE;
		}
	}
	return FALSE;
}

static void
meta_changed_cb (G_GNUC_UNUSED GdaMetaStore *store, GSList *changes, G_GNUC_UNUSED gpointer data)
{
	GSList *gl;
	for (gl = changes; gl; gl = gl->next) {
		gchar *gstr = stringify_a_change ((GdaMetaStoreChange *) gl->data);
		if (!find_expected_change (gstr)) {
			g_print ("Unexpected GdaMetaStoreChange: %s", gstr);
			g_free (gstr);
			if (expected_changes) {
				gchar *estr = (gchar *) expected_changes->data;
				g_print ("Expected: %s\n", estr);
			}
			else
				g_print ("No change expected\n");
			exit (EXIT_FAILURE);
		}
		g_free (gstr);
	}
	if (expected_changes) {
		/* expected more changes */
		gchar *estr = (gchar *) expected_changes->data;
		g_print ("Received no change but EXPECTED GdaMetaStoreChange: %s", estr);
		exit (EXIT_FAILURE);
	}
}

static GError *
suggest_update_cb (G_GNUC_UNUSED GdaMetaStore *store, GdaMetaContext *context, G_GNUC_UNUSED gpointer data)
{
	gint i;
	g_print ("test: Update suggested for table %s:\n", context->table_name);
	for (i = 0; i < context->size; i++) {
		gchar *str;
		str = gda_value_stringify (context->column_values[i]);
		g_print ("\t%s => %s\n", context->column_names[i], str);
		g_free (str);
	}
	return NULL;
}

/*
 * Loading a CSV file
 * ... is a (-1 terminated) list of pairs composed of:
 *   - a column number (gint)
 *   - the column type (gchar *)
 */
GdaDataModel *
common_load_csv_file (const gchar *data_file, ...)
{
	GdaDataModel *model;
	GdaSet *options;
	va_list args;
	gchar *fname;
	gint cnum;
	
	/* create options */
	options = gda_set_new (NULL);
	va_start (args, data_file);
	for (cnum = va_arg (args, gint); cnum >= 0; cnum= va_arg (args, gint)) {
		GdaHolder *holder;
		GValue *v;
		gchar *id;

		id = g_strdup_printf ("G_TYPE_%d", cnum);
		holder = gda_holder_new (G_TYPE_GTYPE, id);
		g_free (id);
		
		v = gda_value_new (G_TYPE_GTYPE);
		g_value_set_gtype (v, gda_g_type_from_string (va_arg (args, gchar*)));
		if (! gda_holder_take_value (holder, v, NULL)) {
			va_end (args);
			g_object_unref (holder);
			g_object_unref (options);
			return NULL;
		}
		
		gda_set_add_holder (options, holder);
		g_object_unref (holder);
	}
	va_end (args);
	
	/* load file */
	fname = g_build_filename (TOP_SRC_DIR, "tests", "meta-store", data_file, NULL);
	model = gda_data_model_import_new_file (fname, TRUE, options);
	g_object_unref (options);
	
	GSList *errors;
	if ((errors = gda_data_model_import_get_errors (GDA_DATA_MODEL_IMPORT (model)))) {
		g_print ("Error importing file '%s':\n", fname);
		for (; errors; errors = errors->next) {
			GError *error = (GError*) errors->data;
			g_print ("\t* %s\n", error && error->message ? error->message : "No detail");
		}
		g_object_unref (model);
		model = NULL;
		exit (1);
	}
	g_free (fname);
	
	return model;
}


/*
 * Declaring an expected GdaMetaStoreChange change
 * ... is a NULL terminated list of:
 *    - para name (gchar*)
 *    - param value (gchar *)
 */
void
common_declare_expected_change (const gchar *table_name, GdaMetaStoreChangeType type, ...)
{
	if (!table_name) {
		/* clear any remaining changes not handled */
		if (expected_changes) {
			/* expected more changes */
			gchar *estr = (gchar *) expected_changes->data;
			g_print ("Received no change but EXPECTED GdaMetaStoreChange: %s", estr);
			exit (EXIT_FAILURE);
		}
		g_slist_free_full (expected_changes, (GDestroyNotify) g_free);
		expected_changes = NULL;
	}
	else {
		GdaMetaStoreChange *ac;
		ac = gda_meta_store_change_new ();
		gda_meta_store_change_set_change_type (ac, type);
		gda_meta_store_change_set_table_name (ac, table_name);

		/* use args */
		va_list args;
		gchar *pname;
		va_start (args, type);
		for (pname = va_arg (args, gchar*); pname; pname = va_arg (args, gchar*)) {
			GValue *v;
			g_value_set_string ((v = gda_value_new (G_TYPE_STRING)), va_arg (args, gchar*));
			g_hash_table_insert (gda_meta_store_change_get_keys (ac), g_strdup (pname), v);
		}
		va_end (args);

		gchar *str;
		str = stringify_a_change (ac);
		expected_changes = g_slist_append (expected_changes, str);
		gda_meta_store_change_free (ac);
	}
}

void
common_declare_expected_insertions_from_model (const gchar *table_name, GdaDataModel *model)
{
	gint ncols, nrows, i, j;
	ncols = gda_data_model_get_n_columns (model);
	nrows = gda_data_model_get_n_rows (model);
	for (i = 0; i < nrows; i++) {
		GdaMetaStoreChange *ac;
		ac = gda_meta_store_change_new ();
		gda_meta_store_change_set_change_type (ac, GDA_META_STORE_ADD);
		gda_meta_store_change_set_table_name (ac, table_name);

		for (j = 0; j < ncols; j++) {
			gchar *str;
			const GValue *value;
			GError *lerror = NULL;
			str = g_strdup_printf ("+%d", j);
			value = gda_data_model_get_value_at (model, j, i, &lerror);
			if (!value) {
				g_print ("Can't get data model's value: %s",
					 lerror && lerror->message ? lerror->message : "No detail");
				exit (1);
			}
			g_hash_table_insert (gda_meta_store_change_get_keys (ac), str, gda_value_copy (value));
		}

		gchar *str;
		str = stringify_a_change (ac);
		expected_changes = g_slist_append (expected_changes, str);
		gda_meta_store_change_free (ac);
	}
}


/*
 * Clean all tables in connection
 */
void
common_drop_all_tables (GdaMetaStore *store)
{
	gchar *table_names [] = {
		"_attributes",
		"_information_schema_catalog_name",
		"_schemata",
		"_builtin_data_types",
		"_udt",
		"_udt_columns",
		"_enums",
		"_element_types",
		"_domains",
		"_tables",
		"_views",
		"_collations",
		"_character_sets",
		"_routines",
		"_triggers",
		"_columns",
		"_table_constraints",
		"_referential_constraints",
		"_key_column_usage",
		"_check_column_usage",
		"_view_column_usage",
		"_domain_constraints",
		"_parameters",
		"_routine_columns",
		"_table_indexes",
		"_index_column_usage",
		"__declared_fk"
	};
	gchar *view_names [] = {
		"_all_types",
		"_detailed_fk"
	};
	
	GdaConnection *cnc = gda_meta_store_get_internal_connection (store);
	GdaSqlParser *parser;
	parser = gda_sql_parser_new ();
	
	gint i;
		
	/* drop views */
	for (i = (sizeof (view_names) / sizeof (gchar*)) - 1 ; i >= 0; i--) {
		GdaStatement *stmt;
		gchar *sql;
		gint res;
		GError *error = NULL;

		sql = g_strdup_printf ("DROP VIEW IF EXISTS %s", view_names[i]);
		stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
		g_free (sql);
		res = gda_connection_statement_execute_non_select (cnc, stmt, NULL, NULL, &error);
		if (res == -1) {
			g_print ("DROP VIEW IF EXISTS'%s' error: %s\n", view_names[i],
				error && error->message ? error->message : "No detail");
			if (error)
				g_error_free (error);
		}
	}
	
	/* drop tables */
	for (i = (sizeof (table_names) / sizeof (gchar*)) - 1 ; i >= 0; i--) {
		GdaStatement *stmt;
		gchar *sql;
		gint res;
		GError *error = NULL;
		
		sql = g_strdup_printf ("DROP TABLE IF EXISTS %s", table_names[i]);
		stmt = gda_sql_parser_parse_string (parser, sql, NULL, NULL);
		g_free (sql);
		res = gda_connection_statement_execute_non_select (cnc, stmt, NULL, NULL, &error);
		if (res == -1) {
			g_print ("DROP TABLE IF EXISTS '%s' error: %s\n", table_names[i],
				error && error->message ? error->message : "No detail");
			if (error)
				g_error_free (error);
		}
	}
}

/*
 *
 * Test groups
 *
 */
void
tests_group_1 (GdaMetaStore *store)
{
	common_declare_meta_store (store);

	test_information_schema_catalog_name (store);
	test_schemata_1 (store);
	test_schemata_2 (store);
	test_builtin_data_types (store);
	//test_domains (store);
	test_tables (store);
	test_views (store);
	test_routines (store);
	test_triggers (store);
	//test_columns (store);
	test_table_constraints (store);
	test_referential_constraints (store);
	/*test_key_column_usage (store);*/
	test_domain_constraints (store);
	test_parameters (store);
}


/*
 *
 * Test groups
 *
 * Apply Group 2 for MySQL provider
 *
 */
void
tests_group_2 (GdaMetaStore *store)
{
	common_declare_meta_store (store);

	test_information_schema_catalog_name (store);
	test_schemata_1 (store);
	test_schemata_2 (store);
	//test_builtin_data_types (store);
	//test_domains (store);
	test_tables (store);
	test_views (store);
	//test_routines (store);
	test_triggers (store);
	//test_columns (store);
	test_table_constraints (store);
	test_referential_constraints (store);
	/*test_key_column_usage (store);*/
	test_domain_constraints (store);
	test_parameters (store);
}

/*
 *
 *
 * Individual Tests
 *
 *
 */
GdaDataModel *import;
GError *error = NULL;
	
#define TEST_HEADER g_print ("... TEST '%s' ...\n", __FUNCTION__)
#define TEST_MODIFY(s,n,m,c,e,...) \
if (!gda_meta_store_modify ((s),(n),(m),(c),(e),__VA_ARGS__)) { \
		g_print ("Error while modifying GdaMetaStore, table '%s': %s\n", \
			 (n), error && error->message ? error->message : "No detail"); \
		exit (EXIT_FAILURE); \
	}

#define DECL_CHANGE(n,t,...) common_declare_expected_change ((n),(t),__VA_ARGS__)
#define TEST_END(m) \
	if (m) g_object_unref (m); \
	common_declare_expected_change (NULL, GDA_META_STORE_ADD, -1)
	
void
test_information_schema_catalog_name (GdaMetaStore *store)
{
#define TNAME "_information_schema_catalog_name"	
	TEST_HEADER;

	import = common_load_csv_file ("data_information_schema_catalog_name.csv", -1);
	DECL_CHANGE (TNAME, GDA_META_STORE_ADD, "+0", "meta", NULL);
	TEST_MODIFY (store, TNAME, import, NULL, &error, NULL);
	TEST_END (import);
#undef TNAME
}

void
test_schemata_1 (GdaMetaStore *store)
{
#define TNAME "_schemata"
	TEST_HEADER;

	import = common_load_csv_file ("data_schemata.csv", 3, "gboolean", 4, "gboolean", -1);
	DECL_CHANGE (TNAME, GDA_META_STORE_ADD, "+0", "meta", "+1", "pg_catalog", "+2", "a user", "+3", "TRUE", "+4", "FALSE", NULL);
	DECL_CHANGE (TNAME, GDA_META_STORE_ADD, "+0", "meta", "+1", "pub", "+2", "postgres", "+3", "FALSE", "+4", "FALSE", NULL);
	DECL_CHANGE (TNAME, GDA_META_STORE_ADD, "+0", "meta", "+1", "information_schema", "+2", "postgres", "+3", "TRUE", "+4", "TRUE", NULL);
	TEST_MODIFY (store, TNAME, import, NULL, &error, NULL);
	TEST_END (import);
#undef TNAME
}

void
test_schemata_2 (GdaMetaStore *store)
{
#define TNAME "_schemata"
	TEST_HEADER;

	import = common_load_csv_file ("data_schemata_1.csv", 3, "gboolean", 4, "gboolean", -1);	
	DECL_CHANGE (TNAME, GDA_META_STORE_MODIFY, "-0", "meta", "-1", "pg_catalog", "+0", "meta", "+1", "pg_catalog", "+2", "postgres", "+3", "TRUE", "+4", "FALSE", NULL);
	
	GValue *v1, *v2;
	g_value_set_string (v1 = gda_value_new (G_TYPE_STRING), "meta");
	g_value_set_string (v2 = gda_value_new (G_TYPE_STRING), "pg_catalog");
	TEST_MODIFY (store, TNAME, import, 
		"catalog_name=##cn::string AND schema_name=##sn::string", &error,
		"cn", v1, "sn", v2, NULL);
	
	gda_value_free (v1);
	gda_value_free (v2);
	TEST_END (import);
#undef TNAME
}

void
test_builtin_data_types (GdaMetaStore *store)
{
#define TNAME "_builtin_data_types"
	TEST_HEADER;

	import = common_load_csv_file ("data_builtin_data_types.csv", 5, "gboolean", -1);
	common_declare_expected_insertions_from_model (TNAME, import);
	TEST_MODIFY (store, TNAME, import, NULL, &error, NULL);
	TEST_END (import);

	/* remove last line */
	GValue *v1;
	g_value_set_string (v1 = gda_value_new (G_TYPE_STRING), "pg_catalog.to_remove");
	DECL_CHANGE (TNAME, GDA_META_STORE_REMOVE, "-0", "to_remove",
		     "-1", "pg_catalog.to_remove",
		     "-2", "NULL",
		     "-3", "NULL",
		     "-4", "NULL",
		     "-5", "TRUE",
		     NULL);
	TEST_MODIFY (store, TNAME, NULL, 
		     "full_type_name=##ftn::string", &error, 
		     "ftn", v1, NULL);
	gda_value_free (v1);
	TEST_END (NULL);
#undef TNAME
}

void
test_domains (GdaMetaStore *store)
{
#define TNAME "_domains"
	TEST_HEADER;

	/* insert 1st part of the domains */
	import = common_load_csv_file ("data_domains.csv", 5, "gint", 6, "gint", 13, "gint", 14, "gint", 19, "boolean", -1);
	common_declare_expected_insertions_from_model (TNAME, import);
	TEST_MODIFY (store, TNAME, import, NULL, &error, NULL);
	TEST_END (import);

	/* simulate a domains update updating only 1 domain */
	GValue *v1, *v2, *v3;
	g_value_set_string (v1 = gda_value_new (G_TYPE_STRING), "meta");
	g_value_set_string (v2 = gda_value_new (G_TYPE_STRING), "information_schema");
	g_value_set_string (v3 = gda_value_new (G_TYPE_STRING), "sql_identifier");
	import = common_load_csv_file ("data_domains_1.csv", 5, "gint", 6, "gint", 13, "gint", 14, "gint", 19, "boolean", -1);
	common_declare_expected_insertions_from_model (TNAME, import);
	TEST_MODIFY (store, TNAME, import, 
		     "domain_catalog=##dc::string AND domain_schema=##ds::string AND domain_name=##dn::string", &error, 
		     "dc", v1, "ds", v2, "dn", v3, NULL);
	TEST_END (import);
#undef TNAME
}

void
test_tables (GdaMetaStore *store)
{
#define TNAME "_tables"
	TEST_HEADER;

	import = common_load_csv_file ("data_tables.csv", 4, "boolean", -1);
	common_declare_expected_insertions_from_model (TNAME, import);
	TEST_MODIFY (store, TNAME, import, NULL, &error, NULL);
	TEST_END (import);
#undef TNAME
}

void
test_views (GdaMetaStore *store)
{
#define TNAME "_views"
	TEST_HEADER;

	/* load CSV file */
	import = common_load_csv_file ("data_views.csv", 5, "boolean", -1);
	common_declare_expected_insertions_from_model (TNAME, import);
	TEST_MODIFY (store, TNAME, import, NULL, &error, NULL);
	TEST_END (import);

	/* remove some lines */
	DECL_CHANGE (TNAME, GDA_META_STORE_REMOVE, "-0", "meta", "-1", "pg_catalog", "-2", "pg_locks",
		     "-3", "SELECT l.locktype, l.\"database\", l.relation, l.page, l.tuple, l.transactionid, l.classid, l.objid, l.objsubid, l.\"transaction\", l.pid, l.\"mode\", l.\"granted\" FROM pg_lock_status() l(locktype text, \"database\" oid, relation oid, page integer, tuple smallint, transactionid xid, classid oid, objid oid, objsubid smallint, \"transaction\" xid, pid integer, \"mode\" text, \"granted\" boolean);",
		     "-4", "NULL",
		     "-5", "FALSE",
		     NULL);
	DECL_CHANGE (TNAME, GDA_META_STORE_REMOVE, "-0", "meta", "-1", "pg_catalog", "-2", "pg_stats",
		     "-3", "SELECT n.nspname AS schemaname, c.relname AS tablename, a.attname, s.stanullfrac AS null_frac, s.stawidth AS avg_width, s.stadistinct AS n_distinct, CASE 1 WHEN s.stakind1 THEN s.stavalues1 WHEN s.stakind2 THEN s.stavalues2 WHEN s.stakind3 THEN s.stavalues3 WHEN s.stakind4 THEN s.stavalues4 ELSE NULL::anyarray END AS most_common_vals, CASE 1 WHEN s.stakind1 THEN s.stanumbers1 WHEN s.stakind2 THEN s.stanumbers2 WHEN s.stakind3 THEN s.stanumbers3 WHEN s.stakind4 THEN s.stanumbers4 ELSE NULL::real[] END AS most_common_freqs, CASE 2 WHEN s.stakind1 THEN s.stavalues1 WHEN s.stakind2 THEN s.stavalues2 WHEN s.stakind3 THEN s.stavalues3 WHEN s.stakind4 THEN s.stavalues4 ELSE NULL::anyarray END AS histogram_bounds, CASE 3 WHEN s.stakind1 THEN s.stanumbers1[1] WHEN s.stakind2 THEN s.stanumbers2[1] WHEN s.stakind3 THEN s.stanumbers3[1] WHEN s.stakind4 THEN s.stanumbers4[1] ELSE NULL::real END AS correlation FROM pg_statistic s JOIN pg_class c ON c.oid = s.starelid JOIN pg_attribute a ON c.oid = a.attrelid AND a.attnum = s.staattnum LEFT JOIN pg_namespace n ON n.oid = c.relnamespace WHERE has_table_privilege(c.oid, 'select'::text);",
		     "-4", "NULL",
		     "-5", "FALSE",
		     NULL);
	TEST_MODIFY (store, TNAME, NULL, 
		     "table_catalog='meta' AND table_schema='pg_catalog'", &error, NULL);
	TEST_END (NULL);
#undef TNAME
}

void
test_routines (GdaMetaStore *store)
{
#define TNAME "_routines"
	TEST_HEADER;

	/* load CSV file */
	import = common_load_csv_file ("data_routines.csv", 8, "boolean", 9, "gint", 15, "boolean", 17, "boolean", -1);
	common_declare_expected_insertions_from_model (TNAME, import);
	TEST_MODIFY (store, TNAME, import, NULL, &error, NULL);
	TEST_END (import);

	/* remove some lines */
	DECL_CHANGE (TNAME, GDA_META_STORE_REMOVE, "-0", "meta", "-1", "information_schema",
		     "-2", "_pg_numeric_precision_radix_11324",
		     "-3", "meta",
		     "-4", "information_schema",
		     "-5", "_pg_numeric_precision_radix",
		     "-6", "FUNCTION",
		     "-7", "pg_catalog.int4",
		     "-8", "FALSE",
		     "-9", "2",
		     "-10", "SQL",
		     "-11", "SELECT CASE WHEN $1 IN (21, 23, 20, 700, 701) THEN 2 WHEN $1 IN (1700) THEN 10 ELSE null END",
		     "-12", "NULL",
		     "-13", "SQL",
		     "-14", "GENERAL",
		     "-15", "TRUE",
		     "-16", "MODIFIES",
		     "-17", "TRUE",
		     "-18", "NULL",
		     "-19", "information_schema._pg_numeric_precision_radix",
		     "-20", "information_schema._pg_numeric_precision_radix",
		     "-21", "postgres",
		     NULL);
	TEST_MODIFY (store, TNAME, NULL, 
		     "specific_name='_pg_numeric_precision_radix_11324'", &error, NULL);
	TEST_END (NULL);
#undef TNAME
}

void
test_triggers (GdaMetaStore *store)
{
#define TNAME "_triggers"
	TEST_HEADER;

	/* load CSV file */
	import = common_load_csv_file ("data_triggers.csv", -1);
	common_declare_expected_insertions_from_model (TNAME, import);
	TEST_MODIFY (store, TNAME, import, NULL, &error, NULL);
	TEST_END (import);
#undef TNAME
}

void
test_columns (GdaMetaStore *store)
{
#define TNAME "_columns"
	TEST_HEADER;

	/* load CSV file */
	import = common_load_csv_file ("data_columns.csv", 4, "gint", 6, "boolean", 8, "gint", 11, "gint", 12, "gint", 13, "gint", 14, "gint", 15, "gint", 23, "boolean", -1);
	common_declare_expected_insertions_from_model (TNAME, import);
	TEST_MODIFY (store, TNAME, import, NULL, &error, NULL);
	TEST_END (import);
#undef TNAME
}

void
test_table_constraints (GdaMetaStore *store)
{
#define TNAME "_table_constraints"
	TEST_HEADER;

	/* load CSV file */
	import = common_load_csv_file ("data_table_constraints.csv", 8, "boolean", 9, "boolean", -1);
	common_declare_expected_insertions_from_model (TNAME, import);
	TEST_MODIFY (store, TNAME, import, NULL, &error, NULL);
	TEST_END (import);
#undef TNAME
}

void
test_referential_constraints (GdaMetaStore *store)
{
#define TNAME "_referential_constraints"
	TEST_HEADER;

	/* load CSV file */
	import = common_load_csv_file ("data_referential_constraints.csv", -1);
	common_declare_expected_insertions_from_model (TNAME, import);
	TEST_MODIFY (store, TNAME, import, NULL, &error, NULL);
	TEST_END (import);
#undef TNAME
}

void
test_key_column_usage (GdaMetaStore *store)
{
#define TNAME "_key_column_usage"
	TEST_HEADER;

	/* load CSV file */
	import = common_load_csv_file ("data_key_column_usage.csv", 7, "gint", -1);
	common_declare_expected_insertions_from_model (TNAME, import);
	TEST_MODIFY (store, TNAME, import, NULL, &error, NULL);
	TEST_END (import);
#undef TNAME
}

void
test_view_column_usage (GdaMetaStore *store)
{
#define TNAME "_view_column_usage"
	TEST_HEADER;

	/* load CSV file */
	import = common_load_csv_file ("data_view_column_usage.csv", -1);
	common_declare_expected_insertions_from_model (TNAME, import);
	TEST_MODIFY (store, TNAME, import, NULL, &error, NULL);
	TEST_END (import);
#undef TNAME
}

void
test_domain_constraints (GdaMetaStore *store)
{
#define TNAME "_domain_constraints"
	TEST_HEADER;

	/* load CSV file */
	import = common_load_csv_file ("data_domain_constraints.csv", 7, "boolean", 8, "boolean", -1);
	common_declare_expected_insertions_from_model (TNAME, import);
	TEST_MODIFY (store, TNAME, import, NULL, &error, NULL);
	TEST_END (import);
#undef TNAME
}

void
test_parameters (GdaMetaStore *store)
{
#define TNAME "_parameters"
	TEST_HEADER;

	/* load CSV file */
	import = common_load_csv_file ("data_parameters.csv", 3, "gint", -1);
	common_declare_expected_insertions_from_model (TNAME, import);
	TEST_MODIFY (store, TNAME, import, NULL, &error, NULL);
	TEST_END (import);
#undef TNAME
}

gboolean
test_setup (const gchar *prov_id, const gchar *dbcreate_str) {
	GError *error = NULL;
	GdaServerOperation *opndb;

	g_assert_null (quark_list);

	quark_list = gda_quark_list_new_from_string (dbcreate_str);

	/* We will use a unique name for database for every test.
	 * The format of the database name is:
	 * dbXXXXX where XXXXX is a string generated from the random int32 numbers
	 * that correspond to ASCII codes for characters a-z
	 */
	dbname = test_random_string ("db", 5);

	opndb = gda_server_operation_prepare_create_database (prov_id, dbname, &error);

	g_assert_nonnull (opndb);

	const gchar *value = NULL;
	gboolean res;
	value = gda_quark_list_find (quark_list, "HOST");
	res = gda_server_operation_set_value_at (opndb, value, NULL, "/SERVER_CNX_P/HOST");
	g_assert_true (res);

	value = gda_quark_list_find (quark_list, "ADM_LOGIN");
	res = gda_server_operation_set_value_at (opndb, value, NULL, "/SERVER_CNX_P/ADM_LOGIN");
	g_assert_true (res);

	value = gda_quark_list_find (quark_list, "ADM_PASSWORD");
	res = gda_server_operation_set_value_at (opndb, value, NULL, "/SERVER_CNX_P/ADM_PASSWORD");
	g_assert_true (res);

	/* This operation may fail if the template1 database is locked by other process. We need
	 * to try again short time after when template1 is available
	 */
	res = FALSE;

	for (gint i = 0; i < 100; ++i) {
		g_print ("Attempt to create database... %d\n", i);
		res = gda_server_operation_perform_create_database (opndb, prov_id, &error);
		if (!res) {
			g_clear_error (&error);
			g_usleep(1E5);
			continue;
		} else {
			break;
		}
	}

	if (!res) {
	    g_warning ("Creating database error: %s",
		       error && error->message ? error->message : "No error was set");
	    g_clear_error (&error);
	    g_free (dbname);
	    gda_quark_list_free (quark_list);
	    return FALSE;
	}
	return TRUE;
}


gboolean
test_finish (GdaConnection *cnc) {
	GError *error = NULL;

	if (!gda_connection_close (cnc, &error)) {
		g_warning ("Error clossing connection to database: %s",
		           error && error->message ? error->message : "No error was set");
		g_clear_error (&error);
		return FALSE;
	}

	return TRUE;
}
