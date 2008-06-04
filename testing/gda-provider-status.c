/* GDA - provider status report
 * Copyright (C) 2008 The GNOME Foundation.
 *
 * AUTHORS:
 * 	Vivien Malerba <malerba@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include <libgda/libgda.h>
#include <sql-parser/gda-sql-parser.h>
#include "html.h"
#include <libgda/gda-enum-types.h>

/* options */
gboolean ask_pass = FALSE;
gchar *outfile = NULL;
gchar *raw_prov = NULL;

static GOptionEntry entries[] = {
        { "no-password-ask", 'p', 0, G_OPTION_ARG_NONE, &ask_pass, "Don't ast for a password when it is empty", NULL },
        { "prov", 'P', 0, G_OPTION_ARG_STRING, &raw_prov, "Provider to be used", NULL },
        { "output-file", 'o', 0, G_OPTION_ARG_STRING, &outfile, "Output file", "output file"},
        { NULL }
};

HtmlConfig *config;

static GdaConnection *open_connection (const gchar *cnc_string, GError **error);
static gboolean report_provider_status (GdaServerProvider *prov, GdaConnection *cnc);

int
main (int argc, char *argv[])
{
	GOptionContext *context;
	GError *error = NULL;
	int exit_status = EXIT_SUCCESS;
	GSList *list, *cnc_list = NULL;
	GdaServerProvider *prov = NULL;

	context = g_option_context_new ("[DSN|connection string]...");
	g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
        if (!g_option_context_parse (context, &argc, &argv, &error)) {
                g_print ("Can't parse arguments: %s\n", error->message);
		exit_status = EXIT_FAILURE;
		goto cleanup;
        }
        g_option_context_free (context);
        gda_init ();
	ask_pass = !ask_pass;

	config = g_new0 (HtmlConfig, 1);
	html_init_config (HTML_CONFIG (config));
	config->index = html_file_new (HTML_CONFIG (config), 
				       "index.html", "Providers status");
	config->dir = g_strdup (".");

	if (raw_prov) {
		prov = gda_config_get_provider_object (raw_prov, &error);
		if (!prov) {
			g_print ("Can't load the '%s' provider: %s\n", raw_prov,
				 error && error->message ? error->message : "No detail");
			return EXIT_FAILURE;
		}
		if (!report_provider_status (prov, NULL)) {
			exit_status = EXIT_FAILURE;
			goto cleanup;

		}
	}

	/* use connections if specified */
	gint i;
	for (i = 1; i < argc; i++) {
		/* open connection */
		GdaConnection *cnc;
		cnc = open_connection (argv[i], &error);
		if (!cnc) {
			g_print ("Can't open connection to '%s': %s\n", argv[i],
				 error && error->message ? error->message : "No detail");
			exit_status = EXIT_FAILURE;
			goto cleanup;
		}
		cnc_list = g_slist_append (cnc_list, cnc);
	}
	if (getenv ("GDA_SQL_CNC")) {
		GdaConnection *cnc;
		cnc = open_connection (getenv ("GDA_SQL_CNC"), &error);
		if (!cnc) {
			g_print ("Can't open connection defined by GDA_SQL_CNC: %s\n",
				 error && error->message ? error->message : "No detail");
			exit_status = EXIT_FAILURE;
			goto cleanup;
		}
		cnc_list = g_slist_append (cnc_list, cnc);
	}

	
	/* report provider's status for all the connections */
	for (list = cnc_list; list; list = list->next) {
		if (!report_provider_status (prov, GDA_CONNECTION (list->data))) {
			exit_status = EXIT_FAILURE;
			goto cleanup;

		}
	}

	g_slist_foreach (HTML_CONFIG (config)->all_files, (GFunc) html_file_write, config);
	
	/* cleanups */
 cleanup:
	g_slist_foreach (cnc_list, (GFunc) g_object_unref, NULL);
	g_slist_free (cnc_list);

	return exit_status;
}


/*
 * Open a connection
 */
static GdaConnection*
open_connection (const gchar *cnc_string, GError **error)
{
	GdaConnection *cnc = NULL;

	GdaDataSourceInfo *info;
	gchar *user, *pass, *real_cnc, *real_provider, *real_auth_string = NULL;
	gda_connection_string_split (cnc_string, &real_cnc, &real_provider, &user, &pass);
	if (!real_cnc) {
		g_free (user);
		g_free (pass);
		g_free (real_provider);
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_DSN_NOT_FOUND_ERROR, 
			     "Malformed connection string '%s'", cnc_string);
		return NULL;
	}

	if (ask_pass) {
		if (user && !*user) {
			gchar buf[80];
			g_print ("\tUsername for '%s': ", cnc_string);
			if (scanf ("%80s", buf) == -1) {
				g_free (real_cnc);
				g_free (user);
				g_free (pass);
				g_free (real_provider);
				g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_DSN_NOT_FOUND_ERROR, 
					     "No username for '%s'", cnc_string);
				return NULL;
			}
			g_free (user);
			user = g_strdup (buf);
		}
		if (pass && !*pass) {
			gchar buf[80];
			g_print ("\tPassword for '%s': ", cnc_string);
			if (scanf ("%80s", buf) == -1) {
				g_free (real_cnc);
				g_free (user);
				g_free (pass);
				g_free (real_provider);
				g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_DSN_NOT_FOUND_ERROR, 
					     "No password for '%s'", cnc_string);
				return NULL;
			}
			g_free (pass);
			pass = g_strdup (buf);
		}
		if (user || pass) {
			gchar *s1;
			s1 = gda_rfc1738_encode (user);
			if (pass) {
				gchar *s2;
				s2 = gda_rfc1738_encode (pass);
				real_auth_string = g_strdup_printf ("USERNAME=%s;PASSWORD=%s", s1, s2);
				g_free (s2);
			}
			else
				real_auth_string = g_strdup_printf ("USERNAME=%s", s1);
			g_free (s1);
		}
	}
	
	info = gda_config_get_dsn (real_cnc);
	if (info && !real_provider)
		cnc = gda_connection_open_from_dsn (cnc_string, real_auth_string, 0, error);
	else 
		cnc = gda_connection_open_from_string (NULL, cnc_string, real_auth_string, 0, error);
	
	g_free (real_cnc);
	g_free (user);
	g_free (pass);
	g_free (real_provider);
	g_free (real_auth_string);

	return cnc;
}

static gboolean
report_provider_status (GdaServerProvider *prov, GdaConnection *cnc)
{
	gchar *str;
	HtmlFile *file = config->index;

	if (prov && cnc && (prov != gda_connection_get_provider_obj (cnc)))
		/* ignoring connection as it has a different provider */
		return TRUE;
	g_assert (prov || cnc);

	/* section */
	if (cnc)
		str = g_strdup_printf ("Report for connection '%s'", gda_connection_get_cnc_string (cnc));
	else
		str = g_strdup_printf ("Report for '%s' provider", gda_server_provider_get_name (prov));
	html_add_header (HTML_CONFIG (config), file, str);
	g_free (str);

	/* provider info */
	if (!prov)
		prov = gda_connection_get_provider_obj (cnc);
	xmlNodePtr table, tr, td, hnode;
	GdaSqlParser *parser;

	hnode = xmlNewChild (file->body, NULL, "h3", "General information");	
	table = xmlNewChild (file->body, NULL, "table", NULL);
	//xmlSetProp(table, "width", (xmlChar*)"100%");
	tr = xmlNewChild (table, NULL, "tr", NULL);
	xmlNewChild (tr, NULL, "th", "Property");
	xmlNewChild (tr, NULL, "th", "Value");

	tr = xmlNewChild (table, NULL, "tr", NULL);
	xmlNewChild (tr, NULL, "td", "Provider's name");
	xmlNewChild (tr, NULL, "td", gda_server_provider_get_name (prov));

	tr = xmlNewChild (table, NULL, "tr", NULL);
	xmlNewChild (tr, NULL, "td", "Provider's version");
	xmlNewChild (tr, NULL, "td", gda_server_provider_get_version (prov));

	tr = xmlNewChild (table, NULL, "tr", NULL);
	xmlNewChild (tr, NULL, "td", "Provider's server version");
	xmlNewChild (tr, NULL, "td", cnc ? gda_server_provider_get_server_version (prov, cnc) : "---");

	parser = gda_server_provider_create_parser (prov, cnc);
	tr = xmlNewChild (table, NULL, "tr", NULL);
	xmlNewChild (tr, NULL, "td", "Creates its own SQL parser");
	xmlNewChild (tr, NULL, "td", parser ? "Yes" : "No");
	if (parser)
		g_object_unref (parser);

	/* supported features */
	GdaConnectionFeature f;
	hnode = xmlNewChild (file->body, NULL, "h3", "Supported features");
	table = xmlNewChild (file->body, NULL, "table", NULL);

	tr = xmlNewChild (table, NULL, "tr", NULL);
	xmlNewChild (tr, NULL, "th", "Feature");
	xmlNewChild (tr, NULL, "th", "Supported ?");
	for (f = 0; f < GDA_CONNECTION_FEATURE_LAST; f++) {
		GEnumValue *ev;

		ev = g_enum_get_value ((GEnumClass *) g_type_class_ref (GDA_TYPE_CONNECTION_FEATURE), f);

		tr = xmlNewChild (table, NULL, "tr", NULL);
		xmlNewChild (tr, NULL, "td", ev->value_name);
		xmlNewChild (tr, NULL, "td", gda_server_provider_supports_feature (prov, cnc, f) ? "Yes" : "No");
	}
	
	/* supported operations */
	GdaServerOperationType op;
	hnode = xmlNewChild (file->body, NULL, "h3", "Supported server operations");
	table = xmlNewChild (file->body, NULL, "table", NULL);

	tr = xmlNewChild (table, NULL, "tr", NULL);
	xmlNewChild (tr, NULL, "th", "Server operation");
	xmlNewChild (tr, NULL, "th", "Supported ?");
	for (op = 0; op < GDA_SERVER_OPERATION_NB; op++) {
		GEnumValue *ev;

		ev = g_enum_get_value ((GEnumClass *) g_type_class_ref (GDA_TYPE_SERVER_OPERATION_TYPE), op);

		tr = xmlNewChild (table, NULL, "tr", NULL);
		xmlNewChild (tr, NULL, "td", ev->value_name);
		xmlNewChild (tr, NULL, "td", gda_server_provider_supports_operation (prov, cnc, op, NULL) ? "Yes" : "No");
	}

	/* virtual methods implementation */
	gint i;
	typedef void (*AFunc) (void);
	typedef struct {
		const gchar *name;
		gboolean     should_be;
		void       (*func) (void);
	} ProvFunc;
	GdaServerProviderClass *pclass = (GdaServerProviderClass*) 
		G_OBJECT_GET_CLASS (prov);
	ProvFunc fa[] = {
		{"get_name", TRUE, (AFunc) pclass->get_name},
		{"get_version", TRUE, (AFunc) pclass->get_version},
		{"get_server_version", TRUE, (AFunc) pclass->get_server_version},
		{"supports_feature", TRUE, (AFunc) pclass->supports_feature},
		{"get_data_handler", TRUE, (AFunc) pclass->get_data_handler},
		{"get_def_dbms_type", TRUE, (AFunc) pclass->get_def_dbms_type},
		{"escape_string", TRUE, (AFunc) pclass->escape_string},
		{"unescape_string", TRUE, (AFunc) pclass->unescape_string},
		{"open_connection", TRUE, (AFunc) pclass->open_connection},
		{"close_connection", TRUE, (AFunc) pclass->close_connection},
		{"get_database", TRUE, (AFunc) pclass->get_database},
		{"supports_operation", TRUE, (AFunc) pclass->supports_operation},
		{"create_operation", FALSE, (AFunc) pclass->create_operation},
		{"render_operation", FALSE, (AFunc) pclass->render_operation},
		{"perform_operation", FALSE, (AFunc) pclass->perform_operation},
		{"begin_transaction", FALSE, (AFunc) pclass->begin_transaction},
		{"commit_transaction", FALSE, (AFunc) pclass->commit_transaction},
		{"rollback_transaction", FALSE, (AFunc) pclass->rollback_transaction},
		{"add_savepoint", FALSE, (AFunc) pclass->add_savepoint},
		{"rollback_savepoint", FALSE, (AFunc) pclass->rollback_savepoint},
		{"delete_savepoint", FALSE, (AFunc) pclass->delete_savepoint},
		{"create_parser", FALSE, (AFunc) pclass->create_parser},
		{"statement_to_sql", TRUE, (AFunc) pclass->statement_to_sql},
		{"statement_prepare", TRUE, (AFunc) pclass->statement_prepare},
		{"statement_execute", TRUE, (AFunc) pclass->statement_execute},
	};
	hnode = xmlNewChild (file->body, NULL, "h3", "Main virtual methods implementation");
	table = xmlNewChild (file->body, NULL, "table", NULL);

	tr = xmlNewChild (table, NULL, "tr", NULL);
	xmlNewChild (tr, NULL, "th", "Server's virtual method");
	xmlNewChild (tr, NULL, "th", "Implemented ?");

	for (i = 0; i < sizeof (fa) / sizeof (ProvFunc); i++) {
		gchar *str;
		ProvFunc *pf = &(fa[i]);
		tr = xmlNewChild (table, NULL, "tr", NULL);
		str = g_strdup_printf ("%s ()", pf->name);
		xmlNewChild (tr, NULL, "td", str);
		g_free (str);
		td = xmlNewChild (tr, NULL, "td", pf->func ? "Yes" : "No");
		if (pf->should_be && !pf->func)
			xmlSetProp(td, "class", (xmlChar*)"error");
	}
	
	/* meta data implementation */
	ProvFunc md[] = {
		{"_info", TRUE, (AFunc) pclass->meta_funcs._info},
 		{"_btypes", TRUE, (AFunc) pclass->meta_funcs._btypes},
		{"_udt", TRUE, (AFunc) pclass->meta_funcs._udt},
		{"udt", TRUE, (AFunc) pclass->meta_funcs.udt},
		{"_udt_cols", TRUE, (AFunc) pclass->meta_funcs._udt_cols},
		{"udt_cols", TRUE, (AFunc) pclass->meta_funcs.udt_cols},
		{"_enums", TRUE, (AFunc) pclass->meta_funcs._enums},
		{"enums", TRUE, (AFunc) pclass->meta_funcs.enums},
		{"_domains", TRUE, (AFunc) pclass->meta_funcs._domains},
		{"domains", TRUE, (AFunc) pclass->meta_funcs.domains},
		{"_constraints_dom", TRUE, (AFunc) pclass->meta_funcs._constraints_dom},
		{"constraints_dom", TRUE, (AFunc) pclass->meta_funcs.constraints_dom},
		{"_el_types", TRUE, (AFunc) pclass->meta_funcs._el_types},
		{"el_types", TRUE, (AFunc) pclass->meta_funcs.el_types},
		{"_collations", TRUE, (AFunc) pclass->meta_funcs._collations},
		{"collations", TRUE, (AFunc) pclass->meta_funcs.collations},
		{"_character_sets", TRUE, (AFunc) pclass->meta_funcs._character_sets},
		{"character_sets", TRUE, (AFunc) pclass->meta_funcs.character_sets},
		{"_schemata", TRUE, (AFunc) pclass->meta_funcs._schemata},
		{"schemata", TRUE, (AFunc) pclass->meta_funcs.schemata},
		{"_tables_views", TRUE, (AFunc) pclass->meta_funcs._tables_views},
		{"tables_views", TRUE, (AFunc) pclass->meta_funcs.tables_views},
		{"_columns", TRUE, (AFunc) pclass->meta_funcs._columns},
		{"columns", TRUE, (AFunc) pclass->meta_funcs.columns},
		{"_view_cols", TRUE, (AFunc) pclass->meta_funcs._view_cols},
		{"view_cols", TRUE, (AFunc) pclass->meta_funcs.view_cols},
		{"_constraints_tab", TRUE, (AFunc) pclass->meta_funcs._constraints_tab},
		{"constraints_tab", TRUE, (AFunc) pclass->meta_funcs.constraints_tab},
		{"_constraints_ref", TRUE, (AFunc) pclass->meta_funcs._constraints_ref},
		{"constraints_ref", TRUE, (AFunc) pclass->meta_funcs.constraints_ref},
		{"_key_columns", TRUE, (AFunc) pclass->meta_funcs._key_columns},
		{"key_columns", TRUE, (AFunc) pclass->meta_funcs.key_columns},
		{"_check_columns", TRUE, (AFunc) pclass->meta_funcs._check_columns},
		{"check_columns", TRUE, (AFunc) pclass->meta_funcs.check_columns},
		{"_triggers", TRUE, (AFunc) pclass->meta_funcs._triggers},
		{"triggers", TRUE, (AFunc) pclass->meta_funcs.triggers},
		{"_routines", TRUE, (AFunc) pclass->meta_funcs._routines},
		{"routines", TRUE, (AFunc) pclass->meta_funcs.routines},
		{"_routine_col", TRUE, (AFunc) pclass->meta_funcs._routine_col},
		{"routine_col", TRUE, (AFunc) pclass->meta_funcs.routine_col},
		{"_routine_par", TRUE, (AFunc) pclass->meta_funcs._routine_par},
		{"routine_par", TRUE, (AFunc) pclass->meta_funcs.routine_par},
	};
	hnode = xmlNewChild (file->body, NULL, "h3", "Meta data methods implementation");
	table = xmlNewChild (file->body, NULL, "table", NULL);

	tr = xmlNewChild (table, NULL, "tr", NULL);
	xmlNewChild (tr, NULL, "th", "Meta data's method");
	xmlNewChild (tr, NULL, "th", "Implemented ?");

	for (i = 0; i < sizeof (md) / sizeof (ProvFunc); i++) {
		gchar *str;
		ProvFunc *pf = &(md[i]);
		tr = xmlNewChild (table, NULL, "tr", NULL);
		str = g_strdup_printf ("%s ()", pf->name);
		xmlNewChild (tr, NULL, "td", str);
		g_free (str);
		td = xmlNewChild (tr, NULL, "td", pf->func ? "Yes" : "No");
		if (pf->should_be && !pf->func)
			xmlSetProp(td, "class", (xmlChar*)"error");
	}

	/* distributed transaction implementation */
	gboolean has_xa = gda_server_provider_supports_feature (prov, cnc, 
			  GDA_CONNECTION_FEATURE_XA_TRANSACTIONS);
	hnode = xmlNewChild (file->body, NULL, "h3", "Distributed transaction implementation");
	if (pclass->xa_funcs) {
		if (!has_xa) {
			xmlNodePtr para;
			para = xmlNewChild (file->body, NULL, "para", 
					    "The provider has the 'xa_funcs' part but "
					    "reports that distributed transactions are "
					    "not supported.");
			xmlSetProp(para, "class", (xmlChar*)"warning");
		}
			
		ProvFunc dt[] = {
			{"xa_start", TRUE, (AFunc) pclass->xa_funcs->xa_start},
			{"xa_end", FALSE, (AFunc) pclass->xa_funcs->xa_end},
			{"xa_prepare", TRUE, (AFunc) pclass->xa_funcs->xa_prepare},
			{"xa_commit", TRUE, (AFunc) pclass->xa_funcs->xa_commit},
			{"xa_rollback", TRUE, (AFunc) pclass->xa_funcs->xa_rollback},
			{"xa_recover", TRUE, (AFunc) pclass->xa_funcs->xa_recover},
		};
		
		table = xmlNewChild (file->body, NULL, "table", NULL);
		
		tr = xmlNewChild (table, NULL, "tr", NULL);
		xmlNewChild (tr, NULL, "th", "Meta data's method");
		xmlNewChild (tr, NULL, "th", "Implemented ?");
		
		for (i = 0; i < sizeof (dt) / sizeof (ProvFunc); i++) {
			gchar *str;
			ProvFunc *pf = &(dt[i]);
			tr = xmlNewChild (table, NULL, "tr", NULL);
			str = g_strdup_printf ("%s ()", pf->name);
			xmlNewChild (tr, NULL, "td", str);
			g_free (str);
			td = xmlNewChild (tr, NULL, "td", pf->func ? "Yes" : "No");
			if (pf->should_be && !pf->func)
				xmlSetProp(td, "class", (xmlChar*)"error");
		}
	}
	else {
		if (has_xa) {
			xmlNodePtr para;
			para = xmlNewChild (file->body, NULL, "para", 
					    "The provider does not have the 'xa_funcs' part but "
					    "reports that distributed transactions are "
					    "supported.");
			xmlSetProp(para, "class", (xmlChar*)"warning");
		}
		xmlNewChild (tr, NULL, "para", "Not implemented");
	}
	

	return TRUE;
}
