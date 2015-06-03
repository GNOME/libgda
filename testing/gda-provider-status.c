/*
 * Copyright (C) 2008 - 2014 Vivien Malerba <malerba@gnome-db.org>
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


#include <libgda/libgda.h>
#include <sql-parser/gda-sql-parser.h>
#include "html.h"
#include <libgda/gda-enum-types.h>
#include <libgda/sqlite/virtual/gda-virtual-provider.h>

/* options */
gboolean ask_pass = FALSE;
gchar *outfile = NULL;

static GOptionEntry entries[] = {
        { "no-password-ask", 'p', 0, G_OPTION_ARG_NONE, &ask_pass, "Don't ast for a password when it is empty", NULL },
        { "output-file", 'o', 0, G_OPTION_ARG_STRING, &outfile, "Output file", "output file"},
        { NULL, 0, 0, 0, NULL, NULL, NULL }
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

	/* parse command line arguments for connections */
	if (argc > 1) {
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
	}
	else {
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
		else {
			/* report status for all providers */
			GdaDataModel *providers;
			gint i, nb;
			
			providers = gda_config_list_providers ();
			nb = gda_data_model_get_n_rows (providers);
			for (i = 0; i < nb; i++) {
				GdaServerProvider *prov = NULL;
				const gchar *pname;
				const GValue *cvalue;

				cvalue = gda_data_model_get_value_at (providers, 0, i, &error);
				if (!cvalue) 
					g_error ("Can't load next provider: %s\n",
						 error && error->message ? error->message : "No detail");
				pname = g_value_get_string (cvalue);
				prov = gda_config_get_provider (pname, &error);
				if (!prov) 
					g_error ("Can't load the '%s' provider: %s\n", pname,
						 error && error->message ? error->message : "No detail");
				if (!report_provider_status (prov, NULL)) {
					exit_status = EXIT_FAILURE;
					goto cleanup;
				}
			}
			g_object_unref (providers);
		}
	}
	
	/* report provider's status for all the connections */
	for (list = cnc_list; list; list = list->next) {
		if (!report_provider_status (NULL, GDA_CONNECTION (list->data))) {
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

	GdaDsnInfo *info;
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
	
	info = gda_config_get_dsn_info (real_cnc);
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
	gchar *header_str;
	HtmlFile *file = config->index;
	gboolean is_virt;

	typedef void (*AFunc) (void);
	typedef struct {
		const gchar *name;
		gboolean     should_be;
		void       (*func) (void);
	} ProvFunc;
	GdaServerProviderClass *pclass;

	if (prov && cnc && (prov != gda_connection_get_provider (cnc)))
		/* ignoring connection as it has a different provider */
		return TRUE;
	g_assert (prov || cnc);

	/* section */
	if (cnc)
		header_str = g_strdup_printf ("Report for connection '%s'", gda_connection_get_cnc_string (cnc));
	else
		header_str = g_strdup_printf ("Report for '%s' provider", gda_server_provider_get_name (prov));

	/* provider info */
	if (!prov)
		prov = gda_connection_get_provider (cnc);
	is_virt = GDA_IS_VIRTUAL_PROVIDER (prov);
	pclass = (GdaServerProviderClass*) G_OBJECT_GET_CLASS (prov);
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
		{"supports_operation", is_virt ? FALSE : TRUE, (AFunc) pclass->supports_operation},
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
		{"identifier_quote", TRUE, (AFunc) pclass->identifier_quote}
	};

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
	gboolean has_xa = gda_server_provider_supports_feature (prov, cnc, 
								GDA_CONNECTION_FEATURE_XA_TRANSACTIONS);


	xmlNodePtr table, tr, td, span;
	GdaSqlParser *parser;
	GString *string;
	gsize i;
	GdaProviderInfo *pinfo;

	pinfo = gda_config_get_provider_info (gda_server_provider_get_name (prov));
	g_assert (pinfo);

	table = xmlNewChild (file->body, NULL, BAD_CAST "table", NULL);
	xmlSetProp (table, BAD_CAST "width", BAD_CAST "100%");
	tr = xmlNewChild (table, NULL, BAD_CAST "tr", NULL);
	td = xmlNewTextChild (tr, NULL, BAD_CAST "th", BAD_CAST header_str);
	xmlSetProp (td, BAD_CAST "colspan", BAD_CAST  "4");

	/* line 1 */
	tr = xmlNewChild (table, NULL, BAD_CAST "tr", NULL);
	xmlNewChild (tr, NULL, BAD_CAST "td", BAD_CAST "Provider's name:");
	td = xmlNewTextChild (tr, NULL, BAD_CAST "td", BAD_CAST gda_server_provider_get_name (prov));
	xmlSetProp (td, BAD_CAST "width", (xmlChar*) "35%");
	xmlNewChild (tr, NULL, BAD_CAST "td", BAD_CAST "Provider is virtual:");
	td = xmlNewChild (tr, NULL, BAD_CAST "td", BAD_CAST (is_virt ? "Yes (uses the SQLite engine)" : "No"));
	xmlSetProp (td, BAD_CAST "width", (xmlChar*) "35%");

	/* line 2 */
	tr = xmlNewChild (table, NULL, BAD_CAST "tr", NULL);
	xmlNewChild (tr, NULL, BAD_CAST "td", BAD_CAST "Provider's version:");
	xmlNewTextChild (tr, NULL, BAD_CAST "td", BAD_CAST gda_server_provider_get_version (prov));
	xmlNewChild (tr, NULL, BAD_CAST "td", BAD_CAST "Provider's server version:");
	xmlNewTextChild (tr, NULL, BAD_CAST "td",
			 BAD_CAST (cnc ? gda_server_provider_get_server_version (prov, cnc) : "(non connected)"));

	/* line 3 */
	tr = xmlNewChild (table, NULL, BAD_CAST "tr", NULL);
	xmlNewChild (tr, NULL, BAD_CAST "td", BAD_CAST "Provider's description:");
	xmlNewTextChild (tr, NULL, BAD_CAST "td", BAD_CAST pinfo->description);
	xmlNewChild (tr, NULL, BAD_CAST "td", BAD_CAST "Filename:");
	xmlNewTextChild (tr, NULL, BAD_CAST "td", BAD_CAST pinfo->location);

	/* line 4 */
	parser = gda_server_provider_create_parser (prov, cnc);
	tr = xmlNewChild (table, NULL, BAD_CAST "tr", NULL);
	xmlNewChild (tr, NULL, BAD_CAST "td", BAD_CAST "Creates its own SQL parser:");
	xmlNewChild (tr, NULL, BAD_CAST "td", BAD_CAST (parser ? "Yes" : "No"));
	if (parser)
		g_object_unref (parser);
	xmlNewChild (tr, NULL, BAD_CAST "td", BAD_CAST "Non implemented base methods:");
	span = NULL;
	td = xmlNewChild (tr, NULL, BAD_CAST "td", NULL);
	for (i = 0; i < sizeof (fa) / sizeof (ProvFunc); i++) {
		gchar *str;
		ProvFunc *pf = &(fa[i]);

		if (pf->func)
			continue;

		if (span)
			str = g_strdup_printf (", %s()", pf->name);
		else
			str = g_strdup_printf ("%s()", pf->name);
		span = xmlNewTextChild (td, NULL, BAD_CAST "span", BAD_CAST str);
		g_free (str);
		if (pf->should_be)
			xmlSetProp (span, BAD_CAST "class", BAD_CAST "error");
	}
	if (!span)
		xmlNodeSetContent (td, BAD_CAST "---");

	/* line 5 */
	tr = xmlNewChild (table, NULL, BAD_CAST "tr", NULL);
	xmlNewChild (tr, NULL, BAD_CAST "td", BAD_CAST "Non implemented meta data methods:");
	span = NULL;
	td = xmlNewChild (tr, NULL, BAD_CAST "td", NULL);
	for (i = 0; i < sizeof (md) / sizeof (ProvFunc); i++) {
		gchar *str;
		ProvFunc *pf = &(md[i]);

		if (pf->func)
			continue;

		if (span)
			str = g_strdup_printf (", %s()", pf->name);
		else
			str = g_strdup_printf ("%s()", pf->name);
		span = xmlNewTextChild (td, NULL, BAD_CAST "span", BAD_CAST str);
		g_free (str);
		if (pf->should_be)
			xmlSetProp (span, BAD_CAST "class", BAD_CAST "error");
	}
	if (!span)
		xmlNodeSetContent (td, BAD_CAST "---");

	xmlNewChild (tr, NULL, BAD_CAST "td", BAD_CAST "Non implemented XA transactions:");
	if (pclass->xa_funcs) {
		if (!has_xa) {
			td = xmlNewChild (tr, NULL, BAD_CAST "td", 
					  BAD_CAST "The provider has the 'xa_funcs' part but "
					  "reports that distributed transactions are "
					  "not supported.");
			xmlSetProp (td, BAD_CAST "class", BAD_CAST "warning");
		}
		else {
			ProvFunc dt[] = {
				{"xa_start", TRUE, (AFunc) pclass->xa_funcs->xa_start},
				{"xa_end", FALSE, (AFunc) pclass->xa_funcs->xa_end},
				{"xa_prepare", TRUE, (AFunc) pclass->xa_funcs->xa_prepare},
				{"xa_commit", TRUE, (AFunc) pclass->xa_funcs->xa_commit},
				{"xa_rollback", TRUE, (AFunc) pclass->xa_funcs->xa_rollback},
				{"xa_recover", TRUE, (AFunc) pclass->xa_funcs->xa_recover},
			};
			span = NULL;
			td = xmlNewChild (tr, NULL, BAD_CAST "td", NULL);
			for (i = 0; i < sizeof (dt) / sizeof (ProvFunc); i++) {
				gchar *str;
				ProvFunc *pf = &(dt[i]);
					
				if (pf->func)
					continue;
					
				if (span)
					str = g_strdup_printf (", %s()", pf->name);
				else
					str = g_strdup_printf ("%s()", pf->name);
				span = xmlNewTextChild (td, NULL, BAD_CAST "span", BAD_CAST str);
				g_free (str);
				if (pf->should_be)
					xmlSetProp (span, BAD_CAST "class", BAD_CAST "error");
			}
			if (!span)
				xmlNodeSetContent (td, BAD_CAST "---");
		}
	}
	else {
		if (has_xa) {
			td = xmlNewTextChild (tr, NULL, BAD_CAST "td",
					   BAD_CAST "The provider does not have the 'xa_funcs' part but "
					  "reports that distributed transactions are "
					  "supported.");
			xmlSetProp (td, BAD_CAST "class", BAD_CAST "warning");
		}
		else
			xmlNewChild (tr, NULL, BAD_CAST "td", BAD_CAST "---");
	}	

	/* line 6 */
	tr = xmlNewChild (table, NULL, BAD_CAST "tr", NULL);
	xmlNewChild (tr, NULL, BAD_CAST "td", BAD_CAST "Connection's parameters:");
	if (pinfo->dsn_params && pinfo->dsn_params->holders) {
		GSList *list;
		td = xmlNewChild (tr, NULL, BAD_CAST "td", NULL);
		for (list = pinfo->dsn_params->holders; list; list = list->next) {
			gchar *str, *descr;
			GdaHolder *holder = GDA_HOLDER (list->data);
			g_object_get (G_OBJECT (holder), "description", &descr, NULL);
			if (descr)
				str = g_strdup_printf ("%s: %s", gda_holder_get_id (holder), descr);
			else
				str = g_strdup (gda_holder_get_id (holder));
			g_free (descr);
			xmlNewTextChild (td, NULL, BAD_CAST "div", BAD_CAST str);
			g_free (str);
		}
	}
	else {
		td = xmlNewChild (tr, NULL, BAD_CAST "td", BAD_CAST "None provided");
		xmlSetProp (td, BAD_CAST "class", BAD_CAST "error");
	}
	xmlNewChild (tr, NULL, BAD_CAST "td", BAD_CAST "Authentication's parameters:");
	if (pinfo->auth_params) {
		GSList *list;
		if (pinfo->auth_params->holders) {
			td = xmlNewChild (tr, NULL, BAD_CAST "td", NULL);
			for (list = pinfo->auth_params->holders; list; list = list->next) {
				gchar *str, *descr;
				GdaHolder *holder = GDA_HOLDER (list->data);
				g_object_get (G_OBJECT (holder), "description", &descr, NULL);
				if (descr)
					str = g_strdup_printf ("%s: %s", gda_holder_get_id (holder), descr);
				else
					str = g_strdup (gda_holder_get_id (holder));
				g_free (descr);
				xmlNewTextChild (td, NULL, BAD_CAST "div", BAD_CAST str);
				g_free (str);
			}
		}
		else
			td = xmlNewChild (tr, NULL, BAD_CAST "td", BAD_CAST "None required");
	}
	else {
		td = xmlNewChild (tr, NULL, BAD_CAST "td", BAD_CAST "None provided");
		xmlSetProp (td, BAD_CAST "class", BAD_CAST "error");
	}

	/* line 7 */
	GdaConnectionFeature f;
	string = NULL;
	tr = xmlNewChild (table, NULL, BAD_CAST "tr", NULL);
	xmlNewChild (tr, NULL, BAD_CAST "td", BAD_CAST "Supported features:");
	for (f = 0; f < GDA_CONNECTION_FEATURE_LAST; f++) {
		if (gda_server_provider_supports_feature (prov, cnc, f)) {
			GEnumValue *ev;
			
			ev = g_enum_get_value ((GEnumClass *) g_type_class_ref (GDA_TYPE_CONNECTION_FEATURE), f);
			if (!string)
				string = g_string_new (ev->value_name);
			else
				g_string_append_printf (string, ", %s", ev->value_name);
		}
	}
	if (string) {
		xmlNewTextChild (tr, NULL, BAD_CAST "td", BAD_CAST string->str);
		g_string_free (string, TRUE);
	}
	else
		xmlNewChild (tr, NULL, BAD_CAST "td", BAD_CAST "---");

	string = NULL;
	xmlNewChild (tr, NULL, BAD_CAST "td", BAD_CAST "Unsupported features:");
	for (f = 0; f < GDA_CONNECTION_FEATURE_LAST; f++) {
		if (!gda_server_provider_supports_feature (prov, cnc, f)) {
			GEnumValue *ev;
			
			ev = g_enum_get_value ((GEnumClass *) g_type_class_ref (GDA_TYPE_CONNECTION_FEATURE), f);
			if (!string)
				string = g_string_new (ev->value_name);
			else
				g_string_append_printf (string, ", %s", ev->value_name);
		}
	}
	if (string) {
		xmlNewTextChild (tr, NULL, BAD_CAST "td", BAD_CAST string->str);
		g_string_free (string, TRUE);
	}
	else
		xmlNewChild (tr, NULL, BAD_CAST "td", BAD_CAST "---");
		
	g_free (header_str);

	return TRUE;
}
