/* GDA library
 * Copyright (C) 2005 - 2006 The GNOME Foundation.
 *
 * AUTHORS:
 *	Vivien Malerba <malerba@gnome-db.org>
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gmodule.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-server-provider-extra.h>

#include "html.h"

/* options */
gchar *pass = NULL;
gchar *user = NULL;
gchar *dir = NULL;

static GOptionEntry entries[] = {
	{ "output-dir", 'o', 0, G_OPTION_ARG_STRING, &dir, "Output dir", "output directory"},
	{ "user", 'U', 0, G_OPTION_ARG_STRING, &user, "Username", "username" },
	{ "password", 'P', 0, G_OPTION_ARG_STRING, &pass, "Password", "password" },
	{ NULL }
};

typedef struct {
	HtmlConfig  config;
	GList      *dsn_to_test;
	GdaDataSourceInfo *current_dsn;	
} TestConfig;

/*
 * actual tests
 */
static void list_all_sections (TestConfig *config);
static void list_all_providers (TestConfig *config);
static void detail_provider (TestConfig *config, const gchar *provider);
static void detail_datasource (TestConfig *config, GdaDataSourceInfo *dsn);
static void list_datasource_info (TestConfig *config, xmlNodePtr parent, gchar *dsn);

static void test_provider (TestConfig *config, HtmlFile *file, GdaServerProvider *provider, GdaConnection *cnc);
static void test_data_handler (TestConfig *config, xmlNodePtr table1, xmlNodePtr table2, xmlNodePtr table3,
			       GdaDataHandler *dh, GType type, GdaServerProvider *prov);



gint main (int argc, char **argv) {
	GError *error = NULL;	
	GOptionContext *context;
	TestConfig *config;
	gchar *str;
	GSList *list;
	xmlNodePtr node;
	GList *dsnlist;

	/* command line parsing */
	context = g_option_context_new ("<Data source> - GDA diagnosis");
	g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
	if (!g_option_context_parse (context, &argc, &argv, &error)) {
		g_warning ("Can't parse arguments: %s", error->message);
		exit (1);
	}
	g_option_context_free (context);
	
	/* init */
	gda_init ("gda-diagnose", PACKAGE_VERSION, argc, argv);
	if (!dir)
		dir = "GDA_DIAGNOSE_OUTPUT";
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

	/* create config structure */
	config = g_new0 (TestConfig, 1);
	html_init_config (HTML_CONFIG (config));
	config->config.index = html_file_new (HTML_CONFIG (config), "index.html", "Gda diagnostic");		
        config->config.dir = str;

	{
		time_t now;
		struct tm *stm;

		now = time (NULL);
		stm = localtime (&now);
		str = g_strdup_printf (_("Generated @ %s"), asctime (stm));
		node = xmlNewChild (config->config.index->body, NULL, "p", str);
		g_free (str);
	}

	if (argc == 2) {
		GdaDataSourceInfo *dsn;
		dsn = gda_config_find_data_source (argv[1]);
		
		if (!dsn)
			g_error ("Can't find data source: %s\n", argv[1]);

		config->dsn_to_test = g_list_append (NULL, dsn);
	}
	else 
		config->dsn_to_test = gda_config_get_data_source_list ();
		/*gda_config_free_data_source_list (dsns);*/

	/* do tests */
	list_all_sections (config);
	list_all_providers (config);
	dsnlist = config->dsn_to_test;
	while (dsnlist) {
		detail_datasource (config, (GdaDataSourceInfo *)(dsnlist->data));
		dsnlist = g_list_next (dsnlist);
	}

	/* actual files writing to disk */
	list = config->config.all_files;
	while (list) {
		html_file_write ((HtmlFile*) list->data, HTML_CONFIG (config));
		list = g_slist_next (list);
	}
	g_slist_foreach (config->config.all_files, (GFunc) html_file_free, NULL);
	g_slist_free (config->config.all_files);

	return 0;
}








/*
 * Lists all the sections in the config files (local to the user and global) in the index page
 */
static void 
list_all_sections (TestConfig *config)
{
	xmlNodePtr node, li;
	GList *sections;
        GList *l;
        gchar *tmp;

	node = html_add_header (HTML_CONFIG (config), config->config.index, _("Data sources"));
	node = xmlNewChild (config->config.index->body, NULL, "p", _("List of data sources from the config files "
							      "(user and system wide)"));
	node = xmlNewChild (config->config.index->body, NULL, "ul", NULL);
	html_declare_node (HTML_CONFIG (config), "/config/ul", node);

        /* get all sections keys */
        sections = gda_config_list_sections ("/apps/libgda/Datasources");
        for (l = sections; l != NULL; l = l->next) {
                gchar *section = (gchar *) l->data;

		li = xmlNewChild (node, NULL, "li", section);
		tmp = g_strdup_printf ("/config/%s", section);
		html_declare_node_own (HTML_CONFIG (config), tmp, li);

		list_datasource_info (config, li, section);
        }
        gda_config_free_list (sections);
}

static void
list_datasource_info (TestConfig *config, xmlNodePtr parent, gchar *dsn)
{
	GList *keys;
	gchar *root;
	xmlNodePtr ul;
	gchar *tmp;

	/* list keys in dsn */
	root = g_strdup_printf ("/apps/libgda/Datasources/%s", dsn);
	keys = gda_config_list_keys (root);
	if (keys) {
		GList *l;
		
		ul = xmlNewChild (parent, NULL, "ul", NULL);
		xmlSetProp(ul, "class", (xmlChar*)"none");
		for (l = keys; l != NULL; l = l->next) {
			gchar *value;
			gchar *key = (gchar *) l->data;
			
			tmp = g_strdup_printf ("%s/%s", root, key);
			value = gda_config_get_string (tmp);
			g_free (tmp);
			
			tmp = g_strdup_printf ("Key '%s' = '%s'", key, value);
			xmlNewChild (ul, NULL, "li", tmp);
			g_free (tmp);
		}
		gda_config_free_list (keys);
	}
	g_free (root);
}

/*
 * make a list of all the providers in the index page
 */
static void
list_all_providers (TestConfig *config)
{
	xmlNodePtr node;
	GList *providers;

	node = html_add_header (HTML_CONFIG (config), config->config.index, _("Providers"));
	node = xmlNewChild (config->config.index->body, NULL, "p", _("Installed providers"));
	node = xmlNewChild (config->config.index->body, NULL, "ul", NULL);
	html_declare_node (HTML_CONFIG (config), "/providers/ul", node);	

	providers = gda_config_get_provider_list ();
	while (providers) {
		GdaProviderInfo *info = (GdaProviderInfo *) providers->data;
		xmlNodePtr ul;
		gchar *tmp;
		
		ul = xmlNewChild (node, NULL, "li", info->id);
		tmp = g_strdup_printf ("/providers/%s", info->id);
		html_declare_node_own (HTML_CONFIG (config), tmp, ul);
		
		detail_provider (config, info->id);

		providers = g_list_next (providers);
	}
}







/*
 * makes a specific page for the provider
 */
typedef struct {
	GModule              *handle;
	GdaServerProvider    *provider;

	/* entry points to the plugin */
	const gchar        *(*plugin_get_name) (void);
	const gchar        *(*plugin_get_description) (void);
	GdaServerProvider  *(*plugin_create_provider) (void);
	gchar              *(*get_dsn_spec) (void);
} LoadedProvider;

static void
detail_provider (TestConfig *config, const gchar *provider)
{
	xmlNodePtr node, ul;
	HtmlFile *file;
	gchar *filestr, *title, *str;
	GdaProviderInfo *info;
	GSList *params;
	LoadedProvider *prv = NULL;

	info = gda_config_get_provider_by_name (provider);
	if (!info)
		g_error ("Can't find provider '%s'", provider);

	filestr = g_strdup_printf ("%s.html", provider);
	title = g_strdup_printf (_("Provider '%s' details"), provider);
	file = html_file_new (HTML_CONFIG (config), filestr, title);
	g_free (title);

	str = g_strdup_printf ("/providers/%s", provider);
	html_add_link_to_node (HTML_CONFIG (config), str, "Details", filestr);
	g_free (str);
	g_free (filestr);

	node = html_add_header (HTML_CONFIG (config), file, _("Provider's information"));
	ul = xmlNewChild (file->body, NULL, "ul", NULL);

	html_render_attribute_str (ul, "li", _("Location"), info->location);
	html_render_attribute_str (ul, "li", _("Description"), info->description);

	if (info->gda_params) {
		xmlNodePtr table, td, tr;

		node = xmlNewChild (ul, NULL, "li", _("Data source's parameters:"));
		table = xmlNewChild (node, NULL, "table", NULL);
		tr = xmlNewChild (table, NULL, "tr", NULL);
		td = xmlNewChild (tr, NULL, "th", _("Parameter name"));
		td = xmlNewChild (tr, NULL, "th", _("Description"));
		td = xmlNewChild (tr, NULL, "th", _("Type"));

		params = info->gda_params->parameters;
		while (params) {
			gchar *tmp;

			g_object_get (G_OBJECT (params->data), "string_id", &tmp, NULL);
			tr = xmlNewChild (table, NULL, "tr", NULL);
			td = xmlNewChild (tr, NULL, "td", tmp);
			td = xmlNewChild (tr, NULL, "td", gda_object_get_name (GDA_OBJECT (params->data)));
			td = xmlNewChild (tr, NULL, "td", 
					  gda_g_type_to_string (gda_parameter_get_g_type (GDA_PARAMETER (params->data))));

			params = g_slist_next (params);
		}
	}
	else {
		node = xmlNewChild (ul, NULL, "li", _("Data source's parameters: none provided"));	
		html_mark_node_error (HTML_CONFIG (config), node);
	}

	if (info->dsn_spec) {
		xmlNodePtr tt;

		node = xmlNewChild (ul, NULL, "li", _("Detailed data source's parameters"));
		tt = xmlNewChild (node, NULL, "pre", info->dsn_spec);
	}
	else {
		node = xmlNewChild (ul, NULL, "li", _("Detailed data source's parameters: none provided"));
		html_mark_node_error (HTML_CONFIG (config), node);
	}

	/* loading provider */
	prv = g_new0 (LoadedProvider, 1);
	prv->handle = g_module_open (info->location, G_MODULE_BIND_LAZY);

	if (!prv->handle)
		g_error ("Can't load provider '%s''s module: %s", provider, g_module_error ());
	
	g_module_symbol (prv->handle, "plugin_get_name",
			 (gpointer) &prv->plugin_get_name);
	g_module_symbol (prv->handle, "plugin_get_description",
			 (gpointer) &prv->plugin_get_description);
	g_module_symbol (prv->handle, "plugin_create_provider",
			 (gpointer) &prv->plugin_create_provider);
	g_module_symbol (prv->handle, "plugin_get_dsn_spec",
			 (gpointer) &prv->get_dsn_spec);
	
	if (!prv->plugin_create_provider) 
		g_error ("Provider '%s' does not implement entry function", provider);

	prv->provider = prv->plugin_create_provider ();
	if (!prv->provider)
		g_error ("Could not create GdaServerProvider object from plugin ('%s' provider)", provider);
	else {
		xmlNodePtr table, td, tr;

		node = html_add_header (HTML_CONFIG (config), file, _("Plugin's entry functions"));
		table = xmlNewChild (node, NULL, "table", NULL);
		tr = xmlNewChild (table, NULL, "tr", NULL);
		td = xmlNewChild (tr, NULL, "th", _("Function"));
		td = xmlNewChild (tr, NULL, "th", _("Present?"));
		
		tr = xmlNewChild (table, NULL, "tr", NULL);
		td = xmlNewChild (tr, NULL, "td", "plugin_get_name()");
		td = xmlNewChild (tr, NULL, "td", prv->plugin_get_name ? _("Yes") : _("No"));
		
		tr = xmlNewChild (table, NULL, "tr", NULL);
		td = xmlNewChild (tr, NULL, "td", "plugin_get_description()");
		td = xmlNewChild (tr, NULL, "td", prv->plugin_get_description ? _("Yes") : _("No"));
				
		tr = xmlNewChild (table, NULL, "tr", NULL);
		td = xmlNewChild (tr, NULL, "td", "plugin_create_provider()");
		td = xmlNewChild (tr, NULL, "td", prv->plugin_create_provider ? _("Yes") : _("No"));
		
		tr = xmlNewChild (table, NULL, "tr", NULL);
		td = xmlNewChild (tr, NULL, "td", "plugin_get_dsn_spec()");
		td = xmlNewChild (tr, NULL, "td", prv->get_dsn_spec ? _("Yes") : _("No"));

		test_provider (config, file, prv->provider, NULL);
		g_object_unref (prv->provider);
	}
}

typedef struct {
	gchar   *name;
	GFunc    func;
	gboolean required;
} ProviderFunc;


/*
 * Test a provider instance
 */
static void
test_provider (TestConfig *config, HtmlFile *file, GdaServerProvider *provider, GdaConnection *cnc)
{
	xmlNodePtr table, td, tr, p;
	GdaServerProviderClass *class = (GdaServerProviderClass *) G_OBJECT_GET_CLASS (provider);
	xmlNodePtr node;
	gint i;
	
	ProviderFunc func_test [] = {
		{"get_version", (GFunc) class->get_version, TRUE},
		{"get_server_version", (GFunc) class->get_server_version, TRUE},
		{"get_info", (GFunc) class->get_info, TRUE},
		{"supports", (GFunc) class->supports_feature, TRUE},
		{"get_schema", (GFunc) class->get_schema, TRUE},

		{"get_data_handler", (GFunc) class->get_data_handler, TRUE},
		{"string_to_value", (GFunc) class->string_to_value, FALSE},
		{"get_def_dbms_type", (GFunc) class->get_def_dbms_type, FALSE},

		{"open_connection", (GFunc) class->open_connection, TRUE},
		{"close_connection", (GFunc) class->close_connection, TRUE},
		{"get_database", (GFunc) class->get_database, TRUE},
		{"change_database", (GFunc) class->change_database, FALSE},

		{"supports_operation", (GFunc) class->supports_operation, FALSE},
		{"create_operation", (GFunc) class->create_operation, FALSE},
		{"render_operation", (GFunc) class->render_operation, FALSE},
		{"perform_operation", (GFunc) class->perform_operation, FALSE},

		{"execute_command", (GFunc) class->execute_command, TRUE},
		{"get_last_insert_id", (GFunc) class->get_last_insert_id, FALSE},

		{"begin_transaction", (GFunc) class->begin_transaction, FALSE},
		{"commit_transaction", (GFunc) class->commit_transaction, FALSE},
		{"rollback_transaction", (GFunc) class->rollback_transaction, FALSE},

		{"create_blob", (GFunc) class->create_blob, FALSE},
		{"fetch_blob", (GFunc) class->fetch_blob, FALSE},
	};

	g_assert (provider);
	if (cnc)
		g_assert (GDA_IS_CONNECTION (cnc));

	/*
	 * Virtual functions
	 */
	node = html_add_header (HTML_CONFIG (config), file, _("GdaServerProvider object's virtual functions"));
	table = xmlNewChild (node, NULL, "table", NULL);
	tr = xmlNewChild (table, NULL, "tr", NULL);
	td = xmlNewChild (tr, NULL, "th", _("Function"));
	td = xmlNewChild (tr, NULL, "th", _("Present?"));
	td = xmlNewChild (tr, NULL, "th", cnc ? _("Result") : _("Result without any connection"));

	/* get_version */
	tr = xmlNewChild (table, NULL, "tr", NULL);
	td = xmlNewChild (tr, NULL, "td", "get_version()");
	td = xmlNewChild (tr, NULL, "td",  class->get_version ? _("Yes") : _("No"));
	td = xmlNewChild (tr, NULL, "td",  (class->get_version) (provider));

	/* get_server_version */
	tr = xmlNewChild (table, NULL, "tr", NULL);
	td = xmlNewChild (tr, NULL, "td", "get_server_version()");
	td = xmlNewChild (tr, NULL, "td",  class->get_server_version ? _("Yes") : _("No"));
	if (class->get_server_version) {
		const gchar *tmp;

		tmp = (class->get_server_version) (provider, cnc);
		td = xmlNewChild (tr, NULL, "td",  tmp ? tmp : "NULL");
	}

	/* get_info */
	tr = xmlNewChild (table, NULL, "tr", NULL);
	td = xmlNewChild (tr, NULL, "td", "get_info()");
	td = xmlNewChild (tr, NULL, "td",  class->get_info ? _("Yes") : _("No"));
	if (class->get_info) {
		GdaServerProviderInfo *pinfo;

		pinfo = (class->get_info) (provider, cnc);
		if (pinfo) {
			xmlNodePtr ul;

			td = xmlNewChild (tr, NULL, "td",  NULL);
			ul = xmlNewChild (td, NULL, "ul",  NULL);
			xmlSetProp(ul, "class", (xmlChar*)"none");

			html_render_attribute_str (ul, "li", "provider_name", pinfo->provider_name);
			html_render_attribute_bool (ul, "li", "is_case_insensitive", pinfo->is_case_insensitive);
			html_render_attribute_bool (ul, "li", "implicit_data_types_casts", pinfo->implicit_data_types_casts);
			html_render_attribute_bool (ul, "li", "alias_needs_as_keyword", pinfo->alias_needs_as_keyword);
		}
		else {
			td = xmlNewChild (tr, NULL, "td",  "NULL");
			html_mark_node_error (HTML_CONFIG (config), td);
		}
	}
	else
		td = xmlNewChild (tr, NULL, "td", "---");	

	/* supports */
	tr = xmlNewChild (table, NULL, "tr", NULL);
	td = xmlNewChild (tr, NULL, "td", "supports()");
	td = xmlNewChild (tr, NULL, "td",  class->supports_feature ? _("Yes") : _("No"));
	if (class->supports_feature) {
		xmlNodePtr ul;
		GdaConnectionFeature feature;
		const gchar *feature_name[] = {"Aggregates",
					       "Blobs",
					       "Indexes",
					       "Inheritance",
					       "Namespaces",
					       "Procedures",
					       "Sequences",
					       "SQL",
					       "Transactions",
					       "Triggers",
					       "Updatable cursors",
					       "Users",
					       "Views",
					       "XML queries"};

		td = xmlNewChild (tr, NULL, "td",  NULL);
		ul = xmlNewChild (td, NULL, "ul",  NULL);
		xmlSetProp(ul, "class", (xmlChar*)"none");
		for (feature=GDA_CONNECTION_FEATURE_AGGREGATES; 
		     feature <= GDA_CONNECTION_FEATURE_XML_QUERIES;
		     feature++) 
			html_render_attribute_bool (ul, "li", feature_name[feature],
					       (class->supports_feature) (provider, cnc, feature));
	}
	else 
		html_mark_node_error (HTML_CONFIG (config), td);

	/* Other functions */
	for (i=0; i < (sizeof (func_test) / sizeof (ProviderFunc)); i++) {
		gchar *tmp;

		tmp = g_strdup_printf ("%s ()", func_test[i].name);
		tr = xmlNewChild (table, NULL, "tr", NULL);
		td = xmlNewChild (tr, NULL, "td", tmp);
		g_free (tmp);
		td = xmlNewChild (tr, NULL, "td",  func_test[i].func ? _("Yes") : _("No"));
		if (func_test[i].required && !func_test[i].func)
			html_mark_node_error (HTML_CONFIG (config), td);
		td = xmlNewChild (tr, NULL, "td", NULL);
	}

	/*
	 * Schemas rendering
	 */
	if (cnc) {
		node = html_add_header (HTML_CONFIG (config), file, _("Schemas reporting"));
		if (!class->get_schema) {
			node = xmlNewChild (file->body, NULL, "h3", _("Schemas not reported by this provider"));
			html_mark_node_error (HTML_CONFIG (config), node);
		}
		else {
			typedef struct {
				GdaConnectionFeature feature;
				GdaConnectionSchema  schema;
			} SchemaFeature;
			gint i;

			SchemaFeature schemas [] = {
				{-1, GDA_CONNECTION_SCHEMA_DATABASES},
				{-1, GDA_CONNECTION_SCHEMA_LANGUAGES},
				{-1, GDA_CONNECTION_SCHEMA_TYPES},
				{-1, GDA_CONNECTION_SCHEMA_TABLES},
				{GDA_CONNECTION_FEATURE_VIEWS, GDA_CONNECTION_SCHEMA_VIEWS},
				{GDA_CONNECTION_FEATURE_SEQUENCES, GDA_CONNECTION_SCHEMA_SEQUENCES},
				{GDA_CONNECTION_FEATURE_PROCEDURES, GDA_CONNECTION_SCHEMA_PROCEDURES},
				{GDA_CONNECTION_FEATURE_AGGREGATES, GDA_CONNECTION_SCHEMA_AGGREGATES},
				{GDA_CONNECTION_FEATURE_INDEXES, GDA_CONNECTION_SCHEMA_INDEXES},
				{GDA_CONNECTION_FEATURE_NAMESPACES, GDA_CONNECTION_SCHEMA_NAMESPACES},
				{GDA_CONNECTION_FEATURE_TRIGGERS, GDA_CONNECTION_SCHEMA_TRIGGERS},
				{GDA_CONNECTION_FEATURE_USERS, GDA_CONNECTION_SCHEMA_USERS},
			};

			const gchar *schema_name[] = {"Aggregates",
						      "Databases",
						      "Fields",
						      "Indexes",
						      "Languages",
						      "Namespaces",
						      "Parent tables",
						      "Procedures",
						      "Sequences",
						      "Tables",
						      "Triggers",
						      "Types",
						      "Users",
						      "Views",
						      "Constraints"};
			
			for (i = 0; i < (sizeof (schemas) / sizeof (SchemaFeature)); i++) {
				SchemaFeature *current = &(schemas[i]);
				GdaDataModel *model;
				xmlNodePtr sect;
				
				if ((current->feature == -1) ||
				    (class->supports_feature && (class->supports_feature) (provider, cnc, current->feature))) {
					sect = xmlNewChild (node, NULL, "h3", schema_name[current->schema]);
					xmlNewChild (node, NULL, "p", "\n");
					
					model = (class->get_schema) (provider, cnc, current->schema, NULL);
					if (model && (gda_data_model_get_n_rows (model) != 0)) {
						GError *error = NULL;
						if (! gda_server_provider_test_schema_model (model, current->schema, &error)) {
							gchar *str;

							str = g_strdup_printf (_("Schema Error: %s"),
									       error && error->message ? error->message :
									       _("Unknown error"));
							node = xmlNewChild (sect, NULL, "p", str);
							g_free (str);
							g_error_free (error);
							html_mark_node_error (HTML_CONFIG (config), node);
						}

						html_render_data_model (sect, model);

						if ((current->schema == GDA_CONNECTION_SCHEMA_TABLES) ||
						    (current->schema == GDA_CONNECTION_SCHEMA_VIEWS)) {
							/* list the table's and view's fields */
							gint nrows, row;

							nrows = gda_data_model_get_n_rows (model);
							for (row = 0; row < nrows; row++) {
								GdaDataModel *fields;
								GdaParameterList *params;
								GdaParameter *param;
								gchar *tmp;
								const GValue *name;
								
								param = gda_parameter_new (G_TYPE_STRING);
								name = gda_data_model_get_value_at (model, 0, row);
								gda_object_set_name (GDA_OBJECT (param), "name");
								gda_parameter_set_value (param, name);
								params = gda_parameter_list_new (NULL);
								gda_parameter_list_add_param (params, param);
								g_object_unref (param);

								fields = (class->get_schema) (provider, cnc, 
									     GDA_CONNECTION_SCHEMA_FIELDS, params);
								g_object_unref (params);
								tmp = g_strdup_printf (_("Fields for '%s'"),
										       g_value_get_string (name));
								xmlNewChild (sect, NULL, "h4", tmp);
								g_free (tmp);
								if (fields) {
									if (gda_data_model_get_n_rows (fields) != 0)
										html_render_data_model (sect, fields);
									else {
										node = xmlNewChild (sect, NULL, "p",
											   _("No field returned"));
										html_mark_node_error (HTML_CONFIG (config), node);	
									}
									g_object_unref (fields);
								}
								else {
									node = xmlNewChild (sect, NULL, "p", 
											    _("Error"));
									html_mark_node_error (HTML_CONFIG (config), node);	
								}
							}
						}

						if (current->schema == GDA_CONNECTION_SCHEMA_TABLES) {
							/* list the table's constraints */
							gint nrows, row;

							nrows = gda_data_model_get_n_rows (model);
							for (row = 0; row < nrows; row++) {
								GdaDataModel *constraints;
								GdaParameterList *params;
								GdaParameter *param;
								gchar *tmp;
								const GValue *name;
								
								param = gda_parameter_new (G_TYPE_STRING);
								name = gda_data_model_get_value_at (model, 0, row);
								gda_object_set_name (GDA_OBJECT (param), "name");
								gda_parameter_set_value (param, name);
								params = gda_parameter_list_new (NULL);
								gda_parameter_list_add_param (params, param);
								g_object_unref (param);

								constraints = (class->get_schema) (provider, cnc, 
									     GDA_CONNECTION_SCHEMA_CONSTRAINTS, params);
								g_object_unref (params);
								tmp = g_strdup_printf (_("Constraints for '%s'"),
										       g_value_get_string (name));
								xmlNewChild (sect, NULL, "h4", tmp);
								g_free (tmp);
								if (constraints) {
									if (gda_data_model_get_n_rows (constraints) != 0)
										html_render_data_model (sect, constraints);
									else
										xmlNewChild (sect, NULL, "p",
											     _("No constraint"));
									g_object_unref (constraints);
								}
								else {
									node = xmlNewChild (sect, NULL, "p", 
											    _("Error"));
									html_mark_node_error (HTML_CONFIG (config), node);	
								}
							}
						}
						g_object_unref (model);
					}
					else {
						if (model) {
							node = xmlNewChild (sect, NULL, "p", _("Nothing to return"));
							g_object_unref (model);
						}
						else {
							node = xmlNewChild (sect, NULL, "p", _("Error"));
							html_mark_node_error (HTML_CONFIG (config), node);
						}
					}
				}
			}
		}
	}

	/*
	 * Data types testing
	 */
	node = html_add_header (HTML_CONFIG (config), file, _("Data types handling"));
	if (!class->get_data_handler) {
		node = xmlNewChild (file->body, NULL, "h3", _("This provider does not support data handlers"));
		html_mark_node_error (HTML_CONFIG (config), node);
	}
	else {
		GType type;
		gint i;
		xmlNodePtr hnode, htable1, htable2, htable3;

                #define NB_TESTED_TYPES 19
		GType tested_types[NB_TESTED_TYPES];
		tested_types [0] = G_TYPE_INT64;
		tested_types [1] = G_TYPE_UINT64;
		tested_types [2] = GDA_TYPE_BINARY;
		tested_types [3] = GDA_TYPE_BLOB;
		tested_types [4] = G_TYPE_BOOLEAN;
		tested_types [5] = G_TYPE_DATE;
		tested_types [6] = G_TYPE_DOUBLE;
		tested_types [7] = G_TYPE_INT;
		tested_types [8] = GDA_TYPE_NUMERIC;
		tested_types [9] = G_TYPE_FLOAT;
		tested_types [10] = GDA_TYPE_SHORT;
		tested_types [11] = GDA_TYPE_USHORT;
		tested_types [12] = G_TYPE_STRING;
		tested_types [13] = GDA_TYPE_TIME;
		tested_types [14] = GDA_TYPE_TIMESTAMP;
		tested_types [15] = G_TYPE_CHAR;
		tested_types [16] = G_TYPE_UCHAR;
		tested_types [17] = G_TYPE_ULONG;
		tested_types [18] = G_TYPE_UINT;
		
		p = xmlNewChild (node, NULL, "p", _("Data handler marked as error cannot handle the data type "
						    "they are returned for."));
		
		table = xmlNewChild (node, NULL, "table", NULL);
		tr = xmlNewChild (table, NULL, "tr", NULL);
		td = xmlNewChild (tr, NULL, "th", "Gda type");
		td = xmlNewChild (tr, NULL, "th", "Returned data handler");
		td = xmlNewChild (tr, NULL, "th", "Proposed initial value (as SQL)");
		td = xmlNewChild (tr, NULL, "th", "Default DBMS type returned by provider");

		hnode = html_add_header (HTML_CONFIG (config), file, _("Data handlers test"));
		xmlNewChild (hnode, NULL, "h3", _("get_sql_from_value() and get_str_from_value()"));
		htable1 = xmlNewChild (hnode, NULL, "table", NULL);
		tr = xmlNewChild (htable1, NULL, "tr", NULL);
		td = xmlNewChild (tr, NULL, "th", "Gda type");
		td = xmlNewChild (tr, NULL, "th", "Value as string");
		td = xmlNewChild (tr, NULL, "th", "Translated as SQL");
		td = xmlNewChild (tr, NULL, "th", "Translated as a string");

		xmlNewChild (hnode, NULL, "h3", _("get_value_from_sql()"));
		htable2 = xmlNewChild (hnode, NULL, "table", NULL);
		tr = xmlNewChild (htable2, NULL, "tr", NULL);
		td = xmlNewChild (tr, NULL, "th", "Gda type");
		td = xmlNewChild (tr, NULL, "th", "String");
		td = xmlNewChild (tr, NULL, "th", "Value converted by data handler to SQL");

		xmlNewChild (hnode, NULL, "h3", _("get_value_from_str()"));
		htable3 = xmlNewChild (hnode, NULL, "table", NULL);
		tr = xmlNewChild (htable3, NULL, "tr", NULL);
		td = xmlNewChild (tr, NULL, "th", "Gda type");
		td = xmlNewChild (tr, NULL, "th", "String");
		td = xmlNewChild (tr, NULL, "th", "Value converted by data handler to string");
		td = xmlNewChild (tr, NULL, "th", "Value converted by data handler to SQL");

		for (i = 0; i < NB_TESTED_TYPES; i++) {
			GdaDataHandler *dh;
			const gchar *dbms_type = NULL;
			gchar *tmp;
			type = tested_types [i];

			tr = xmlNewChild (table, NULL, "tr", NULL);
			td = xmlNewChild (tr, NULL, "th", gda_g_type_to_string (type));
			
			tmp = g_strdup_printf ("/providers/%p/%s", provider, gda_g_type_to_string (type));
			html_declare_node_own (HTML_CONFIG (config), tmp, td);

			dh = (class->get_data_handler) (provider, cnc, type, NULL);
			if (dh) {
				td = xmlNewChild (tr, NULL, "td", gda_data_handler_get_descr (dh));
				if (! gda_data_handler_accepts_g_type (dh, type))
					html_mark_node_error (HTML_CONFIG (config), td);
				else {
					GValue *ivalue;
					/* extra tests for that data handler */
					test_data_handler (config, 
							   htable1, htable2, htable3, dh, type, provider);

					ivalue = gda_data_handler_get_sane_init_value (dh, type);
					if (ivalue) {
						tmp = gda_data_handler_get_sql_from_value (dh, ivalue);
						td = xmlNewChild (tr, NULL, "td", tmp);
						gda_value_free (ivalue);
					}
					else {
						td = xmlNewChild (tr, NULL, "td", "none");
						xmlSetProp(td, "class", (xmlChar*)"null");
					}
				}
			}
			else {
				td = xmlNewChild (tr, NULL, "td", "");
				td = xmlNewChild (tr, NULL, "td", "");
			}
			
			if (class->get_def_dbms_type) {
				dbms_type = (class->get_def_dbms_type) (provider, cnc, type);
				td = xmlNewChild (tr, NULL, "td", dbms_type ? dbms_type : "none");
				if (dh && !dbms_type)
					html_mark_node_error (HTML_CONFIG (config), td);
				else
					xmlSetProp(td, "class", (xmlChar*)"null");
			}
			else
				td = xmlNewChild (tr, NULL, "td", "");
		}
	}

	/*html_file_write (file, config);*/
}

typedef struct {
	gchar    *str; /* string */
	gboolean  free_str; /* TRUE if @str is dynamically allocated */
	GValue *value; /* value to compare to when converted to a value if not NULL */
} StringValue;

static void string_values_free (StringValue *values);
static void real_test_data_handler (TestConfig *config, GdaDataHandler *dh, GType type,
				    xmlNodePtr table1, xmlNodePtr table2, xmlNodePtr table3,
				    GSList *values, StringValue *sql_values, StringValue *str_values);

/*
 * test a data handler instance
 *
 * @table1: a table node for the get_sql_from_value() and get_str_from_value() tests
 * @table2: a table node to test the get_value_from_sql() function
 * @table3: a table node to test the get_value_from_str() function
 */
static void
test_data_handler (TestConfig *config, 
		   xmlNodePtr table1, xmlNodePtr table2, xmlNodePtr table3,
		   GdaDataHandler *dh, GType type, GdaServerProvider *prov)
{
	GSList *values = NULL, *list;
	GValue *tmpval;

	g_assert (dh);

	if (type == G_TYPE_INT64) {
		StringValue int_sql_values [] = {
			{g_strdup_printf ("%lld", G_MININT64), TRUE, NULL},
			{g_strdup_printf ("%lld", G_MAXINT64), TRUE, NULL},
			{NULL, FALSE, NULL}};

		g_value_set_int64 (tmpval = gda_value_new (G_TYPE_INT64), G_MININT64);
		values = g_slist_prepend (NULL, tmpval);
		g_value_set_int64 (tmpval = gda_value_new (G_TYPE_INT64), G_MAXINT64);
		values = g_slist_prepend (values, tmpval);

		real_test_data_handler (config, dh, type, table1, table2, table3, 
					values, int_sql_values, int_sql_values);
		string_values_free (int_sql_values);
	}
        else if (type == G_TYPE_UINT64) {
		StringValue uint_sql_values [] = {
			{g_strdup_printf ("%llu", G_MAXUINT64), TRUE, NULL},
			{NULL, FALSE, NULL}};

		g_value_set_uint64 (tmpval = gda_value_new (G_TYPE_UINT64), G_MAXUINT64);
		values = g_slist_prepend (NULL, tmpval);

		real_test_data_handler (config, dh, type, table1, table2, table3, 
					values, uint_sql_values, uint_sql_values);
		string_values_free (uint_sql_values);
	}
        else if (type == G_TYPE_BOOLEAN) {
		StringValue bool_sql_values [] = {
			{"TRUE", FALSE, NULL},
			{"true", FALSE, NULL},
			{"t", FALSE, NULL},
			{"FALSE", FALSE, NULL},
			{"false", FALSE, NULL},
			{"f", FALSE, NULL},
			{NULL, FALSE, NULL}};

		g_value_set_boolean (tmpval = gda_value_new (G_TYPE_BOOLEAN), TRUE);
		values = g_slist_prepend (NULL, tmpval);
		g_value_set_boolean (tmpval = gda_value_new (G_TYPE_BOOLEAN), FALSE);
		values = g_slist_prepend (values, tmpval);

		g_value_set_boolean (tmpval = gda_value_new (G_TYPE_BOOLEAN), TRUE);
		bool_sql_values[1].value = tmpval;
		g_value_set_boolean (tmpval = gda_value_new (G_TYPE_BOOLEAN), TRUE);
		bool_sql_values[2].value = tmpval;
		g_value_set_boolean (tmpval = gda_value_new (G_TYPE_BOOLEAN), FALSE);
		bool_sql_values[4].value = tmpval;
		g_value_set_boolean (tmpval = gda_value_new (G_TYPE_BOOLEAN), FALSE);
		bool_sql_values[5].value = tmpval;
		

		real_test_data_handler (config, dh, type, table1, table2, table3, 
					values, bool_sql_values, bool_sql_values);
		string_values_free (bool_sql_values);
	}
        else if (type == G_TYPE_DATE) {
		GDate *date;

		StringValue date_sql_values [] = {
			{"12-26-1900", FALSE, NULL},
			{"26/12/1900", FALSE, NULL},
			{"'12-26-1900'", FALSE, NULL},
			{"'26/12/1900'", FALSE, NULL},
			{NULL, FALSE, NULL}};

		date = g_date_new_dmy (26, 12, 1900);
		g_value_set_boxed (tmpval = gda_value_new (G_TYPE_DATE), date);
		values = g_slist_prepend (NULL, tmpval);
		
		g_value_set_boxed (tmpval = gda_value_new (G_TYPE_DATE), date);
		date_sql_values[0].value = tmpval;
		g_value_set_boxed (tmpval = gda_value_new (G_TYPE_DATE), date);
		date_sql_values[1].value = tmpval;
		g_value_set_boxed (tmpval = gda_value_new (G_TYPE_DATE), date);
		date_sql_values[2].value = tmpval;
		g_value_set_boxed (tmpval = gda_value_new (G_TYPE_DATE), date);
		date_sql_values[3].value = tmpval;
		g_date_free (date);

		real_test_data_handler (config, dh, type, table1, table2, table3, 
					values, date_sql_values, date_sql_values);
		string_values_free (date_sql_values);
	}
        else if (type == G_TYPE_INT) {
		StringValue int_sql_values [] = {
			{g_strdup_printf ("%d", G_MININT32), TRUE, NULL},
			{g_strdup_printf ("%d", G_MAXINT32), TRUE, NULL},
			{NULL, FALSE, NULL}};

		g_value_set_int (tmpval = gda_value_new (G_TYPE_INT), G_MININT32);
		values = g_slist_prepend (NULL, tmpval);
		g_value_set_int (tmpval = gda_value_new (G_TYPE_INT), G_MAXINT32);
		values = g_slist_prepend (values, tmpval);

		real_test_data_handler (config, dh, type, table1, table2, table3, 
					values, int_sql_values, int_sql_values);
		string_values_free (int_sql_values);
	}
        else if (type == G_TYPE_UINT) {
		StringValue uint_sql_values [] = {
			{g_strdup_printf ("%u", G_MAXUINT32), TRUE, NULL},
			{NULL, FALSE, NULL}};

		g_value_set_uint (tmpval = gda_value_new (G_TYPE_UINT), G_MAXUINT32);
		values = g_slist_prepend (NULL, tmpval);

		real_test_data_handler (config, dh, type, table1, table2, table3, 
					values, uint_sql_values, uint_sql_values);
		string_values_free (uint_sql_values);
	}
        else if (type == GDA_TYPE_SHORT) {
		StringValue int_sql_values [] = {
			{g_strdup_printf ("%d", G_MINSHORT), TRUE, NULL},
			{g_strdup_printf ("%d", G_MAXSHORT), TRUE, NULL},
			{NULL, FALSE, NULL}};

		gda_value_set_short (tmpval = gda_value_new (GDA_TYPE_SHORT), G_MINSHORT);
		values = g_slist_prepend (NULL, tmpval);
		gda_value_set_short (tmpval = gda_value_new (GDA_TYPE_SHORT), G_MAXSHORT);
		values = g_slist_prepend (values, tmpval);

		real_test_data_handler (config, dh, type, table1, table2, table3, 
					values, int_sql_values, int_sql_values);
		string_values_free (int_sql_values);
	}
        else if (type == GDA_TYPE_USHORT) {
		StringValue uint_sql_values [] = {
			{g_strdup_printf ("%u", G_MAXUSHORT), TRUE, NULL},
			{NULL, FALSE, NULL}};

		gda_value_set_ushort (tmpval = gda_value_new (GDA_TYPE_USHORT), G_MAXUSHORT);
		values = g_slist_prepend (NULL, tmpval);

		real_test_data_handler (config, dh, type, table1, table2, table3, 
					values, uint_sql_values, uint_sql_values);
		string_values_free (uint_sql_values);
	}
        else if (type == G_TYPE_STRING) {
		StringValue string_sql_values [] = {
			{"hello", FALSE, NULL},
			{"hello ()", FALSE, NULL},
			{"hell'o", FALSE, NULL},
			{"hell''o", FALSE, NULL},
			{"simple quote: '", FALSE, NULL},
			{"double quote: \"", FALSE, NULL},
			{"CR: \n", FALSE, NULL},
			{"'hello'", FALSE, NULL},
			{"'hello ()'", FALSE, NULL},
			{"'hell\\'o'", FALSE, NULL},
			{"'hell\\'\\'o'", FALSE, NULL},
			{"'hell''o'", FALSE, NULL},
			{"'simple quote: ''", FALSE, NULL},
			{"'simple quote: \''", FALSE, NULL},
			{"'double quote: \"'", FALSE, NULL},
			{"'CR: \n'", FALSE, NULL},
			{NULL, FALSE, NULL}};

		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), "hell'o");
		values = g_slist_prepend (NULL, tmpval);
		g_value_set_string (tmpval = gda_value_new (G_TYPE_STRING), "hell''o");
		values = g_slist_prepend (values, tmpval);

		real_test_data_handler (config, dh, type, table1, table2, table3, 
					values, string_sql_values, string_sql_values);
		string_values_free (string_sql_values);
	}
	else if (type == GDA_TYPE_TIME) {
		GdaTime tim;

		StringValue date_sql_values [] = {
			{"11:50:55", FALSE, NULL},
			{"23:50:55", FALSE, NULL},
			{"'11:50:55'", FALSE, NULL},
			{"'23:50:55'", FALSE, NULL},
			{NULL, FALSE, NULL}};

		tim.hour = 23;
		tim.minute = 50;
		tim.second = 55;
		tim.timezone = 0;
		gda_value_set_time (tmpval = gda_value_new (GDA_TYPE_TIME), &tim);
		values = g_slist_prepend (NULL, tmpval);
		tim.hour = 11;
		gda_value_set_time (tmpval = gda_value_new (GDA_TYPE_TIME), &tim);
		values = g_slist_prepend (NULL, tmpval);
		
		gda_value_set_time (tmpval = gda_value_new (GDA_TYPE_TIME), &tim);
		date_sql_values[0].value = tmpval;
		gda_value_set_time (tmpval = gda_value_new (GDA_TYPE_TIME), &tim);
		date_sql_values[2].value = tmpval;
		tim.hour = 23;
		gda_value_set_time (tmpval = gda_value_new (GDA_TYPE_TIME), &tim);
		date_sql_values[1].value = tmpval;
		gda_value_set_time (tmpval = gda_value_new (GDA_TYPE_TIME), &tim);
		date_sql_values[3].value = tmpval;

		real_test_data_handler (config, dh, type, table1, table2, table3, 
					values, date_sql_values, date_sql_values);
		string_values_free (date_sql_values);
	}
        else if (type == GDA_TYPE_TIMESTAMP){
		GdaTimestamp stamp;

		StringValue date_sql_values [] = {
			{"12-26-1900 11:50:55", FALSE, NULL},
			{"12-26-1900 23:50:55", FALSE, NULL},
			{"'12-26-1900 11:50:55'", FALSE, NULL},
			{"'12-26-1900 23:50:55'", FALSE, NULL},
			{NULL, FALSE, NULL}};
		
		stamp.year = 1900;
		stamp.month = 12;
		stamp.day = 26;
		stamp.hour = 23;
		stamp.minute = 50;
		stamp.second = 55;
		stamp.fraction = 0;
		stamp.timezone = 0;
		gda_value_set_timestamp (tmpval = gda_value_new (GDA_TYPE_TIMESTAMP), &stamp);
		values = g_slist_prepend (NULL, tmpval);
		stamp.hour = 11;
		gda_value_set_timestamp (tmpval = gda_value_new (GDA_TYPE_TIMESTAMP), &stamp);
		values = g_slist_prepend (NULL, tmpval);
		
		gda_value_set_timestamp (tmpval = gda_value_new (GDA_TYPE_TIMESTAMP), &stamp);
		date_sql_values[0].value = tmpval;
		gda_value_set_timestamp (tmpval = gda_value_new (GDA_TYPE_TIMESTAMP), &stamp);
		date_sql_values[2].value = tmpval;
		stamp.hour = 23;
		gda_value_set_timestamp (tmpval = gda_value_new (GDA_TYPE_TIMESTAMP), &stamp);
		date_sql_values[1].value = tmpval;
		gda_value_set_timestamp (tmpval = gda_value_new (GDA_TYPE_TIMESTAMP), &stamp);
		date_sql_values[3].value = tmpval;

		real_test_data_handler (config, dh, type, table1, table2, table3, 
					values, date_sql_values, date_sql_values);
		string_values_free (date_sql_values);
	}
        else if (type == G_TYPE_CHAR) {
		StringValue int_sql_values [] = {
			{g_strdup_printf ("%d", (gchar) 0x80), TRUE, NULL},
			{g_strdup_printf ("%d", (gchar) 0x7F), TRUE, NULL},
			{NULL, FALSE, NULL}};

		g_value_set_char (tmpval = gda_value_new (G_TYPE_CHAR), (gchar) 0x7F);
		values = g_slist_prepend (NULL, tmpval);
		g_value_set_char (tmpval = gda_value_new (G_TYPE_CHAR), (gchar) 0x80);
		values = g_slist_prepend (values, tmpval);

		real_test_data_handler (config, dh, type, table1, table2, table3, 
					values, int_sql_values, int_sql_values);
		string_values_free (int_sql_values);
	}
        else if (type == G_TYPE_UCHAR){
		StringValue uint_sql_values [] = {
			{g_strdup_printf ("%hhu", (gchar) 0xFF), TRUE, NULL},
			{NULL, FALSE, NULL}};

		g_value_set_uchar (tmpval = gda_value_new (G_TYPE_UCHAR), (gchar) 0xFF);
		values = g_slist_prepend (NULL, tmpval);

		real_test_data_handler (config, dh, type, table1, table2, table3, 
					values, uint_sql_values, uint_sql_values);
		string_values_free (uint_sql_values);
	}

	list = values;
	while (list) {
		if (list->data)
			gda_value_free ((GValue *) (list->data));
		list = g_slist_next (list);
	}

	g_slist_free (values);
}

static void
string_values_free (StringValue *values)
{
	gint i=0;
	StringValue *v;	

	v = values;
	while (v && v->str) {
		if (v->str && v->value)
			gda_value_free (v->value);
		if (v->str && v->free_str)
			g_free (v->str);
		i++;
		v = &(values [i]);
	}
}

static void
real_test_data_handler (TestConfig *config, GdaDataHandler *dh, GType type,
			xmlNodePtr table1, xmlNodePtr table2, xmlNodePtr table3,
			GSList *values, StringValue *sql_values, StringValue *str_values)
{
	xmlNodePtr tr, td;
	GValue *value, *cmpvalue;
	GSList *list;
	gchar *tmp;

	/* get_sql_from_value() and get_str_from_value() */
	value = gda_data_handler_get_sane_init_value (dh, type);
	if (value) {
		gchar *str;
		tr = xmlNewChild (table1, NULL, "tr", NULL);

		td = xmlNewChild (tr, NULL, "td", gda_g_type_to_string (type));

		tmp = gda_value_stringify (value);
		str = g_strdup_printf (_("%s (init value)"), tmp);
		g_free (tmp);
		td = xmlNewChild (tr, NULL, "td", str);
		g_free (str);
		
		tmp = gda_data_handler_get_sql_from_value (dh, value);
		td = xmlNewChild (tr, NULL, "td", tmp);
		g_free (tmp);

		tmp = gda_data_handler_get_str_from_value (dh, value);
		td = xmlNewChild (tr, NULL, "td", tmp);
		g_free (tmp);

		gda_value_free (value);
	}

	list = values;
	while (list) {
		tr = xmlNewChild (table1, NULL, "tr", NULL);
		value = (GValue *) (list->data);

		td = xmlNewChild (tr, NULL, "td", gda_g_type_to_string (type));

		tmp = gda_value_stringify (value);
		td = xmlNewChild (tr, NULL, "td", tmp);
		g_free (tmp);

		tmp = gda_data_handler_get_sql_from_value (dh, value);
		td = xmlNewChild (tr, NULL, "td", tmp);
		g_free (tmp);

		tmp = gda_data_handler_get_str_from_value (dh, value);
		td = xmlNewChild (tr, NULL, "td", tmp);
		g_free (tmp);

		list = g_slist_next (list);
	}

	/* get_value_from_sql */
	if (sql_values) {
		gchar *sql;
		gint i = 0;

		sql = sql_values[i].str;
		while (sql) {
			tr = xmlNewChild (table2, NULL, "tr", "\n");
			td = xmlNewChild (tr, NULL, "td", gda_g_type_to_string (type));
			td = xmlNewChild (tr, NULL, "td", sql);

			value = gda_data_handler_get_value_from_sql (dh, sql, type);
			if (value) {
				tmp = gda_data_handler_get_sql_from_value (dh, value);
				td = xmlNewChild (tr, NULL, "td", tmp);

				cmpvalue = sql_values[i].value;
				if (cmpvalue) {
					if (gda_value_compare (value, cmpvalue))
						html_mark_node_error (HTML_CONFIG (config), td);
				}
				else 
					if (!tmp || strcmp (tmp, sql))
						html_mark_node_error (HTML_CONFIG (config), td);
				g_free (tmp);

				gda_value_free (value);
			}
			else {
				td = xmlNewChild (tr, NULL, "td", "NULL");
				xmlSetProp(td, "class", (xmlChar*)"null");
			}

			i++;
			sql = sql_values[i].str;
		}
	}

	/* get_value_from_str */
	if (str_values) {
		gchar *sql;
		gint i = 0;

		sql = str_values[i].str;
		while (sql) {
			tr = xmlNewChild (table3, NULL, "tr", NULL);
			td = xmlNewChild (tr, NULL, "td", gda_g_type_to_string (type));
			td = xmlNewChild (tr, NULL, "td", sql);

			value = gda_data_handler_get_value_from_str (dh, sql, type);
			if (value) {
				/* str column */
				tmp = gda_data_handler_get_str_from_value (dh, value);
				td = xmlNewChild (tr, NULL, "td", tmp);

				cmpvalue = sql_values[i].value;
				if (cmpvalue) {
					if (gda_value_compare (value, cmpvalue))
						html_mark_node_error (HTML_CONFIG (config), td);
				}
				else 
					if (!tmp || strcmp (tmp, sql))
						html_mark_node_error (HTML_CONFIG (config), td);
				g_free (tmp);

				/* sql column */
				tmp = gda_data_handler_get_sql_from_value (dh, value);
				td = xmlNewChild (tr, NULL, "td", tmp);
				g_free (tmp);

				gda_value_free (value);
			}
			else {
				td = xmlNewChild (tr, NULL, "td", "NULL");
				xmlSetProp(td, "class", (xmlChar*)"null");
			}

			i++;
			sql = str_values[i].str;
		}
	}
}






static void
detail_datasource (TestConfig *config, GdaDataSourceInfo *dsn)
{
	GdaClient *client;
	HtmlFile *file;
	gchar *filestr, *title, *str;
	GdaConnection *cnc;
	xmlNodePtr node;
	GdaServerProvider *provider;
	GError *error = NULL;

	filestr = g_strdup_printf ("%s.html", dsn->name);
	title = g_strdup_printf (_("Datasource '%s' details"), dsn->name);
	file = html_file_new (HTML_CONFIG (config), filestr, title);
	g_free (title);

	str = g_strdup_printf ("/config/%s", dsn->name);
	html_add_link_to_node (HTML_CONFIG (config), str, "Details", filestr);
	g_free (str);
	g_free (filestr);

	node = html_add_header (HTML_CONFIG (config), file, _("Datasource's information"));
	list_datasource_info (config, file->body, dsn->name);

	client = gda_client_new ();
	cnc = gda_client_open_connection (client, dsn->name, 
					  user ? user : dsn->username, 
					  pass ? pass : ((dsn->password) ? dsn->password : ""),
					  0, &error);
	if (!cnc) {
		xmlNodePtr p;

		p = xmlNewChild (node, NULL, "p", _("Can't open connection."));
		html_mark_node_error (HTML_CONFIG (config), p);

		g_warning (_("Can't open connection to DSN %s: %s\n"), dsn->name,
			   error && error->message ? error->message : "???");
		g_error_free (error);

		return;
	}

	provider = gda_connection_get_provider_obj (cnc);
	test_provider (config, file, provider, cnc);
	gda_connection_close (cnc);

	g_object_unref (G_OBJECT (client));
}
