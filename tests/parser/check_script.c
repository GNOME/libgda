/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
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
#include <stdio.h>
#include <glib.h>
#include <glib-object.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <gmodule.h>
#include <libgda/libgda.h>
#include <sql-parser/gda-sql-parser.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

static GdaSqlParser *create_parser_for_provider (const gchar *prov_name);
static gint do_test (GdaSqlParser *parser, const xmlChar *id, const xmlChar *file, xmlNodePtr test_node);

int 
main (G_GNUC_UNUSED int argc, G_GNUC_UNUSED char** argv)
{
	xmlDocPtr doc;
        xmlNodePtr root, node;
	GdaSqlParser *parser;
	gint failures = 0;
	gint ntests = 0;
	gchar *fname;
	GHashTable *parsers_hash;
	GdaDataModel *providers_model;
	gint i;

	gda_init ();

	/* load file */
	fname = g_build_filename (ROOT_DIR, "tests", "parser", "testscripts.xml", NULL);
	if (! g_file_test (fname, G_FILE_TEST_EXISTS)) {
                g_print ("File '%s' does not exist\n", fname);
                exit (1);
        }

	/* create parsers */
	parsers_hash = g_hash_table_new (g_str_hash, g_str_equal);
	providers_model = gda_config_list_providers ();
	for (i = 0; i < gda_data_model_get_n_rows (providers_model); i++) {
		const GValue *pname;
		GError *lerror = NULL;
		pname = gda_data_model_get_value_at (providers_model, 0, i, &lerror);
		if (!pname) {
			g_print ("Can't get data model's value: %s",
				 lerror && lerror->message ? lerror->message : "No detail");
			exit (1);
		}
		parser = create_parser_for_provider (g_value_get_string (pname));
		g_hash_table_insert (parsers_hash, g_strdup (g_value_get_string (pname)), parser);
		g_print ("Created parser for provider %s\n", g_value_get_string (pname));
	}
	g_object_unref (providers_model);
	g_hash_table_insert (parsers_hash, "", gda_sql_parser_new ());

	/* use test data */
	doc = xmlParseFile (fname);
	g_free (fname);
	g_assert (doc);
	root = xmlDocGetRootElement (doc);
	g_assert (!strcmp ((gchar*) root->name, "testdata"));
	for (node = root->children; node; node = node->next) {
		if (strcmp ((gchar*) node->name, "test"))
			continue;
		xmlChar *id;
		xmlChar *file;
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

		id = xmlGetProp (node, BAD_CAST "id");
		file = xmlGetProp (node, BAD_CAST "file");
		
		if (id && file) {
			failures += do_test (parser, id, file, node);
			ntests++;
		}

		/* mem free */
		if (id)	xmlFree (id);
		if (file) xmlFree (file);
	}
	xmlFreeDoc (doc);

	g_print ("TESTS COUNT: %d\n", ntests);
	g_print ("FAILURES: %d\n", failures);
  
	return failures != 0 ? 1 : 0;
}

/*
 * Returns: the number of failures
 */
static gint
do_test (GdaSqlParser *parser, const xmlChar *id, const xmlChar *file, xmlNodePtr test_node)
{
	gboolean failures = 0;
	GdaBatch *batch;
	GError *error = NULL;
	gchar *full_file;

	full_file = g_build_filename (ROOT_DIR, "tests", "parser", "scripts", file, NULL);

#ifdef GDA_DEBUG
	g_print ("===== TEST %s FILE: %s\n", id, full_file);
#endif

	batch = gda_sql_parser_parse_file_as_batch (parser, full_file, &error);
	g_free (full_file);

	if (!batch) {
		g_print ("ERROR for test '%s':\n   *got error while parsing: %s\n", id,
			 error && error->message ? error->message: "No detail");
		if (error)
			g_error_free (error);
		failures ++;
	}
	else {
		const GSList *stmt_list;
		xmlNodePtr snode;
		guint nb_stmt = 0;

		stmt_list = gda_batch_get_statements (batch);

		/* number of statements */
		for (snode = test_node->children; snode; snode = snode->next) {
			if (!strcmp ((gchar*) snode->name, "stmt"))
				nb_stmt++;
		}
		if (nb_stmt != g_slist_length ((GSList*) stmt_list)) {
			g_print ("ERROR for test '%s':\n   *expected %d statements and got %d\n", id,
				 nb_stmt, g_slist_length ((GSList*) stmt_list));
			failures ++;
		}

		/* each statement */
		GSList *list;
		for (snode = test_node->children, list = (GSList*) stmt_list;
		     list;
		     list = list->next) {
			gchar *sql;
			sql = gda_statement_to_sql (GDA_STATEMENT (list->data), NULL, &error);
			if (!sql) {
				g_print ("ERROR for test '%s':\n   *can't convert statement at position %d to SQL: %s\n", id,
					 g_slist_position ((GSList*) stmt_list, list),
					 error && error->message ? error->message: "No detail");
				if (error)
					g_error_free (error);
				failures ++;
			}
			else {
				/* find the next <stmt> node */
				xmlChar *expected = NULL;
				for (; snode; snode = snode->next) {
					if (strcmp ((gchar*) snode->name, "stmt"))
						continue;
					expected = xmlNodeGetContent (snode);
					snode = snode->next;
					break;
				}
				if (!expected) {
					g_print ("ERROR for test '%s':\n   *missing <stmt> tag for statement at position %d\n", id,
						 g_slist_position ((GSList*) stmt_list, list));	 
					failures ++;
				}

				gchar *ptr;
				for (ptr = sql; *ptr; ptr++) {
					if ((*ptr == '\n') || (*ptr == '\t') || (*ptr == '\r'))
						*ptr = ' ';
				}
				g_strstrip (sql);
				if (expected) {
					if (g_strcmp0 (sql, (const gchar*) expected)) {
						g_print ("ERROR for test '%s', statement at position %d:\n   *exp:%s\n   *got:%s\n", id,
							 g_slist_position ((GSList*) stmt_list, list), expected, sql);	 
						failures ++;
						break;
					}
				}
				else
					g_print ("% 2d: %s\n", g_slist_position ((GSList*) stmt_list, list), sql);
				g_free (sql);
			}
		}
		g_object_unref (batch);
	}

	return failures;
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
