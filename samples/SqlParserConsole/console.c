/*
 * Copyright (C) 2008 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
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
#include <gmodule.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <libgda/libgda.h>

#include <sql-parser/gda-sql-parser.h>
#include <json-glib/json-glib.h>
#include "graph.h"

#define MAX_BUFFER_SIZE 4000

static GdaSqlParser *create_parser_for_provider (const gchar *prov_name);

gchar *prov_name = NULL;

static GOptionEntry entries[] = {
	{ "prov", 'p', 0, G_OPTION_ARG_STRING, &prov_name, "Provider [SQLite|MySQL|PostgreSQL|...]", "provider"},
	{NULL}
};

int 
main (int argc,char** argv)
{
	gint size;
	char buf[MAX_BUFFER_SIZE+1];
	GError *cerror = NULL;
	GdaSqlParser *parser;
	gboolean display_as_graph = FALSE;

	GOptionContext *context;
	context = g_option_context_new ("Console");
        g_option_context_add_main_entries (context, entries, NULL);
        if (!g_option_context_parse (context, &argc, &argv, &cerror)) {
                g_print ("Can't parse arguments: %s\n", cerror->message);
		exit (1);
        }

	gda_init ();

	/* create a parser */
	parser = create_parser_for_provider (prov_name);

	g_print ("\nUse \\? for help.\n");

	g_print ("\n> ");
	fflush(stdout);
  
	while ((size = read (fileno (stdin), buf, MAX_BUFFER_SIZE)) >  0) {
		GdaBatch *batch;
		const gchar *remain = NULL;
		GError *error = NULL;

		buf[size]='\0';
		if (buf [size-1] == '\n')
			buf [size-1] = 0;

		if (!strcmp (buf, "\\?") || !strcmp (buf, "\\h")) {
#ifdef GDA_DEBUG
			g_print ("\t\\debug to turn parser debug ON or OFF\n");
#endif
			g_print ("\t\\d     to turn parser mode to DELIMIT\n");
			g_print ("\t\\p     to turn parser mode to PARSE\n");
			g_print ("\t\\graph to save statements to graph or to return to normal display\n");
			goto next;
		}
#ifdef GDA_DEBUG
		else if (!strcmp (buf, "\\debug")) {
			static gboolean debug = FALSE;
			debug = !debug;
			g_object_set (G_OBJECT (parser), "debug", debug, NULL);
			g_print ("Debug mode set to %s\n", debug ? "ON" : "OFF");
			goto next;
		}
#endif
		else if (!strcmp (buf, "\\p")) {
			g_object_set (G_OBJECT (parser), "mode", GDA_SQL_PARSER_MODE_PARSE, NULL);
			g_print ("Operating mode set to PARSE\n");
			goto next;
		}
		else if (!strcmp (buf, "\\d")) {
			g_object_set (G_OBJECT (parser), "mode", GDA_SQL_PARSER_MODE_DELIMIT, NULL);
			g_print ("Operating mode set to DELIMIT\n");
			goto next;
		}
		else if (!strcmp (buf, "\\graph")) {
			display_as_graph = !display_as_graph;
			goto next;
		}
        		
		batch = gda_sql_parser_parse_string_as_batch (parser, buf, &remain, &error);
		if (remain)
			g_print ("===> REMAIN: @%s@\n", remain);
		else
			g_print ("===> No REMAIN\n");
		if (!batch) {
			if (error && (error->domain == GDA_SQL_PARSER_ERROR) &&
			    (error->code == GDA_SQL_PARSER_EMPTY_SQL_ERROR));
			else
				g_print ("===> ERROR: %s\n", error && error->message ? error->message : "No detail");
			if (error)
				g_error_free (error);
		}
		else {
			const GSList *slist;
			for (slist = gda_batch_get_statements (batch); slist; slist = slist->next) {
				if (! gda_statement_check_structure (GDA_STATEMENT (slist->data), &error)) {
					g_print ("===> Statement structure error: %s\n", 
						 error && error->message ? error->message : "No detail");
					if (error) {
						g_error_free (error);
						error = NULL;
					}
				}
			}

			if (display_as_graph) {
				static gint id = 0;
				for (slist = gda_batch_get_statements (batch); slist; slist = slist->next) {
					GdaStatement *stmt = GDA_STATEMENT (slist->data);
					GdaSqlStatement *sqlst;
					gchar *dot;
					gchar *fname;
					
					g_object_get (G_OBJECT (stmt), "structure", &sqlst, NULL);
					dot = sql_statement_to_graph (sqlst);
					gda_sql_statement_free (sqlst);
					
					id++;
					fname = g_strdup_printf ("statement-%03d.dot", id);
					if (! g_file_set_contents (fname, dot, -1, &error)) {
						g_print ("===> Could not write to file '%s': %s\n", fname,
							 error && error->message ? error->message : "No detail");
						if (error) {
							g_error_free (error);
							error = NULL;
						}
					}
					else {
						g_print ("===> graph written to file '%s'\n", fname);
						g_print ("     convert to PNG with: dot -Tpng -ostatement-%03d.png %s\n", id, fname);
					}
					g_free (fname);
					g_free (dot);
				}
			}
			else {
				gchar *str;
				str = gda_batch_serialize (batch);
				g_print ("===> RESULT: %s\n", str);
				{
					JsonParser *jparser;
					
					g_print ("===> JSON:\n");
					jparser = json_parser_new ();
					if (!json_parser_load_from_data (jparser, str, -1, &error)) {
						g_print ("===> Hummm: %s\n", 
							 error && error->message ? error->message : "No detail");
						g_error_free (error);
					}
					else {
						JsonGenerator *jgen;
						gchar *out;
						jgen = json_generator_new ();
						g_object_set (G_OBJECT (jgen), "pretty", TRUE, "indent", 5, NULL);
						json_generator_set_root (jgen, json_parser_get_root (jparser));
						out = json_generator_to_data (jgen, NULL);
						g_print ("===> RESULT:\n%s\n", out);
						g_free (out);
						g_object_unref (jgen);
					}
					g_object_unref (jparser);
				}

				g_free (str);
			}
			g_object_unref (batch);	
		}

	next:
		printf("\n> ");
		fflush(stdout);
	}
	g_object_unref (parser);
  
	return 0;
}

static GdaSqlParser *
create_parser_for_provider (const gchar *prov_name)
{
	GdaServerProvider *prov;
	GdaSqlParser *parser;
	GError *error = NULL;

	prov = gda_config_get_provider (prov_name ? prov_name : "SQLite", &error);
	if (!prov) {
		g_print ("Could not instantiate provider for '%s': %s\n", prov_name,
			 error && error->message ? error->message : "No detail");
		if (error)
			g_error_free (error);
		prov = gda_config_get_provider ("SQLite", NULL);
		g_assert (prov);
	}
	parser = gda_server_provider_create_parser (prov, NULL);
	if (parser)
		g_print ("Parser started, %s SQL parser.", 
			 gda_server_provider_get_name (prov));
	else {
		parser = gda_sql_parser_new ();
		g_print ("Parser started.");
	}

	return parser;
}
