#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gmodule.h>
#include <glib/gi18n-lib.h>

#include "html.h"

/* options */
gchar *dir = NULL;
gchar *infile = NULL;

static GOptionEntry entries[] = {
	{ "output-dir", 'o', 0, G_OPTION_ARG_STRING, &dir, "Output dir", "output directory"},
	{ NULL }
};

typedef struct {
	HtmlConfig  config;
	xmlDocPtr   tests_doc;
	GdaDict    *dict; /* current dictionary */
	HtmlFile   *file; /* current file */
	xmlNodePtr  main_ul;
	
} TestConfig;

/*
 * actual tests
 */
static void test_all_queries (TestConfig *config);
static void test_and_destroy_dict (TestConfig *config);

gint main (int argc, char **argv) {
	GError *error = NULL;	
	GOptionContext *context;
	TestConfig *config;
	gchar *str;
	GSList *list;
	xmlNodePtr node, ul;

	/* command line parsing */
	context = g_option_context_new ("<tests file> - SQL parsing testing");
	g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
	if (!g_option_context_parse (context, &argc, &argv, &error)) {
		g_warning ("Can't parse arguments: %s", error->message);
		exit (1);
	}
	g_option_context_free (context);
	
	/* init */
	gda_init ("gda-test-sql", PACKAGE_VERSION, argc, argv);
	if (!dir)
		dir = "GDA_TESTS_SQL_OUTPUT";
	if (g_path_is_absolute (dir))
		str = g_strdup (dir);
	else
		str = g_strdup_printf ("./%s", dir);

	if (g_file_test (dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		if (g_access (dir, W_OK))
			g_error ("Can't write to %s", dir);
		g_print ("Output dir '%s' exists and is writable\n", str);
	}
	else {
		if (g_mkdir_with_parents (dir, 0700))
			g_error ("Can't create directory %s", dir);
		g_print ("Output dir '%s' created\n", str);
	}

	if (argc == 2) 
		infile = argv[1];

	if (!infile) {
		g_print ("No tests file specified\n");
		exit (1);
	}

	/* create config structure */
	config = g_new0 (TestConfig, 1);
	html_init_config (HTML_CONFIG (config));
	config->config.index = html_file_new (HTML_CONFIG (config), 
					      "index.html", "Gda SQL parsing tests");		
        config->config.dir = str;
	node = xmlNewChild (config->config.index->body, NULL, "h1", _("List of SQL parsing tests"));
	ul = xmlNewChild (config->config.index->body, NULL, "ul", NULL);
	config->main_ul = ul;

	{
		time_t now;
		struct tm *stm;

		now = time (NULL);
		stm = localtime (&now);
		str = g_strdup_printf (_("Generated @ %s"), asctime (stm));
		node = xmlNewChild (config->config.index->body, NULL, "p", str);
		g_free (str);
	}

	/* open file containing the tests */
	config->tests_doc = xmlParseFile (infile);
        if (!config->tests_doc) {
                g_print ("Cant' load XML file '%s'\n", infile);
                exit (1);
        }
	
	/* do tests */
	test_all_queries (config);
	xmlFreeDoc (config->tests_doc);

	/* actual files writing to disk */
	list = config->config.all_files;
	while (list) {
		html_file_write ((HtmlFile*) list->data, HTML_CONFIG (config));
		list = g_slist_next (list);
	}
	g_slist_foreach (config->config.all_files, (GFunc) html_file_free, NULL);
	g_slist_free (config->config.all_files);

	test_and_destroy_dict (config);

	return 0;
}

/*
 * This function dumps the dictionary to a temporary file, then load that file into another
 * dictionary, and save it again and compares with the first dictionary.
 */
static void
test_and_destroy_dict (TestConfig *config)
{
	if (config->dict) {
		gchar *dictfile;
		GError *error = NULL;
		gboolean err = FALSE;
		
		dictfile = g_build_filename (g_get_tmp_dir (), "gda_sql_testXXXXXX", NULL);
		mkstemp (dictfile);

		if (!gda_dict_save_xml_file (config->dict, dictfile, &error)) {
			g_warning (_("Can't write dictionary to file %s: %s\n"), dictfile,
				   error ? error->message : _("Unknown error"));
			if (error) {
				g_error_free (error);
				error = NULL;
			}
		}
		else {
			GdaDict *loaded_dict;

			/* g_print (_("Dictionary file written to temporary location %s\n"), dictfile); */
			loaded_dict = GDA_DICT (gda_dict_new ());
			if (!gda_dict_load_xml_file (loaded_dict, dictfile, &error)) {
				g_print (_("Error loading dictionary file '%s':\n%s\n"), dictfile,
					 error ? error->message : _("Unspecified"));
				if (error) {
					g_error_free (error);
					error = NULL;
				}
			}
			else {
				gchar *dictfile2;

				dictfile2 = g_build_filename (g_get_tmp_dir (), "gda_sql_testXXXXXX", NULL);
				mkstemp (dictfile2);

				if (!gda_dict_save_xml_file (loaded_dict, dictfile2, &error)) {
					g_warning (_("Can't write dictionary to file %s: %s\n"), dictfile,
						   error ? error->message : _("Unknown error"));
					if (error) {
						g_error_free (error);
						error = NULL;
					}
				}
				else {
					gchar *cmde, *chout = NULL, *cherr = NULL;
					gint chstatus;

					cmde = g_strdup_printf ("cmp %s %s", dictfile, dictfile2);
					if (! g_spawn_command_line_sync (cmde, &chout, &cherr, &chstatus, &error)) {
						g_print (_("Can't run command '%s':\n%s\n"), cmde, error->message);
						g_error_free (error);
						error = NULL;
					}
					else {
						gchar *cmde2, *chout2 = NULL, *cherr2 = NULL;
						switch (chstatus) {
						case 0:
							g_print ("Loading and saving dictionary is OK\n");
							break;
						default:
							g_print ("Loading and saving the dictionary is not OK:\n");
							cmde2 = g_strdup_printf ("diff -u %s %s", dictfile, dictfile2);
							if (! g_spawn_command_line_sync (cmde2, &chout2, &cherr2, &chstatus, &error)) {
								g_print (_("Can't run command '%s':\n%s\n"), cmde, error->message);
								g_error_free (error);
							}
							g_print ("%s\n", chout2);
							g_free (cmde2);
							g_free (chout2);
							g_free (cherr2);
							break;
						}
						g_unlink (dictfile2);
					}
					g_free (cmde);
					g_free (chout);
					g_free (cherr);
				}
				g_free (dictfile2);
			}
			g_object_unref (loaded_dict);
			g_unlink (dictfile);
		}
		g_free (dictfile);

		g_object_unref (G_OBJECT (config->dict));
		config->dict = NULL;
	}
}

static void add_notice (HtmlFile *file);
static void make_query_test (TestConfig *config, 
			     const gchar *sql, gboolean parsed, const gchar *rendered, xmlNodePtr table);
static void
test_all_queries (TestConfig *config)
{
	xmlNodePtr node, subnode;
	GError *error = NULL;

	node = xmlDocGetRootElement (config->tests_doc);
        if (strcmp (node->name, "test_scenario")) {
                g_print ("XML file top node is not <test_scenario>\n");
                exit (1);
        }

	subnode = node->children;
	while (subnode) {
		/* new dictionary */
		if (!strcmp (subnode->name, "dictionary")) {
			gchar *filename = xmlGetProp (subnode, "name");
			test_and_destroy_dict (config);			
			if (filename) {
				config->dict = GDA_DICT (gda_dict_new ());
				g_print ("Loading dictionary %s\n", filename);
				if (!gda_dict_load_xml_file (config->dict, filename, &error)) {
					g_print ("Error occurred:\n\t%s\n", error->message);
					g_error_free (error);
					exit (1);
				}
				g_free (filename);
			}
		}
		
		/* new output file */
		if (!strcmp (subnode->name, "output_file")) {
			gchar *filename = xmlGetProp (subnode, "name");
			gchar *descr = xmlGetProp (subnode, "descr");
			xmlNodePtr li;
			gchar *tmp;

			li = xmlNewChild (config->main_ul, NULL, "li", descr ? descr : filename);
			tmp = g_strdup_printf ("/test/%s", filename);
			html_declare_node_own (HTML_CONFIG (config), tmp, li);
			
			config->file = html_file_new (HTML_CONFIG (config), filename, descr);
			add_notice (config->file);
			html_add_link_to_node (HTML_CONFIG (config), tmp, "Details", filename);
			g_free (descr);
			g_free (filename);
		}
		
		/* new test group */
		if (!strcmp (subnode->name, "test_group")) {
			xmlNodePtr table, tr, td;
			xmlNodePtr test;
			gchar *descr = xmlGetProp (subnode, "descr");
			
			html_add_header (HTML_CONFIG (config), config->file, descr);
			table = xmlNewChild (config->file->body, NULL, "table", NULL);
			xmlSetProp (table, "width", "100%");
			tr = xmlNewChild (table, NULL, "tr", NULL);
			td = xmlNewChild (tr, NULL, "th", _("SQL tested"));
			xmlSetProp (td, "colspan", "4");
			tr = xmlNewChild (table, NULL, "tr", NULL);
			td = xmlNewChild (tr, NULL, "th", _("Action"));
			xmlSetProp (td, "width", "5%");
			td = xmlNewChild (tr, NULL, "th", _("Query Name"));
			xmlSetProp (td, "width", "5%");
			td = xmlNewChild (tr, NULL, "th", _("Query Status"));
			xmlSetProp (td, "width", "5%");
			td = xmlNewChild (tr, NULL, "th", _("Rendered SQL / error if parsing is not done"));
			xmlSetProp (td, "width", "85%");
			g_free (descr);

			test = subnode->children;
			while (test) {
				if (!strcmp (test->name, "test")) {
					gchar *sql = xmlGetProp (test, "sql");
					gchar *parsed = xmlGetProp (test, "parsed");
					gchar *rendered = xmlGetProp (test, "rendered");

					if (sql) {
						make_query_test (config, sql, 
								 parsed && (*parsed=='0') ? FALSE : TRUE,
								 rendered,
								 table);
						g_free (sql);
					}
					
					g_free (parsed);
					g_free (rendered);
				}
				test = test->next;
			}
		}
		subnode = subnode->next;
	}
}

static void 
add_notice (HtmlFile *file)
{
	xmlNodePtr node, table, tr, td;

	node = xmlNewChild (file->body, NULL, "h2", "Color legend");
	table = xmlNewChild (file->body, NULL, "table", NULL);

	tr = xmlNewChild (table, NULL, "tr", NULL);
	td = xmlNewChild (tr, NULL, "td", "Red text");
	xmlSetProp (td, "class", "error");
	td = xmlNewChild (tr, NULL, "td", "The test failed");

	tr = xmlNewChild (table, NULL, "tr", NULL);
	td = xmlNewChild (tr, NULL, "td", "Orange text");
	xmlSetProp (td, "class", "warning");
	td = xmlNewChild (tr, NULL, "td", "There are some SQL differences, the test may have failed but more data is required in the test to make sure");

	tr = xmlNewChild (table, NULL, "tr", NULL);
	td = xmlNewChild (tr, NULL, "td", "Non parsed");
	td = xmlNewChild (tr, NULL, "td", "The query did not manage to parse the SQL and so considers the SQL as a string");

	tr = xmlNewChild (table, NULL, "tr", NULL);
	td = xmlNewChild (tr, NULL, "td", "Active");
	td = xmlNewChild (tr, NULL, "td", "The query is active: "
			  "all the elements in the query have been found in the dictionary");

	tr = xmlNewChild (table, NULL, "tr", NULL);
	td = xmlNewChild (tr, NULL, "td", "Inactive");
	xmlSetProp (td, "id", "inactive");
	td = xmlNewChild (tr, NULL, "td", "The query is inactive: "
			  "some element(s) in the query are missing in the dictionary");
}

static const gchar *
get_query_status (GdaQuery *query)
{
	if (gda_query_get_query_type (query) == GDA_QUERY_TYPE_NON_PARSED_SQL)
		return _("non-parsed");
	
	return gda_referer_activate (GDA_REFERER (query)) ? _("Active") : _("Inactive");
}

static void
make_query_test (TestConfig *config, const gchar *sql, gboolean parsed, const gchar *rendered, xmlNodePtr table)
{
	GdaQuery *query;
        GError *error = NULL;
	xmlNodePtr tr, td, actiontd;
	gchar *sql2;
	gboolean is_non_parsed;

	query = (GdaQuery *) gda_query_new_from_sql (config->dict, sql, &error);
	gda_dict_assume_query (config->dict, query);
	g_object_unref (query);

	tr = xmlNewChild (table, NULL, "tr", NULL);
	td = xmlNewChild (tr, NULL, "td", sql);
	xmlSetProp (td, "colspan", "4");

	/* first parsing */
	tr = xmlNewChild (table, NULL, "tr", NULL);
	actiontd = xmlNewChild (tr, NULL, "td", _("Parsing"));
	td = xmlNewChild (tr, NULL, "td", gda_object_get_id (GDA_OBJECT (query)));
	td = xmlNewChild (tr, NULL, "td", get_query_status (query));
	if (!gda_referer_activate (GDA_REFERER (query)))
		xmlSetProp (td, "id", "inactive");

	is_non_parsed = (gda_query_get_query_type (query) == GDA_QUERY_TYPE_NON_PARSED_SQL);

	if (is_non_parsed) {
		td = xmlNewChild (tr, NULL, "td", error && error->message ? error->message : _("Unknown error"));
	}
	else {
		/* rendering */
		sql2 = gda_renderer_render_as_sql (GDA_RENDERER (query), NULL, 
						   GDA_RENDERER_EXTRA_VAL_ATTRS, &error);
		if (sql2) {
			/* rendering OK */
			GdaQuery *copy, *copy2;
			gchar *sql3;
			xmlNodePtr actiontd_c;
			
			g_strchomp (sql2);
			td = xmlNewChild (tr, NULL, "td", sql2);
			if (rendered && strcmp (sql2, rendered)) 
				html_mark_node_error (HTML_CONFIG (config), td);
			
			if (!rendered && strcmp (sql2, sql))
				html_mark_node_warning (HTML_CONFIG (config), td);
			
			/* copy test */
			copy = (GdaQuery *) gda_query_new_copy (query, NULL);
			copy2 = (GdaQuery *) gda_query_new_copy (copy, NULL);
			g_object_unref (copy);
			copy = copy2;
			tr = xmlNewChild (table, NULL, "tr", NULL);
			actiontd_c = xmlNewChild (tr, NULL, "td", _("Copied"));
			td = xmlNewChild (tr, NULL, "td", gda_object_get_id (GDA_OBJECT (copy)));
			td = xmlNewChild (tr, NULL, "td", get_query_status (copy));
			if (!gda_referer_activate (GDA_REFERER (copy)))
				xmlSetProp (td, "id", "inactive");
			
			/* rendering */
			sql3 = gda_renderer_render_as_sql (GDA_RENDERER (query), NULL, 
							   GDA_RENDERER_EXTRA_VAL_ATTRS, &error);
			g_strchomp (sql3);
			if (sql3) 
				td = xmlNewChild (tr, NULL, "td", sql3);
			else {
				/* rendering error */
				td = xmlNewChild (tr, NULL, "td", 
						  error && error->message ? error->message : "---");
				html_mark_node_warning (HTML_CONFIG (config), td);
			}
			
			/* validity test */
			if (gda_query_get_query_type (query) != gda_query_get_query_type (copy))
				html_mark_node_error (HTML_CONFIG (config), actiontd_c);
			else {
				if (strcmp (sql2, sql3))
					html_mark_node_error (HTML_CONFIG (config), actiontd_c);
			}
			g_object_unref (copy);
			
			g_free (sql3);
			g_free (sql2);
		}
		else {
			/* rendering error */
			td = xmlNewChild (tr, NULL, "td", 
					  error && error->message ? error->message : "---");
			html_mark_node_warning (HTML_CONFIG (config), td);
		}
	}

	/* test validity */
	if ((parsed && is_non_parsed) ||
	    (!parsed && !is_non_parsed))
		html_mark_node_error (HTML_CONFIG (config), actiontd);
}
