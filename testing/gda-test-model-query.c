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
	context = g_option_context_new ("<tests file> - GdaDataModelQuery modification query generation testing");
	g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
	if (!g_option_context_parse (context, &argc, &argv, &error)) {
		g_warning ("Can't parse arguments: %s", error->message);
		exit (1);
	}
	g_option_context_free (context);
	
	/* init */
	gda_init ("gda-test-sql", PACKAGE_VERSION, argc, argv);
	if (!dir)
		dir = "GDA_TESTS_MODEL_QUERY_OUTPUT";
	if (g_path_is_absolute (dir))
		str = g_strdup (dir);
	else
		str = g_strdup_printf ("./%s", dir);

	if (g_file_test (dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		if (access (dir, W_OK))
			g_error ("Can't write to %s", dir);
		g_print ("Output dir '%s' exists and is writable\n", str);
	}
	else {
		if (g_mkdir (dir, 0700))
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
					      "index.html", "GdaDataModelQuery modification query generation testing");		
        config->config.dir = str;
	node = xmlNewChild (config->config.index->body, NULL, "h1", _("List of tests"));
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
			loaded_dict = gda_dict_new ();
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
				g_unlink (dictfile);
			}
			g_object_unref (loaded_dict);
		}
		g_free (dictfile);

		g_object_unref (G_OBJECT (config->dict));
		config->dict = NULL;
	}
}

static void make_query_test (TestConfig *config, const gchar *sql, xmlNodePtr table);
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
				config->dict = gda_dict_new ();
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
			xmlSetProp (td, "colspan", "3");
			tr = xmlNewChild (table, NULL, "tr", NULL);
			td = xmlNewChild (tr, NULL, "th", _("UPDATE query"));
			td = xmlNewChild (tr, NULL, "th", _("INSERT query"));
			td = xmlNewChild (tr, NULL, "th", _("DELETE query"));
			g_free (descr);

			test = subnode->children;
			while (test) {
				if (!strcmp (test->name, "test")) {
					gchar *sql = xmlGetProp (test, "sql");

					if (sql) {
						make_query_test (config, sql, table);
						g_free (sql);
					}
				}
				test = test->next;
			}
		}
		subnode = subnode->next;
	}
}

static void
make_query_test (TestConfig *config, const gchar *sql, xmlNodePtr table)
{
	GdaQuery *query;
        GError *error = NULL;
	xmlNodePtr tr, td;
	GdaDataModel *model;

	query = gda_query_new_from_sql (config->dict, sql, &error);
	if (! gda_query_is_select_query (query)) {
		g_object_unref (query);
		return;
	}
	if (error) {
		g_error_free (error);
		error = NULL;
	}
	gda_dict_assume_object (config->dict, (GdaObject *) query);
	g_object_unref (query);	

	tr = xmlNewChild (table, NULL, "tr", NULL);
	td = xmlNewChild (tr, NULL, "td", sql);
	xmlSetProp (td, "colspan", "3");

	/* compute queries, and display the results */
	model = gda_data_model_query_new (query);
	tr = xmlNewChild (table, NULL, "tr", NULL);
	if (!gda_data_model_query_compute_modification_queries (GDA_DATA_MODEL_QUERY (model), NULL, 0, &error)) {
		td = xmlNewChild (tr, NULL, "td", error && error->message ? error->message : "???");
		xmlSetProp (td, "colspan", "3");
		html_mark_node_warning (HTML_CONFIG (config), td);
		g_error_free (error);
	}
	else {
		GdaQuery *query;
		gchar *sql;
		g_object_get (G_OBJECT (model), "insert_query", &query, NULL);
		sql = gda_renderer_render_as_sql (GDA_RENDERER (query), NULL, GDA_RENDERER_EXTRA_VAL_ATTRS, &error);
		if (sql) {
			td = xmlNewChild (tr, NULL, "td", sql);	
			g_free (sql);
		}
		else {
			td = xmlNewChild (tr, NULL, "td", error && error->message ? error->message : "???");
			xmlSetProp (td, "colspan", "3");
			g_error_free (error);
			error = NULL;
		}
		g_object_get (G_OBJECT (model), "update_query", &query, NULL);
		sql = gda_renderer_render_as_sql (GDA_RENDERER (query), NULL, GDA_RENDERER_EXTRA_VAL_ATTRS, &error);
		if (sql) {
			td = xmlNewChild (tr, NULL, "td", sql);	
			g_free (sql);
		}
		else {
			td = xmlNewChild (tr, NULL, "td", error && error->message ? error->message : "???");
			xmlSetProp (td, "colspan", "3");
			g_error_free (error);
			error = NULL;
		}
		g_object_get (G_OBJECT (model), "delete_query", &query, NULL);
		sql = gda_renderer_render_as_sql (GDA_RENDERER (query), NULL, GDA_RENDERER_EXTRA_VAL_ATTRS, &error);
		if (sql) {
			td = xmlNewChild (tr, NULL, "td", sql);	
			g_free (sql);
		}
		else {
			td = xmlNewChild (tr, NULL, "td", error && error->message ? error->message : "???");
			xmlSetProp (td, "colspan", "3");
			g_error_free (error);
			error = NULL;
		}
	}
	g_object_unref (model);
}
