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
static gint do_test (GdaSqlParser *parser, const xmlChar *id, const xmlChar *sql, const xmlChar *expected, 
		     const xmlChar *error_line, const xmlChar *error_col);

int 
main (int argc, char** argv)
{
	xmlDocPtr doc;
        xmlNodePtr root, node;
	GdaSqlParser *parser;
	gint failures = 0;
	gint ntests = 0;
	gchar *fname;
	GHashTable *parsers_hash;
	GList *list;

	gda_init ("Parser check", ".1", argc, argv);

	/* load file */
	fname = g_build_filename (ROOT_DIR, "tests", "parser", "testdata.xml", NULL);
	if (! g_file_test (fname, G_FILE_TEST_EXISTS)) {
                g_print ("File '%s' does not exist\n", fname);
                exit (1);
        }

	/* create parsers */
	parsers_hash = g_hash_table_new (g_str_hash, g_str_equal);
	for (list = gda_config_get_provider_list (); list; list = list->next) {
		GdaProviderInfo *pinfo = (GdaProviderInfo*) list->data;
		parser = create_parser_for_provider (pinfo->id);
		g_hash_table_insert (parsers_hash, pinfo->id, parser);
		g_print ("Created parser for provider %s\n", pinfo->id);
	}
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
		g_assert (parser);

		id = xmlGetProp (node, BAD_CAST "id");
		for (snode = node->children; snode; snode = snode->next) {
			if (!strcmp ((gchar*) snode->name, "sql"))
				sql = xmlNodeGetContent (snode);
			else if (!strcmp ((gchar*) snode->name, "expected")) {
				xmlChar *expected;
				xmlChar *mode;

				expected = xmlNodeGetContent (snode);
				mode = xmlGetProp (snode, BAD_CAST "mode");
				if (sql) {
					g_object_set (G_OBJECT (parser), "mode", 
						      mode && !strcmp ((gchar *) mode, "delim") ? 
						      GDA_SQL_PARSER_MODE_DELIMIT : GDA_SQL_PARSER_MODE_PARSE, 
						      NULL);
					failures += do_test (parser, id,  sql, expected, NULL, NULL);
					ntests++;
				}
				else
					g_print ("===== Test '%s' doe not have any <sql> tag!\n", id);
				if (expected) xmlFree (expected);
				if (mode) xmlFree (mode);
			}
			else if (!strcmp ((gchar*) snode->name, "error")) {
				xmlChar *error_line, *error_col;
				xmlChar *mode;
				mode = xmlGetProp (snode, BAD_CAST "mode");
				error_line = xmlGetProp (snode, BAD_CAST "line");
				error_col = xmlGetProp (snode, BAD_CAST "col");
				if (sql) {
					g_object_set (G_OBJECT (parser), "mode", 
						      mode && !strcmp ((gchar *) mode, "delim") ? 
						      GDA_SQL_PARSER_MODE_DELIMIT : GDA_SQL_PARSER_MODE_PARSE, 
						      NULL);
					failures += do_test (parser, id, sql, NULL, error_line, error_col);
					ntests++;
				}
				else
					g_print ("===== Test '%s' doe not have any <sql> tag!\n", id);
				
				if (mode) xmlFree (mode);
				if (error_line)	xmlFree (error_line);
				if (error_col) xmlFree (error_col);
			}
		}

		/* mem free */
		if (sql) xmlFree (sql);
		if (id)	xmlFree (id);
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
do_test (GdaSqlParser *parser, const xmlChar *id, const xmlChar *sql, const xmlChar *expected, 
	 const xmlChar *error_line, const xmlChar *error_col)
{
	gboolean failures = 0;
#ifdef GDA_DEBUG
	GdaSqlParserMode mode;
	g_object_get (G_OBJECT (parser), "mode", &mode, NULL);
	g_print ("===== TEST %s (%s mode) SQL: @%s@\n", id, 
		 mode == GDA_SQL_PARSER_MODE_PARSE ? "PARSE" : "DELIMIT", sql);
#endif
	if (expected) {
		GdaBatch *batch;
		GError *error = NULL;
		
		batch = gda_sql_parser_parse_string_as_batch (parser, (gchar *) sql, NULL, &error);
		if (batch) {
			gchar *ser;
			ser = gda_batch_serialize (batch);
			if (strcmp (ser, (gchar *) expected)) {
				g_print ("ERROR for test '%s':\n   *exp: %s\n   *got: %s\n",
					 id, expected, ser);
				failures ++;
			}
			g_free (ser);
			g_object_unref (batch);
		}
		else {
			if (error && (error->domain == GDA_SQL_PARSER_ERROR) &&
			    (error->code == GDA_SQL_PARSER_EMPTY_SQL_ERROR) && 
			    (*expected == 0))
				/* OK */;
			else {
				g_print ("ERROR for test '%s':\n   *got error: %s\n", id,
					 error && error->message ? error->message: "No detail");
				if (error)
					g_error_free (error);
				failures ++;
			}
		}
	}
	else if (error_line && error_col) {
		GdaBatch *batch;
		GError *error = NULL;
		
		batch = gda_sql_parser_parse_string_as_batch (parser, (gchar *) sql, NULL, &error);
		if (batch) {
			gchar *ser;
			ser = gda_batch_serialize (batch);
			g_print ("ERROR for test '%s':\n   *got: %s\n",
				 id, ser);
			g_free (ser);
			g_object_unref (batch);
			failures ++;
		}
		else {
			/* FIXME: test error line and col */
			gint line, col;
			g_object_get (parser, "line-error", &line, "column-error", &col, NULL);
			if ((atoi ((gchar*) error_line) != line) || (atoi ((gchar *) error_col) != col)) {
				g_print ("ERROR for test '%s':\n   *exp line=%s, col=%s\n   *got line=%d, col=%d\n",
					 id, error_line, error_col, line, col);
				failures ++;
			}
			if (error)
				g_error_free (error);
		}
	}
	else
		g_print ("Missing or incomplete expected result for test '%s'!\n", id);	
	
	return failures;
}

static GdaSqlParser *
create_parser_for_provider (const gchar *prov_name)
{
	GdaProviderInfo *pinfo = NULL;
	GdaServerProvider *prov;
	GModule *handle;
	GdaServerProvider *(*plugin_create_provider) (void);
	GdaSqlParser *parser;

	pinfo = gda_config_get_provider_by_name (prov_name);
	if (!pinfo) 
		pinfo = gda_config_get_provider_by_name ("SQLite");
	g_assert (pinfo);

	handle = g_module_open (pinfo->location, G_MODULE_BIND_LAZY);
	g_assert (handle);
	g_module_symbol (handle, "plugin_create_provider",
			 (gpointer) &plugin_create_provider);
	g_assert (plugin_create_provider);
	prov = plugin_create_provider ();
	g_assert (prov);
	parser = gda_server_provider_create_parser (prov, NULL);
	if (!parser)
		parser = gda_sql_parser_new ();
	g_object_unref (prov);

	return parser;
}
