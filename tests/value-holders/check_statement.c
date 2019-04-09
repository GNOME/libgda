/*
 * Copyright (C) 2008 - 2013 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
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
#include <libgda/libgda.h>
#include <sql-parser/gda-sql-parser.h>
#include <string.h>
#include <gmodule.h>
#include "common.h"

static GdaSqlParser *create_parser_for_provider (const gchar *prov_name);

/* 
 * tests
 */
typedef gboolean (*TestFunc) (GError **);
static gboolean test1 (GError **error);
static gboolean test2 (GError **error);

GHashTable *data;
TestFunc tests[] = {
	test1,
	test2
};

int 
main ()
{
#if GLIB_CHECK_VERSION(2,36,0)
#else
	g_type_init ();
#endif
	gda_init ();

	guint failures = 0;
	guint i, ntests = 0;
  
	data = tests_common_load_data ("stmt.data");
	for (i = 0; i < sizeof (tests) / sizeof (TestFunc); i++) {
		GError *error = NULL;
		if (! tests[i] (&error)) {
			g_print ("Test %d failed: %s\n", i+1, 
				 error && error->message ? error->message : "No detail");
			if (error)
				g_error_free (error);
			failures ++;
		}
		ntests ++;
	}

	g_print ("TESTS COUNT: %d\n", ntests);
	g_print ("FAILURES: %d\n", failures);

	return failures != 0 ? 1 : 0;
}

typedef struct {
	gboolean result; /* TRUE if test is supposed to succeed */
	gchar *id; /* same id as in data file */
	gchar *sql;
} ATest;

static ATest test1_data[] = {
	{FALSE, NULL,    "SELECT ##p1::gint, 12.3 /* name:p2 */"},
	{TRUE,  "T1.1",  "SELECT ##p1::gint, 12.3 /* name:p2 type:gfloat */"},
	{TRUE,  "T1.2",  "SELECT ##p1::gint, 12.3 /* name:p1 type:gfloat */"},
	{TRUE,  "T1.3",  "SELECT a, b FROM (SELECT version(##vers::date), ##aparam::string)"} 
};

/*
 * get_parameters()
 */
static gboolean
test1 (GError **error)
{
	GdaSqlParser *parser;
	guint i;

	parser = gda_sql_parser_new ();
	
	for (i = 0; i < sizeof (test1_data) / sizeof (ATest); i++) {
		ATest *test = &(test1_data[i]);
		GdaStatement *stmt;
		GError *lerror = NULL;

		stmt = gda_sql_parser_parse_string (parser, test->sql, NULL, &lerror);
		if (!stmt) {
			if (test->result) {
				if (lerror) 
					g_propagate_error (error, lerror);
				return FALSE;
			}
		}
		else { 
			GdaSet *set;
			if (!gda_statement_get_parameters (stmt, &set, &lerror)) {
				if (test->result) {
					if (lerror) 
						g_propagate_error (error, lerror);
					return FALSE;
				}
			}
			else if (set) {
				if (!tests_common_check_set (data, test->id, set, &lerror)) {
					if (lerror) 
						g_propagate_error (error, lerror);
					return FALSE;
				}
				g_object_unref (set);
			}
			g_object_unref (stmt);
		}

		if (lerror)
			g_error_free (lerror);
	}

	g_object_unref (parser);

	return TRUE;
}

/*
 * render SQL
 */
static gboolean
test2 (GError **error)
{
	GdaSqlParser *parser = NULL;
	GHashTable *parsers_hash;
	GdaDataModel *providers_model;
	gint i;

	/* create parsers */
	parsers_hash = g_hash_table_new (g_str_hash, g_str_equal);
	providers_model = gda_config_list_providers ();
	for (i = 0; i < gda_data_model_get_n_rows (providers_model); i++) {
		const GValue *pname;
		pname = gda_data_model_get_value_at (providers_model, 0, i, error);
		if (!pname)
			return FALSE;
		parser = create_parser_for_provider (g_value_get_string (pname));
		g_hash_table_insert (parsers_hash, g_strdup (g_value_get_string (pname)), parser);
		g_print ("Created parser for provider %s\n", g_value_get_string (pname));
	}
	g_object_unref (providers_model);
	g_hash_table_insert (parsers_hash, "", gda_sql_parser_new ());
	
	xmlDocPtr doc;
        xmlNodePtr root, node;
	gchar *fname;
	
	fname = g_build_filename (ROOT_DIR, "tests", "parser", "testdata.xml", NULL);
	if (! g_file_test (fname, G_FILE_TEST_EXISTS)) {
                g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "File '%s' does not exist\n", fname);
		return FALSE;
        }
	doc = xmlParseFile (fname);
        g_free (fname);
        g_assert (doc);
        root = xmlDocGetRootElement (doc);
	g_assert (!strcmp ((gchar*) root->name, "testdata"));
        for (node = root->children; node; node = node->next) {
		if (strcmp ((gchar*) node->name, "test"))
                        continue;
		xmlNodePtr snode;
		xmlChar *sql = NULL;
		xmlChar *id;
		xmlChar *prov_name;

		prov_name = xmlGetProp (node, BAD_CAST "provider");
		if (prov_name) {
			parser = g_hash_table_lookup (parsers_hash, (gchar *) prov_name);
			xmlFree (prov_name);
		}
		else
			parser = g_hash_table_lookup (parsers_hash, "");
		if (!parser)
			continue;

		for (snode = node->children; snode && strcmp ((gchar*) snode->name, "sql"); snode = snode->next);
		if (!snode)
			continue;
		sql = xmlNodeGetContent (snode);
		if (!sql)
			continue;
		
		GdaStatement *stmt;
		GError *lerror = NULL;
		
		stmt = gda_sql_parser_parse_string (parser, (const gchar*) sql, NULL, &lerror);
		xmlFree (sql);
		id = xmlGetProp (node, BAD_CAST "id");
		g_print ("===== TEST %s\n", id);

		if (!stmt) {
			/* skip that SQL if it can't be parsed */
			g_error_free (lerror);
			continue;
		}
		else { 
			GdaStatement *stmt2;
			gchar *rsql;
			gchar *ser1, *ser2;
			
			rsql = gda_statement_to_sql_extended (stmt, NULL, NULL, 0, NULL, &lerror);
			if (!rsql) {
				g_print ("REM: test '%s' can't be rendered: %s\n", id,
					 lerror && lerror->message ? lerror->message : "No detail");
				xmlFree (id);
				continue;
			}
			
			/*g_print ("--> rendered SQL: %s\n", rsql);*/
			stmt2 = gda_sql_parser_parse_string (parser, rsql, NULL, error);
			if (!stmt2) 
				return FALSE;
			
			GdaSqlStatement *sqlst;
			
			g_object_get (G_OBJECT (stmt), "structure", &sqlst, NULL);
			g_free (sqlst->sql);
			sqlst->sql = NULL;
			ser1 = gda_sql_statement_serialize (sqlst);
			gda_sql_statement_free (sqlst);
			
			g_object_get (G_OBJECT (stmt2), "structure", &sqlst, NULL);
			g_free (sqlst->sql);
			sqlst->sql = NULL;
			ser2 = gda_sql_statement_serialize (sqlst);
			gda_sql_statement_free (sqlst);
			
			if (strcmp (ser1, ser2)) {
				g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC,
					     "Statement failed, ID: %s\nSQL: %s\nSER1: %s\nSER2 :%s", 
					     id, rsql, ser1, ser2);
				g_free (ser1);
				g_free (ser2);
				return FALSE;
			}
			
			g_free (rsql);
			g_free (ser1);
			g_free (ser2);
			g_object_unref (stmt);
			g_object_unref (stmt2);
		}
		
		xmlFree (id);

		if (lerror)
			g_error_free (lerror);
	}

	g_object_unref (parser);

	return TRUE;
}

static GdaSqlParser *
create_parser_for_provider (const gchar *prov_name)
{
	GdaServerProvider *prov;
	GdaSqlParser *parser;
	GError *error = NULL;

	prov = gda_config_get_provider (prov_name, &error);
	if (!prov) 
		g_error ("Could not create provider for '%s': %s\n", prov_name,
			 error && error->message ? error->message : "No detail");

	parser = gda_server_provider_create_parser (prov, NULL);
	if (!parser)
		parser = gda_sql_parser_new ();

	return parser;
}
