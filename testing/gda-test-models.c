#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <glib/gi18n-lib.h>

#include "html.h"

/* options */
gchar *dir = NULL;
gchar *infile = NULL;
extern xmlDtdPtr gda_array_dtd;

static GOptionEntry entries[] = {
	{ "output-dir", 'o', 0, G_OPTION_ARG_STRING, &dir, "Output dir", "output directory"},
	{ NULL }
};

typedef struct {
	HtmlConfig    config;
	xmlDocPtr     tests_doc;
        GdaDict      *dict; /* current dictionary */
	HtmlFile     *file; /* current file */
	xmlNodePtr    main_ul;	

	/* rem: for simplicity, models are never destroyed */
	GHashTable   *models; /* key = model name, value = GdaDataModel */ 
	GHashTable   *model_aliases; /* key = GdaDataMode, value = test alias */
} TestConfig;

/*
 * actual tests
 */
static void test_models (TestConfig *config);
static void test_and_destroy_dict (TestConfig *config);

gint main (int argc, char **argv) {
	GError *error = NULL;	
	GOptionContext *context;
	TestConfig *config;
	gchar *str;
	GSList *list;
	xmlNodePtr node, ul;

	/* command line parsing */
	context = g_option_context_new ("<tests file> - GdaDataModel testing");
	g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
	if (!g_option_context_parse (context, &argc, &argv, &error)) {
		g_warning ("Can't parse arguments: %s", error->message);
		exit (1);
	}
	g_option_context_free (context);
	
	/* init */
	gda_init ("gda-test-models", PACKAGE_VERSION, argc, argv);
	if (!dir)
		dir = "GDA_TESTS_MODELS_OUTPUT";
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
					      "index.html", "Gda SQL parsing tests");		
        config->config.dir = str;
	node = xmlNewChild (config->config.index->body, NULL, "h1", _("List of SQL parsing tests"));
	ul = xmlNewChild (config->config.index->body, NULL, "ul", NULL);
	config->main_ul = ul;
	config->models = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	config->model_aliases = g_hash_table_new_full (NULL, NULL, NULL, g_free);

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
	test_models (config);
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

	g_hash_table_destroy (config->models);
	g_hash_table_destroy (config->model_aliases);

	return 0;
}

/*
 * This function dumps the dictionary to a temporary file, then load that file into another
 * dictionary, and save it again and compares with the first dictionary.
 */
static void
test_and_destroy_dict (TestConfig *config)
{
	return;
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
							g_print ("%s\n", chout);
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

static void     register_model (TestConfig *config, GdaDataModel *model, const gchar *alias);
static gboolean model_check (TestConfig *config, GdaDataModel *model);
static void     handle_error (TestConfig *config, gchar *errmsg, gboolean expected_res); /* show error messages */

static void action_load_file (TestConfig *config, const gchar *model_name, xmlNodePtr node, gboolean expected_res);
static void action_create_proxy (TestConfig *config, const gchar *proxy_name, xmlNodePtr node, gboolean expected_res);
static void action_apply_proxy (TestConfig *config, GdaDataModel *model, xmlNodePtr node, gboolean expected_res);
static void action_append_data (TestConfig *config, GdaDataModel *model, xmlNodePtr node, gboolean expected_res);
static void action_set_value (TestConfig *config, GdaDataModel *model, xmlNodePtr node, gboolean expected_res);
static void action_remove_row (TestConfig *config, GdaDataModel *model, xmlNodePtr node, gboolean expected_res);
static void action_idle (TestConfig *config, xmlNodePtr node);
static void action_show_model (TestConfig *config, GdaDataModel *model, gboolean complete);

static void
test_models (TestConfig *config)
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
		

		/* header */
		if (!strcmp (subnode->name, "header")) {
			gchar *descr = xmlGetProp (subnode, "descr");

			html_add_header (HTML_CONFIG (config), config->file, descr);
			g_free (descr);
		}

		/* actions */
		if (!strcmp (subnode->name, "action")) {
			gchar *descr = xmlGetProp (subnode, "descr");
			gchar *type = xmlGetProp (subnode, "type");
			gchar *model_name = xmlGetProp (subnode, "model");
			gchar *expect_str = xmlGetProp (subnode, "expect");
			gboolean done = FALSE;
			gboolean expect_res;
			GdaDataModel *model = NULL;

			expect_res = !expect_str || (*expect_str == 't') || (*expect_str == 'T') ? TRUE : FALSE;
			g_free (expect_str);
			
			if (descr) {
				xmlNewChild (config->file->body, NULL, "div", descr);
				g_free (descr);
			}

			if (model_name) 
				model = g_hash_table_lookup (config->models, model_name);

			if (!strcmp (type, "load_file")) {
				done = TRUE;
				action_load_file (config, model_name, subnode, expect_res);
			}

			if (!strcmp (type, "create_proxy")) {
				done = TRUE;
				action_create_proxy (config, model_name, subnode, expect_res);
			}

			if (!strcmp (type, "apply_proxy")) {
				done = TRUE;
				action_apply_proxy (config, model, subnode, expect_res);
			}

			if (!done && !strcmp (type, "append_data")) {
				done = TRUE;
				action_append_data (config, model, subnode, expect_res);
			}

			if (!done && !strcmp (type, "set_value")) {
				done = TRUE;
				action_set_value (config, model, subnode, expect_res);
			}

			if (!done && !strcmp (type, "remove_row")) {
				done = TRUE;
				action_remove_row (config, model, subnode, expect_res);
			}

			if (!done && !strcmp (type, "idle")) {
				done = TRUE;
				action_idle (config, subnode);
			}

			if (!done && !strcmp (type, "show_model")) {
				done = TRUE;
				action_show_model (config, model, TRUE);
			}

			if (!done && !strcmp (type, "show_model_data")) {
				done = TRUE;
				action_show_model (config, model, FALSE);
			}

			

			g_free (type);
			g_free (model_name);
		}
		subnode = subnode->next;
	}
}

static void
action_load_file (TestConfig *config, const gchar *model_name, xmlNodePtr node, gboolean expected_res)
{
	GdaDataModel *import = NULL;
	gchar *file;
	gchar *errmsg = NULL;

	file = xmlGetProp (node, "file");
	if (!file)
		errmsg = g_strdup (_("Missing 'file' attribute"));
	else {
		GSList *errors;

		import = gda_data_model_import_new_file (file, FALSE, NULL);
		errors = gda_data_model_import_get_errors (GDA_DATA_MODEL_IMPORT (import));
		if (errors) {
			GError *err = (GError *) errors->data;
			
			errmsg = g_strdup (err && err->message ? err->message : _("Unknown error"));
			g_object_unref (import);
			import = NULL;
		}
		g_free (file);
	}

	if (import) {
		GdaDataModel *model;
		GError *error = NULL;

		model = gda_data_model_array_copy_model (import, &error);
		gda_object_set_id (GDA_OBJECT (model), gda_object_get_id (GDA_OBJECT (import)));
		if (model)
			register_model (config, model, model_name);
		else {
			errmsg = g_strdup (error && error->message ? error->message : _("Unknown error"));
			g_error_free (error);
		}
		g_object_unref (import);
	}

	handle_error (config, errmsg, expected_res);
}

static void 
action_create_proxy (TestConfig *config, const gchar *proxy_name, xmlNodePtr node, gboolean expected_res)
{
	GdaDataModel *proxy, *proxied;
	gchar *str;

	str = xmlGetProp (node, "proxied_model");
	if (!str) {
		g_warning ("%s(): test error: attibute 'proxied_model' not found", __FUNCTION__);
		return;
	}

	proxied = g_hash_table_lookup (config->models, str);
	g_free (str);
	if (!proxied) {
		g_warning ("%s(): test error: can't find GdaDatamodel to proxy", __FUNCTION__);
	}

	proxy = (GdaDataModel *) gda_data_proxy_new (proxied);
	gda_object_set_name (GDA_OBJECT (proxy), proxy_name);
	register_model (config, proxy, proxy_name);
}

static void
action_apply_proxy (TestConfig *config, GdaDataModel *model, xmlNodePtr node, gboolean expected_res)
{
	gchar *str;
	gchar *errmsg = NULL;
	gint row = -1;
	GError *error = NULL;

	g_return_if_fail (model_check (config, model));
	g_return_if_fail (GDA_IS_DATA_PROXY (model));

	str = xmlGetProp (node, "row");
	if (str) {
		row = atoi (str);
		g_free (str);
	}

	if (row < 0) {
		if (! gda_data_proxy_apply_all_changes (GDA_DATA_PROXY (model), &error)) {
			errmsg = g_strdup (error && error->message ? error->message : _("Unknown error"));
			if (error)
				g_error_free (error);
		}
	}
	else {
		if (! gda_data_proxy_apply_row_changes (GDA_DATA_PROXY (model), row, &error)) {
			errmsg = g_strdup (error && error->message ? error->message : _("Unknown error"));
			if (error)
				g_error_free (error);
		}
	}

	handle_error (config, errmsg, expected_res);
}

static void
action_append_data (TestConfig *config, GdaDataModel *model, xmlNodePtr node, gboolean expected_res)
{
	gchar *errmsg = NULL;
	GError *error = NULL;

	g_return_if_fail (model_check (config, model));

	if (! gda_data_model_add_data_from_xml_node (model, node->children, &error)) {
		errmsg = g_strdup (error && error->message ? error->message : _("Unknown error"));
		if (error)
			g_error_free (error);
	}
	
	handle_error (config, errmsg, expected_res);
}

static void
action_set_value (TestConfig *config, GdaDataModel *model, xmlNodePtr node, gboolean expected_res)
{
	gchar *str;
	gchar *errmsg = NULL;
	gint row, col;
	GdaColumn *column;
	xmlNodePtr vnode = node->children;

	g_return_if_fail (model_check (config, model));

	str = xmlGetProp (node, "row");
	if (!str) {
		g_warning ("%s(): test error: attibute 'row' not found", __FUNCTION__);
		return;
	}
	row = atoi (str);
	g_free (str);

	str = xmlGetProp (node, "col");
	if (!str) {
		g_warning ("%s(): test error: attibute 'col' not found", __FUNCTION__);
		return;
	}
	col = atoi (str);
	g_free (str);

	while (xmlNodeIsText (vnode))
		vnode = vnode->next;
	if (strcmp (vnode->name, "gda_value")) {
		g_warning ("%s(): test error: expected <gda_value>, got <%s>", __FUNCTION__, vnode->name);
		return;
	}
	
	column = gda_data_model_describe_column (model, col);
	if (!column)
		errmsg = g_strdup_printf ("Can't find column %d", col);
	else {
		GType gdatype;

		gdatype = gda_column_get_gda_type (column);
		if ((gdatype == G_TYPE_INVALID) ||
		    (gdatype == GDA_TYPE_NULL)) {
			errmsg = g_strdup (_("Cannot retrieve column data type (type is UNKNOWN or not specified)"));
		}
		else {
			gchar *isnull;
			GValue *value = NULL;

			isnull = xmlGetProp (vnode, "isnull");
			if (isnull) {
				if ((*isnull == 'f') || (*isnull == 'F')) {
					g_free (isnull);
					isnull = NULL;
				}
			}

			if (!isnull) {
				value = g_new0 (GValue, 1);
				if (!gda_value_set_from_string (value, xmlNodeGetContent (vnode), gdatype)) {
					g_free (value);
					errmsg = g_strdup_printf (_("Cannot interpret string as a valid %s value"), 
								   gda_type_to_string (gdatype));
				}
			}
			else
				g_free (isnull);
			
			
			if (!errmsg) {
				GError *error = NULL;
				
				if (!gda_data_model_set_value_at (model, col, row, value, &error)) {
					errmsg = g_strdup (error && error->message ? error->message : _("Unknown error"));
					if (error)
						g_error_free (error);
				}
				else {
					if (value)
						gda_value_free (value);
				}
			}
		}
	}
	
	handle_error (config, errmsg, expected_res);
}

static void
action_remove_row (TestConfig *config, GdaDataModel *model, xmlNodePtr node, gboolean expected_res)
{
	gchar *str;
	gchar *errmsg = NULL;
	GError *error = NULL;

	g_return_if_fail (model_check (config, model));

	str = xmlGetProp (node, "row");
	if (!str) {
		g_warning ("%s(): test error: attibute 'row' not found", __FUNCTION__);
		return;
	}
	
	if (!gda_data_model_remove_row (model, atoi (str), &error)) {
		errmsg = g_strdup (error && error->message ? error->message : _("Unknown error"));
		if (error)
			g_error_free (error);
	}
	
	handle_error (config, errmsg, expected_res);

	g_free (str);
}


static void
action_show_model (TestConfig *config, GdaDataModel *model, gboolean complete)
{
	g_return_if_fail (model_check (config, model));

	if (complete)
		html_render_data_model_all (config->file->body, model);
	else
		html_render_data_model (config->file->body, model);
}


gboolean
idle_end_cb (GMainLoop *loop)
{
	if (g_main_loop_is_running (loop))
		g_main_loop_quit (loop);
	return FALSE; /* remove timeout */
}

static void 
action_idle (TestConfig *config, xmlNodePtr node)
{
	GMainLoop *loop;
	gchar *str;
	gint wait_sec;

	str = xmlGetProp (node, "wait_sec");
	if (!str) {
		g_warning ("%s(): test error: attibute 'wait_sec' not found", __FUNCTION__);
		return;
	}
	
	wait_sec = atoi (str);
	g_free (str);

	loop = g_main_loop_new (NULL, FALSE);
	g_timeout_add (wait_sec * 1000, (GSourceFunc) idle_end_cb, loop);
	g_main_loop_run (loop);
	g_main_loop_unref (loop);
}


static gboolean
model_check (TestConfig *config, GdaDataModel *model)
{
	if (! GDA_IS_DATA_MODEL (model)) {
		xmlNodePtr notice;
			
		notice = xmlNewChild (config->file->body, NULL, "div", _("GdaDataModel required for this action"));
		html_mark_node_error (HTML_CONFIG (config), notice);
		return FALSE;
	}
	return TRUE;
}

static void
handle_error (TestConfig *config, gchar *errmsg, gboolean expected_res)
{
	if (errmsg) {
		xmlNodePtr notice;
			
		notice = xmlNewChild (config->file->body, NULL, "div", errmsg);
		g_free (errmsg); /* FREED HERE */
		if (expected_res)
			html_mark_node_error (HTML_CONFIG (config), notice);
	}
	else {
		if (!expected_res) {
			xmlNodePtr notice;
			
			notice = xmlNewChild (config->file->body, NULL, "div", "Should have failed");
			html_mark_node_error (HTML_CONFIG (config), notice);
		}
	}
}


/*
 * Signals
 */
static void
model_changed_cb (GdaDataModel *model, TestConfig *config)
{
	gchar *str;
	xmlNodePtr notice;

	str = g_strdup_printf (_("Model %s (name=%s) has changed!"), 
			       (gchar *) g_hash_table_lookup (config->model_aliases, model),
			       gda_object_get_name (GDA_OBJECT (model)));
	notice = xmlNewChild (config->file->body, NULL, "div", str);
	g_free (str);
	html_mark_node_notice (HTML_CONFIG (config), notice);
}

static void
model_row_inserted_cb (GdaDataModel *model, gint row, TestConfig *config)
{
	gchar *str;
	xmlNodePtr notice;

	str = g_strdup_printf (_("Model %s (name=%s): row %d inserted!"), 
			       (gchar *) g_hash_table_lookup (config->model_aliases, model),
			       gda_object_get_name (GDA_OBJECT (model)), row);
	notice = xmlNewChild (config->file->body, NULL, "div", str);
	g_free (str);
	html_mark_node_notice (HTML_CONFIG (config), notice);
}

static void
model_row_updated_cb (GdaDataModel *model, gint row, TestConfig *config)
{
	gchar *str;
	xmlNodePtr notice;

	str = g_strdup_printf (_("Model %s (name=%s): row %d updated!"), 
			       (gchar *) g_hash_table_lookup (config->model_aliases, model),
			       gda_object_get_name (GDA_OBJECT (model)), row);
	notice = xmlNewChild (config->file->body, NULL, "div", str);
	g_free (str);
	html_mark_node_notice (HTML_CONFIG (config), notice);
}

static void
model_row_removed_cb (GdaDataModel *model, gint row, TestConfig *config)
{
	gchar *str;
	xmlNodePtr notice;

	str = g_strdup_printf (_("Model %s (name=%s): row %d removed!"), 
			       (gchar *) g_hash_table_lookup (config->model_aliases, model),
			       gda_object_get_name (GDA_OBJECT (model)), row);
	notice = xmlNewChild (config->file->body, NULL, "div", str);
	g_free (str);
	html_mark_node_notice (HTML_CONFIG (config), notice);
}


static void
register_model (TestConfig *config, GdaDataModel *model, const gchar *alias)
{
	GdaDataModel *exist;

	g_hash_table_insert (config->models, g_strdup (alias), model);
	g_hash_table_insert (config->model_aliases, model, g_strdup (alias));

	if (gda_object_get_name (GDA_OBJECT (model)))
		g_hash_table_insert (config->models, g_strdup (gda_object_get_name (GDA_OBJECT (model))), 
				     model);

	g_signal_connect (G_OBJECT (model), "changed",
			  G_CALLBACK (model_changed_cb), config);
	g_signal_connect (G_OBJECT (model), "row_inserted",
			  G_CALLBACK (model_row_inserted_cb), config);
	g_signal_connect (G_OBJECT (model), "row_updated",
			  G_CALLBACK (model_row_updated_cb), config);
	g_signal_connect (G_OBJECT (model), "row_removed",
			  G_CALLBACK (model_row_removed_cb), config);
	/* rem: for simplicity, models are never destroyed */
	/*gda_object_connect_destroy (GDA_OBJECT (model),
	  G_CALLBACK (model_destroyed_cb), config);*/
}
